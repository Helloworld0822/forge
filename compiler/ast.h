#ifndef HYLO_AST_H
#define HYLO_AST_H

#include "common.h"

typedef enum {
    TY_VOID,
    TY_INT,
    TY_FLOAT,
    TY_BOOL,
    TY_STRING
} HyloTypeKind;

typedef struct HyloType {
    HyloTypeKind kind;
} HyloType;

typedef enum {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_BOOL,
    EXPR_STRING,
    EXPR_IDENT,
    EXPR_BINARY,
    EXPR_CALL,
    EXPR_RECV
} ExprKind;

typedef enum {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,
    BIN_EQ, BIN_NE, BIN_LT, BIN_LE, BIN_GT, BIN_GE,
    BIN_AND, BIN_OR
} BinOp;

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Block Block;
typedef struct Param Param;
typedef struct Field Field;

struct Expr {
    ExprKind kind;
    HyloType type;
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
        HyloStr string_val;
        HyloStr ident;
        struct {
            BinOp op;
            Expr *left;
            Expr *right;
        } binary;
        struct {
            HyloStr name;
            Expr **args;
            size_t arg_count;
        } call;
    } as;
};

struct Stmt {
    enum {
        STMT_LET,
        STMT_EXPR,
        STMT_RETURN,
        STMT_IF,
        STMT_WHILE,
        STMT_SPAWN,
    STMT_SEND,
    STMT_YIELD,
    STMT_ASSIGN,
    STMT_BLOCK
    } kind;
    union {
        struct {
            bool mutable_;
            HyloStr name;
            HyloType type;
            Expr *init;
        } let;
        Expr *expr;
        Expr *ret;
        struct {
            Expr *cond;
            Block *then_br;
            Block *else_br;
        } if_stmt;
        struct {
            Expr *cond;
            Block *body;
        } while_stmt;
        struct {
            HyloStr coro_name;
            Expr **args;
            size_t arg_count;
        } spawn;
        struct {
            Expr *target;
            int tag;
            Expr *value;
        } send;
        struct {
            HyloStr name;
            Expr *value;
        } assign;
        Block *block;
    } as;
    Stmt *next;
};

struct Block {
    Stmt *first;
    Stmt *last;
};

struct Param {
    HyloStr name;
    HyloType type;
    Param *next;
};

typedef struct {
    HyloStr name;
    Param *params;
    HyloType ret_type;
    Block body;
} FnDecl;

typedef struct {
    HyloStr name;
    Param *params;
    Block body;
} CoroDecl;

typedef struct {
    HyloStr name;
    Block body;
    CoroDecl *coros;
    size_t coro_count;
    Block *on_receive;
    Param receive_param;
    bool has_receive;
} ProcessDecl;

typedef struct {
    HyloStr name;
    enum { SUP_RESTART_CORO, SUP_RESTART_PROCESS, SUP_RESTART_ALL } policy;
    HyloStr *children;
    size_t child_count;
} SupervisorDecl;

typedef struct {
    ProcessDecl *processes;
    size_t process_count;
    FnDecl *functions;
    size_t fn_count;
    SupervisorDecl *supervisors;
    size_t supervisor_count;
} Program;

HyloType hylo_type_void(void);
HyloType hylo_type_int(void);
HyloType hylo_type_float(void);
HyloType hylo_type_bool(void);
HyloType hylo_type_string(void);
HyloType hylo_type_from_name(HyloStr name);

Expr *expr_int(int64_t v);
Expr *expr_float(double v);
Expr *expr_bool(bool v);
Expr *expr_string(HyloStr v);
Expr *expr_ident(HyloStr name);
Expr *expr_binary(BinOp op, Expr *l, Expr *r);
Expr *expr_call(HyloStr name, Expr **args, size_t n);
Expr *expr_recv(void);

Block block_new(void);
void block_append(Block *b, Stmt *s);
Stmt *stmt_let(bool mut, HyloStr name, HyloType ty, Expr *init);
Stmt *stmt_expr(Expr *e);
Stmt *stmt_return(Expr *e);
Stmt *stmt_if(Expr *cond, Block *then_br, Block *else_br);
Stmt *stmt_while(Expr *cond, Block *body);
Stmt *stmt_spawn(HyloStr name, Expr **args, size_t n);
Stmt *stmt_send(Expr *target, int tag, Expr *value);
Stmt *stmt_yield(void);
Stmt *stmt_assign(HyloStr name, Expr *value);
Stmt *stmt_block(Block *b);

void program_free(Program *p);

#endif
