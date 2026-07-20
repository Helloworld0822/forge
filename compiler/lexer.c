#include "lexer.h"

static bool is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_ident(char c) {
    return is_ident_start(c) || isdigit((unsigned char)c);
}

static Token make_token(Lexer *lx, TokenKind kind, HyloStr lexeme) {
  Token t = { kind, lexeme, 0, 0.0, lx->line, lx->col };
  return t;
}

static void skip_ws(Lexer *lx) {
    while (lx->pos < lx->len) {
        char c = lx->src[lx->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            lx->pos++;
            lx->col++;
        } else if (c == '\n') {
            lx->pos++;
            lx->line++;
            lx->col = 1;
        } else if (c == '/' && lx->pos + 1 < lx->len && lx->src[lx->pos + 1] == '/') {
            lx->pos += 2;
            lx->col += 2;
            while (lx->pos < lx->len && lx->src[lx->pos] != '\n') {
                lx->pos++;
                lx->col++;
            }
        } else {
            break;
        }
    }
}

static TokenKind keyword_kind(HyloStr s) {
    struct { const char *kw; TokenKind kind; } kws[] = {
        {"process", TOK_KW_PROCESS}, {"coroutine", TOK_KW_COROUTINE},
        {"supervisor", TOK_KW_SUPERVISOR}, {"spawn", TOK_KW_SPAWN},
        {"send", TOK_KW_SEND}, {"recv", TOK_KW_RECV}, {"yield", TOK_KW_YIELD},
        {"on", TOK_KW_ON}, {"receive", TOK_KW_RECEIVE},
        {"let", TOK_KW_LET}, {"mut", TOK_KW_MUT},
        {"fn", TOK_KW_FN}, {"return", TOK_KW_RETURN},
        {"if", TOK_KW_IF}, {"else", TOK_KW_ELSE}, {"while", TOK_KW_WHILE},
        {"true", TOK_KW_TRUE}, {"false", TOK_KW_FALSE},
        {"int", TOK_KW_INT}, {"float", TOK_KW_FLOAT},
        {"bool", TOK_KW_BOOL}, {"string", TOK_KW_STRING}, {"void", TOK_KW_VOID},
        {"restart", TOK_KW_RESTART},
    };
    for (size_t i = 0; i < sizeof(kws) / sizeof(kws[0]); i++) {
        HyloStr kw = hylo_str(kws[i].kw);
        if (hylo_str_eq(s, kw)) return kws[i].kind;
    }
    return TOK_IDENT;
}

void lexer_init(Lexer *lx, const char *src, size_t len) {
    lx->src = src;
    lx->pos = 0;
    lx->len = len;
    lx->line = 1;
    lx->col = 1;
    lx->has_current = false;
}

static Token read_number(Lexer *lx) {
    size_t start = lx->pos;
    int col = lx->col;
    bool is_float = false;
    while (lx->pos < lx->len && isdigit((unsigned char)lx->src[lx->pos])) {
        lx->pos++;
        lx->col++;
    }
    if (lx->pos < lx->len && lx->src[lx->pos] == '.') {
        is_float = true;
        lx->pos++;
        lx->col++;
        while (lx->pos < lx->len && isdigit((unsigned char)lx->src[lx->pos])) {
            lx->pos++;
            lx->col++;
        }
    }
    HyloStr lex = { (char *)lx->src + start, lx->pos - start };
    char buf[64];
    size_t n = lex.len < 63 ? lex.len : 63;
    memcpy(buf, lex.data, n);
    buf[n] = '\0';
    Token t = make_token(lx, is_float ? TOK_FLOAT : TOK_INT, lex);
    t.line = lx->line;
    t.col = col;
    if (is_float) t.float_val = atof(buf);
    else t.int_val = atoll(buf);
    return t;
}

static Token read_string(Lexer *lx) {
    int line = lx->line, col = lx->col;
    lx->pos++;
    lx->col++;
    size_t start = lx->pos;
    while (lx->pos < lx->len && lx->src[lx->pos] != '"') {
        if (lx->src[lx->pos] == '\\' && lx->pos + 1 < lx->len) {
            lx->pos += 2;
            lx->col += 2;
            continue;
        }
        if (lx->src[lx->pos] == '\n') {
            lx->line++;
            lx->col = 1;
        } else {
            lx->col++;
        }
        lx->pos++;
    }
    if (lx->pos >= lx->len) hylo_die("unterminated string");
    HyloStr lex = { (char *)lx->src + start, lx->pos - start };
    lx->pos++;
    lx->col++;
    Token t = make_token(lx, TOK_STRING, lex);
    t.line = line;
    t.col = col;
    return t;
}

