#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define MAX_WORDS 65536

static void fatal(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static const char *cond_suffix(int cond) {
    switch (cond & 7) {
        case 0: return "";
        case 1: return ".cs";
        case 2: return ".cc";
        case 3: return ".eq";
        case 4: return ".ne";
        case 5: return ".ge";
        case 6: return ".ls";
        default: return ""; /* 111 is only AR0 marker, not a condition suffix */
    }
}

static const char *reg_name(int reg) {
    static const char *names[16] = {
        "R0","R1","R2","R3","R4","R5","R6","R7",
        "R8","R9","R10","R11","ZERO","ONE","FLAG","PC"
    };
    if (reg < 0 || reg > 15) return "R?";
    return names[reg];
}

static void format_regmem(char *buf, size_t n, bool is_mem, int reg) {
    if (is_mem) snprintf(buf, n, "[%s]", reg_name(reg));
    else snprintf(buf, n, "%s", reg_name(reg));
}

static int load_words(const char *path, bool big_endian, uint16_t words[MAX_WORDS]) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open input file '%s'\n", path);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        fatal("fseek failed");
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        fatal("ftell failed");
    }
    if ((sz & 1L) != 0) {
        fclose(f);
        fprintf(stderr, "input size is odd (%ld bytes); expected an even number of bytes\n", sz);
        return -1;
    }
    long count = sz / 2;
    if (count > MAX_WORDS) {
        fclose(f);
        fprintf(stderr, "input has %ld words; maximum supported is %d\n", count, MAX_WORDS);
        return -1;
    }
    rewind(f);
    for (long i = 0; i < count; i++) {
        int b0 = fgetc(f);
        int b1 = fgetc(f);
        if (b0 == EOF || b1 == EOF) {
            fclose(f);
            fatal("short read");
        }
        if (big_endian) words[i] = (uint16_t)(((uint16_t)b0 << 8) | (uint16_t)b1);
        else words[i] = (uint16_t)(((uint16_t)b1 << 8) | (uint16_t)b0);
    }
    fclose(f);
    return (int)count;
}

static bool is_jmp_word(uint16_t w) {
    return (w & 0x18C0u) == 0x18C0u;
}

static int8_t decode_jmp_rel(uint16_t w) {
    uint8_t u = (uint8_t)((((w >> 10) & 1u) << 7) | (((w >> 9) & 1u) << 6) | (w & 0x3Fu));
    return (int8_t)u;
}

