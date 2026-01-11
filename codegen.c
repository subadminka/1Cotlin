#include "common.h"

static void gen_expr(CodeGen *cg, Expr *e);

static void emit8(CodeBuf *c, uint8_t v) {
    if (c->len == c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 1024;
        c->data = (uint8_t *)realloc(c->data, nc);
        if (!c->data) die("out of memory");
        c->cap = nc;
    }
    c->data[c->len++] = v;
}


static void emit32(CodeBuf *c, uint32_t v) {
    emit8(c, (uint8_t)(v & 0xFF));
    emit8(c, (uint8_t)((v >> 8) & 0xFF));
    emit8(c, (uint8_t)((v >> 16) & 0xFF));
    emit8(c, (uint8_t)((v >> 24) & 0xFF));
}


static void emit64(CodeBuf *c, uint64_t v) {
    emit32(c, (uint32_t)(v & 0xFFFFFFFFu));
    emit32(c, (uint32_t)(v >> 32));
}


static void fixups_push(CodeGen *cg, Fixup f) {
    if (cg->fixup_count == cg->fixup_cap) {
        size_t nc = cg->fixup_cap ? cg->fixup_cap * 2 : 64;
        cg->fixups = (Fixup *)realloc(cg->fixups, nc * sizeof(Fixup));
        if (!cg->fixups) die("out of memory");
        cg->fixup_cap = nc;
    }
    cg->fixups[cg->fixup_count++] = f;
}


static int new_label(CodeGen *cg) {
    if (cg->label_count == cg->label_cap) {
        size_t nc = cg->label_cap ? cg->label_cap * 2 : 64;
        cg->labels = (Label *)realloc(cg->labels, nc * sizeof(Label));
        if (!cg->labels) die("out of memory");
        cg->label_cap = nc;
    }
    int id = (int)cg->label_count++;
    cg->labels[id].pos = -1;
    return id;
}


static void place_label(CodeGen *cg, int id) {
    cg->labels[id].pos = (int)cg->code.len;
}


static void emit_rel32_label(CodeGen *cg, int label_id) {
    Fixup f = {FIX_LABEL, cg->code.len, label_id, 0};
    emit32(&cg->code, 0);
    fixups_push(cg, f);
}


static void emit_rel32_rip(CodeGen *cg, uint32_t target_rva) {
    Fixup f = {FIX_RIP, cg->code.len, 0, target_rva};
    emit32(&cg->code, 0);
    fixups_push(cg, f);
}


static void emit_mov_rax_imm64(CodeGen *cg, uint64_t v) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0xB8);
    emit64(&cg->code, v);
}


static void emit_mov_rcx_imm32(CodeGen *cg, uint32_t v) {
    emit8(&cg->code, 0xB9);
    emit32(&cg->code, v);
}


static void emit_mov_r8d_imm32(CodeGen *cg, uint32_t v) {
    emit8(&cg->code, 0x41);
    emit8(&cg->code, 0xB8);
    emit32(&cg->code, v);
}


static void emit_mov_r9_imm32(CodeGen *cg, uint32_t v) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0xC7);
    emit8(&cg->code, 0xC1);
    emit32(&cg->code, v);
}

static void emit_mov_rdx_imm32(CodeGen *cg, uint32_t v) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0xC7);
    emit8(&cg->code, 0xC2);
    emit32(&cg->code, v);
}


static void emit_mov_rcx_from_rbp(CodeGen *cg, int32_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x8D);
    emit32(&cg->code, (uint32_t)disp);
}


static void emit_mov_rax_from_rbp(CodeGen *cg, int32_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x85);
    emit32(&cg->code, (uint32_t)disp);
}

static void emit_mov_rax_from_rcx_disp8(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x41);
    emit8(&cg->code, disp);
}

static void emit_mov_rdx_from_rcx_disp8(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x51);
    emit8(&cg->code, disp);
}

static void emit_mov_rcx_from_rdx_disp8(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x4A);
    emit8(&cg->code, disp);
}

static void emit_mov_membase_rcx_from_rax(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x41);
    emit8(&cg->code, disp);
}

static void emit_mov_rax_from_rcx(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC8);
}

static void emit_mov_rcx_from_rax(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC1);
}

static void emit_mov_r8_from_rax(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC0);
}

static void emit_mov_r8_from_rcx_disp8(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x41);
    emit8(&cg->code, disp);
}

static void emit_mov_rax_from_r8(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC0);
}

static void emit_mov_r9_from_rax(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC1);
}

