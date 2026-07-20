#include "ast.h"

HyloType hylo_type_void(void) { return (HyloType){ TY_VOID }; }
HyloType hylo_type_int(void) { return (HyloType){ TY_INT }; }
HyloType hylo_type_float(void) { return (HyloType){ TY_FLOAT }; }
HyloType hylo_type_bool(void) { return (HyloType){ TY_BOOL }; }
HyloType hylo_type_string(void) { return (HyloType){ TY_STRING }; }

HyloType hylo_type_from_name(HyloStr name) {
    if (hylo_str_eq(name, hylo_str("int"))) return hylo_type_int();
    if (hylo_str_eq(name, hylo_str("float"))) return hylo_type_float();
    if (hylo_str_eq(name, hylo_str("bool"))) return hylo_type_bool();
    if (hylo_str_eq(name, hylo_str("string"))) return hylo_type_string();
    if (hylo_str_eq(name, hylo_str("void"))) return hylo_type_void();
    return hylo_type_void();
}

static Expr *expr_new(ExprKind kind) {
    Expr *e = (Expr *)calloc(1, sizeof(Expr));
    if (!e) hylo_die("out of memory");
    e->kind = kind;
    return e;
}

Expr *expr_int(int64_t v) {
    Expr *e = expr_new(EXPR_INT);
    e->type = hylo_type_int();
    e->as.int_val = v;
    return e;
}

Expr *expr_float(double v) {
    Expr *e = expr_new(EXPR_FLOAT);
    e->type = hylo_type_float();
    e->as.float_val = v;
    return e;
}

Expr *expr_bool(bool v) {
    Expr *e = expr_new(EXPR_BOOL);
    e->type = hylo_type_bool();
    e->as.bool_val = v;
    return e;
}

Expr *expr_string(HyloStr v) {
    Expr *e = expr_new(EXPR_STRING);
    e->type = hylo_type_string();
    e->as.string_val = v;
    return e;
}

Expr *expr_ident(HyloStr name) {
    Expr *e = expr_new(EXPR_IDENT);
    e->as.ident = name;
    return e;
}

Expr *expr_binary(BinOp op, Expr *l, Expr *r) {
    Expr *e = expr_new(EXPR_BINARY);
    e->as.binary.op = op;
    e->as.binary.left = l;
    e->as.binary.right = r;
    e->type = l->type;
    return e;
}

Expr *expr_call(HyloStr name, Expr **args, size_t n) {
    Expr *e = expr_new(EXPR_CALL);
    e->as.call.name = name;
    e->as.call.args = args;
    e->as.call.arg_count = n;
    return e;
}

Expr *expr_qual_call(HyloStr module, HyloStr name, Expr **args, size_t n) {
    Expr *e = expr_new(EXPR_QUAL_CALL);
    e->as.qual_call.module = module;
    e->as.qual_call.name = name;
    e->as.qual_call.args = args;
    e->as.qual_call.arg_count = n;
    return e;
}

Expr *expr_recv(void) {
    Expr *e = expr_new(EXPR_RECV);
    e->type = hylo_type_int();
    return e;
}

Block block_new(void) {
    return (Block){ NULL, NULL };
}

void block_append(Block *b, Stmt *s) {
    if (!b->first) {
        b->first = b->last = s;
    } else {
        b->last->next = s;
        b->last = s;
    }
}

static Stmt *stmt_new(int kind) {
    Stmt *s = (Stmt *)calloc(1, sizeof(Stmt));
    if (!s) hylo_die("out of memory");
    s->kind = kind;
    return s;
}

Stmt *stmt_let(bool mut, HyloStr name, HyloType ty, Expr *init) {
    Stmt *s = stmt_new(STMT_LET);
    s->as.let.mutable_ = mut;
    s->as.let.name = name;
    s->as.let.type = ty;
    s->as.let.init = init;
    return s;
}

Stmt *stmt_expr(Expr *e) {
    Stmt *s = stmt_new(STMT_EXPR);
    s->as.expr = e;
    return s;
}

Stmt *stmt_return(Expr *e) {
    Stmt *s = stmt_new(STMT_RETURN);
    s->as.ret = e;
    return s;
}

Stmt *stmt_if(Expr *cond, Block *then_br, Block *else_br) {
    Stmt *s = stmt_new(STMT_IF);
    s->as.if_stmt.cond = cond;
    s->as.if_stmt.then_br = then_br;
    s->as.if_stmt.else_br = else_br;
    return s;
}

