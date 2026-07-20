#include "codegen.h"
#include "mod_registry.h"
#include <stdarg.h>
#include <string.h>

typedef struct {
    FILE *out;
    int indent;
    Program *prog;
    const char *state_prefix;
    struct ForgeLocal { ForgeStr name; ForgeType type; } *locals;
    size_t local_count;
    size_t local_cap;
    ForgeStr *imports;
    size_t import_count;
} Codegen;

static void cg_push_local(Codegen *cg, ForgeStr name, ForgeType ty) {
    for (size_t i = 0; i < cg->local_count; i++) {
        if (forge_str_eq(cg->locals[i].name, name)) {
            cg->locals[i].type = ty;
            return;
        }
    }
    if (cg->local_count == cg->local_cap) {
        cg->local_cap = cg->local_cap ? cg->local_cap * 2 : 8;
        cg->locals = (struct ForgeLocal *)realloc(cg->locals, cg->local_cap * sizeof(struct ForgeLocal));
    }
    cg->locals[cg->local_count].name = name;
    cg->locals[cg->local_count].type = ty;
    cg->local_count++;
}

static ForgeType cg_lookup_local(Codegen *cg, ForgeStr name) {
    for (size_t i = 0; i < cg->local_count; i++) {
        if (forge_str_eq(cg->locals[i].name, name)) return cg->locals[i].type;
    }
    return forge_type_int();
}

static int cg_stdlib_returns_string(Codegen *cg, ForgeStr name) {
    const char *mapped = forge_std_c_name(name, cg->imports, cg->import_count);
    if (!mapped) return 0;
    static const char *str_fns[] = {
        "fr_str_concat", "fr_str_sub", "fr_str_trim", "fr_fs_read",
        "fr_tcp_recv", "fr_udp_recv", "fr_http_get", "fr_http_post",
        "fr_http_req_method", "fr_http_req_path", "fr_http_req_body",
        "fr_json_get_string", "fr_json_stringify_str", "fr_json_stringify_int",
        "fr_udp_peer", "fr_os_getenv", "fr_os_argv", NULL
    };
    for (int i = 0; str_fns[i]; i++) {
        if (strcmp(mapped, str_fns[i]) == 0) return 1;
    }
    return 0;
}

static int qual_call_returns_string(ForgeStr fn) {
    static const char *names[] = {
        "hello", "greet", "shout", "format", "read", "recv", "get", "path", "body",
        "peer", "concat", "trim", "sub", "message", NULL
    };
    for (int i = 0; names[i]; i++) {
        if (forge_str_eq(fn, forge_str(names[i]))) return 1;
    }
    return 0;
}

static int cg_expr_is_string(Codegen *cg, Expr *e) {
    if (e->kind == EXPR_STRING) return 1;
    if (e->kind == EXPR_IDENT) return cg_lookup_local(cg, e->as.ident).kind == TY_STRING;
    if (e->kind == EXPR_CALL) return cg_stdlib_returns_string(cg, e->as.call.name);
    if (e->kind == EXPR_QUAL_CALL) return qual_call_returns_string(e->as.qual_call.name);
    return 0;
}

static void cg_indent(Codegen *cg) {
    for (int i = 0; i < cg->indent; i++) fputs("    ", cg->out);
}

static void cg_line(Codegen *cg, const char *fmt, ...) {
    cg_indent(cg);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(cg->out, fmt, ap);
    va_end(ap);
    fputc('\n', cg->out);
}

static const char *c_type(ForgeType ty) {
    switch (ty.kind) {
    case TY_INT: return "int64_t";
    case TY_FLOAT: return "double";
    case TY_BOOL: return "int";
    case TY_STRING: return "const char*";
    default: return "void";
    }
}

static void emit_expr(Codegen *cg, Expr *e);
static void emit_stmts(Codegen *cg, Stmt *s, const char *proc_var, bool in_coro, const char *state_var);
static CoroDecl *find_coro(ProcessDecl *proc, ForgeStr name);

