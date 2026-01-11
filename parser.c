#include "common.h"

static Expr *parse_expression(Parser *p);
static Stmt *parse_statement(Parser *p);

static Token *peek(Parser *p) {
    return &p->items[p->pos];
}

static Token *peek_n(Parser *p, size_t n) {
    return &p->items[p->pos + n];
}


static Token *advance(Parser *p) {
    return &p->items[p->pos++];
}


static int match(Parser *p, TokenKind kind, const char *text) {
    Token *t = peek(p);
    if (t->kind != kind) return 0;
    if (text && strcmp(t->text, text) != 0) return 0;
    p->pos++;
    return 1;
}


static Token *expect(Parser *p, TokenKind kind, const char *text) {
    Token *t = peek(p);
    if (t->kind != kind || (text && strcmp(t->text, text) != 0)) {
        fprintf(stderr, "expected %s\n", text ? text : "token");
        exit(1);
    }
    return advance(p);
}


static Expr *new_expr(ExprKind kind) {
    Expr *e = (Expr *)xmalloc(sizeof(Expr));
    memset(e, 0, sizeof(Expr));
    e->kind = kind;
    return e;
}


static Stmt *new_stmt(StmtKind kind) {
    Stmt *s = (Stmt *)xmalloc(sizeof(Stmt));
    memset(s, 0, sizeof(Stmt));
    s->kind = kind;
    return s;
}


static void strings_push(Parser *p, StringLit *s) {
    if (p->strings_count == p->strings_cap) {
        size_t nc = p->strings_cap ? p->strings_cap * 2 : 32;
        p->strings = (StringLit **)realloc(p->strings, nc * sizeof(StringLit *));
        if (!p->strings) die("out of memory");
        p->strings_cap = nc;
    }
    p->strings[p->strings_count++] = s;
}

static Expr *parse_expression(Parser *p);
static Stmt *parse_statement(Parser *p);


static Expr *parse_primary(Parser *p) {
    Token *t = peek(p);
    if (t->kind == TK_NUM) {
        advance(p);
        Expr *e = new_expr(EX_NUM);
        e->v.num = t->num;
        return e;
    }
    if (t->kind == TK_STR) {
        advance(p);
        StringLit *s = (StringLit *)xmalloc(sizeof(StringLit));
        s->len = strlen(t->text);
        s->data = t->text;
        s->rva = 0;
        strings_push(p, s);
        Expr *e = new_expr(EX_STR);
        e->v.str = s;
        return e;
    }
    if (t->kind == TK_KW && (strcmp(t->text, "истина.ок") == 0 || strcmp(t->text, "ложь.падение") == 0)) {
        advance(p);
        Expr *e = new_expr(EX_BOOL);
        e->v.boolv = strcmp(t->text, "истина.ок") == 0;
        return e;
    }
    if (t->kind == TK_ID) {
        advance(p);
        Expr *e = new_expr(EX_VAR);
        e->v.var = t->text;
        return e;
    }
    if (t->kind == TK_SYM && strcmp(t->text, "(") == 0) {
        if (peek_n(p, 1)->kind == TK_ID &&
            peek_n(p, 2)->kind == TK_SYM && strcmp(peek_n(p, 2)->text, ")") == 0 &&
            peek_n(p, 3)->kind == TK_OP && strcmp(peek_n(p, 3)->text, "=>") == 0) {
            advance(p);
            char *param = expect(p, TK_ID, 0)->text;
            expect(p, TK_SYM, ")");
            expect(p, TK_OP, "=>");
            Expr *body = parse_expression(p);
            Expr *e = new_expr(EX_LAMBDA);
            e->v.lambda.param = param;
            e->v.lambda.body = body;
            return e;
        }
        advance(p);
        Expr *e = parse_expression(p);
        expect(p, TK_SYM, ")");
        return e;
    }
    die("bad expression");
    return 0;
}

