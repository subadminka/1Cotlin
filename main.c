#include "common.h"

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: 1cotlinc <file> [out.exe]\n");
        return 1;
    }

    size_t len = 0;
    char *src = read_file(argv[1], &len);
    if (!src) die("failed to read input");

    Lexer lx = {0};
    lx.src = src;
    lx.len = len;
    lex_all(&lx);

    Parser p = {0};
    p.items = lx.items;
    p.count = lx.count;
    p.pos = 0;

    Stmt *prog = parse_program(&p);

    SymTab st = {0};
    int max_stack = 0;
    int max_repeat = 0;
    sem_stmt(prog, &st, &max_stack, &max_repeat, 0);

    CodeGen cg = {0};
    cg.sym = st;
    cg.loop_slots = max_repeat;
    cg.rdata_rva = 0x2000;
    layout_rdata(&cg, p.strings, p.strings_count);

    size_t locals_size = st.count * 8;
    size_t temps_size = 8 + 8 + 32;
    size_t loops_size = cg.loop_slots * 8;
    size_t vstack_size = max_stack * 8;
    size_t locals_total = locals_size + temps_size + loops_size + vstack_size;

    cg.stdout_offset = -16 - (int64_t)locals_size - 8;
    cg.bytes_written_offset = cg.stdout_offset - 8;
    cg.intbuf_offset = cg.bytes_written_offset - 32;
    cg.loop_slots_offset = cg.intbuf_offset - 8;
    cg.vstack_base_offset = -16 - (int64_t)locals_total;
    cg.frame_size = align_up(32 + locals_total, 16);

    gen_prolog(&cg);

    int loop_depth = 0;
    gen_stmt(&cg, prog, &loop_depth);

    gen_epilog(&cg);

    const char *out = (argc == 3) ? argv[2] : default_output(argv[1]);
    write_pe(out, &cg, p.strings, p.strings_count);

    return 0;
}