static void emit_expr(Codegen *cg, Expr *e) {
    switch (e->kind) {
    case EXPR_INT:
        fprintf(cg->out, "%lld", (long long)e->as.int_val);
        break;
    case EXPR_FLOAT:
        fprintf(cg->out, "%g", e->as.float_val);
        break;
    case EXPR_BOOL:
        fprintf(cg->out, "%s", e->as.bool_val ? "1" : "0");
        break;
    case EXPR_STRING: {
        fputc('"', cg->out);
        for (size_t i = 0; i < e->as.string_val.len; i++) {
            char c = e->as.string_val.data[i];
            if (c == '"' || c == '\\') fputc('\\', cg->out);
            fputc(c, cg->out);
        }
        fputc('"', cg->out);
        break;
    }
    case EXPR_IDENT:
        if (cg->state_prefix) fputs(cg->state_prefix, cg->out);
        fprintf(cg->out, "%.*s", (int)e->as.ident.len, e->as.ident.data);
        break;
    case EXPR_BINARY: {
        const char *op = "?";
        switch (e->as.binary.op) {
        case BIN_ADD: op = "+"; break;
        case BIN_SUB: op = "-"; break;
        case BIN_MUL: op = "*"; break;
        case BIN_DIV: op = "/"; break;
        case BIN_MOD: op = "%"; break;
        case BIN_EQ: op = "=="; break;
        case BIN_NE: op = "!="; break;
        case BIN_LT: op = "<"; break;
        case BIN_LE: op = "<="; break;
        case BIN_GT: op = ">"; break;
        case BIN_GE: op = ">="; break;
        case BIN_AND: op = "&&"; break;
        case BIN_OR: op = "||"; break;
        }
        fputc('(', cg->out);
        emit_expr(cg, e->as.binary.left);
        fprintf(cg->out, " %s ", op);
        emit_expr(cg, e->as.binary.right);
        fputc(')', cg->out);
        break;
    }
    case EXPR_CALL: {
        ForgeStr name = e->as.call.name;
        if (forge_str_eq(name, forge_str("println"))) {
            fputs("({ ", cg->out);
            for (size_t i = 0; i < e->as.call.arg_count; i++) {
                Expr *arg = e->as.call.args[i];
                if (i > 0) fputs(" ", cg->out);
                if (cg_expr_is_string(cg, arg)) {
                    fputs("fr_print_str(", cg->out);
                    emit_expr(cg, arg);
                    fputs("); ", cg->out);
                } else {
                    fputs("fr_print_int(", cg->out);
                    emit_expr(cg, arg);
                    fputs("); ", cg->out);
                }
            }
            fputs("fr_println(); })", cg->out);
            break;
        }
        const char *c_fn = forge_std_c_name(name, cg->imports, cg->import_count);
        if (c_fn) {
            fprintf(cg->out, "%s(", c_fn);
        } else {
            fprintf(cg->out, "%.*s(", (int)name.len, name.data);
        }
        for (size_t i = 0; i < e->as.call.arg_count; i++) {
            if (i) fputc(',', cg->out);
            emit_expr(cg, e->as.call.args[i]);
        }
        fputc(')', cg->out);
        break;
    }
    case EXPR_RECV:
        fputs("(fr_recv(fr_coro_process(__coro), &__recv_msg), __recv_msg.value)", cg->out);
        break;
    case EXPR_QUAL_CALL: {
        char sym[128];
        forge_lib_mangle(sym, sizeof(sym), e->as.qual_call.module, e->as.qual_call.name);
        fprintf(cg->out, "%s(", sym);
        for (size_t i = 0; i < e->as.qual_call.arg_count; i++) {
            if (i) fputc(',', cg->out);
            emit_expr(cg, e->as.qual_call.args[i]);
        }
        fputc(')', cg->out);
        break;
    }
    }
}

static CoroDecl *find_coro(ProcessDecl *proc, ForgeStr name) {
    for (size_t i = 0; i < proc->coro_count; i++) {
        if (forge_str_eq(proc->coros[i].name, name)) return &proc->coros[i];
    }
    return NULL;
}

