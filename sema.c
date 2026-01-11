#include "common.h"

void sym_add(SymTab *st, const char *name) {
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->items[i].name, name) == 0) die("duplicate variable");
    }
    if (st->count == st->cap) {
        size_t nc = st->cap ? st->cap * 2 : 32;
        st->items = (Sym *)realloc(st->items, nc * sizeof(Sym));
        if (!st->items) die("out of memory");
        st->cap = nc;
    }
    st->items[st->count].name = (char *)name;
    st->items[st->count].index = (int)st->count;
    st->count++;
}


int sym_find(SymTab *st, const char *name) {
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->items[i].name, name) == 0) return (int)i;
    }
    return -1;
}


static int expr_has_string(Expr *e) {
    if (!e) return 0;
    if (e->kind == EX_STR) return 1;
    if (e->kind == EX_BIN) return expr_has_string(e->v.bin.left) || expr_has_string(e->v.bin.right);
    if (e->kind == EX_UNARY) return expr_has_string(e->v.un.expr);
    return 0;
}


static int expr_depth(Expr *e) {
    if (!e) return 0;
    switch (e->kind) {
        case EX_NUM:
        case EX_BOOL:
        case EX_VAR:
        case EX_STR:
            return 0;
        case EX_UNARY:
            return expr_depth(e->v.un.expr);
        case EX_BIN:
            if (e->v.bin.op == OP_AND || e->v.bin.op == OP_OR) {
                int a = expr_depth(e->v.bin.left);
                int b = expr_depth(e->v.bin.right);
                return a > b ? a : b;
            } else {
                int a = expr_depth(e->v.bin.left);
                int b = expr_depth(e->v.bin.right);
                int m = 1 + b;
                return a > m ? a : m;
            }
    }
    return 0;
}


void sem_stmt(Stmt *s, SymTab *st, int *max_stack, int *max_repeat, int repeat_depth) {
    if (!s) return;
    if (s->kind == ST_BLOCK) {
        for (size_t i = 0; i < s->v.block.count; i++) {
            sem_stmt(s->v.block.items[i], st, max_stack, max_repeat, repeat_depth);
        }
        return;
    }
    if (s->kind == ST_PRINT) {
        int d = expr_depth(s->v.print.expr);
        if (d > *max_stack) *max_stack = d;
        return;
    }
    if (s->kind == ST_LET) {
        if (expr_has_string(s->v.let.expr)) die("string in variable assignment");
        int d = expr_depth(s->v.let.expr);
        if (d > *max_stack) *max_stack = d;
        sym_add(st, s->v.let.name);
        return;
    }
    if (s->kind == ST_SET) {
        if (sym_find(st, s->v.set.name) < 0) die("unknown variable");
        if (expr_has_string(s->v.set.expr)) die("string in assignment");
        int d = expr_depth(s->v.set.expr);
        if (d > *max_stack) *max_stack = d;
        return;
    }
    if (s->kind == ST_IF) {
        if (expr_has_string(s->v.ifs.cond)) die("string in if");
        int d = expr_depth(s->v.ifs.cond);
        if (d > *max_stack) *max_stack = d;
        sem_stmt(s->v.ifs.thenb, st, max_stack, max_repeat, repeat_depth);
        sem_stmt(s->v.ifs.elseb, st, max_stack, max_repeat, repeat_depth);
        return;
    }
    if (s->kind == ST_REPEAT) {
        if (expr_has_string(s->v.repeat.count)) die("string in repeat");
        int d = expr_depth(s->v.repeat.count);
        if (d > *max_stack) *max_stack = d;
        int nd = repeat_depth + 1;
        if (nd > *max_repeat) *max_repeat = nd;
        sem_stmt(s->v.repeat.body, st, max_stack, max_repeat, nd);
        return;
    }
}