static void emit_mov_r12_from_rax(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC4);
}

static void emit_mov_rax_from_r12(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xE0);
}

static void emit_mov_rdx_from_r12(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xE2);
}

static void emit_mov_rax_from_r9(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC8);
}

static void emit_cmp_r8_rax(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x39);
    emit8(&cg->code, 0xC0);
}

static void emit_cmp_r9_rax(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x39);
    emit8(&cg->code, 0xC1);
}

static void emit_cmp_rdx_rax(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x39);
    emit8(&cg->code, 0xC2);
}

static void emit_mov_rdx_from_rax(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC2);
}

static void emit_mov_rax_from_rdx(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xD0);
}

static void emit_mov_r8_from_rdx_disp8(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x42);
    emit8(&cg->code, disp);
}

static void emit_mov_rdx_from_rdx_disp8(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x52);
    emit8(&cg->code, disp);
}

static void emit_mov_r9_from_rdx_disp8(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x4A);
    emit8(&cg->code, disp);
}

static void emit_mov_mem_rdx_from_rax(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x42);
    emit8(&cg->code, disp);
}

static void emit_mov_mem_rdx_from_rcx(CodeGen *cg, uint8_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x4A);
    emit8(&cg->code, disp);
}

static void emit_mov_rcx_from_r8(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC1);
}

static void emit_mov_rcx_from_r9(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC9);
}

static void emit_mov_r8_from_rcx(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC8);
}

static void emit_mov_r9_from_rcx(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC9);
}

static void emit_lea_r8_rdx_rax(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8D);
    emit8(&cg->code, 0x04);
    emit8(&cg->code, 0xC2);
}

static void emit_mov_r8_from_rbp(CodeGen *cg, int32_t disp) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x85);
    emit32(&cg->code, (uint32_t)disp);
}

static void emit_mov_r9_from_rbp(CodeGen *cg, int32_t disp) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x8D);
    emit32(&cg->code, (uint32_t)disp);
}

static void emit_mov_mem_rax_from_rcx(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x08);
}

static void emit_mov_mem_rax_from_r8(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x00);
}

static void emit_mov_rax_from_mem_rax(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x00);
}

static void emit_add_rax_rdx(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x01);
    emit8(&cg->code, 0xD0);
}

static void emit_add_rax_r9(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x01);
    emit8(&cg->code, 0xC8);
}

static void emit_dec_rax(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0xFF);
    emit8(&cg->code, 0xC8);
}

static void emit_jbe_label(CodeGen *cg, int label_id) {
    emit8(&cg->code, 0x0F);
    emit8(&cg->code, 0x86);
    emit_rel32_label(cg, label_id);
}

static void emit_jge_label(CodeGen *cg, int label_id) {
    emit8(&cg->code, 0x0F);
    emit8(&cg->code, 0x8D);
    emit_rel32_label(cg, label_id);
}

static void emit_jle_label(CodeGen *cg, int label_id) {
    emit8(&cg->code, 0x0F);
    emit8(&cg->code, 0x8E);
    emit_rel32_label(cg, label_id);
}

static void emit_lea_r8_rcx_rax(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8D);
    emit8(&cg->code, 0x44);
    emit8(&cg->code, 0xC1);
    emit8(&cg->code, 0x10);
}

static void emit_lea_r8_rcx_rdx(CodeGen *cg) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8D);
    emit8(&cg->code, 0x44);
    emit8(&cg->code, 0xD1);
    emit8(&cg->code, 0x10);
}

static void emit_mov_mem_r8_from_rax(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x00);
}

static void emit_mov_mem_r8_from_r9(CodeGen *cg) {
    emit8(&cg->code, 0x4D);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x08);
}

static void emit_mov_rax_from_mem_r8(CodeGen *cg) {
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x00);
}

static void emit_shl_rax_3(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0xC1);
    emit8(&cg->code, 0xE0);
    emit8(&cg->code, 0x03);
}

static void emit_add_rax_imm8(CodeGen *cg, uint8_t v) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xC0);
    emit8(&cg->code, v);
}


static void emit_mov_rbp_from_rax(CodeGen *cg, int32_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x85);
    emit32(&cg->code, (uint32_t)disp);
}


static void emit_lea_rbx_rbp(CodeGen *cg, int32_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8D);
    emit8(&cg->code, 0x9D);
    emit32(&cg->code, (uint32_t)disp);
}


