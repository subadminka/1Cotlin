#ifndef COTLIN_COMMON_H
#define COTLIN_COMMON_H


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    TK_EOF,
    TK_ID,
    TK_NUM,
    TK_STR,
    TK_KW,
    TK_OP,
    TK_SYM
} TokenKind;

typedef struct {
    TokenKind kind;
    char *text;
    int64_t num;
} Token;

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    Token *items;
    size_t count;
    size_t cap;
} Lexer;

typedef struct StringLit {
    char *data;
    size_t len;
    uint32_t rva;
} StringLit;

typedef enum {
    EX_NUM,
    EX_BOOL,
    EX_VAR,
    EX_STR,
    EX_BIN,
    EX_UNARY
} ExprKind;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_AND,
    OP_OR,
    OP_NEG,
    OP_NOT
} OpKind;

typedef struct Expr Expr;

struct Expr {
    ExprKind kind;
    union {
        int64_t num;
        int boolv;
        char *var;
        StringLit *str;
        struct {
            OpKind op;
            Expr *left;
            Expr *right;
        } bin;
        struct {
            OpKind op;
            Expr *expr;
        } un;
    } v;
};

typedef enum {
    ST_BLOCK,
    ST_PRINT,
    ST_LET,
    ST_SET,
    ST_IF,
    ST_REPEAT
} StmtKind;

typedef struct Stmt Stmt;

struct Stmt {
    StmtKind kind;
    union {
        struct { Expr *expr; } print;
        struct { char *name; Expr *expr; } let;
        struct { char *name; Expr *expr; } set;
        struct { Expr *cond; Stmt *thenb; Stmt *elseb; } ifs;
        struct { Expr *count; Stmt *body; } repeat;
        struct { Stmt **items; size_t count; } block;
    } v;
};

typedef struct {
    Token *items;
    size_t count;
    size_t pos;
    StringLit **strings;
    size_t strings_count;
    size_t strings_cap;
} Parser;

typedef struct {
    char *name;
    int index;
} Sym;

typedef struct {
    Sym *items;
    size_t count;
    size_t cap;
} SymTab;

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} CodeBuf;

typedef enum {
    FIX_LABEL,
    FIX_RIP
} FixKind;

typedef struct {
    FixKind kind;
    size_t offset;
    int label_id;
    uint32_t target_rva;
} Fixup;

typedef struct {
    int pos;
} Label;

typedef struct {
    CodeBuf code;
    Fixup *fixups;
    size_t fixup_count;
    size_t fixup_cap;
    Label *labels;
    size_t label_count;
    size_t label_cap;
    SymTab sym;
    int64_t stdout_offset;
    int64_t bytes_written_offset;
    int64_t intbuf_offset;
    int64_t vstack_base_offset;
    int64_t loop_slots_offset;
    int loop_slots;
    uint32_t text_rva;
    uint32_t rdata_rva;
    uint32_t iat_getstd_rva;
    uint32_t iat_write_rva;
    uint32_t iat_exit_rva;
    uint32_t iat_setconcp_rva;
    size_t frame_size;
} CodeGen;


typedef struct {
    size_t rdata_size;
    size_t import_desc_off;
    size_t ilt_off;
    size_t iat_off;
    size_t hn_getstd;
    size_t hn_write;
    size_t hn_exit;
    size_t hn_setconcp;
    size_t dll_name;
} RDataLayout;

void die(const char *msg);
void *xmalloc(size_t n);
char *xstrndup(const char *s, size_t n);
size_t align_up(size_t v, size_t a);
char *read_file(const char *path, size_t *out_len);
char *default_output(const char *in);
void lex_all(Lexer *lx);
Stmt *parse_program(Parser *p);
void sym_add(SymTab *st, const char *name);
int sym_find(SymTab *st, const char *name);
void sem_stmt(Stmt *s, SymTab *st, int *max_stack, int *max_repeat, int repeat_depth);
void gen_stmt(CodeGen *cg, Stmt *s, int *loop_depth);
void gen_prolog(CodeGen *cg);
void gen_epilog(CodeGen *cg);
void patch_fixups(CodeGen *cg);
RDataLayout layout_rdata(CodeGen *cg, StringLit **strings, size_t strings_count);
void write_pe(const char *out, CodeGen *cg, StringLit **strings, size_t strings_count);

#endif
