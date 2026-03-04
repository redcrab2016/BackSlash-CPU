#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>

#define MAX_TOKENS 131072
#define MAX_TEXT 1
#define MAX_FUNCS 256
#define MAX_LOCALS 256
#define MAX_ARGS 4
#define MAX_OPS 131072
#define MAX_LABELS 8192
#define MAX_NAME 64
#define MAX_WORDS 65536
#define MAX_PATCH 8192

typedef enum {
    TK_EOF=0, TK_IDENT, TK_NUM,
    TK_INT, TK_RETURN, TK_IF, TK_ELSE, TK_WHILE, TK_AT,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_COMMA, TK_SEMI,
    TK_PLUS, TK_MINUS, TK_TILDE, TK_BANG,
    TK_ASSIGN,
    TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE,
    TK_AND, TK_OR, TK_XOR
} TokenKind;

typedef struct {
    TokenKind kind;
    char text[MAX_NAME];
    long num;
    int line;
} Token;

typedef struct {
    Token toks[MAX_TOKENS];
    int count;
    int pos;
    const char *filename;
} TokBuf;

typedef struct {
    char name[MAX_NAME];
    int slot;
} Local;

typedef struct {
    char name[MAX_NAME];
    uint16_t addr;
} Global;

typedef enum {
    OP_LABEL,
    OP_COMMENT,
    OP_AR0,
    OP_CPY_RR,
    OP_CPY_RM,
    OP_CPY_MR,
    OP_ADD_RR,
    OP_SUB_RR,
    OP_AND_RR,
    OP_OR_RR,
    OP_XOR_R0R,
    OP_JMP_REL,      /* conditional or unconditional relative label */
    OP_JABS,         /* absolute jump to label */
    OP_JABS_IF_ZERO, /* if R0==0 jump abs to label */
    OP_JABS_IF_NZ,   /* if R0!=0 jump abs to label */
    OP_CALL,         /* absolute call to function label */
    OP_RET,
    OP_HALT,
    OP_LOAD_SLOT,    /* R0 = slot */
    OP_STORE_SLOT,   /* slot = R0 */
    OP_PUSH_R0,
    OP_POP_REG,
    OP_BOOL_FROM_FLAGS,
    OP_BOOL_NOT,
    OP_BOOL_LE,
    OP_BOOL_GT,
    OP_NOP
} OpKind;

typedef struct {
    OpKind kind;
    int a, b, c;
    char s[MAX_NAME];
    int line;
    uint16_t addr;
    int size;
} Op;

typedef struct {
    char name[MAX_NAME];
    char end_label[MAX_NAME];
    int param_count;
    char params[MAX_ARGS][MAX_NAME];
    Local locals[MAX_LOCALS];
    int local_count;
    Op ops[MAX_OPS];
    int op_count;
    int line;
} Function;

typedef struct {
    Function funcs[MAX_FUNCS];
    int func_count;
    Global globals[MAX_LOCALS];
    int global_count;
    uint16_t sp_init;
    int label_seq;
    const char *filename;
    int errors;
    bool big_endian;
} Compiler;

typedef struct {
    uint16_t words[MAX_WORDS];
    int nwords;
    char lines[MAX_WORDS][128];
    int line_count;
    struct { char name[MAX_NAME]; uint16_t addr; } labels[MAX_LABELS];
    int label_count;
} Final;

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void cerror(Compiler *c, int line, const char *fmt, ...) {
    va_list ap;
    c->errors++;
    fprintf(stderr, "%s:%d: error: ", c->filename ? c->filename : "<input>", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) die("cannot open %s", path);
    if (fseek(f, 0, SEEK_END) != 0) die("fseek failed");
    long n = ftell(f);
    if (n < 0) die("ftell failed");
    rewind(f);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) die("out of memory");
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) die("read failed");
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static bool is_ident_start(int ch) { return isalpha(ch) || ch == '_'; }
static bool is_ident_char(int ch) { return isalnum(ch) || ch == '_'; }

static void add_token(TokBuf *tb, Token t) {
    if (tb->count >= MAX_TOKENS) die("too many tokens");
    tb->toks[tb->count++] = t;
}

static void lex(const char *src, TokBuf *tb, Compiler *c) {
    int line = 1;
    const char *p = src;
    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }
        if (*p == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
        if (*p == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(*p == '*' && p[1] == '/')) { if (*p == '\n') line++; p++; }
            if (*p) p += 2;
            continue;
        }
        Token t; memset(&t, 0, sizeof(t)); t.line = line;
        if (is_ident_start((unsigned char)*p)) {
            int n = 0;
            while (is_ident_char((unsigned char)*p)) {
                if (n < MAX_NAME - 1) t.text[n++] = *p;
                p++;
            }
            t.text[n] = '\0';
            if (!strcmp(t.text, "int")) t.kind = TK_INT;
            else if (!strcmp(t.text, "return")) t.kind = TK_RETURN;
            else if (!strcmp(t.text, "if")) t.kind = TK_IF;
            else if (!strcmp(t.text, "else")) t.kind = TK_ELSE;
            else if (!strcmp(t.text, "while")) t.kind = TK_WHILE;
            else if (!strcmp(t.text, "__at")) t.kind = TK_AT;
            else t.kind = TK_IDENT;
            add_token(tb, t);
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            char *end = NULL;
            long v;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) v = strtol(p, &end, 16);
            else v = strtol(p, &end, 10);
            t.kind = TK_NUM;
            t.num = v & 0xFFFF;
            snprintf(t.text, sizeof(t.text), "%ld", t.num);
            p = end;
            add_token(tb, t);
            continue;
        }
        switch (*p) {
            case '(': t.kind = TK_LPAREN; p++; break;
            case ')': t.kind = TK_RPAREN; p++; break;
            case '{': t.kind = TK_LBRACE; p++; break;
            case '}': t.kind = TK_RBRACE; p++; break;
            case ',': t.kind = TK_COMMA; p++; break;
            case ';': t.kind = TK_SEMI; p++; break;
            case '+': t.kind = TK_PLUS; p++; break;
            case '-': t.kind = TK_MINUS; p++; break;
            case '~': t.kind = TK_TILDE; p++; break;
            case '!':
                if (p[1] == '=') { t.kind = TK_NE; p += 2; }
                else { t.kind = TK_BANG; p++; }
                break;
            case '=':
                if (p[1] == '=') { t.kind = TK_EQ; p += 2; }
                else { t.kind = TK_ASSIGN; p++; }
                break;
            case '<':
                if (p[1] == '=') { t.kind = TK_LE; p += 2; }
                else { t.kind = TK_LT; p++; }
                break;
            case '>':
                if (p[1] == '=') { t.kind = TK_GE; p += 2; }
                else { t.kind = TK_GT; p++; }
                break;
            case '&': t.kind = TK_AND; p++; break;
            case '|': t.kind = TK_OR; p++; break;
            case '^': t.kind = TK_XOR; p++; break;
            default:
                cerror(c, line, "unexpected character '%c'", *p);
                p++;
                continue;
        }
        add_token(tb, t);
    }
    Token eof; memset(&eof, 0, sizeof(eof)); eof.kind = TK_EOF; eof.line = line;
    add_token(tb, eof);
}