static void emit_lea_rdx_rbp(CodeGen *cg, int32_t disp) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8D);
    emit8(&cg->code, 0x95);
    emit32(&cg->code, (uint32_t)disp);
}


static void emit_lea_r9_rbp(CodeGen *cg, int32_t disp) {
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8D);
    emit8(&cg->code, 0x8D);
    emit32(&cg->code, (uint32_t)disp);
}


static void emit_lea_rdx_rip(CodeGen *cg, uint32_t target_rva) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8D);
    emit8(&cg->code, 0x15);
    emit_rel32_rip(cg, target_rva);
}


static void emit_call_iat(CodeGen *cg, uint32_t iat_rva) {
    emit8(&cg->code, 0xFF);
    emit8(&cg->code, 0x15);
    emit_rel32_rip(cg, iat_rva);
}


static void emit_push_rax(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0x03);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xC3);
    emit8(&cg->code, 0x08);
}


static void emit_pop_rcx(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xEB);
    emit8(&cg->code, 0x08);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x0B);
}

static void emit_pop_rdx(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xEB);
    emit8(&cg->code, 0x08);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x13);
}

static void emit_pop_r8(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xEB);
    emit8(&cg->code, 0x08);
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x03);
}

static void emit_pop_r9(CodeGen *cg) {
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xEB);
    emit8(&cg->code, 0x08);
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0x0B);
}

static void emit_heap_alloc(CodeGen *cg, uint32_t flags) {
    emit_mov_rcx_from_rbp(cg, (int32_t)cg->heap_offset);
    emit_mov_rdx_imm32(cg, flags);
    emit_mov_r8_from_rax(cg);
    emit_call_iat(cg, cg->iat_heapalloc_rva);
}

void gen_prolog(CodeGen *cg) {
    emit8(&cg->code, 0x55);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xE5);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x81);
    emit8(&cg->code, 0xEC);
    emit32(&cg->code, (uint32_t)cg->frame_size);
    emit_lea_rbx_rbp(cg, (int32_t)cg->vstack_base_offset);

    emit_mov_rcx_imm32(cg, 65001);
    emit_call_iat(cg, cg->iat_setconcp_rva);

    emit_call_iat(cg, cg->iat_getprocheap_rva);
    emit_mov_rbp_from_rax(cg, (int32_t)cg->heap_offset);

    emit_mov_rcx_imm32(cg, 0xFFFFFFF5);
    emit_call_iat(cg, cg->iat_getstd_rva);
    emit_mov_rbp_from_rax(cg, (int32_t)cg->stdout_offset);
}

void gen_epilog(CodeGen *cg) {
    emit_mov_rcx_imm32(cg, 0);
    emit_call_iat(cg, cg->iat_exit_rva);
}

static void gen_expr(CodeGen *cg, Expr *e);