Stmt *stmt_while(Expr *cond, Block *body) {
    Stmt *s = stmt_new(STMT_WHILE);
    s->as.while_stmt.cond = cond;
    s->as.while_stmt.body = body;
    return s;
}

Stmt *stmt_spawn(HyloStr name, Expr **args, size_t n) {
    Stmt *s = stmt_new(STMT_SPAWN);
    s->as.spawn.coro_name = name;
    s->as.spawn.args = args;
    s->as.spawn.arg_count = n;
    return s;
}

Stmt *stmt_send(Expr *target, int tag, Expr *value) {
    Stmt *s = stmt_new(STMT_SEND);
    s->as.send.target = target;
    s->as.send.tag = tag;
    s->as.send.value = value;
    return s;
}

Stmt *stmt_yield(void) {
    return stmt_new(STMT_YIELD);
}

Stmt *stmt_assign(HyloStr name, Expr *value) {
    Stmt *s = stmt_new(STMT_ASSIGN);
    s->as.assign.name = name;
    s->as.assign.value = value;
    return s;
}

Stmt *stmt_block(Block *b) {
    Stmt *s = stmt_new(STMT_BLOCK);
    s->as.block = b;
    return s;
}

static void free_expr(Expr *e) {
    if (!e) return;
    switch (e->kind) {
    case EXPR_BINARY:
        free_expr(e->as.binary.left);
        free_expr(e->as.binary.right);
        break;
    case EXPR_CALL:
        for (size_t i = 0; i < e->as.call.arg_count; i++) free_expr(e->as.call.args[i]);
        free(e->as.call.args);
        break;
    case EXPR_QUAL_CALL:
        for (size_t i = 0; i < e->as.qual_call.arg_count; i++) free_expr(e->as.qual_call.args[i]);
        free(e->as.qual_call.args);
        break;
    default:
        break;
    }
    free(e);
}

static void free_stmts(Stmt *s) {
    while (s) {
        Stmt *next = s->next;
        switch (s->kind) {
        case STMT_LET: free_expr(s->as.let.init); break;
        case STMT_EXPR: free_expr(s->as.expr); break;
        case STMT_RETURN: free_expr(s->as.ret); break;
        case STMT_IF:
            free_expr(s->as.if_stmt.cond);
            free_stmts(s->as.if_stmt.then_br->first);
            free(s->as.if_stmt.then_br);
            if (s->as.if_stmt.else_br) {
                free_stmts(s->as.if_stmt.else_br->first);
                free(s->as.if_stmt.else_br);
            }
            break;
        case STMT_WHILE:
            free_expr(s->as.while_stmt.cond);
            free_stmts(s->as.while_stmt.body->first);
            free(s->as.while_stmt.body);
            break;
        case STMT_SPAWN:
            for (size_t i = 0; i < s->as.spawn.arg_count; i++) free_expr(s->as.spawn.args[i]);
            free(s->as.spawn.args);
            break;
        case STMT_SEND:
            free_expr(s->as.send.target);
            free_expr(s->as.send.value);
            break;
        case STMT_ASSIGN:
            free_expr(s->as.assign.value);
            break;
        case STMT_BLOCK:
            free_stmts(s->as.block->first);
            free(s->as.block);
            break;
        default:
            break;
        }
        free(s);
        s = next;
    }
}

void program_free(Program *p) {
    free(p->imports);
    if (p->library.present) {
        free(p->library.imports);
        for (size_t i = 0; i < p->library.fn_count; i++) {
            free_stmts(p->library.functions[i].body.first);
        }
        free(p->library.functions);
    }
    for (size_t i = 0; i < p->process_count; i++) {
        ProcessDecl *pd = &p->processes[i];
        free_stmts(pd->body.first);
        for (size_t j = 0; j < pd->coro_count; j++) {
            free_stmts(pd->coros[j].body.first);
        }
        free(pd->coros);
        if (pd->has_receive) {
            free_stmts(pd->on_receive->first);
            free(pd->on_receive);
        }
    }
    free(p->processes);
    for (size_t i = 0; i < p->fn_count; i++) {
        free_stmts(p->functions[i].body.first);
    }
    free(p->functions);
    for (size_t i = 0; i < p->supervisor_count; i++) {
        free(p->supervisors[i].children);
    }
    free(p->supervisors);
}