static Expr *parse_postfix(Parser *p) {
    Expr *expr = parse_primary(p);
    while (match(p, TK_SYM, "(")) {
        Expr **args = 0;
        size_t argc = 0;
        if (!match(p, TK_SYM, ")")) {
            while (1) {
                Expr *a = parse_expression(p);
                args = (Expr **)realloc(args, (argc + 1) * sizeof(Expr *));
                if (!args) die("out of memory");
                args[argc++] = a;
                if (match(p, TK_SYM, ")")) break;
                expect(p, TK_SYM, ",");
            }
        }
        if (expr->kind != EX_VAR) die("call on non-name");
        Expr *call = new_expr(EX_CALL);
        call->v.call.name = expr->v.var;
        call->v.call.args = args;
        call->v.call.argc = argc;
        expr = call;
    }
    return expr;
}


static Expr *parse_unary(Parser *p) {
    Token *t = peek(p);
    if (t->kind == TK_OP && strcmp(t->text, "-") == 0) {
        advance(p);
        Expr *e = new_expr(EX_UNARY);
        e->v.un.op = OP_NEG;
        e->v.un.expr = parse_unary(p);
        return e;
    }
    if (t->kind == TK_KW && strcmp(t->text, "не.а") == 0) {
        advance(p);
        Expr *e = new_expr(EX_UNARY);
        e->v.un.op = OP_NOT;
        e->v.un.expr = parse_unary(p);
        return e;
    }
    return parse_postfix(p);
}


static Expr *parse_factor(Parser *p) {
    Expr *left = parse_unary(p);
    while (1) {
        Token *t = peek(p);
        if (t->kind == TK_OP && (strcmp(t->text, "*") == 0 || strcmp(t->text, "/") == 0)) {
            advance(p);
            Expr *e = new_expr(EX_BIN);
            e->v.bin.op = (strcmp(t->text, "*") == 0) ? OP_MUL : OP_DIV;
            e->v.bin.left = left;
            e->v.bin.right = parse_unary(p);
            left = e;
            continue;
        }
        break;
    }
    return left;
}


static Expr *parse_term(Parser *p) {
    Expr *left = parse_factor(p);
    while (1) {
        Token *t = peek(p);
        if (t->kind == TK_OP && (strcmp(t->text, "+") == 0 || strcmp(t->text, "-") == 0)) {
            advance(p);
            Expr *e = new_expr(EX_BIN);
            e->v.bin.op = (strcmp(t->text, "+") == 0) ? OP_ADD : OP_SUB;
            e->v.bin.left = left;
            e->v.bin.right = parse_factor(p);
            left = e;
            continue;
        }
        break;
    }
    return left;
}


static Expr *parse_compare(Parser *p) {
    Expr *left = parse_term(p);
    while (1) {
        Token *t = peek(p);
        if (t->kind == TK_OP && (
            strcmp(t->text, "<") == 0 || strcmp(t->text, ">") == 0 ||
            strcmp(t->text, "<=") == 0 || strcmp(t->text, ">=") == 0)) {
            advance(p);
            Expr *e = new_expr(EX_BIN);
            if (strcmp(t->text, "<") == 0) e->v.bin.op = OP_LT;
            else if (strcmp(t->text, ">") == 0) e->v.bin.op = OP_GT;
            else if (strcmp(t->text, "<=") == 0) e->v.bin.op = OP_LE;
            else e->v.bin.op = OP_GE;
            e->v.bin.left = left;
            e->v.bin.right = parse_term(p);
            left = e;
            continue;
        }
        break;
    }
    return left;
}


static Expr *parse_equality(Parser *p) {
    Expr *left = parse_compare(p);
    while (1) {
        Token *t = peek(p);
        if (t->kind == TK_OP && (strcmp(t->text, "==") == 0 || strcmp(t->text, "!=") == 0 || strcmp(t->text, "=/=") == 0)) {
            advance(p);
            Expr *e = new_expr(EX_BIN);
            e->v.bin.op = (strcmp(t->text, "==") == 0) ? OP_EQ : OP_NE;
            e->v.bin.left = left;
            e->v.bin.right = parse_compare(p);
            left = e;
            continue;
        }
        break;
    }
    return left;
}


static Expr *parse_logic_and(Parser *p) {
    Expr *left = parse_equality(p);
    while (1) {
        Token *t = peek(p);
        if (t->kind == TK_KW && strcmp(t->text, "и.также") == 0) {
            advance(p);
            Expr *e = new_expr(EX_BIN);
            e->v.bin.op = OP_AND;
            e->v.bin.left = left;
            e->v.bin.right = parse_equality(p);
            left = e;
            continue;
        }
        break;
    }
    return left;
}