static Token *peek(TokBuf *tb) { return &tb->toks[tb->pos]; }
static Token *next(TokBuf *tb) { return &tb->toks[tb->pos++]; }
static bool accept(TokBuf *tb, TokenKind k) { if (peek(tb)->kind == k) { tb->pos++; return true; } return false; }
static Token *expect(TokBuf *tb, TokenKind k, Compiler *c, const char *msg) {
    if (peek(tb)->kind != k) { cerror(c, peek(tb)->line, "%s", msg); return peek(tb); }
    return next(tb);
}

static Function *new_function(Compiler *c, const char *name, int line) {
    if (c->func_count >= MAX_FUNCS) die("too many functions");
    Function *f = &c->funcs[c->func_count++];
    memset(f, 0, sizeof(*f));
    strncpy(f->name, name, sizeof(f->name)-1);
    f->line = line;
    snprintf(f->end_label, sizeof(f->end_label), "__%s_ret", name);
    return f;
}

static int find_local(Function *f, const char *name) {
    for (int i = 0; i < f->local_count; i++) if (!strcmp(f->locals[i].name, name)) return i;
    return -1;
}

static int add_local(Compiler *c, Function *f, const char *name, int line) {
    if (find_local(f, name) >= 0) { cerror(c, line, "duplicate local '%s'", name); return 0; }
    if (f->local_count >= MAX_LOCALS) { cerror(c, line, "too many locals in function '%s'", f->name); return 0; }
    strncpy(f->locals[f->local_count].name, name, sizeof(f->locals[f->local_count].name)-1);
    f->locals[f->local_count].slot = f->local_count;
    return f->local_count++;
}

static int local_slot(Compiler *c, Function *f, const char *name, int line) {
    int idx = find_local(f, name);
    if (idx < 0) { cerror(c, line, "unknown variable '%s'", name); return 0; }
    return f->locals[idx].slot;
}

static int find_global(Compiler *c, const char *name) {
    for (int i = 0; i < c->global_count; i++) if (!strcmp(c->globals[i].name, name)) return i;
    return -1;
}

static int add_global(Compiler *c, const char *name, uint16_t addr, int line) {
    if (find_global(c, name) >= 0) { cerror(c, line, "duplicate global '%s'", name); return 0; }
    if (c->global_count >= MAX_LOCALS) { cerror(c, line, "too many globals"); return 0; }
    strncpy(c->globals[c->global_count].name, name, sizeof(c->globals[c->global_count].name)-1);
    c->globals[c->global_count].addr = addr;
    return c->global_count++;
}

static bool lookup_symbol(Compiler *c, Function *f, const char *name, int line, bool *is_global, int *slot, uint16_t *addr) {
    int idx = find_local(f, name);
    if (idx >= 0) {
        if (is_global) *is_global = false;
        if (slot) *slot = f->locals[idx].slot;
        if (addr) *addr = 0;
        return true;
    }
    idx = find_global(c, name);
    if (idx >= 0) {
        if (is_global) *is_global = true;
        if (slot) *slot = -1;
        if (addr) *addr = c->globals[idx].addr;
        return true;
    }
    cerror(c, line, "unknown variable '%s'", name);
    return false;
}

static void emit(Function *f, OpKind kind, int a, int b, int c, const char *s, int line) {
    if (f->op_count >= MAX_OPS) die("too many ops");
    Op *op = &f->ops[f->op_count++];
    memset(op, 0, sizeof(*op));
    op->kind = kind; op->a = a; op->b = b; op->c = c; op->line = line;
    if (s) strncpy(op->s, s, sizeof(op->s)-1);
}

static void emit_label(Function *f, const char *name) { emit(f, OP_LABEL, 0,0,0, name, f->line); }
static void emit_nop(Function *f, int line) { emit(f, OP_NOP,0,0,0,NULL,line); }

static void new_label(Compiler *c, char *out, size_t outsz, const char *prefix) {
    snprintf(out, outsz, "__%s_%04d", prefix, c->label_seq++);
}

static int precedence(TokenKind k) {
    switch (k) {
        case TK_OR: return 10;
        case TK_XOR: return 20;
        case TK_AND: return 30;
        case TK_EQ: case TK_NE: return 40;
        case TK_LT: case TK_LE: case TK_GT: case TK_GE: return 50;
        case TK_PLUS: case TK_MINUS: return 60;
        default: return -1;
    }
}

static void gen_expr(Compiler *c, TokBuf *tb, Function *f);

static void emit_load_const(Function *f, uint16_t value, int line) {
    emit(f, OP_AR0, (value >> 4) & 0x0FFF, 1, 0, NULL, line);
    emit(f, OP_CPY_RR, 1, 0, 0, NULL, line);
    emit(f, OP_AR0, value & 0x000F, 0, 0, NULL, line);
    emit(f, OP_OR_RR, 0, 1, 0, NULL, line);
}

static void emit_push_r0(Function *f, int line) {
    emit(f, OP_SUB_RR, 10, 13, 0, NULL, line);
    emit(f, OP_CPY_MR, 10, 0, 0, NULL, line);
}

static void emit_pop_reg(Function *f, int reg, int line) {
    emit(f, OP_CPY_RM, reg, 10, 0, NULL, line);
    emit(f, OP_ADD_RR, 10, 13, 0, NULL, line);
}

