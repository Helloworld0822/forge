#include "optimize.h"

static Expr *fold_binary(BinOp op, Expr *l, Expr *r) {
    if (l->kind == EXPR_INT && r->kind == EXPR_INT) {
        int64_t a = l->as.int_val, b = r->as.int_val, v = 0;
        switch (op) {
        case BIN_ADD: v = a + b; break;
        case BIN_SUB: v = a - b; break;
        case BIN_MUL: v = a * b; break;
        case BIN_DIV:
            if (b == 0) return expr_binary(op, l, r);
            v = a / b;
            break;
        case BIN_MOD:
            if (b == 0) return expr_binary(op, l, r);
            v = a % b;
            break;
        case BIN_EQ: return expr_bool(a == b);
        case BIN_NE: return expr_bool(a != b);
        case BIN_LT: return expr_bool(a < b);
        case BIN_LE: return expr_bool(a <= b);
        case BIN_GT: return expr_bool(a > b);
        case BIN_GE: return expr_bool(a >= b);
        default: return expr_binary(op, l, r);
        }
        free(l);
        free(r);
        return expr_int(v);
    }
    if (l->kind == EXPR_FLOAT && r->kind == EXPR_FLOAT) {
        double a = l->as.float_val, b = r->as.float_val, v = 0;
        switch (op) {
        case BIN_ADD: v = a + b; break;
        case BIN_SUB: v = a - b; break;
        case BIN_MUL: v = a * b; break;
        case BIN_DIV:
            if (b == 0.0) return expr_binary(op, l, r);
            v = a / b;
            break;
        default: return expr_binary(op, l, r);
        }
        free(l);
        free(r);
        return expr_float(v);
    }
    if (l->kind == EXPR_BOOL && r->kind == EXPR_BOOL) {
        bool a = l->as.bool_val, b = r->as.bool_val, v = false;
        switch (op) {
        case BIN_EQ: v = a == b; break;
        case BIN_NE: v = a != b; break;
        case BIN_AND: v = a && b; break;
        case BIN_OR: v = a || b; break;
        default: return expr_binary(op, l, r);
        }
        free(l);
        free(r);
        return expr_bool(v);
    }
    return expr_binary(op, l, r);
}

static Expr *simplify_binary(BinOp op, Expr *l, Expr *r) {
    if (op == BIN_ADD) {
        if (l->kind == EXPR_INT && l->as.int_val == 0) { free(l); return r; }
        if (r->kind == EXPR_INT && r->as.int_val == 0) { free(r); return l; }
    }
    if (op == BIN_SUB && r->kind == EXPR_INT && r->as.int_val == 0) {
        free(r);
        return l;
    }
    if (op == BIN_MUL) {
        if (l->kind == EXPR_INT && l->as.int_val == 0) { free(l); return r; }
        if (r->kind == EXPR_INT && r->as.int_val == 0) { free(r); return l; }
        if (l->kind == EXPR_INT && l->as.int_val == 1) { free(l); return r; }
        if (r->kind == EXPR_INT && r->as.int_val == 1) { free(r); return l; }
    }
    return fold_binary(op, l, r);
}

static Expr *optimize_expr(Expr *e) {
    if (!e) return NULL;
    switch (e->kind) {
    case EXPR_BINARY:
        e->as.binary.left = optimize_expr(e->as.binary.left);
        e->as.binary.right = optimize_expr(e->as.binary.right);
        {
            Expr *folded = simplify_binary(e->as.binary.op, e->as.binary.left, e->as.binary.right);
            if (folded != e) {
                free(e);
                return folded;
            }
            e->type = e->as.binary.left->type;
            return e;
        }
    case EXPR_CALL:
        for (size_t i = 0; i < e->as.call.arg_count; i++)
            e->as.call.args[i] = optimize_expr(e->as.call.args[i]);
        return e;
    case EXPR_QUAL_CALL:
        for (size_t i = 0; i < e->as.qual_call.arg_count; i++)
            e->as.qual_call.args[i] = optimize_expr(e->as.qual_call.args[i]);
        return e;
    case EXPR_INDEX:
        e->as.index.base = optimize_expr(e->as.index.base);
        e->as.index.index = optimize_expr(e->as.index.index);
        return e;
    case EXPR_FIELD:
        e->as.field.base = optimize_expr(e->as.field.base);
        e->type = e->as.field.base->type;
        return e;
    default:
        return e;
    }
}

static void optimize_block(Block *b) {
    if (!b) return;
    for (Stmt *s = b->first; s; s = s->next) {
        switch (s->kind) {
        case STMT_LET:
            s->as.let.init = optimize_expr(s->as.let.init);
            break;
        case STMT_EXPR:
            s->as.expr = optimize_expr(s->as.expr);
            break;
        case STMT_RETURN:
            s->as.ret = optimize_expr(s->as.ret);
            break;
        case STMT_IF:
            s->as.if_stmt.cond = optimize_expr(s->as.if_stmt.cond);
            optimize_block(s->as.if_stmt.then_br);
            optimize_block(s->as.if_stmt.else_br);
            break;
        case STMT_WHILE:
            s->as.while_stmt.cond = optimize_expr(s->as.while_stmt.cond);
            optimize_block(s->as.while_stmt.body);
            break;
        case STMT_FOR:
            if (s->as.for_stmt.init) optimize_block(&(Block){ s->as.for_stmt.init, s->as.for_stmt.init });
            s->as.for_stmt.cond = optimize_expr(s->as.for_stmt.cond);
            if (s->as.for_stmt.step) optimize_block(&(Block){ s->as.for_stmt.step, s->as.for_stmt.step });
            optimize_block(s->as.for_stmt.body);
            break;
        case STMT_SPAWN:
            for (size_t i = 0; i < s->as.spawn.arg_count; i++)
                s->as.spawn.args[i] = optimize_expr(s->as.spawn.args[i]);
            break;
        case STMT_SEND:
            s->as.send.target = optimize_expr(s->as.send.target);
            s->as.send.value = optimize_expr(s->as.send.value);
            break;
        case STMT_ASSIGN:
            s->as.assign.value = optimize_expr(s->as.assign.value);
            break;
        case STMT_BLOCK:
            optimize_block(s->as.block);
            break;
        default:
            break;
        }
    }
}

void optimize_program(Program *prog) {
    for (size_t i = 0; i < prog->fn_count; i++)
        if (!prog->functions[i].is_extern) optimize_block(&prog->functions[i].body);
    for (size_t i = 0; i < prog->native_count; i++)
        optimize_block(&prog->natives[i].body);
    for (size_t i = 0; i < prog->process_count; i++) {
        ProcessDecl *pd = &prog->processes[i];
        optimize_block(&pd->body);
        for (size_t j = 0; j < pd->coro_count; j++)
            optimize_block(&pd->coros[j].body);
        optimize_block(pd->on_receive);
    }
    if (prog->library.present) {
        for (size_t i = 0; i < prog->library.fn_count; i++)
            optimize_block(&prog->library.functions[i].body);
    }
}