static Expr *parse_logic_or(Parser *p) {
    Expr *left = parse_logic_and(p);
    while (1) {
        Token *t = peek(p);
        if (t->kind == TK_KW && strcmp(t->text, "или.иначе") == 0) {
            advance(p);
            Expr *e = new_expr(EX_BIN);
            e->v.bin.op = OP_OR;
            e->v.bin.left = left;
            e->v.bin.right = parse_logic_and(p);
            left = e;
            continue;
        }
        break;
    }
    return left;
}


static Expr *parse_expression(Parser *p) {
    return parse_logic_or(p);
}


static Stmt *parse_block(Parser *p) {
    expect(p, TK_SYM, "{");
    Stmt *b = new_stmt(ST_BLOCK);
    b->v.block.items = 0;
    b->v.block.count = 0;
    while (!match(p, TK_SYM, "}")) {
        if (peek(p)->kind == TK_EOF) die("expected }");
        Stmt *s = parse_statement(p);
        b->v.block.items = (Stmt **)realloc(b->v.block.items, (b->v.block.count + 1) * sizeof(Stmt *));
        if (!b->v.block.items) die("out of memory");
        b->v.block.items[b->v.block.count++] = s;
    }
    return b;
}


static Stmt *parse_statement(Parser *p) {
    Token *t = peek(p);
    if (t->kind == TK_KW && strcmp(t->text, "исп.команду.print") == 0) {
        advance(p);
        Stmt *s = new_stmt(ST_PRINT);
        expect(p, TK_SYM, "(");
        s->v.print.expr = parse_expression(p);
        expect(p, TK_SYM, ")");
        match(p, TK_SYM, ";");
        return s;
    }
    if (t->kind == TK_KW && strcmp(t->text, "пусть") == 0) {
        advance(p);
        Token *id = expect(p, TK_ID, 0);
        expect(p, TK_OP, "=");
        Stmt *s = new_stmt(ST_LET);
        s->v.let.name = id->text;
        s->v.let.expr = parse_expression(p);
        match(p, TK_SYM, ";");
        return s;
    }
    if (t->kind == TK_KW && strcmp(t->text, "в") == 0) {
        advance(p);
        expect(p, TK_KW, "таком");
        expect(p, TK_KW, "случае");
        Stmt *s = new_stmt(ST_IF);
        s->v.ifs.cond = parse_expression(p);
        s->v.ifs.thenb = parse_block(p);
        s->v.ifs.elseb = 0;
        if (peek(p)->kind == TK_KW && strcmp(peek(p)->text, "иначе.если") == 0) {
            advance(p);
            s->v.ifs.elseb = parse_block(p);
        }
        return s;
    }
    if (t->kind == TK_KW && strcmp(t->text, "повторять.раз") == 0) {
        advance(p);
        Stmt *s = new_stmt(ST_REPEAT);
        s->v.repeat.count = parse_expression(p);
        s->v.repeat.body = parse_block(p);
        return s;
    }
    if (t->kind == TK_ID && peek_n(p, 1)->kind == TK_OP && strcmp(peek_n(p, 1)->text, "=") == 0) {
        Token *id = advance(p);
        expect(p, TK_OP, "=");
        Stmt *s = new_stmt(ST_SET);
        s->v.set.name = id->text;
        s->v.set.expr = parse_expression(p);
        match(p, TK_SYM, ";");
        return s;
    }
    {
        Stmt *s = new_stmt(ST_EXPR);
        s->v.expr.expr = parse_expression(p);
        match(p, TK_SYM, ";");
        return s;
    }
}


Stmt *parse_program(Parser *p) {
    Stmt *b = new_stmt(ST_BLOCK);
    b->v.block.items = 0;
    b->v.block.count = 0;
    while (peek(p)->kind != TK_EOF) {
        Stmt *s = parse_statement(p);
        b->v.block.items = (Stmt **)realloc(b->v.block.items, (b->v.block.count + 1) * sizeof(Stmt *));
        if (!b->v.block.items) die("out of memory");
        b->v.block.items[b->v.block.count++] = s;
    }
    return b;
}