static Token read_ident(Lexer *lx) {
    size_t start = lx->pos;
    int line = lx->line, col = lx->col;
    while (lx->pos < lx->len && is_ident(lx->src[lx->pos])) {
        lx->pos++;
        lx->col++;
    }
    HyloStr lex = { (char *)lx->src + start, lx->pos - start };
    TokenKind kind = keyword_kind(lex);
    Token t = make_token(lx, kind, lex);
    t.line = line;
    t.col = col;
    return t;
}

Token lexer_next(Lexer *lx) {
    if (lx->has_current) {
        lx->has_current = false;
        return lx->current;
    }
    skip_ws(lx);
    if (lx->pos >= lx->len) return make_token(lx, TOK_EOF, hylo_str(""));

    char c = lx->src[lx->pos];
    int line = lx->line, col = lx->col;

    if (isdigit((unsigned char)c)) return read_number(lx);
    if (is_ident_start(c)) return read_ident(lx);
    if (c == '"') return read_string(lx);

    lx->pos++;
    lx->col++;

    switch (c) {
    case '(': return make_token(lx, TOK_LPAREN, hylo_str("("));
    case ')': return make_token(lx, TOK_RPAREN, hylo_str(")"));
    case '{': return make_token(lx, TOK_LBRACE, hylo_str("{"));
    case '}': return make_token(lx, TOK_RBRACE, hylo_str("}"));
    case '[': return make_token(lx, TOK_LBRACKET, hylo_str("["));
    case ']': return make_token(lx, TOK_RBRACKET, hylo_str("]"));
    case ',': return make_token(lx, TOK_COMMA, hylo_str(","));
    case ':': return make_token(lx, TOK_COLON, hylo_str(":"));
    case ';': return make_token(lx, TOK_SEMI, hylo_str(";"));
    case '.': return make_token(lx, TOK_DOT, hylo_str("."));
    case '+': return make_token(lx, TOK_PLUS, hylo_str("+"));
    case '-': return make_token(lx, TOK_MINUS, hylo_str("-"));
    case '*': return make_token(lx, TOK_STAR, hylo_str("*"));
    case '/': return make_token(lx, TOK_SLASH, hylo_str("/"));
    case '%': return make_token(lx, TOK_PERCENT, hylo_str("%"));
    case '!':
        if (lx->pos < lx->len && lx->src[lx->pos] == '=') {
            lx->pos++; lx->col++;
            return make_token(lx, TOK_NE, hylo_str("!="));
        }
        return make_token(lx, TOK_BANG, hylo_str("!"));
    case '=':
        if (lx->pos < lx->len && lx->src[lx->pos] == '=') {
            lx->pos++; lx->col++;
            return make_token(lx, TOK_EQEQ, hylo_str("=="));
        }
        return make_token(lx, TOK_EQ, hylo_str("="));
    case '<':
        if (lx->pos < lx->len && lx->src[lx->pos] == '=') {
            lx->pos++; lx->col++;
            return make_token(lx, TOK_LE, hylo_str("<="));
        }
        return make_token(lx, TOK_LT, hylo_str("<"));
    case '>':
        if (lx->pos < lx->len && lx->src[lx->pos] == '=') {
            lx->pos++; lx->col++;
            return make_token(lx, TOK_GE, hylo_str(">="));
        }
        return make_token(lx, TOK_GT, hylo_str(">"));
    case '&':
        if (lx->pos < lx->len && lx->src[lx->pos] == '&') {
            lx->pos++; lx->col++;
            return make_token(lx, TOK_ANDAND, hylo_str("&&"));
        }
        break;
    case '|':
        if (lx->pos < lx->len && lx->src[lx->pos] == '|') {
            lx->pos++; lx->col++;
            return make_token(lx, TOK_OROR, hylo_str("||"));
        }
        break;
    default:
        break;
    }
    fprintf(stderr, "hylo: unexpected character '%c' at %d:%d\n", c, line, col);
    exit(1);
}

Token lexer_peek(Lexer *lx) {
    if (!lx->has_current) {
        lx->current = lexer_next(lx);
        lx->has_current = true;
    }
    return lx->current;
}

bool lexer_match(Lexer *lx, TokenKind kind) {
    Token t = lexer_peek(lx);
    if (t.kind == kind) {
        lexer_next(lx);
        return true;
    }
    return false;
}