static void emit_coro_state_struct(Codegen *cg, CoroDecl *coro) {
    fprintf(cg->out, "typedef struct {\n");
    fprintf(cg->out, "    int _forge_step;\n");
    fprintf(cg->out, "    fr_process_t *proc;\n");
    for (Param *p = coro->params; p; p = p->next) {
        fprintf(cg->out, "    %s %.*s;\n", c_type(p->type), (int)p->name.len, p->name.data);
    }
    for (Stmt *s = coro->body.first; s; s = s->next) {
        if (s->kind == STMT_LET) {
            fprintf(cg->out, "    %s %.*s;\n", c_type(s->as.let.type),
                    (int)s->as.let.name.len, s->as.let.name.data);
        }
    }
    fprintf(cg->out, "} %.*s_state_t;\n\n", (int)coro->name.len, coro->name.data);
}

static int count_yield_points(Stmt *s) {
    int n = 0;
    while (s) {
        switch (s->kind) {
        case STMT_YIELD: n++; break;
        case STMT_IF:
            n += count_yield_points(s->as.if_stmt.then_br->first);
            if (s->as.if_stmt.else_br) n += count_yield_points(s->as.if_stmt.else_br->first);
            break;
        case STMT_WHILE: n += count_yield_points(s->as.while_stmt.body->first); break;
        case STMT_BLOCK: n += count_yield_points(s->as.block->first); break;
        default: break;
        }
        s = s->next;
    }
    return n;
}

static void emit_coro_body(Codegen *cg, CoroDecl *coro, const char *state_var) {
    int step = 0;
    Stmt *s = coro->body.first;
    while (s) {
        if (s->kind == STMT_YIELD) {
            cg_line(cg, "case %d:", step++);
            cg_line(cg, "    fr_coro_set_step(__coro, %d);", step);
            cg_line(cg, "    return fr_yield(__coro);");
            cg_line(cg, "case %d:", step);
            s = s->next;
            continue;
        }

        if (s->kind == STMT_LET) {
            if (step == 0 && s == coro->body.first) {
                /* locals declared in struct */
            } else {
                cg_line(cg, "%s %.*s = ", c_type(s->as.let.type),
                        (int)s->as.let.name.len, s->as.let.name.data);
                if (s->as.let.init) emit_expr(cg, s->as.let.init);
                else fputs("0", cg->out);
                fputc(';', cg->out);
                fputc('\n', cg->out);
            }
            s = s->next;
            continue;
        }

        if (count_yield_points(s) > 0 || s->kind == STMT_IF || s->kind == STMT_WHILE || s->kind == STMT_BLOCK) {
            cg_line(cg, "case %d:", step++);
        }

        switch (s->kind) {
        case STMT_EXPR:
            cg_indent(cg);
            emit_expr(cg, s->as.expr);
            fputs(";\n", cg->out);
            break;
        case STMT_RETURN:
            cg_indent(cg);
            if (s->as.ret) emit_expr(cg, s->as.ret);
            fputs(";\n", cg->out);
            cg_line(cg, "return FR_CORO_DONE;");
            break;
        case STMT_IF:
            cg_line(cg, "if (");
            emit_expr(cg, s->as.if_stmt.cond);
            fputs(") {\n", cg->out);
            cg->indent++;
            emit_coro_body(cg, &(CoroDecl){ .body = *s->as.if_stmt.then_br }, state_var);
            cg->indent--;
            cg_indent(cg);
            fputs("}", cg->out);
            if (s->as.if_stmt.else_br) {
                fputs(" else {\n", cg->out);
                cg->indent++;
                emit_coro_body(cg, &(CoroDecl){ .body = *s->as.if_stmt.else_br }, state_var);
                cg->indent--;
                cg_line(cg, "}");
            } else {
                fputc('\n', cg->out);
            }
            break;
        case STMT_WHILE:
            cg_line(cg, "while (");
            emit_expr(cg, s->as.while_stmt.cond);
            fputs(") {\n", cg->out);
            cg->indent++;
            emit_coro_body(cg, &(CoroDecl){ .body = *s->as.while_stmt.body }, state_var);
            cg->indent--;
            cg_line(cg, "}");
            break;
        case STMT_SPAWN:
            cg_line(cg, "%.*s_spawn(%s->proc", (int)s->as.spawn.coro_name.len,
                    s->as.spawn.coro_name.data, state_var);
            for (size_t i = 0; i < s->as.spawn.arg_count; i++) {
                fputs(", ", cg->out);
                emit_expr(cg, s->as.spawn.args[i]);
            }
            fputs(");\n", cg->out);
            break;
        case STMT_SEND:
            cg_indent(cg);
            fputs("fr_send(", cg->out);
            emit_expr(cg, s->as.send.target);
            fprintf(cg->out, ", %d, ", s->as.send.tag);
            emit_expr(cg, s->as.send.value);
            fputs(", NULL, 0);\n", cg->out);
            break;
        case STMT_ASSIGN:
            cg_indent(cg);
            if (cg->state_prefix) fputs(cg->state_prefix, cg->out);
            fprintf(cg->out, "%.*s = ", (int)s->as.assign.name.len, s->as.assign.name.data);
            emit_expr(cg, s->as.assign.value);
            fputs(";\n", cg->out);
            break;
        case STMT_BLOCK:
            cg->indent++;
            emit_coro_body(cg, &(CoroDecl){ .body = *s->as.block }, state_var);
            cg->indent--;
            break;
        default:
            break;
        }
        s = s->next;
    }
}