static void emit_load_slot(Function *f, int slot, int line) {
    emit_load_const(f, (uint16_t)(slot + 1), line);
    emit(f, OP_CPY_RR, 8, 9, 0, NULL, line);
    emit(f, OP_SUB_RR, 8, 0, 0, NULL, line);
    emit(f, OP_CPY_RM, 0, 8, 0, NULL, line);
}

static void emit_store_slot(Function *f, int slot, int line) {
    emit(f, OP_CPY_RR, 2, 0, 0, NULL, line);
    emit_load_const(f, (uint16_t)(slot + 1), line);
    emit(f, OP_CPY_RR, 8, 9, 0, NULL, line);
    emit(f, OP_SUB_RR, 8, 0, 0, NULL, line);
    emit(f, OP_CPY_RR, 0, 2, 0, NULL, line);
    emit(f, OP_CPY_MR, 8, 0, 0, NULL, line);
}

static void emit_load_abs(Function *f, uint16_t addr, int line) {
    emit_load_const(f, addr, line);
    emit(f, OP_CPY_RR, 8, 0, 0, NULL, line);
    emit(f, OP_CPY_RM, 0, 8, 0, NULL, line);
}

static void emit_store_abs(Function *f, uint16_t addr, int line) {
    emit(f, OP_CPY_RR, 2, 0, 0, NULL, line);
    emit_load_const(f, addr, line);
    emit(f, OP_CPY_RR, 8, 0, 0, NULL, line);
    emit(f, OP_CPY_RR, 0, 2, 0, NULL, line);
    emit(f, OP_CPY_MR, 8, 0, 0, NULL, line);
}

static void emit_load_addr_from_r0(Function *f, int line) {
    emit(f, OP_CPY_RR, 8, 0, 0, NULL, line);
    emit(f, OP_CPY_RM, 0, 8, 0, NULL, line);
}

static void emit_store_addr_from_stack(Function *f, int line) {
    emit(f, OP_CPY_RR, 2, 0, 0, NULL, line);
    emit_pop_reg(f, 8, line);
    emit(f, OP_CPY_RR, 0, 2, 0, NULL, line);
    emit(f, OP_CPY_MR, 8, 0, 0, NULL, line);
}

static void gen_primary(Compiler *c, TokBuf *tb, Function *f) {
    Token *t = peek(tb);
    if (accept(tb, TK_NUM)) {
        emit_load_const(f, (uint16_t)t->num, t->line);
        return;
    }
    if (accept(tb, TK_LPAREN)) {
        gen_expr(c, tb, f);
        expect(tb, TK_RPAREN, c, "expected ')' after expression");
        return;
    }
    if (t->kind == TK_IDENT) {
        Token *id = next(tb);
        if (accept(tb, TK_LPAREN)) {
            if (!strcmp(id->text, "__load16")) {
                gen_expr(c, tb, f);
                expect(tb, TK_RPAREN, c, "expected ')' after __load16 argument");
                emit_load_addr_from_r0(f, id->line);
                return;
            }
            if (!strcmp(id->text, "__store16")) {
                gen_expr(c, tb, f);
                emit_push_r0(f, id->line);
                expect(tb, TK_COMMA, c, "expected ',' in __store16(address, value)");
                gen_expr(c, tb, f);
                expect(tb, TK_RPAREN, c, "expected ')' after __store16 arguments");
                emit_store_addr_from_stack(f, id->line);
                return;
            }
            int argc = 0;
            if (!accept(tb, TK_RPAREN)) {
                for (;;) {
                    gen_expr(c, tb, f);
                    emit_push_r0(f, id->line);
                    argc++;
                    if (!accept(tb, TK_COMMA)) break;
                }
                expect(tb, TK_RPAREN, c, "expected ')' after arguments");
            }
            if (argc > MAX_ARGS) {
                cerror(c, id->line, "function calls support at most %d arguments", MAX_ARGS);
                argc = MAX_ARGS;
            }
            for (int i = argc - 1; i >= 0; i--) emit_pop_reg(f, i + 1, id->line);
            emit(f, OP_CALL, 0,0,0, id->text, id->line);
            return;
        }
        bool is_global = false;
        int slot = -1;
        uint16_t addr = 0;
        if (!lookup_symbol(c, f, id->text, id->line, &is_global, &slot, &addr)) {
            emit_load_const(f, 0, id->line);
            return;
        }
        if (is_global) emit_load_abs(f, addr, id->line);
        else emit_load_slot(f, slot, id->line);
        return;
    }
    cerror(c, t->line, "expected primary expression");
    emit_load_const(f, 0, t->line);
    next(tb);
}

static void gen_unary(Compiler *c, TokBuf *tb, Function *f) {
    Token *t = peek(tb);
    if (accept(tb, TK_PLUS)) { gen_unary(c, tb, f); return; }
    if (accept(tb, TK_MINUS)) {
        gen_unary(c, tb, f);
        emit(f, OP_CPY_RR, 2, 0, 0, NULL, t->line);
        emit_load_const(f, 0, t->line);
        emit(f, OP_SUB_RR, 0, 2, 0, NULL, t->line);
        return;
    }
    if (accept(tb, TK_TILDE)) {
        gen_unary(c, tb, f);
        emit(f, OP_CPY_RR, 2, 0, 0, NULL, t->line);
        emit_load_const(f, 0xFFFF, t->line);
        emit(f, OP_XOR_R0R, 2, 0, 0, NULL, t->line);
        return;
    }
    if (accept(tb, TK_BANG)) {
        gen_unary(c, tb, f);
        emit(f, OP_BOOL_NOT, 0,0,0,NULL,t->line);
        return;
    }
    gen_primary(c, tb, f);
}