static void gen_expr(CodeGen *cg, Expr *e) {
    switch (e->kind) {
        case EX_NUM:
            emit_mov_rax_imm64(cg, (uint64_t)e->v.num);
            return;
        case EX_BOOL:
            emit_mov_rax_imm64(cg, (uint64_t)(e->v.boolv ? 1 : 0));
            return;
        case EX_VAR: {
            if (cg->lambda_param_name && strcmp(e->v.var, cg->lambda_param_name) == 0) {
                emit_mov_rax_from_rbp(cg, (int32_t)cg->lambda_param_offset);
                return;
            }
            int idx = sym_find(&cg->sym, e->v.var);
            if (idx < 0) die("unknown variable");
            int32_t disp = (int32_t)(-16 - idx * 8);
            emit_mov_rax_from_rbp(cg, disp);
            return;
        }
        case EX_STR:
            die("string in expression");
            return;
        case EX_UNARY:
            gen_expr(cg, e->v.un.expr);
            if (e->v.un.op == OP_NEG) {
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0xF7);
                emit8(&cg->code, 0xD8);
            } else if (e->v.un.op == OP_NOT) {
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x85);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0x94);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0xB6);
                emit8(&cg->code, 0xC0);
            }
            return;
        case EX_LAMBDA:
            die("lambda in expression");
            return;
        case EX_CALL: {
            const char *name = e->v.call.name;
            size_t argc = e->v.call.argc;
            Expr **args = e->v.call.args;
            if (strcmp(name, "создать.лист.цифр") == 0) {
                int32_t cap_disp = (int32_t)cg->temp_offset;
                int l_zero = new_label(cg);
                int l_done = new_label(cg);
                // ok list header is [len, cap, data], dont ask
                if (argc == 0) {
                    emit_mov_rax_imm64(cg, 8);
                } else {
                    gen_expr(cg, args[0]);
                }
                emit_mov_rbp_from_rax(cg, cap_disp);
                emit_mov_rax_imm64(cg, 24);
                emit_heap_alloc(cg, 8);
                emit_mov_rdx_from_rax(cg);
                emit_mov_r12_from_rax(cg);
                emit_mov_rax_imm64(cg, 0);
                emit_mov_mem_rdx_from_rax(cg, 0x00);
                emit_mov_rax_from_rbp(cg, cap_disp);
                emit_mov_mem_rdx_from_rax(cg, 0x08);
                emit_mov_rax_from_rbp(cg, cap_disp);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x85);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0x84);
                emit_rel32_label(cg, l_zero);
                emit_mov_rax_from_rbp(cg, cap_disp);
                emit_shl_rax_3(cg);
                emit_heap_alloc(cg, 8);
                emit_mov_r8_from_rax(cg);
                emit_mov_rdx_from_r12(cg);
                emit_mov_rax_from_r8(cg);
                emit_mov_mem_rdx_from_rax(cg, 0x10);
                emit8(&cg->code, 0xE9);
                emit_rel32_label(cg, l_done);
                place_label(cg, l_zero);
                emit_mov_rax_imm64(cg, 0);
                emit_mov_mem_rdx_from_rax(cg, 0x10);
                place_label(cg, l_done);
                emit_mov_rax_from_r12(cg);
                return;
            }
            if (strcmp(name, "создать.массив.цифр") == 0) {
                int32_t len_disp = (int32_t)cg->temp_offset;
                int l_zero = new_label(cg);
                int l_done = new_label(cg);
                gen_expr(cg, args[0]);
                emit_mov_rbp_from_rax(cg, len_disp);
                emit_mov_rax_imm64(cg, 24);
                emit_heap_alloc(cg, 8);
                emit_mov_rdx_from_rax(cg);
                emit_mov_r12_from_rax(cg);
                emit_mov_rax_from_rbp(cg, len_disp);
                emit_mov_mem_rdx_from_rax(cg, 0x00);
                emit_mov_rax_from_rbp(cg, len_disp);
                emit_mov_mem_rdx_from_rax(cg, 0x08);
                emit_mov_rax_from_rbp(cg, len_disp);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x85);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0x84);
                emit_rel32_label(cg, l_zero);
                emit_mov_rax_from_rbp(cg, len_disp);
                emit_shl_rax_3(cg);
                emit_heap_alloc(cg, 8);
                emit_mov_r8_from_rax(cg);
                emit_mov_rdx_from_r12(cg);
                emit_mov_rax_from_r8(cg);
                emit_mov_mem_rdx_from_rax(cg, 0x10);
                emit8(&cg->code, 0xE9);
                emit_rel32_label(cg, l_done);
                place_label(cg, l_zero);
                emit_mov_rax_imm64(cg, 0);
                emit_mov_mem_rdx_from_rax(cg, 0x10);
                place_label(cg, l_done);
                emit_mov_rax_from_r12(cg);
                return;
            }
            if (strcmp(name, "сколько.внутри") == 0) {
                gen_expr(cg, args[0]);
                emit_mov_rcx_from_rax(cg);
                emit_mov_rax_from_rcx_disp8(cg, 0x00);
                return;
            }
            if (strcmp(name, "дай.по.индексу") == 0) {
                gen_expr(cg, args[0]);
                emit_mov_rcx_from_rax(cg);
                gen_expr(cg, args[1]);
                emit_mov_rdx_from_rcx_disp8(cg, 0x10);
                emit_shl_rax_3(cg);
                emit_add_rax_rdx(cg);
                emit_mov_rax_from_mem_rax(cg);
                return;
            }
            if (strcmp(name, "сунь.по.индексу") == 0) {
                int32_t list_disp = (int32_t)cg->temp_offset;
                int32_t idx_disp = (int32_t)cg->temp2_offset;
                gen_expr(cg, args[0]);
                emit_mov_rbp_from_rax(cg, list_disp);
                gen_expr(cg, args[1]);
                emit_mov_rbp_from_rax(cg, idx_disp);
                gen_expr(cg, args[2]);
                emit_mov_r8_from_rax(cg);
                emit_mov_rax_from_rbp(cg, list_disp);
                emit_mov_rdx_from_rax(cg);
                emit_mov_rax_from_rbp(cg, idx_disp);
                emit_mov_rcx_from_rax(cg);
                emit_mov_rax_from_rcx(cg);
                emit_shl_rax_3(cg);
                emit_mov_r9_from_rdx_disp8(cg, 0x10);
                emit_add_rax_r9(cg);
                emit_mov_mem_rax_from_r8(cg);
                emit_mov_rax_from_r8(cg);
                return;
            }
            if (strcmp(name, "впихни.в.лист") == 0) {
                int32_t list_disp = (int32_t)cg->temp_offset;
                int32_t val_disp = (int32_t)cg->temp2_offset;
                int l_done = new_label(cg);
                gen_expr(cg, args[0]);
                emit_mov_rbp_from_rax(cg, list_disp);
                gen_expr(cg, args[1]);
                emit_mov_rbp_from_rax(cg, val_disp);
                emit_mov_rax_from_rbp(cg, list_disp);
                emit_mov_rdx_from_rax(cg);
                emit_mov_rcx_from_rdx_disp8(cg, 0x00);
                emit_mov_r8_from_rdx_disp8(cg, 0x08);
                emit_mov_rax_from_rcx(cg);
                emit_cmp_r8_rax(cg);
                emit_jbe_label(cg, l_done);
                emit_mov_r9_from_rdx_disp8(cg, 0x10);
                emit_mov_rax_from_rbp(cg, val_disp);
                emit_mov_r8_from_rax(cg);
                emit_mov_rax_from_rcx(cg);
                emit_shl_rax_3(cg);
                emit_add_rax_r9(cg);
                emit_mov_mem_rax_from_r8(cg);
                emit_mov_rax_from_rcx(cg);
                emit_add_rax_imm8(cg, 1);
                emit_mov_mem_rdx_from_rax(cg, 0x00);
                place_label(cg, l_done);
                emit_mov_rax_from_rdx(cg);
                return;
            }
            if (strcmp(name, "достань.последний") == 0) {
                int l_empty = new_label(cg);
                int l_done = new_label(cg);
                gen_expr(cg, args[0]);
                emit_mov_rcx_from_rax(cg);
                emit_mov_rdx_from_rax(cg);
                emit_mov_rax_from_rcx_disp8(cg, 0x00);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x85);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0x84);
                emit_rel32_label(cg, l_empty);
                emit_dec_rax(cg);
                emit_mov_mem_rdx_from_rax(cg, 0x00);
                emit_mov_r9_from_rdx_disp8(cg, 0x10);
                emit_shl_rax_3(cg);
                emit_add_rax_r9(cg);
                emit_mov_rax_from_mem_rax(cg);
                emit8(&cg->code, 0xE9);
                emit_rel32_label(cg, l_done);
                place_label(cg, l_empty);
                emit_mov_rax_imm64(cg, 0);
                place_label(cg, l_done);
                return;
            }
            if (strcmp(name, "диапазон.от.0.до") == 0) {
                int32_t len_disp = (int32_t)cg->temp_offset;
                int32_t list_disp = (int32_t)cg->temp2_offset;
                int l_zero = new_label(cg);
                int l_loop = new_label(cg);
                int l_loop_done = new_label(cg);
                int l_done = new_label(cg);
                // boring loop, just fills 0..n-1
                gen_expr(cg, args[0]);
                emit_mov_rbp_from_rax(cg, len_disp);
                emit_mov_rax_imm64(cg, 24);
                emit_heap_alloc(cg, 8);
                emit_mov_rbp_from_rax(cg, list_disp);
                emit_mov_rdx_from_rax(cg);
                emit_mov_r12_from_rax(cg);
                emit_mov_rax_from_rbp(cg, len_disp);
                emit_mov_mem_rdx_from_rax(cg, 0x00);
                emit_mov_rax_from_rbp(cg, len_disp);
                emit_mov_mem_rdx_from_rax(cg, 0x08);
                emit_mov_rax_from_rbp(cg, len_disp);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x85);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0x84);
                emit_rel32_label(cg, l_zero);
                emit_mov_rax_from_rbp(cg, len_disp);
                emit_shl_rax_3(cg);
                emit_heap_alloc(cg, 8);
                emit_mov_r8_from_rax(cg);
                emit_mov_rdx_from_r12(cg);
                emit_mov_rax_from_r8(cg);
                emit_mov_mem_rdx_from_rax(cg, 0x10);
                emit_mov_rdx_from_rax(cg);
                emit_mov_rax_imm64(cg, 0);
                place_label(cg, l_loop);
                emit_mov_r9_from_rbp(cg, len_disp);
                emit_cmp_r9_rax(cg);
                emit_jbe_label(cg, l_loop_done);
                emit_lea_r8_rdx_rax(cg);
                emit_mov_mem_r8_from_rax(cg);
                emit_add_rax_imm8(cg, 1);
                emit8(&cg->code, 0xE9);
                emit_rel32_label(cg, l_loop);
                place_label(cg, l_loop_done);
                emit8(&cg->code, 0xE9);
                emit_rel32_label(cg, l_done);
                place_label(cg, l_zero);
                emit_mov_rax_imm64(cg, 0);
                emit_mov_mem_rdx_from_rax(cg, 0x10);
                place_label(cg, l_done);
                emit_mov_rax_from_rbp(cg, list_disp);
                return;
            }
            die("unknown call");
            return;
        }
        case EX_BIN:
            if (e->v.bin.op == OP_AND || e->v.bin.op == OP_OR) {
                int l_end = new_label(cg);
                int l_done = new_label(cg);
                gen_expr(cg, e->v.bin.left);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x85);
                emit8(&cg->code, 0xC0);
                if (e->v.bin.op == OP_AND) {
                    emit8(&cg->code, 0x0F);
                    emit8(&cg->code, 0x84);
                    emit_rel32_label(cg, l_end);
                } else {
                    emit8(&cg->code, 0x0F);
                    emit8(&cg->code, 0x85);
                    emit_rel32_label(cg, l_end);
                }
                gen_expr(cg, e->v.bin.right);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x85);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0x95);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0xB6);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0xE9);
                emit_rel32_label(cg, l_done);
                place_label(cg, l_end);
                if (e->v.bin.op == OP_AND) {
                    emit_mov_rax_imm64(cg, 0);
                } else {
                    emit_mov_rax_imm64(cg, 1);
                }
                place_label(cg, l_done);
                return;
            }
            gen_expr(cg, e->v.bin.left);
            emit_push_rax(cg);
            gen_expr(cg, e->v.bin.right);
            emit_pop_rcx(cg);
            if (e->v.bin.op == OP_ADD) {
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x01);
                emit8(&cg->code, 0xC8);
                return;
            }
            if (e->v.bin.op == OP_SUB) {
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x29);
                emit8(&cg->code, 0xC1);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x89);
                emit8(&cg->code, 0xC8);
                return;
            }
            if (e->v.bin.op == OP_MUL) {
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0xAF);
                emit8(&cg->code, 0xC1);
                return;
            }
            if (e->v.bin.op == OP_DIV) {
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x91);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x99);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0xF7);
                emit8(&cg->code, 0xF9);
                return;
            }
            if (e->v.bin.op == OP_EQ || e->v.bin.op == OP_NE || e->v.bin.op == OP_LT ||
                e->v.bin.op == OP_GT || e->v.bin.op == OP_LE || e->v.bin.op == OP_GE) {
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x39);
                emit8(&cg->code, 0xC1);
                emit8(&cg->code, 0x0F);
                if (e->v.bin.op == OP_EQ) emit8(&cg->code, 0x94);
                else if (e->v.bin.op == OP_NE) emit8(&cg->code, 0x95);
                else if (e->v.bin.op == OP_LT) emit8(&cg->code, 0x9C);
                else if (e->v.bin.op == OP_LE) emit8(&cg->code, 0x9E);
                else if (e->v.bin.op == OP_GT) emit8(&cg->code, 0x9F);
                else emit8(&cg->code, 0x9D);
                emit8(&cg->code, 0xC0);
                emit8(&cg->code, 0x48);
                emit8(&cg->code, 0x0F);
                emit8(&cg->code, 0xB6);
                emit8(&cg->code, 0xC0);
                return;
            }
    }
}