static void emit_coro_fn(Codegen *cg, ProcessDecl *proc, CoroDecl *coro) {
    emit_coro_state_struct(cg, coro);
    fprintf(cg->out, "static fr_coro_status_t %.*s_fn(fr_coro_t *__coro, void *__userdata) {\n",
            (int)coro->name.len, coro->name.data);
    fprintf(cg->out, "    %.*s_state_t *%s = (%.*s_state_t *)__userdata;\n",
            (int)coro->name.len, coro->name.data, "st",
            (int)coro->name.len, coro->name.data);
    fputs("    fr_msg_t __recv_msg;\n", cg->out);
    fputs("    switch (fr_coro_step(__coro)) {\n", cg->out);
    cg->indent++;
    cg_line(cg, "default:");
    const char *saved_prefix = cg->state_prefix;
    cg->state_prefix = "st->";
    cg->local_count = 0;
    for (Param *p = coro->params; p; p = p->next) cg_push_local(cg, p->name, p->type);
    for (Stmt *s = coro->body.first; s; s = s->next) {
        if (s->kind == STMT_LET) cg_push_local(cg, s->as.let.name, s->as.let.type);
    }
    emit_coro_body(cg, coro, "st");
    cg->state_prefix = saved_prefix;
    cg->local_count = 0;
    cg->indent--;
    fputs("    }\n", cg->out);
    fputs("    return FR_CORO_DONE;\n", cg->out);
    fputs("}\n\n", cg->out);

    fprintf(cg->out, "static void %.*s_spawn(fr_process_t *proc", (int)coro->name.len, coro->name.data);
    for (Param *p = coro->params; p; p = p->next) {
        fprintf(cg->out, ", %s %.*s", c_type(p->type), (int)p->name.len, p->name.data);
    }
    fputs(") {\n", cg->out);
    fprintf(cg->out, "    %.*s_state_t *init = (%.*s_state_t *)calloc(1, sizeof(%.*s_state_t));\n",
            (int)coro->name.len, coro->name.data,
            (int)coro->name.len, coro->name.data,
            (int)coro->name.len, coro->name.data);
    fputs("    init->proc = proc;\n", cg->out);
    for (Param *p = coro->params; p; p = p->next) {
        fprintf(cg->out, "    init->%.*s = %.*s;\n", (int)p->name.len, p->name.data,
                (int)p->name.len, p->name.data);
    }
    for (Stmt *s = coro->body.first; s; s = s->next) {
        if (s->kind == STMT_LET && s->as.let.init) {
            cg_indent(cg);
            fprintf(cg->out, "init->%.*s = ", (int)s->as.let.name.len, s->as.let.name.data);
            const char *saved = cg->state_prefix;
            cg->state_prefix = "init->";
            emit_expr(cg, s->as.let.init);
            cg->state_prefix = saved;
            fputs(";\n", cg->out);
        }
    }
    fprintf(cg->out, "    fr_coro_spawn(proc, %.*s_fn, init, sizeof(%.*s_state_t));\n",
            (int)coro->name.len, coro->name.data,
            (int)coro->name.len, coro->name.data);
    fputs("}\n\n", cg->out);
}