static void gen_binop(Compiler *c, TokBuf *tb, Function *f, int min_prec) {
    gen_unary(c, tb, f);
    for (;;) {
        TokenKind k = peek(tb)->kind;
        int prec = precedence(k);
        if (prec < min_prec) break;
        Token *op = next(tb);
        gen_binop(c, tb, f, prec + 1);
        emit(f, OP_CPY_RR, 2, 0, 0, NULL, op->line);
        emit_pop_reg(f, 0, op->line);
        switch (k) {
            case TK_PLUS: emit(f, OP_ADD_RR, 0, 2, 0, NULL, op->line); break;
            case TK_MINUS: emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line); break;
            case TK_AND: emit(f, OP_AND_RR, 0, 2, 0, NULL, op->line); break;
            case TK_OR: emit(f, OP_OR_RR, 0, 2, 0, NULL, op->line); break;
            case TK_XOR: emit(f, OP_XOR_R0R, 2, 0, 0, NULL, op->line); break;
            case TK_EQ:
                emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line);
                emit(f, OP_BOOL_FROM_FLAGS, 3,0,0,NULL,op->line); /* eq */
                break;
            case TK_NE:
                emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line);
                emit(f, OP_BOOL_FROM_FLAGS, 4,0,0,NULL,op->line); /* ne */
                break;
            case TK_LT:
                emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line);
                emit(f, OP_BOOL_FROM_FLAGS, 6,0,0,NULL,op->line); /* ls */
                break;
            case TK_GE:
                emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line);
                emit(f, OP_BOOL_FROM_FLAGS, 5,0,0,NULL,op->line); /* ge */
                break;
            case TK_LE:
                emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line);
                emit(f, OP_BOOL_LE, 0,0,0,NULL,op->line);
                break;
            case TK_GT:
                emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line);
                emit(f, OP_BOOL_GT, 0,0,0,NULL,op->line);
                break;
            default: break;
        }
    }
}

static void gen_expr_prec(Compiler *c, TokBuf *tb, Function *f, int min_prec);

static void gen_expr_prec(Compiler *c, TokBuf *tb, Function *f, int min_prec) {
    gen_unary(c, tb, f);
    for (;;) {
        TokenKind k = peek(tb)->kind;
        int prec = precedence(k);
        if (prec < min_prec) break;
        Token *op = next(tb);
        emit_push_r0(f, op->line);
        gen_expr_prec(c, tb, f, prec + 1);
        emit(f, OP_CPY_RR, 2, 0, 0, NULL, op->line);
        emit_pop_reg(f, 0, op->line);
        switch (k) {
            case TK_PLUS: emit(f, OP_ADD_RR, 0, 2, 0, NULL, op->line); break;
            case TK_MINUS: emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line); break;
            case TK_AND: emit(f, OP_AND_RR, 0, 2, 0, NULL, op->line); break;
            case TK_OR: emit(f, OP_OR_RR, 0, 2, 0, NULL, op->line); break;
            case TK_XOR: emit(f, OP_XOR_R0R, 2, 0, 0, NULL, op->line); break;
            case TK_EQ: emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line); emit(f, OP_BOOL_FROM_FLAGS, 3,0,0,NULL,op->line); break;
            case TK_NE: emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line); emit(f, OP_BOOL_FROM_FLAGS, 4,0,0,NULL,op->line); break;
            case TK_LT: emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line); emit(f, OP_BOOL_FROM_FLAGS, 6,0,0,NULL,op->line); break;
            case TK_GE: emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line); emit(f, OP_BOOL_FROM_FLAGS, 5,0,0,NULL,op->line); break;
            case TK_LE: emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line); emit(f, OP_BOOL_LE,0,0,0,NULL,op->line); break;
            case TK_GT: emit(f, OP_SUB_RR, 0, 2, 0, NULL, op->line); emit(f, OP_BOOL_GT,0,0,0,NULL,op->line); break;
            default: break;
        }
    }
}

static void gen_assignment2(Compiler *c, TokBuf *tb, Function *f) {
    if (peek(tb)->kind == TK_IDENT && tb->toks[tb->pos + 1].kind == TK_ASSIGN) {
        Token *id = next(tb);
        next(tb);
        gen_assignment2(c, tb, f);
        bool is_global = false;
        int slot = -1;
        uint16_t addr = 0;
        if (!lookup_symbol(c, f, id->text, id->line, &is_global, &slot, &addr)) return;
        if (is_global) emit_store_abs(f, addr, id->line);
        else emit_store_slot(f, slot, id->line);
        return;
    }
    gen_expr_prec(c, tb, f, 0);
}

static void gen_expr(Compiler *c, TokBuf *tb, Function *f) { gen_assignment2(c, tb, f); }

static void parse_statement(Compiler *c, TokBuf *tb, Function *f);

static void parse_block(Compiler *c, TokBuf *tb, Function *f) {
    expect(tb, TK_LBRACE, c, "expected '{'");
    while (peek(tb)->kind != TK_RBRACE && peek(tb)->kind != TK_EOF) {
        if (accept(tb, TK_INT)) {
            Token *id = expect(tb, TK_IDENT, c, "expected identifier after int");
            int slot = add_local(c, f, id->text, id->line);
            if (accept(tb, TK_ASSIGN)) {
                gen_expr(c, tb, f);
                emit_store_slot(f, slot, id->line);
            }
            expect(tb, TK_SEMI, c, "expected ';' after declaration");
            continue;
        }
        parse_statement(c, tb, f);
    }
    expect(tb, TK_RBRACE, c, "expected '}'");
}

static void parse_statement(Compiler *c, TokBuf *tb, Function *f) {
    Token *t = peek(tb);
    if (t->kind == TK_LBRACE) { parse_block(c, tb, f); return; }
    if (accept(tb, TK_RETURN)) {
        gen_expr(c, tb, f);
        expect(tb, TK_SEMI, c, "expected ';' after return");
        emit(f, OP_JABS, 0,0,0, f->end_label, t->line);
        return;
    }
    if (accept(tb, TK_IF)) {
        char lelse[MAX_NAME], lend[MAX_NAME];
        new_label(c, lelse, sizeof(lelse), "else");
        new_label(c, lend, sizeof(lend), "ifend");
        expect(tb, TK_LPAREN, c, "expected '(' after if");
        gen_expr(c, tb, f);
        expect(tb, TK_RPAREN, c, "expected ')' after if condition");
        emit(f, OP_JABS_IF_ZERO, 0,0,0, lelse, t->line);
        parse_statement(c, tb, f);
        if (accept(tb, TK_ELSE)) {
            emit(f, OP_JABS, 0,0,0, lend, t->line);
            emit_label(f, lelse);
            parse_statement(c, tb, f);
            emit_label(f, lend);
        } else {
            emit_label(f, lelse);
        }
        return;
    }
    if (accept(tb, TK_WHILE)) {
        char ltop[MAX_NAME], lend[MAX_NAME];
        new_label(c, ltop, sizeof(ltop), "while");
        new_label(c, lend, sizeof(lend), "wend");
        emit_label(f, ltop);
        expect(tb, TK_LPAREN, c, "expected '(' after while");
        gen_expr(c, tb, f);
        expect(tb, TK_RPAREN, c, "expected ')' after while condition");
        emit(f, OP_JABS_IF_ZERO, 0,0,0, lend, t->line);
        parse_statement(c, tb, f);
        emit(f, OP_JABS, 0,0,0, ltop, t->line);
        emit_label(f, lend);
        return;
    }
    if (accept(tb, TK_SEMI)) { emit_nop(f, t->line); return; }
    gen_expr(c, tb, f);
    expect(tb, TK_SEMI, c, "expected ';' after expression");
}

