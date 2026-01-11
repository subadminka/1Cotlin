#include "common.h"

void sym_add(SymTab *st, const char *name, TypeKind type) {
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
    st->items[st->count].type = type;
    st->count++;
}

int sym_find(SymTab *st, const char *name) {
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->items[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static TypeKind type_expr_inner(Expr *e, SymTab *st, const char *param);
static TypeKind type_expr(Expr *e, SymTab *st);

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
        case EX_BIN: {
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
        case EX_CALL: {
            int d = 0;
            for (size_t i = 0; i < e->v.call.argc; i++) {
                int a = expr_depth(e->v.call.args[i]);
                if (a > d) d = a;
            }
            return d;
        }
        case EX_LAMBDA:
            return expr_depth(e->v.lambda.body);
    }
    return 0;
}

static TypeKind type_expr_inner(Expr *e, SymTab *st, const char *param) {
    if (!e) return TY_INVALID;
    switch (e->kind) {
        case EX_NUM:
        case EX_BOOL:
            return TY_INT;
        case EX_STR:
            return TY_INVALID;
        case EX_VAR: {
            if (param && strcmp(e->v.var, param) == 0) return TY_INT;
            int idx = sym_find(st, e->v.var);
            if (idx < 0) die("unknown variable");
            return st->items[idx].type;
        }
        case EX_UNARY: {
            TypeKind t = type_expr_inner(e->v.un.expr, st, param);
            if (t != TY_INT) die("bad unary");
            return TY_INT;
        }
        case EX_BIN: {
            TypeKind a = type_expr_inner(e->v.bin.left, st, param);
            TypeKind b = type_expr_inner(e->v.bin.right, st, param);
            if (a != TY_INT || b != TY_INT) die("bad binary");
            return TY_INT;
        }
        case EX_LAMBDA: {
            TypeKind t = type_expr_inner(e->v.lambda.body, st, e->v.lambda.param);
            if (t != TY_INT) die("lambda returns non-int");
            return TY_LAMBDA;
        }
        case EX_CALL: {
            const char *name = e->v.call.name;
            size_t argc = e->v.call.argc;
            Expr **args = e->v.call.args;
            // builtin calls are hardcoded, keep it dumb
            if (strcmp(name, "создать.лист.цифр") == 0) {
                if (argc == 0) return TY_LIST;
                if (argc == 1 && type_expr_inner(args[0], st, param) == TY_INT) return TY_LIST;
                die("создать.лист.цифр(args)");
            }
            if (strcmp(name, "создать.массив.цифр") == 0) {
                if (argc == 1 && type_expr_inner(args[0], st, param) == TY_INT) return TY_ARRAY;
                die("создать.массив.цифр(n)");
            }
            if (strcmp(name, "сколько.внутри") == 0) {
                if (argc == 1) {
                    TypeKind t = type_expr_inner(args[0], st, param);
                    if (t == TY_LIST || t == TY_ARRAY) return TY_INT;
                }
                die("сколько.внутри(x)");
            }
            if (strcmp(name, "впихни.в.лист") == 0) {
                if (argc == 2) {
                    TypeKind t = type_expr_inner(args[0], st, param);
                    if (t == TY_LIST && type_expr_inner(args[1], st, param) == TY_INT) return TY_LIST;
                }
                die("впихни.в.лист(list, value)");
            }
            if (strcmp(name, "достань.последний") == 0) {
                if (argc == 1 && type_expr_inner(args[0], st, param) == TY_LIST) return TY_INT;
                die("достань.последний(list)");
            }
            if (strcmp(name, "дай.по.индексу") == 0) {
                if (argc == 2) {
                    TypeKind t = type_expr_inner(args[0], st, param);
                    if ((t == TY_LIST || t == TY_ARRAY) && type_expr_inner(args[1], st, param) == TY_INT) return TY_INT;
                }
                die("дай.по.индексу(list, i)");
            }
            if (strcmp(name, "сунь.по.индексу") == 0) {
                if (argc == 3) {
                    TypeKind t = type_expr_inner(args[0], st, param);
                    if ((t == TY_LIST || t == TY_ARRAY) &&
                        type_expr_inner(args[1], st, param) == TY_INT &&
                        type_expr_inner(args[2], st, param) == TY_INT) return TY_INT;
                }
                die("сунь.по.индексу(list, i, v)");
            }
            if (strcmp(name, "диапазон.от.0.до") == 0) {
                if (argc == 1 && type_expr_inner(args[0], st, param) == TY_INT) return TY_LIST;
                die("диапазон.от.0.до(n)");
            }
            die("unknown call");
        }
    }
    return TY_INVALID;
}

static TypeKind type_expr(Expr *e, SymTab *st) {
    return type_expr_inner(e, st, 0);
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
        TypeKind t = type_expr(s->v.print.expr, st);
        if (s->v.print.expr->kind == EX_STR || t == TY_INT) {
            int d = expr_depth(s->v.print.expr);
            if (d > *max_stack) *max_stack = d;
            return;
        }
        die("print expects int or string");
    }
    if (s->kind == ST_LET) {
        TypeKind t = type_expr(s->v.let.expr, st);
        if (t == TY_INVALID || t == TY_LAMBDA) die("bad let");
        int d = expr_depth(s->v.let.expr);
        if (d > *max_stack) *max_stack = d;
        sym_add(st, s->v.let.name, t);
        return;
    }
    if (s->kind == ST_SET) {
        int idx = sym_find(st, s->v.set.name);
        if (idx < 0) die("unknown variable");
        TypeKind t = type_expr(s->v.set.expr, st);
        if (t != st->items[idx].type) die("type mismatch");
        int d = expr_depth(s->v.set.expr);
        if (d > *max_stack) *max_stack = d;
        return;
    }
    if (s->kind == ST_IF) {
        if (type_expr(s->v.ifs.cond, st) != TY_INT) die("bad if");
        int d = expr_depth(s->v.ifs.cond);
        if (d > *max_stack) *max_stack = d;
        sem_stmt(s->v.ifs.thenb, st, max_stack, max_repeat, repeat_depth);
        sem_stmt(s->v.ifs.elseb, st, max_stack, max_repeat, repeat_depth);
        return;
    }
    if (s->kind == ST_REPEAT) {
        if (type_expr(s->v.repeat.count, st) != TY_INT) die("bad repeat");
        int d = expr_depth(s->v.repeat.count);
        if (d > *max_stack) *max_stack = d;
        int nd = repeat_depth + 1;
        if (nd > *max_repeat) *max_repeat = nd;
        sem_stmt(s->v.repeat.body, st, max_stack, max_repeat, nd);
        return;
    }
    if (s->kind == ST_EXPR) {
        TypeKind t = type_expr(s->v.expr.expr, st);
        if (t == TY_INVALID || t == TY_LAMBDA) die("bad expr");
        int d = expr_depth(s->v.expr.expr);
        if (d > *max_stack) *max_stack = d;
        return;
    }
}