static void emit_stmts(Codegen *cg, Stmt *s, const char *proc_var, bool in_coro, const char *state_var) {
    while (s) {
        switch (s->kind) {
        case STMT_LET:
            cg_push_local(cg, s->as.let.name, s->as.let.type);
            cg_indent(cg);
            fprintf(cg->out, "%s %.*s", c_type(s->as.let.type),
                    (int)s->as.let.name.len, s->as.let.name.data);
            if (s->as.let.init) {
                fputs(" = ", cg->out);
                emit_expr(cg, s->as.let.init);
            }
            fputs(";\n", cg->out);
            break;
        case STMT_EXPR:
            cg_indent(cg);
            emit_expr(cg, s->as.expr);
            fputs(";\n", cg->out);
            break;
        case STMT_RETURN:
            cg_indent(cg);
            fputs("return ", cg->out);
            if (s->as.ret) emit_expr(cg, s->as.ret);
            fputs(";\n", cg->out);
            break;
        case STMT_IF:
            cg_indent(cg);
            fputs("if (", cg->out);
            emit_expr(cg, s->as.if_stmt.cond);
            fputs(") {\n", cg->out);
            cg->indent++;
            emit_stmts(cg, s->as.if_stmt.then_br->first, proc_var, in_coro, state_var);
            cg->indent--;
            cg_indent(cg);
            fputs("}", cg->out);
            if (s->as.if_stmt.else_br) {
                fputs(" else {\n", cg->out);
                cg->indent++;
                emit_stmts(cg, s->as.if_stmt.else_br->first, proc_var, in_coro, state_var);
                cg->indent--;
                cg_line(cg, "}");
            } else {
                fputc('\n', cg->out);
            }
            break;
        case STMT_WHILE:
            cg_indent(cg);
            fputs("while (", cg->out);
            emit_expr(cg, s->as.while_stmt.cond);
            fputs(") {\n", cg->out);
            cg->indent++;
            emit_stmts(cg, s->as.while_stmt.body->first, proc_var, in_coro, state_var);
            cg->indent--;
            cg_line(cg, "}");
            break;
        case STMT_SPAWN:
            cg_indent(cg);
            fprintf(cg->out, "%.*s_spawn(%s", (int)s->as.spawn.coro_name.len,
                    s->as.spawn.coro_name.data, proc_var);
            for (size_t i = 0; i < s->as.spawn.arg_count; i++) {
                fputs(", ", cg->out);
                emit_expr(cg, s->as.spawn.args[i]);
            }
            fputs(");\n", cg->out);
            break;
        case STMT_SEND:
            cg_indent(cg);
            fputs("fr_send(", cg->out);
            emit_expr(cg, s->as.send.target);
            fprintf(cg->out, ", %d, ", s->as.send.tag);
            emit_expr(cg, s->as.send.value);
            fputs(", NULL, 0);\n", cg->out);
            break;
        case STMT_YIELD:
            if (in_coro) cg_line(cg, "return fr_yield(__coro);");
            break;
        case STMT_ASSIGN:
            cg_indent(cg);
            fprintf(cg->out, "%.*s = ", (int)s->as.assign.name.len, s->as.assign.name.data);
            emit_expr(cg, s->as.assign.value);
            fputs(";\n", cg->out);
            break;
        case STMT_BLOCK:
            cg->indent++;
            emit_stmts(cg, s->as.block->first, proc_var, in_coro, state_var);
            cg->indent--;
            break;
        }
        s = s->next;
    }
}