static void parse_global_at(Compiler *c, TokBuf *tb) {
    Token *at = expect(tb, TK_AT, c, "expected __at");
    expect(tb, TK_LPAREN, c, "expected '(' after __at");
    Token *num = expect(tb, TK_NUM, c, "expected numeric address in __at(...)");
    expect(tb, TK_RPAREN, c, "expected ')' after __at address");
    expect(tb, TK_INT, c, "expected 'int' after __at(...) ");
    Token *name = expect(tb, TK_IDENT, c, "expected global variable name");
    add_global(c, name->text, (uint16_t)num->num, at->line);
    if (accept(tb, TK_ASSIGN)) {
        cerror(c, at->line, "fixed-address globals do not support initializers; initialize them in code");
        while (peek(tb)->kind != TK_SEMI && peek(tb)->kind != TK_EOF) next(tb);
    }
    expect(tb, TK_SEMI, c, "expected ';' after fixed-address global declaration");
}

static void parse_function(Compiler *c, TokBuf *tb) {
    expect(tb, TK_INT, c, "expected 'int' at function start");
    Token *name = expect(tb, TK_IDENT, c, "expected function name");
    Function *f = new_function(c, name->text, name->line);
    expect(tb, TK_LPAREN, c, "expected '('");
    if (!accept(tb, TK_RPAREN)) {
        for (;;) {
            expect(tb, TK_INT, c, "expected 'int' in parameter list");
            Token *pid = expect(tb, TK_IDENT, c, "expected parameter name");
            if (f->param_count >= MAX_ARGS) cerror(c, pid->line, "at most %d parameters supported", MAX_ARGS);
            else {
                strncpy(f->params[f->param_count], pid->text, MAX_NAME-1);
                add_local(c, f, pid->text, pid->line);
                f->param_count++;
            }
            if (!accept(tb, TK_COMMA)) break;
        }
        expect(tb, TK_RPAREN, c, "expected ')'");
    }
    parse_block(c, tb, f);
    emit_label(f, f->end_label);
    emit(f, OP_RET, 0,0,0,NULL, name->line);
}

static void parse_program(Compiler *c, TokBuf *tb) {
    while (peek(tb)->kind != TK_EOF) {
        if (peek(tb)->kind == TK_AT) parse_global_at(c, tb);
        else parse_function(c, tb);
    }
}

static int op_size(const Op *op) {
    switch (op->kind) {
        case OP_LABEL: case OP_COMMENT: return 0;
        case OP_AR0: case OP_CPY_RR: case OP_CPY_RM: case OP_CPY_MR:
        case OP_ADD_RR: case OP_SUB_RR: case OP_AND_RR: case OP_OR_RR:
        case OP_XOR_R0R: case OP_JMP_REL: case OP_NOP: return 1;
        case OP_LOAD_SLOT: return 7;
        case OP_STORE_SLOT: return 9;
        case OP_PUSH_R0: return 2;
        case OP_POP_REG: return 2;
        case OP_BOOL_FROM_FLAGS: return 10;
        case OP_BOOL_NOT: return 11;
        case OP_BOOL_LE: return 11;
        case OP_BOOL_GT: return 16;
        case OP_JABS: return 5;
        case OP_JABS_IF_ZERO: return 7;
        case OP_JABS_IF_NZ: return 7;
        case OP_CALL: return 5;
        case OP_RET: return 6;
        case OP_HALT: return 5;
        default: return 0;
    }
}

static int find_func(Compiler *c, const char *name) {
    for (int i = 0; i < c->func_count; i++) if (!strcmp(c->funcs[i].name, name)) return i;
    return -1;
}

static int final_find_label(Final *fin, const char *name) {
    for (int i = 0; i < fin->label_count; i++) if (!strcmp(fin->labels[i].name, name)) return i;
    return -1;
}

static void final_add_label(Final *fin, const char *name, uint16_t addr) {
    if (fin->label_count >= MAX_LABELS) die("too many final labels");
    strncpy(fin->labels[fin->label_count].name, name, MAX_NAME-1);
    fin->labels[fin->label_count].addr = addr;
    fin->label_count++;
}

static void final_emit_asm(Final *fin, const char *fmt, ...) {
    if (fin->line_count >= MAX_WORDS) die("too many assembly lines");
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(fin->lines[fin->line_count++], sizeof(fin->lines[0]), fmt, ap);
    va_end(ap);
}

static uint16_t enc_generic(int cond, int op, int m, int dst, int src) {
    return (uint16_t)(((cond & 7) << 13) | ((op & 7) << 10) | ((m & 3) << 8) | ((dst & 15) << 4) | (src & 15));
}
static uint16_t enc_special_0000110(int cond, int oo, int x) { return (uint16_t)(((cond & 7)<<13) | (0<<10) | (1<<9) | (1<<8) | (0<<7) | ((oo & 3)<<4) | (x & 15)); }
static uint16_t enc_special_0010110(int cond, int oo, int x) { return (uint16_t)(((cond & 7)<<13) | (1<<10) | (1<<9) | (0<<8) | (1<<7) | ((oo & 3)<<4) | (x & 15)); }
static uint16_t enc_ar0(int imm12, int s) { return (uint16_t)(0xE000 | ((s & 1) << 12) | (imm12 & 0x0FFF)); }
static uint16_t enc_jmp(int cond, int8_t off) { return (uint16_t)(((cond & 7)<<13) | 0x1800 | ((uint8_t)off)); }

