#include "common.h"

static void write_u8(FILE *f, uint8_t v) {
    fwrite(&v, 1, 1, f);
}


static void write_u16(FILE *f, uint16_t v) {
    uint8_t b[2] = {v & 0xFF, (v >> 8) & 0xFF};
    fwrite(b, 1, 2, f);
}


static void write_u32(FILE *f, uint32_t v) {
    uint8_t b[4] = {v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF};
    fwrite(b, 1, 4, f);
}


static void write_u64(FILE *f, uint64_t v) {
    write_u32(f, (uint32_t)(v & 0xFFFFFFFFu));
    write_u32(f, (uint32_t)(v >> 32));
}


static void buf_u16(uint8_t *b, size_t off, uint16_t v) {
    b[off + 0] = (uint8_t)(v & 0xFF);
    b[off + 1] = (uint8_t)((v >> 8) & 0xFF);
}


static void buf_u32(uint8_t *b, size_t off, uint32_t v) {
    b[off + 0] = (uint8_t)(v & 0xFF);
    b[off + 1] = (uint8_t)((v >> 8) & 0xFF);
    b[off + 2] = (uint8_t)((v >> 16) & 0xFF);
    b[off + 3] = (uint8_t)((v >> 24) & 0xFF);
}


static void buf_u64(uint8_t *b, size_t off, uint64_t v) {
    buf_u32(b, off, (uint32_t)(v & 0xFFFFFFFFu));
    buf_u32(b, off + 4, (uint32_t)(v >> 32));
}

RDataLayout layout_rdata(CodeGen *cg, StringLit **strings, size_t strings_count) {
    size_t rdata_offset = 0;
    for (size_t i = 0; i < strings_count; i++) {
        rdata_offset = align_up(rdata_offset, 8);
        strings[i]->rva = cg->rdata_rva + (uint32_t)rdata_offset;
        rdata_offset += strings[i]->len + 1;
    }
    rdata_offset = align_up(rdata_offset, 8);
    size_t import_desc_off = rdata_offset;
    rdata_offset += 40;
    size_t ilt_off = rdata_offset;
    rdata_offset += 56;
    size_t iat_off = rdata_offset;
    rdata_offset += 56;
    size_t hn_getstd = rdata_offset;
    rdata_offset += 2 + strlen("GetStdHandle") + 1;
    rdata_offset = align_up(rdata_offset, 2);
    size_t hn_write = rdata_offset;
    rdata_offset += 2 + strlen("WriteFile") + 1;
    rdata_offset = align_up(rdata_offset, 2);
    size_t hn_exit = rdata_offset;
    rdata_offset += 2 + strlen("ExitProcess") + 1;
    rdata_offset = align_up(rdata_offset, 2);
    size_t hn_setconcp = rdata_offset;
    rdata_offset += 2 + strlen("SetConsoleOutputCP") + 1;
    rdata_offset = align_up(rdata_offset, 2);
    size_t hn_getheap = rdata_offset;
    rdata_offset += 2 + strlen("GetProcessHeap") + 1;
    rdata_offset = align_up(rdata_offset, 2);
    size_t hn_heapalloc = rdata_offset;
    rdata_offset += 2 + strlen("HeapAlloc") + 1;
    rdata_offset = align_up(rdata_offset, 2);
    size_t dll_name = rdata_offset;
    rdata_offset += strlen("kernel32.dll") + 1;
    size_t rdata_size = rdata_offset;

    cg->iat_getstd_rva = cg->rdata_rva + (uint32_t)iat_off + 0 * 8;
    cg->iat_write_rva = cg->rdata_rva + (uint32_t)iat_off + 1 * 8;
    cg->iat_exit_rva = cg->rdata_rva + (uint32_t)iat_off + 2 * 8;
    cg->iat_setconcp_rva = cg->rdata_rva + (uint32_t)iat_off + 3 * 8;
    cg->iat_getprocheap_rva = cg->rdata_rva + (uint32_t)iat_off + 4 * 8;
    cg->iat_heapalloc_rva = cg->rdata_rva + (uint32_t)iat_off + 5 * 8;

    RDataLayout l = {0};
    l.rdata_size = rdata_size;
    l.import_desc_off = import_desc_off;
    l.ilt_off = ilt_off;
    l.iat_off = iat_off;
    l.hn_getstd = hn_getstd;
    l.hn_write = hn_write;
    l.hn_exit = hn_exit;
    l.hn_setconcp = hn_setconcp;
    l.hn_getheap = hn_getheap;
    l.hn_heapalloc = hn_heapalloc;
    l.dll_name = dll_name;
    return l;
}