static ProcessDecl *find_process(Program *prog, ForgeStr name) {
    for (size_t i = 0; i < prog->process_count; i++) {
        if (forge_str_eq(prog->processes[i].name, name)) return &prog->processes[i];
    }
    return NULL;
}

static void emit_import_headers(Program *prog, FILE *out) {
    for (size_t i = 0; i < prog->import_count; i++) {
        const char *hdr = forge_std_header(prog->imports[i]);
        if (hdr) fprintf(out, "#include \"%s\"\n", hdr);
        else fprintf(out, "#include \"%.*s.h\"\n", (int)prog->imports[i].len, prog->imports[i].data);
    }
}

static void emit_fn_signature(FILE *out, const char *name, FnDecl *fn) {
    fprintf(out, "%s %s(", c_type(fn->ret_type), name);
    Param *p = fn->params;
    bool first = true;
    while (p) {
        if (!first) fputs(", ", out);
        fprintf(out, "%s %.*s", c_type(p->type), (int)p->name.len, p->name.data);
        first = false;
        p = p->next;
    }
    fputs(")", out);
}

void codegen_emit_library(Program *prog, FILE *out_c, FILE *out_h, const char *runtime_include) {
    if (!prog->library.present) {
        fprintf(stderr, "forge: no library block found\n");
        exit(1);
    }
    LibraryDecl *lib = &prog->library;
    char guard[128];
    snprintf(guard, sizeof(guard), "FRLIB_%.*s_H", (int)lib->name.len, lib->name.data);
    for (char *p = guard; *p; p++) {
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    }

    fprintf(out_h, "#ifndef %s\n#define %s\n\n", guard, guard);
    fprintf(out_h, "#include <stdint.h>\n\n");
    for (size_t i = 0; i < lib->fn_count; i++) {
        char sym[128];
        forge_lib_mangle(sym, sizeof(sym), lib->name, lib->functions[i].name);
        emit_fn_signature(out_h, sym, &lib->functions[i]);
        fputs(";\n", out_h);
    }
    fprintf(out_h, "\n#endif\n");

    fprintf(out_c, "// Generated by Forge compiler (library mode)\n");
    fprintf(out_c, "#include \"%.*s.h\"\n", (int)lib->name.len, lib->name.data);
    fprintf(out_c, "#include \"%s\"\n", runtime_include);
    fputs("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n", out_c);
    for (size_t i = 0; i < lib->import_count; i++) {
        const char *hdr = forge_std_header(lib->imports[i]);
        if (hdr) fprintf(out_c, "#include \"%s\"\n", hdr);
        else fprintf(out_c, "#include \"%.*s.h\"\n", (int)lib->imports[i].len, lib->imports[i].data);
    }
    fputs("\n", out_c);

    Codegen cg = { out_c, 0, prog, NULL, NULL, 0, 0, lib->imports, lib->import_count };
    for (size_t i = 0; i < lib->fn_count; i++) {
        FnDecl *fn = &lib->functions[i];
        char sym[128];
        forge_lib_mangle(sym, sizeof(sym), lib->name, fn->name);
        emit_fn_signature(out_c, sym, fn);
        fputs(" {\n", out_c);
        cg.indent = 1;
        cg.local_count = 0;
        emit_stmts(&cg, fn->body.first, NULL, false, NULL);
        cg.indent = 0;
        fputs("}\n\n", out_c);
    }
    free(cg.locals);
}