static void fin_word(Final *fin, uint16_t w, const char *asmline) {
    if (fin->nwords >= MAX_WORDS) die("program too large");
    fin->words[fin->nwords++] = w;
    if (asmline) final_emit_asm(fin, "%s", asmline);
}

static uint16_t label_addr(Final *fin, const char *name) {
    int i = final_find_label(fin, name);
    if (i < 0) die("internal: unknown label %s", name);
    return fin->labels[i].addr;
}

static void emit_load_const_fin(Final *fin, uint16_t v, int target) {
    char buf[128];
    snprintf(buf, sizeof(buf), "AR0 0x%04X, 1", v & 0xFFF0); fin_word(fin, enc_ar0((v >> 4) & 0x0FFF, 1), buf);
    snprintf(buf, sizeof(buf), "CPY R1, R0"); fin_word(fin, enc_generic(0,0,0,1,0), buf);
    snprintf(buf, sizeof(buf), "AR0 0x%X, 0", v & 0xF); fin_word(fin, enc_ar0(v & 0xF, 0), buf);
    snprintf(buf, sizeof(buf), "OR R0, R1"); fin_word(fin, enc_generic(0,5,0,0,1), buf);
    if (target != 0) { snprintf(buf, sizeof(buf), "CPY R%d, R0", target); fin_word(fin, enc_generic(0,0,0,target,0), buf); }
}

static void emit_abs_jump_fin(Final *fin, uint16_t addr) {
    emit_load_const_fin(fin, addr, 0);
    fin_word(fin, enc_generic(0,0,0,15,0), "CPY PC, R0");
}

static void emit_abs_jump_if_zero_fin(Final *fin, uint16_t addr) {
    uint16_t start = (uint16_t)fin->nwords;
    char buf[64];
    fin_word(fin, enc_generic(0,3,0,0,12), "SUB R0, ZERO");
    snprintf(buf, sizeof(buf), "JMP.ne 0x%04X", (uint16_t)(start + 7));
    fin_word(fin, enc_jmp(4, 5), buf);
    emit_abs_jump_fin(fin, addr);
}

static void emit_abs_jump_if_nz_fin(Final *fin, uint16_t addr) {
    uint16_t start = (uint16_t)fin->nwords;
    char buf[64];
    fin_word(fin, enc_generic(0,3,0,0,12), "SUB R0, ZERO");
    snprintf(buf, sizeof(buf), "JMP.eq 0x%04X", (uint16_t)(start + 7));
    fin_word(fin, enc_jmp(3, 5), buf);
    emit_abs_jump_fin(fin, addr);
}

static void emit_bool_from_cond(Final *fin, int cond, const char *cname) {
    uint16_t start = (uint16_t)fin->nwords;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s 0x%04X", cname, (uint16_t)(start + 6));
    fin_word(fin, enc_jmp(cond, 5), buf);
    fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
    fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
    fin_word(fin, enc_ar0(0,0), "AR0 0x0, 0");
    fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
    snprintf(buf, sizeof(buf), "JMP.al 0x%04X", (uint16_t)(start + 10));
    fin_word(fin, enc_jmp(0, 4), buf);
    fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
    fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
    fin_word(fin, enc_ar0(1,0), "AR0 0x1, 0");
    fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
}