static void emit_print_newline(CodeGen *cg) {
    emit8(&cg->code, 0xC6);
    emit8(&cg->code, 0x85);
    emit32(&cg->code, (uint32_t)cg->intbuf_offset);
    emit8(&cg->code, 0x0A);
    emit_mov_rcx_from_rbp(cg, (int32_t)cg->stdout_offset);
    emit_lea_rdx_rbp(cg, (int32_t)cg->intbuf_offset);
    emit_mov_r8d_imm32(cg, 1);
    emit_lea_r9_rbp(cg, (int32_t)cg->bytes_written_offset);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0xC7);
    emit8(&cg->code, 0x44);
    emit8(&cg->code, 0x24);
    emit8(&cg->code, 0x20);
    emit32(&cg->code, 0);
    emit_call_iat(cg, cg->iat_write_rva);
}

static void emit_print_str(CodeGen *cg, StringLit *s) {
    emit_mov_rcx_from_rbp(cg, (int32_t)cg->stdout_offset);
    emit_lea_rdx_rip(cg, s->rva);
    emit_mov_r8d_imm32(cg, (uint32_t)s->len);
    emit_lea_r9_rbp(cg, (int32_t)cg->bytes_written_offset);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0xC7);
    emit8(&cg->code, 0x44);
    emit8(&cg->code, 0x24);
    emit8(&cg->code, 0x20);
    emit32(&cg->code, 0);
    emit_call_iat(cg, cg->iat_write_rva);
}