static void emit_word_disasm(FILE *out, uint16_t addr, uint16_t w, const bool label_here[MAX_WORDS]) {
    int cond = (w >> 13) & 7;

    if (label_here[addr]) {
        fprintf(out, "L_%04X:\n", addr);
    }

    if (cond == 7) {
        int shift = (w >> 12) & 1;
        unsigned imm = w & 0x0FFFu;
        unsigned value = shift ? (imm << 4) : imm;
        fprintf(out, "    AR0 0x%X, %d\n", value, shift);
        return;
    }

    unsigned base7 = (w >> 6) & 0x7Fu;
    unsigned subop = (w >> 4) & 0x3u;
    unsigned low4 = w & 0xFu;

    if (is_jmp_word(w)) {
        int8_t rel = decode_jmp_rel(w);
        int target_wrap = ((int)addr + 1 + (int)rel) & 0xFFFF;
        int target_expr = (int)addr + 1 + (int)rel;
        if (target_wrap >= 0 && target_wrap < MAX_WORDS && label_here[target_wrap])
            fprintf(out, "    JMP%s L_%04X\n", cond_suffix(cond), target_wrap);
        else if (target_expr < 0)
            fprintf(out, "    JMP%s %d\n", cond_suffix(cond), target_expr);
        else
            fprintf(out, "    JMP%s 0x%X\n", cond_suffix(cond), target_expr);
        return;
    }

    if (base7 == 0x06u) {
        switch (subop) {
            case 0: fprintf(out, "    SAR%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
            case 1: fprintf(out, "    XOR%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
            case 2: fprintf(out, "    SIL%s %u\n", cond_suffix(cond), low4); return;
            case 3: fprintf(out, "    SIR%s %u\n", cond_suffix(cond), low4); return;
        }
    }

    if (base7 == 0x16u) {
        switch (subop) {
            case 0: fprintf(out, "    R2C%s %u\n", cond_suffix(cond), low4); return;
            case 1: fprintf(out, "    C2R%s %u\n", cond_suffix(cond), low4); return;
            case 2: fprintf(out, "    SB0%s %u\n", cond_suffix(cond), low4); return;
            case 3: fprintf(out, "    CB0%s %u\n", cond_suffix(cond), low4); return;
        }
    }

    if (base7 == 0x0Eu) {
        switch (subop) {
            case 0: fprintf(out, "    NEG%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
            case 1: fprintf(out, "    B2W%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
            case 2: fprintf(out, "    W2B%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
            case 3: fprintf(out, "    SWP%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
        }
    }

    if (base7 == 0x1Eu) {
        switch (subop) {
            case 0: fprintf(out, "    SL4%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
            case 1: fprintf(out, "    SL8%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
            case 2: fprintf(out, "    SR4%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
            case 3: fprintf(out, "    SR8%s %s\n", cond_suffix(cond), reg_name((int)low4)); return;
        }
    }

    {
        static const char *mn[8] = {"CPY","NOT","ADD","SUB","AND","OR","SHL","SHR"};
        unsigned op = (w >> 10) & 0x7u;
        bool dmem = ((w >> 9) & 1u) != 0;
        int dreg = (w >> 5) & 0xFu;
        bool smem = ((w >> 4) & 1u) != 0;
        int sreg = w & 0xFu;
        char d[32], s[32];
        format_regmem(d, sizeof(d), dmem, dreg);
        format_regmem(s, sizeof(s), smem, sreg);
        fprintf(out, "    %s%s %s, %s\n", mn[op], cond_suffix(cond), d, s);
    }
}

static void collect_labels(const uint16_t *words, int count, bool label_here[MAX_WORDS]) {
    memset(label_here, 0, MAX_WORDS * sizeof(label_here[0]));
    for (int addr = 0; addr < count; addr++) {
        uint16_t w = words[addr];
        int cond = (w >> 13) & 7;
        if (cond == 7) continue;
        if (is_jmp_word(w)) {
            int8_t rel = decode_jmp_rel(w);
            int target = ((addr + 1 + (int)rel) & 0xFFFF);
            if (target >= 0 && target < count) label_here[target] = true;
        }
    }
}

static void usage(const char *argv0) {
    fprintf(stderr,
        "Usage: %s [--le|--be] input.bin [output.asm]\n"
        "  --le   input words are little-endian (default)\n"
        "  --be   input words are big-endian\n"
        "If output.asm is omitted, assembly is written to stdout.\n",
        argv0);
}

int main(int argc, char **argv) {
    bool big_endian = false;
    const char *infile = NULL;
    const char *outfile = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--be") == 0) big_endian = true;
        else if (strcmp(argv[i], "--le") == 0) big_endian = false;
        else if (!infile) infile = argv[i];
        else if (!outfile) outfile = argv[i];
        else {
            usage(argv[0]);
            return 1;
        }
    }

    if (!infile) {
        usage(argv[0]);
        return 1;
    }

    uint16_t words[MAX_WORDS];
    int count = load_words(infile, big_endian, words);
    if (count < 0) return 1;

    bool label_here[MAX_WORDS];
    collect_labels(words, count, label_here);

    FILE *out = stdout;
    if (outfile) {
        out = fopen(outfile, "w");
        if (!out) {
            fprintf(stderr, "cannot open output file '%s'\n", outfile);
            return 1;
        }
    }

    fprintf(out, "; Disassembly of %s\n", infile);
    fprintf(out, "; Word count: %d\n", count);
    fprintf(out, "; Endianness: %s\n\n", big_endian ? "big-endian" : "little-endian");

    for (int addr = 0; addr < count; addr++) {
        emit_word_disasm(out, (uint16_t)addr, words[addr], label_here);
    }

    if (outfile) fclose(out);
    return 0;
}
