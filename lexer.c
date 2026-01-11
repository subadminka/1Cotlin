#include "common.h"

static void lex_push(Lexer *lx, Token t) {
    if (lx->count == lx->cap) {
        size_t nc = lx->cap ? lx->cap * 2 : 128;
        lx->items = (Token *)realloc(lx->items, nc * sizeof(Token));
        if (!lx->items) die("out of memory");
        lx->cap = nc;
    }
    lx->items[lx->count++] = t;
}


static int lex_peek(Lexer *lx) {
    if (lx->pos >= lx->len) return 0;
    return (unsigned char)lx->src[lx->pos];
}


static int lex_get(Lexer *lx) {
    if (lx->pos >= lx->len) return 0;
    return (unsigned char)lx->src[lx->pos++];
}


static void lex_skip_ws(Lexer *lx) {
    while (1) {
        int ch = lex_peek(lx);
        if (ch == 0) break;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            lx->pos++;
            continue;
        }
        break;
    }
}


static int is_ident_start(int ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch >= 0x80;
}


static int is_ident(int ch) {
    return is_ident_start(ch) || (ch >= '0' && ch <= '9') || ch == '.';
}


void lex_all(Lexer *lx) {
    const char *kw[] = {
        "исп.команду.print",
        "пусть",
        "в",
        "таком",
        "случае",
        "иначе.если",
        "повторять.раз",
        "истина.ок",
        "ложь.падение",
        "и.также",
        "или.иначе",
        "не.а"
    };
    while (1) {
        lex_skip_ws(lx);
        int ch = lex_peek(lx);
        if (ch == 0) break;
        if (is_ident_start(ch)) {
            size_t start = lx->pos;
            lex_get(lx);
            while (is_ident(lex_peek(lx))) lex_get(lx);
            size_t n = lx->pos - start;
            char *txt = xstrndup(lx->src + start, n);
            Token t = {TK_ID, txt, 0};
            for (size_t i = 0; i < sizeof(kw)/sizeof(kw[0]); i++) {
                if (strcmp(txt, kw[i]) == 0) {
                    t.kind = TK_KW;
                    break;
                }
            }
            lex_push(lx, t);
            continue;
        }
        if (ch >= '0' && ch <= '9') {
            size_t start = lx->pos;
            lex_get(lx);
            while (lex_peek(lx) >= '0' && lex_peek(lx) <= '9') lex_get(lx);
            size_t n = lx->pos - start;
            char *txt = xstrndup(lx->src + start, n);
            Token t = {TK_NUM, txt, strtoll(txt, 0, 10)};
            lex_push(lx, t);
            continue;
        }
        if (ch == '"') {
            lex_get(lx);
            size_t start = lx->pos;
            char *buf = (char *)xmalloc(lx->len - start + 1);
            size_t blen = 0;
            while (1) {
                int c = lex_get(lx);
                if (c == 0) die("unterminated string");
                if (c == '"') break;
                if (c == '\\') {
                    int e = lex_get(lx);
                    if (e == 'n') c = '\n';
                    else if (e == 't') c = '\t';
                    else if (e == '"') c = '"';
                    else if (e == '\\') c = '\\';
                    else c = e;
                }
                buf[blen++] = (char)c;
            }
            buf[blen] = 0;
            Token t = {TK_STR, buf, 0};
            lex_push(lx, t);
            continue;
        }
        {
            size_t start = lx->pos;
            int c1 = lex_get(lx);
            int c2 = lex_peek(lx);
            int c3 = (lx->pos + 1 < lx->len) ? (unsigned char)lx->src[lx->pos + 1] : 0;
            if (c1 == '=' && c2 == '/' && c3 == '=') {
                lx->pos++;
                lx->pos++;
                Token t = {TK_OP, xstrndup("=/=", 3), 0};
                lex_push(lx, t);
                continue;
            }
            if ((c1 == '=' && c2 == '=') || (c1 == '!' && c2 == '=') ||
                (c1 == '<' && c2 == '=') || (c1 == '>' && c2 == '=') ||
                (c1 == '=' && c2 == '>')) {
                lex_get(lx);
                char op[3] = {(char)c1, (char)c2, 0};
                Token t = {TK_OP, xstrndup(op, 2), 0};
                lex_push(lx, t);
                continue;
            }
            if (strchr("+-*/=<>", c1)) {
                char op[2] = {(char)c1, 0};
                Token t = {TK_OP, xstrndup(op, 1), 0};
                lex_push(lx, t);
                continue;
            }
            if (strchr("(){};,", c1)) {
                char op[2] = {(char)c1, 0};
                Token t = {TK_SYM, xstrndup(op, 1), 0};
                lex_push(lx, t);
                continue;
            }
            fprintf(stderr, "bad character at %zu: %c\n", start, c1);
            exit(1);
        }
    }
    Token t = {TK_EOF, xstrndup("", 0), 0};
    lex_push(lx, t);
}

