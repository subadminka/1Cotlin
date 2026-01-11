#include "common.h"

void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}


void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) die("out of memory");
    return p;
}


char *xstrndup(const char *s, size_t n) {
    char *p = (char *)xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}


size_t align_up(size_t v, size_t a) {
    return (v + a - 1) & ~(a - 1);
}

static char *utf16le_to_utf8(const uint8_t *raw, size_t n, size_t *out_len) {
    size_t cap = n * 2 + 1;
    char *out = (char *)xmalloc(cap);
    size_t o = 0;
    size_t i = 0;
    if (n >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
        i = 2;
    }
    while (i + 1 < n) {
        uint16_t w1 = (uint16_t)(raw[i] | (raw[i + 1] << 8));
        i += 2;
        uint32_t cp = w1;
        if (w1 >= 0xD800 && w1 <= 0xDBFF) {
            if (i + 1 < n) {
                uint16_t w2 = (uint16_t)(raw[i] | (raw[i + 1] << 8));
                if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
                    cp = 0x10000 + (((uint32_t)w1 - 0xD800) << 10) + ((uint32_t)w2 - 0xDC00);
                    i += 2;
                }
            }
        }
        if (cp <= 0x7F) {
            out[o++] = (char)cp;
        } else if (cp <= 0x7FF) {
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else {
            out[o++] = (char)(0xF0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    out[o] = 0;
    if (out_len) *out_len = o;
    return out;
}

char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *raw = (uint8_t *)xmalloc((size_t)n);
    if (fread(raw, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(raw);
        return 0;
    }
    fclose(f);

    if ((size_t)n >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
        char *out = utf16le_to_utf8(raw, (size_t)n, out_len);
        free(raw);
        return out;
    }
    if ((size_t)n >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
        size_t len = (size_t)n - 3;
        char *out = (char *)xmalloc(len + 1);
        memcpy(out, raw + 3, len);
        out[len] = 0;
        if (out_len) *out_len = len;
        free(raw);
        return out;
    }

    char *buf = (char *)xmalloc((size_t)n + 1);
    memcpy(buf, raw, (size_t)n);
    buf[n] = 0;
    if (out_len) *out_len = (size_t)n;
    free(raw);
    return buf;
}


char *default_output(const char *in) {
    size_t n = strlen(in);
    char *out = (char *)xmalloc(n + 5);
    memcpy(out, in, n);
    char *dot = strrchr(out, '.');
    if (dot) {
        strcpy(dot, ".exe");
    } else {
        strcat(out, ".exe");
    }
    return out;
}