static void emit_print_int(CodeGen *cg) {
    int l_nonzero = new_label(cg);
    int l_pos = new_label(cg);
    int l_loop = new_label(cg);
    int l_after = new_label(cg);
    int l_done = new_label(cg);

    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC1);
    emit_lea_rdx_rbp(cg, (int32_t)(cg->intbuf_offset + 32));
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0xD2);
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0xDA);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x85);
    emit8(&cg->code, 0xC9);
    emit8(&cg->code, 0x0F);
    emit8(&cg->code, 0x85);
    emit_rel32_label(cg, l_nonzero);
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xEB);
    emit8(&cg->code, 0x01);
    emit8(&cg->code, 0x41);
    emit8(&cg->code, 0xC6);
    emit8(&cg->code, 0x03);
    emit8(&cg->code, '0');
    emit8(&cg->code, 0xE9);
    emit_rel32_label(cg, l_done);

    place_label(cg, l_nonzero);
    emit8(&cg->code, 0x45);
    emit8(&cg->code, 0x31);
    emit8(&cg->code, 0xC0);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x85);
    emit8(&cg->code, 0xC9);
    emit8(&cg->code, 0x0F);
    emit8(&cg->code, 0x8D);
    emit_rel32_label(cg, l_pos);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0xF7);
    emit8(&cg->code, 0xD9);
    emit8(&cg->code, 0x41);
    emit8(&cg->code, 0xB8);
    emit32(&cg->code, 1);

    place_label(cg, l_pos);
    place_label(cg, l_loop);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xF9);
    emit8(&cg->code, 0x00);
    emit8(&cg->code, 0x0F);
    emit8(&cg->code, 0x84);
    emit_rel32_label(cg, l_after);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC8);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x31);
    emit8(&cg->code, 0xD2);
    emit_mov_r9_imm32(cg, 10);
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0xF7);
    emit8(&cg->code, 0xF1);
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xEB);
    emit8(&cg->code, 0x01);
    emit8(&cg->code, 0x80);
    emit8(&cg->code, 0xC2);
    emit8(&cg->code, '0');
    emit8(&cg->code, 0x41);
    emit8(&cg->code, 0x88);
    emit8(&cg->code, 0x13);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC1);
    emit8(&cg->code, 0xE9);
    emit_rel32_label(cg, l_loop);

    place_label(cg, l_after);
    emit8(&cg->code, 0x45);
    emit8(&cg->code, 0x85);
    emit8(&cg->code, 0xC0);
    emit8(&cg->code, 0x0F);
    emit8(&cg->code, 0x84);
    emit_rel32_label(cg, l_done);
    emit8(&cg->code, 0x49);
    emit8(&cg->code, 0x83);
    emit8(&cg->code, 0xEB);
    emit8(&cg->code, 0x01);
    emit8(&cg->code, 0x41);
    emit8(&cg->code, 0xC6);
    emit8(&cg->code, 0x03);
    emit8(&cg->code, '-');

    place_label(cg, l_done);
    emit8(&cg->code, 0x4D);
    emit8(&cg->code, 0x8B);
    emit8(&cg->code, 0xC2);
    emit8(&cg->code, 0x4D);
    emit8(&cg->code, 0x29);
    emit8(&cg->code, 0xD8);
    emit_mov_rcx_from_rbp(cg, (int32_t)cg->stdout_offset);
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xDA);
    emit8(&cg->code, 0x4C);
    emit8(&cg->code, 0x89);
    emit8(&cg->code, 0xC0);
    emit_lea_r9_rbp(cg, (int32_t)cg->bytes_written_offset);
    emit8(&cg->code, 0x48);
    emit8(&cg->code, 0xC7);
    emit8(&cg->code, 0x44);
    emit8(&cg->code, 0x24);
    emit8(&cg->code, 0x20);
    emit32(&cg->code, 0);
    emit_call_iat(cg, cg->iat_write_rva);
}