void write_pe(const char *out, CodeGen *cg, StringLit **strings, size_t strings_count) {
    cg->text_rva = 0x1000;
    cg->rdata_rva = 0x2000;

    RDataLayout l = layout_rdata(cg, strings, strings_count);
    patch_fixups(cg);

    size_t headers_size = 0x200;
    size_t text_raw_size = align_up(cg->code.len, 0x200);
    size_t rdata_raw_size = align_up(l.rdata_size, 0x200);
    uint32_t size_of_image = (uint32_t)align_up(cg->rdata_rva + l.rdata_size, 0x1000);

    uint8_t *rdata = (uint8_t *)calloc(1, rdata_raw_size);
    if (!rdata) die("out of memory");

    size_t rdata_offset = 0;
    for (size_t i = 0; i < strings_count; i++) {
        rdata_offset = align_up(rdata_offset, 8);
        memcpy(rdata + rdata_offset, strings[i]->data, strings[i]->len);
        rdata_offset += strings[i]->len + 1;
    }

    buf_u32(rdata, l.import_desc_off + 0, cg->rdata_rva + (uint32_t)l.ilt_off);
    buf_u32(rdata, l.import_desc_off + 12, cg->rdata_rva + (uint32_t)l.dll_name);
    buf_u32(rdata, l.import_desc_off + 16, cg->rdata_rva + (uint32_t)l.iat_off);

    buf_u64(rdata, l.ilt_off + 0 * 8, cg->rdata_rva + (uint32_t)l.hn_getstd);
    buf_u64(rdata, l.ilt_off + 1 * 8, cg->rdata_rva + (uint32_t)l.hn_write);
    buf_u64(rdata, l.ilt_off + 2 * 8, cg->rdata_rva + (uint32_t)l.hn_exit);
    buf_u64(rdata, l.ilt_off + 3 * 8, cg->rdata_rva + (uint32_t)l.hn_setconcp);
    buf_u64(rdata, l.ilt_off + 4 * 8, cg->rdata_rva + (uint32_t)l.hn_getheap);
    buf_u64(rdata, l.ilt_off + 5 * 8, cg->rdata_rva + (uint32_t)l.hn_heapalloc);
    buf_u64(rdata, l.ilt_off + 6 * 8, 0);

    buf_u64(rdata, l.iat_off + 0 * 8, cg->rdata_rva + (uint32_t)l.hn_getstd);
    buf_u64(rdata, l.iat_off + 1 * 8, cg->rdata_rva + (uint32_t)l.hn_write);
    buf_u64(rdata, l.iat_off + 2 * 8, cg->rdata_rva + (uint32_t)l.hn_exit);
    buf_u64(rdata, l.iat_off + 3 * 8, cg->rdata_rva + (uint32_t)l.hn_setconcp);
    buf_u64(rdata, l.iat_off + 4 * 8, cg->rdata_rva + (uint32_t)l.hn_getheap);
    buf_u64(rdata, l.iat_off + 5 * 8, cg->rdata_rva + (uint32_t)l.hn_heapalloc);
    buf_u64(rdata, l.iat_off + 6 * 8, 0);

    buf_u16(rdata, l.hn_getstd + 0, 0);
    memcpy(rdata + l.hn_getstd + 2, "GetStdHandle", strlen("GetStdHandle") + 1);
    buf_u16(rdata, l.hn_write + 0, 0);
    memcpy(rdata + l.hn_write + 2, "WriteFile", strlen("WriteFile") + 1);
    buf_u16(rdata, l.hn_exit + 0, 0);
    memcpy(rdata + l.hn_exit + 2, "ExitProcess", strlen("ExitProcess") + 1);
    buf_u16(rdata, l.hn_setconcp + 0, 0);
    memcpy(rdata + l.hn_setconcp + 2, "SetConsoleOutputCP", strlen("SetConsoleOutputCP") + 1);
    buf_u16(rdata, l.hn_getheap + 0, 0);
    memcpy(rdata + l.hn_getheap + 2, "GetProcessHeap", strlen("GetProcessHeap") + 1);
    buf_u16(rdata, l.hn_heapalloc + 0, 0);
    memcpy(rdata + l.hn_heapalloc + 2, "HeapAlloc", strlen("HeapAlloc") + 1);
    memcpy(rdata + l.dll_name, "kernel32.dll", strlen("kernel32.dll") + 1);

    FILE *f = fopen(out, "wb");
    if (!f) die("failed to open output");

    uint8_t dos_stub[0x80] = {0};
    dos_stub[0] = 'M';
    dos_stub[1] = 'Z';
    dos_stub[0x3C] = 0x80;
    fwrite(dos_stub, 1, sizeof(dos_stub), f);

    fwrite("PE\0\0", 1, 4, f);
    write_u16(f, 0x8664);
    write_u16(f, 2);
    write_u32(f, 0);
    write_u32(f, 0);
    write_u32(f, 0);
    write_u16(f, 0xF0);
    write_u16(f, 0x0022);

    write_u16(f, 0x20B);
    write_u8(f, 0);
    write_u8(f, 0);
    write_u32(f, (uint32_t)text_raw_size);
    write_u32(f, (uint32_t)rdata_raw_size);
    write_u32(f, 0);
    write_u32(f, cg->text_rva);
    write_u32(f, cg->text_rva);
    write_u64(f, 0x140000000ULL);
    write_u32(f, 0x1000);
    write_u32(f, 0x200);
    write_u16(f, 6);
    write_u16(f, 0);
    write_u16(f, 0);
    write_u16(f, 0);
    write_u16(f, 6);
    write_u16(f, 0);
    write_u32(f, 0);
    write_u32(f, size_of_image);
    write_u32(f, (uint32_t)headers_size);
    write_u32(f, 0);
    write_u16(f, 3);
    write_u16(f, 0);
    write_u64(f, 0x100000);
    write_u64(f, 0x1000);
    write_u64(f, 0x100000);
    write_u64(f, 0x1000);
    write_u32(f, 0);
    write_u32(f, 16);

    for (int i = 0; i < 16; i++) {
        if (i == 1) {
            write_u32(f, cg->rdata_rva + (uint32_t)l.import_desc_off);
            write_u32(f, 40);
        } else {
            write_u32(f, 0);
            write_u32(f, 0);
        }
    }

    uint8_t text_name[8] = {'.','t','e','x','t',0,0,0};
    fwrite(text_name, 1, 8, f);
    write_u32(f, (uint32_t)cg->code.len);
    write_u32(f, cg->text_rva);
    write_u32(f, (uint32_t)text_raw_size);
    write_u32(f, (uint32_t)headers_size);
    write_u32(f, 0);
    write_u32(f, 0);
    write_u16(f, 0);
    write_u16(f, 0);
    write_u32(f, 0x60000020);

    uint8_t rdata_name[8] = {'.','r','d','a','t','a',0,0};
    fwrite(rdata_name, 1, 8, f);
    write_u32(f, (uint32_t)l.rdata_size);
    write_u32(f, cg->rdata_rva);
    write_u32(f, (uint32_t)rdata_raw_size);
    write_u32(f, (uint32_t)(headers_size + text_raw_size));
    write_u32(f, 0);
    write_u32(f, 0);
    write_u16(f, 0);
    write_u16(f, 0);
    write_u32(f, 0x40000040);

    long pos = ftell(f);
    while (pos < (long)headers_size) {
        fputc(0, f);
        pos++;
    }

    fwrite(cg->code.data, 1, cg->code.len, f);
    for (size_t i = cg->code.len; i < text_raw_size; i++) fputc(0, f);

    fwrite(rdata, 1, rdata_raw_size, f);

    fclose(f);
    free(rdata);
}