void codegen_emit(Program *prog, FILE *out, const char *runtime_include) {
    Codegen cg = { out, 0, prog, NULL, NULL, 0, 0, prog->imports, prog->import_count };

    fputs("// Generated by Forge compiler (AOT -> C)\n", out);
    fprintf(out, "#include \"%s\"\n", runtime_include);
    fputs("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n", out);
    fputs("#include \"forge/os.h\"\n", out);
    emit_import_headers(prog, out);
    fputs("\n", out);

    for (size_t i = 0; i < prog->process_count; i++) {
        ProcessDecl *proc = &prog->processes[i];
        for (size_t j = 0; j < proc->coro_count; j++) {
            emit_coro_fn(&cg, proc, &proc->coros[j]);
        }
    }

    for (size_t i = 0; i < prog->fn_count; i++) {
        FnDecl *fn = &prog->functions[i];
        fprintf(out, "static %s %.*s(", c_type(fn->ret_type), (int)fn->name.len, fn->name.data);
        Param *p = fn->params;
        bool first = true;
        while (p) {
            if (!first) fputs(", ", out);
            fprintf(out, "%s %.*s", c_type(p->type), (int)p->name.len, p->name.data);
            first = false;
            p = p->next;
        }
        fputs(") {\n", out);
        cg.indent = 1;
        emit_stmts(&cg, fn->body.first, NULL, false, NULL);
        cg.indent = 0;
        fputs("}\n\n", out);
    }

    for (size_t i = 0; i < prog->process_count; i++) {
        ProcessDecl *pd = &prog->processes[i];
        if (!pd->body.first) continue;
        fprintf(out, "static void %.*s_init(fr_process_t *proc) {\n", (int)pd->name.len, pd->name.data);
        cg.indent = 1;
        cg.local_count = 0;
        emit_stmts(&cg, pd->body.first, "proc", false, NULL);
        cg.indent = 0;
        fputs("}\n\n", out);
    }

    ForgeStr entry_name = forge_str("main");
    ProcessDecl *entry = find_process(prog, entry_name);
    if (!entry && prog->process_count > 0) entry = &prog->processes[0];

    if (!entry) {
        fputs("int main(void) { return 0; }\n", out);
        return;
    }

    fputs("int main(int argc, char **argv) {\n", out);
  cg.indent = 1;
    cg_line(&cg, "fr_os_set_args(argc, argv);");
    cg_line(&cg, "fr_scheduler_t *sched = fr_scheduler_create(1);");

    for (size_t i = 0; i < prog->process_count; i++) {
        ProcessDecl *pd = &prog->processes[i];
        fprintf(out, "    fr_process_t *proc_%.*s = fr_process_create(\"%.*s\");\n",
                (int)pd->name.len, pd->name.data, (int)pd->name.len, pd->name.data);
        cg_line(&cg, "fr_scheduler_add_process(sched, proc_%.*s);", (int)pd->name.len, pd->name.data);
    }

    for (size_t i = 0; i < prog->supervisor_count; i++) {
        SupervisorDecl *sup = &prog->supervisors[i];
        const char *pol = "FR_RESTART_PROCESS";
        if (sup->policy == SUP_RESTART_CORO) pol = "FR_RESTART_CORO";
        else if (sup->policy == SUP_RESTART_ALL) pol = "FR_RESTART_ALL";
        fprintf(out, "    fr_process_t *sup_%.*s = fr_supervisor_create(\"%.*s\", %s);\n",
                (int)sup->name.len, sup->name.data, (int)sup->name.len, sup->name.data, pol);
        for (size_t j = 0; j < sup->child_count; j++) {
            ProcessDecl *child = find_process(prog, sup->children[j]);
            if (child) {
                fprintf(out, "    fr_supervisor_add_child(sup_%.*s, proc_%.*s);\n",
                        (int)sup->name.len, sup->name.data,
                        (int)child->name.len, child->name.data);
            }
        }
        cg_line(&cg, "fr_scheduler_add_process(sched, sup_%.*s);", (int)sup->name.len, sup->name.data);
    }

    for (size_t i = 0; i < prog->process_count; i++) {
        ProcessDecl *pd = &prog->processes[i];
        if (pd->body.first) {
            fprintf(out, "    %.*s_init(proc_%.*s);\n",
                    (int)pd->name.len, pd->name.data,
                    (int)pd->name.len, pd->name.data);
        }
    }

    cg_line(&cg, "fr_scheduler_run(sched);");
    cg_line(&cg, "fr_scheduler_destroy(sched);");
    cg_line(&cg, "return 0;");
    cg.indent = 0;
    fputs("}\n", out);
    free(cg.locals);
}