void gen_stmt(CodeGen *cg, Stmt *s, int *loop_depth) {
    if (!s) return;
    if (s->kind == ST_BLOCK) {
        for (size_t i = 0; i < s->v.block.count; i++) {
            gen_stmt(cg, s->v.block.items[i], loop_depth);
        }
        return;
    }
    if (s->kind == ST_PRINT) {
        if (s->v.print.expr->kind == EX_STR) {
            emit_print_str(cg, s->v.print.expr->v.str);
        } else {
            gen_expr(cg, s->v.print.expr);
            emit_print_int(cg);
        }
        emit_print_newline(cg);
        return;
    }
    if (s->kind == ST_LET) {
        int idx = sym_find(&cg->sym, s->v.let.name);
        gen_expr(cg, s->v.let.expr);
        emit_mov_rbp_from_rax(cg, (int32_t)(-16 - idx * 8));
        return;
    }
    if (s->kind == ST_SET) {
        int idx = sym_find(&cg->sym, s->v.set.name);
        gen_expr(cg, s->v.set.expr);
        emit_mov_rbp_from_rax(cg, (int32_t)(-16 - idx * 8));
        return;
    }
    if (s->kind == ST_IF) {
        int l_else = new_label(cg);
        int l_end = new_label(cg);
        gen_expr(cg, s->v.ifs.cond);
        emit8(&cg->code, 0x48);
        emit8(&cg->code, 0x85);
        emit8(&cg->code, 0xC0);
        emit8(&cg->code, 0x0F);
        emit8(&cg->code, 0x84);
        emit_rel32_label(cg, l_else);
        gen_stmt(cg, s->v.ifs.thenb, loop_depth);
        emit8(&cg->code, 0xE9);
        emit_rel32_label(cg, l_end);
        place_label(cg, l_else);
        gen_stmt(cg, s->v.ifs.elseb, loop_depth);
        place_label(cg, l_end);
        return;
    }
    if (s->kind == ST_REPEAT) {
        int slot = (*loop_depth)++;
        if (slot >= cg->loop_slots) die("repeat depth");
        int32_t disp = (int32_t)(cg->loop_slots_offset - slot * 8);
        int l_start = new_label(cg);
        int l_end = new_label(cg);
        gen_expr(cg, s->v.repeat.count);
        emit_mov_rbp_from_rax(cg, disp);
        place_label(cg, l_start);
        emit_mov_rax_from_rbp(cg, disp);
        emit8(&cg->code, 0x48);
        emit8(&cg->code, 0x83);
        emit8(&cg->code, 0xF8);
        emit8(&cg->code, 0x00);
        emit8(&cg->code, 0x0F);
        emit8(&cg->code, 0x8E);
        emit_rel32_label(cg, l_end);
        gen_stmt(cg, s->v.repeat.body, loop_depth);
        emit_mov_rax_from_rbp(cg, disp);
        emit8(&cg->code, 0x48);
        emit8(&cg->code, 0xFF);
        emit8(&cg->code, 0xC8);
        emit_mov_rbp_from_rax(cg, disp);
        emit8(&cg->code, 0xE9);
        emit_rel32_label(cg, l_start);
        place_label(cg, l_end);
        (*loop_depth)--;
        return;
    }
    if (s->kind == ST_EXPR) {
        gen_expr(cg, s->v.expr.expr);
        return;
    }
}


void patch_fixups(CodeGen *cg) {
    for (size_t i = 0; i < cg->fixup_count; i++) {
        Fixup *f = &cg->fixups[i];
        if (f->kind == FIX_LABEL) {
            int pos = cg->labels[f->label_id].pos;
            if (pos < 0) die("label not placed");
            uint32_t target = cg->text_rva + (uint32_t)pos;
            uint32_t next = cg->text_rva + (uint32_t)(f->offset + 4);
            int32_t rel = (int32_t)(target - next);
            memcpy(cg->code.data + f->offset, &rel, 4);
        } else {
            uint32_t next = cg->text_rva + (uint32_t)(f->offset + 4);
            int32_t rel = (int32_t)(f->target_rva - next);
            memcpy(cg->code.data + f->offset, &rel, 4);
        }
    }
}