static void lower_function(Compiler *c, Function *f, Final *fin, bool labels_only) {
    uint16_t pc = fin->nwords;
    if (labels_only) final_add_label(fin, f->name, pc); else final_emit_asm(fin, "%s:", f->name);

    if (!labels_only) {
        final_emit_asm(fin, "; prologue");
        fin_word(fin, enc_generic(0,3,0,10,13), "SUB R10, ONE");
        fin_word(fin, enc_generic(0,0,2,10,9), "CPY [R10], R9");
        fin_word(fin, enc_generic(0,3,0,10,13), "SUB R10, ONE");
        fin_word(fin, enc_generic(0,0,2,10,11), "CPY [R10], R11");
        fin_word(fin, enc_generic(0,0,0,9,10), "CPY R9, R10");
        emit_load_const_fin(fin, (uint16_t)f->local_count, 0);
        fin_word(fin, enc_generic(0,3,0,10,0), "SUB R10, R0");
        for (int i = 0; i < f->param_count; i++) {
            /* store Ri+1 to slot i */
            char tmp[64]; snprintf(tmp, sizeof(tmp), "CPY R0, R%d", i+1);
            fin_word(fin, enc_generic(0,0,0,0,i+1), tmp);
            emit_load_const_fin(fin, (uint16_t)(i + 1), 0);
            fin_word(fin, enc_generic(0,0,0,8,9), "CPY R8, R9");
            fin_word(fin, enc_generic(0,3,0,8,0), "SUB R8, R0");
            fin_word(fin, enc_generic(0,0,2,8,0), "CPY [R8], R0");
        }
    } else {
        fin->nwords += 10 + f->param_count * 8;
    }

    for (int i = 0; i < f->op_count; i++) {
        Op *op = &f->ops[i];
        if (labels_only) {
            if (op->kind == OP_LABEL) final_add_label(fin, op->s, fin->nwords);
            else fin->nwords += op_size(op);
            continue;
        }
        switch (op->kind) {
            case OP_LABEL:
                final_emit_asm(fin, "%s:", op->s);
                break;
            case OP_AR0: {
                char buf[64]; snprintf(buf, sizeof(buf), "AR0 0x%04X, %d", (op->a & 0xFFF) << ((op->b & 1) ? 4 : 0), op->b & 1);
                fin_word(fin, enc_ar0(op->a & 0xFFF, op->b & 1), buf);
                break;
            }
            case OP_CPY_RR: { char buf[64]; snprintf(buf,sizeof(buf),"CPY R%d, R%d", op->a, op->b); fin_word(fin, enc_generic(0,0,0,op->a,op->b), buf); break; }
            case OP_CPY_RM: { char buf[64]; snprintf(buf,sizeof(buf),"CPY R%d, [R%d]", op->a, op->b); fin_word(fin, enc_generic(0,0,1,op->a,op->b), buf); break; }
            case OP_CPY_MR: { char buf[64]; snprintf(buf,sizeof(buf),"CPY [R%d], R%d", op->a, op->b); fin_word(fin, enc_generic(0,0,2,op->a,op->b), buf); break; }
            case OP_ADD_RR: { char buf[64]; snprintf(buf,sizeof(buf),"ADD R%d, R%d", op->a, op->b); fin_word(fin, enc_generic(0,2,0,op->a,op->b), buf); break; }
            case OP_SUB_RR: { char buf[64]; snprintf(buf,sizeof(buf),"SUB R%d, R%d", op->a, op->b); fin_word(fin, enc_generic(0,3,0,op->a,op->b), buf); break; }
            case OP_AND_RR: { char buf[64]; snprintf(buf,sizeof(buf),"AND R%d, R%d", op->a, op->b); fin_word(fin, enc_generic(0,4,0,op->a,op->b), buf); break; }
            case OP_OR_RR: { char buf[64]; snprintf(buf,sizeof(buf),"OR R%d, R%d", op->a, op->b); fin_word(fin, enc_generic(0,5,0,op->a,op->b), buf); break; }
            case OP_XOR_R0R: { char buf[64]; snprintf(buf,sizeof(buf),"XOR R%d", op->a); fin_word(fin, enc_special_0000110(0,1,op->a), buf); break; }
            case OP_PUSH_R0:
                fin_word(fin, enc_generic(0,3,0,10,13), "SUB R10, ONE");
                fin_word(fin, enc_generic(0,0,2,10,0), "CPY [R10], R0");
                break;
            case OP_POP_REG: { char buf1[64], buf2[64]; snprintf(buf1,sizeof(buf1),"CPY R%d, [R10]", op->a); snprintf(buf2,sizeof(buf2),"ADD R10, ONE"); fin_word(fin, enc_generic(0,0,1,op->a,10), buf1); fin_word(fin, enc_generic(0,2,0,10,13), buf2); break; }
            case OP_LOAD_SLOT:
                emit_load_const_fin(fin, (uint16_t)(op->a + 1), 0);
                fin_word(fin, enc_generic(0,0,0,8,9), "CPY R8, R9");
                fin_word(fin, enc_generic(0,3,0,8,0), "SUB R8, R0");
                fin_word(fin, enc_generic(0,0,1,0,8), "CPY R0, [R8]");
                break;
            case OP_STORE_SLOT:
                fin_word(fin, enc_generic(0,0,0,2,0), "CPY R2, R0");
                emit_load_const_fin(fin, (uint16_t)(op->a + 1), 0);
                fin_word(fin, enc_generic(0,0,0,8,9), "CPY R8, R9");
                fin_word(fin, enc_generic(0,3,0,8,0), "SUB R8, R0");
                fin_word(fin, enc_generic(0,0,0,0,2), "CPY R0, R2");
                fin_word(fin, enc_generic(0,0,2,8,0), "CPY [R8], R0");
                break;
            case OP_JMP_REL: {
                uint16_t target = label_addr(fin, op->s);
                int off = (int)target - ((int)fin->nwords + 1);
                if (off < -128 || off > 127) die("relative jump out of range to %s", op->s);
                char buf[64]; snprintf(buf, sizeof(buf), "JMP.%s %s", op->a==0?"al":op->a==3?"eq":op->a==4?"ne":op->a==5?"ge":"ls", op->s);
                fin_word(fin, enc_jmp(op->a, (int8_t)off), buf);
                break;
            }
            case OP_JABS: emit_abs_jump_fin(fin, label_addr(fin, op->s)); break;
            case OP_JABS_IF_ZERO: emit_abs_jump_if_zero_fin(fin, label_addr(fin, op->s)); break;
            case OP_JABS_IF_NZ: emit_abs_jump_if_nz_fin(fin, label_addr(fin, op->s)); break;
            case OP_CALL: emit_abs_jump_fin(fin, label_addr(fin, op->s)); break;
            case OP_RET:
                fin_word(fin, enc_generic(0,0,0,10,9), "CPY R10, R9");
                fin_word(fin, enc_generic(0,0,1,11,10), "CPY R11, [R10]");
                fin_word(fin, enc_generic(0,2,0,10,13), "ADD R10, ONE");
                fin_word(fin, enc_generic(0,0,1,9,10), "CPY R9, [R10]");
                fin_word(fin, enc_generic(0,2,0,10,13), "ADD R10, ONE");
                fin_word(fin, enc_generic(0,0,0,15,11), "CPY PC, R11");
                break;
            case OP_HALT:
                emit_load_const_fin(fin, 0xFFFF, 0);
                fin_word(fin, enc_generic(0,0,0,15,0), "CPY PC, R0");
                break;
            case OP_BOOL_NOT: {
                uint16_t start = (uint16_t)fin->nwords; char buf[64];
                fin_word(fin, enc_generic(0,3,0,0,12), "SUB R0, ZERO");
                snprintf(buf, sizeof(buf), "JMP.ne 0x%04X", (uint16_t)(start + 7));
                fin_word(fin, enc_jmp(4, 5), buf);
                fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
                fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
                fin_word(fin, enc_ar0(1,0), "AR0 0x1, 0");
                fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
                snprintf(buf, sizeof(buf), "JMP.al 0x%04X", (uint16_t)(start + 11));
                fin_word(fin, enc_jmp(0, 4), buf);
                fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
                fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
                fin_word(fin, enc_ar0(0,0), "AR0 0x0, 0");
                fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
                break; }
            case OP_BOOL_FROM_FLAGS:
                if (op->a == 3) emit_bool_from_cond(fin, 3, "JMP.eq __bool_true");
                else if (op->a == 4) emit_bool_from_cond(fin, 4, "JMP.ne __bool_true");
                else if (op->a == 5) emit_bool_from_cond(fin, 5, "JMP.ge __bool_true");
                else emit_bool_from_cond(fin, 6, "JMP.ls __bool_true");
                break;
            case OP_BOOL_LE: {
                uint16_t start = (uint16_t)fin->nwords; char buf[64];
                snprintf(buf, sizeof(buf), "JMP.ls 0x%04X", (uint16_t)(start + 7));
                fin_word(fin, enc_jmp(6, 6), buf);
                snprintf(buf, sizeof(buf), "JMP.eq 0x%04X", (uint16_t)(start + 7));
                fin_word(fin, enc_jmp(3, 5), buf);
                fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
                fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
                fin_word(fin, enc_ar0(0,0), "AR0 0x0, 0");
                fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
                snprintf(buf, sizeof(buf), "JMP.al 0x%04X", (uint16_t)(start + 11));
                fin_word(fin, enc_jmp(0, 4), buf);
                fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
                fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
                fin_word(fin, enc_ar0(1,0), "AR0 0x1, 0");
                fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
                break; }
            case OP_BOOL_GT: {
                uint16_t start = (uint16_t)fin->nwords; char buf[64];
                snprintf(buf, sizeof(buf), "JMP.ge 0x%04X", (uint16_t)(start + 6));
                fin_word(fin, enc_jmp(5, 5), buf);
                fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
                fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
                fin_word(fin, enc_ar0(0,0), "AR0 0x0, 0");
                fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
                snprintf(buf, sizeof(buf), "JMP.al 0x%04X", (uint16_t)(start + 16));
                fin_word(fin, enc_jmp(0, 10), buf);
                snprintf(buf, sizeof(buf), "JMP.eq 0x%04X", (uint16_t)(start + 12));
                fin_word(fin, enc_jmp(3, 5), buf);
                fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
                fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
                fin_word(fin, enc_ar0(1,0), "AR0 0x1, 0");
                fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
                snprintf(buf, sizeof(buf), "JMP.al 0x%04X", (uint16_t)(start + 16));
                fin_word(fin, enc_jmp(0, 4), buf);
                fin_word(fin, enc_ar0(0,1), "AR0 0x000, 1");
                fin_word(fin, enc_generic(0,0,0,1,0), "CPY R1, R0");
                fin_word(fin, enc_ar0(0,0), "AR0 0x0, 0");
                fin_word(fin, enc_generic(0,5,0,0,1), "OR R0, R1");
                break; }
            case OP_NOP:
                fin_word(fin, enc_generic(0,0,0,0,0), "CPY R0, R0");
                break;
            default:
                die("internal: unhandled op kind %d", op->kind);
        }
    }
}

static bool has_main(Compiler *c) { return find_func(c, "main") >= 0; }

static void lower_program(Compiler *c, Final *fin) {
    memset(fin, 0, sizeof(*fin));
    /* First pass: label addresses. */
    final_add_label(fin, "_start", 0);
    fin->nwords += 15; /* startup fixed size: set SP, call main, halt */
    for (int i = 0; i < c->func_count; i++) lower_function(c, &c->funcs[i], fin, true);
    int total_words = fin->nwords;
    fin->nwords = 0;
    fin->line_count = 0;

    final_emit_asm(fin, "; BackSlash 9 tiny C compiler output");
    final_emit_asm(fin, "; entry point and runtime start");
    final_emit_asm(fin, "_start:");
    emit_load_const_fin(fin, c->sp_init, 10);
    emit_abs_jump_fin(fin, label_addr(fin, "main"));
    emit_load_const_fin(fin, 0xFFFF, 0);
    fin_word(fin, enc_generic(0,0,0,15,0), "CPY PC, R0");
    for (int i = 0; i < c->func_count; i++) lower_function(c, &c->funcs[i], fin, false);
    if (fin->nwords != total_words) {
        fprintf(stderr, "internal layout mismatch: pass1=%d pass2=%d\n", total_words, fin->nwords);
        exit(1);
    }
}

static void write_asm(const char *path, Final *fin);

static void write_bin(const char *path, Final *fin, bool be) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "/tmp/bslash9cc_%ld.asm", (long)getpid());
    write_asm(tmp, fin);
    char cmd[1024];
    const char *asmpath = getenv("BSLASH9ASM");
    if (!asmpath || !*asmpath) asmpath = "bslash9asm";
    snprintf(cmd, sizeof(cmd), "%s %s %s %s", asmpath, be ? "--be" : "--le", tmp, path);
    int rc = system(cmd);
    unlink(tmp);
    if (rc != 0) die("assembler invocation failed");
}

static void write_asm(const char *path, Final *fin) {
    FILE *f = path ? fopen(path, "w") : stdout;
    if (!f) die("cannot open %s for writing", path);
    for (int i = 0; i < fin->line_count; i++) fprintf(f, "%s\n", fin->lines[i]);
    if (path) fclose(f);
}

static void usage(void) {
    fprintf(stderr,
        "usage: bslash9cc [-S] [--be|--le] input.c output\n"
        "  -S     output BackSlash 9 assembly instead of raw binary\n"
        "  --be   big-endian binary output\n"
        "  --le   little-endian binary output (default)\n");
}

int main(int argc, char **argv) {
    bool emit_asm = false;
    bool be = false;
    uint16_t sp_init = 0xFFFE;
    const char *in = NULL, *out = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-S")) emit_asm = true;
        else if (!strcmp(argv[i], "--be")) be = true;
        else if (!strcmp(argv[i], "--le")) be = false;
        else if (!strncmp(argv[i], "--sp=", 5)) {
            char *end = NULL;
            unsigned long v = strtoul(argv[i] + 5, &end, 0);
            if (!end || *end) die("invalid --sp value: %s", argv[i] + 5);
            sp_init = (uint16_t)(v & 0xFFFFu);
        }
        else if (!in) in = argv[i];
        else if (!out) out = argv[i];
        else { usage(); return 1; }
    }
    if (!in || !out) { usage(); return 1; }

    Compiler *c = (Compiler *)calloc(1, sizeof(Compiler));
    TokBuf *tb = (TokBuf *)calloc(1, sizeof(TokBuf));
    Final *fin = (Final *)calloc(1, sizeof(Final));
    if (!c || !tb || !fin) die("out of memory");
    c->filename = in; c->big_endian = be; c->sp_init = sp_init;
    char *src = read_file(in);
    tb->filename = in;
    lex(src, tb, c);
    if (c->errors) return 1;
    parse_program(c, tb);
    if (c->errors) return 1;
    if (!has_main(c)) { cerror(c, 1, "program must define int main()" ); return 1; }
    lower_program(c, fin);
    if (emit_asm) write_asm(out, fin);
    else write_bin(out, fin, be);
    free(src);
    free(fin);
    free(tb);
    free(c);
    return 0;
}
