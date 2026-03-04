#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#define MAX_LINES 65536
#define MAX_LABELS 8192
#define MAX_WORDS 65536
#define MAX_LINE_LEN 1024
#define MAX_TOKENS 64

typedef struct {
    int line_no;
    char *text;
    uint16_t addr;
    int words;
} Line;

typedef struct {
    char name[128];
    uint16_t value;
} Label;

typedef struct {
    Line lines[MAX_LINES];
    int line_count;
    Label labels[MAX_LABELS];
    int label_count;
    uint16_t image[MAX_WORDS];
    bool used[MAX_WORDS];
    bool big_endian;
    const char *infile;
    const char *outfile;
    int error_count;
} Asm;

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    memcpy(p, s, n + 1);
    return p;
}

static void fatal(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void asm_error(Asm *a, int line_no, const char *fmt, ...) {
    va_list ap;
    a->error_count++;
    fprintf(stderr, "%s:%d: error: ", a->infile ? a->infile : "<input>", line_no);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

static void trim_inplace(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static void strip_comment(char *s) {
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == ';' || s[i] == '#') { s[i] = '\0'; break; }
        if (s[i] == '/' && s[i + 1] == '/') { s[i] = '\0'; break; }
    }
}

static int ci_cmp(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static bool is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_' || c == '.';
}

static bool is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '.';
}

static int find_label(const Asm *a, const char *name) {
    for (int i = 0; i < a->label_count; i++) {
        if (ci_cmp(a->labels[i].name, name) == 0) return i;
    }
    return -1;
}

static void add_label(Asm *a, int line_no, const char *name, uint16_t value) {
    if (find_label(a, name) >= 0) {
        asm_error(a, line_no, "duplicate label '%s'", name);
        return;
    }
    if (a->label_count >= MAX_LABELS) {
        asm_error(a, line_no, "too many labels");
        return;
    }
    size_t copy_n = strlen(name);
    if (copy_n >= sizeof(a->labels[a->label_count].name)) copy_n = sizeof(a->labels[a->label_count].name) - 1;
    memcpy(a->labels[a->label_count].name, name, copy_n);
    a->labels[a->label_count].name[copy_n] = '\0';
    a->labels[a->label_count].value = value;
    a->label_count++;
}

static bool parse_int_literal(const char *s, long *out) {
    if (!s || !*s) return false;
    char *end = NULL;
    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) base = 16;
    else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        long v = 0;
        for (int i = 2; s[i]; i++) {
            if (s[i] != '0' && s[i] != '1') return false;
            v = (v << 1) | (s[i] - '0');
        }
        *out = v;
        return true;
    }
    errno = 0;
    long v = strtol(s, &end, base);
    if (errno || !end || *end) return false;
    *out = v;
    return true;
}

static bool eval_expr(Asm *a, int line_no, const char *expr, long *out) {
    char buf[MAX_LINE_LEN];
    strncpy(buf, expr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim_inplace(buf);
    if (!buf[0]) {
        asm_error(a, line_no, "empty expression");
        return false;
    }
    long total = 0;
    int sign = +1;
    char *p = buf;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '+') { sign = +1; p++; continue; }
        if (*p == '-') { sign = -1; p++; continue; }
        char term[256];
        int ti = 0;
        if (is_ident_start(*p)) {
            while (*p && is_ident_char(*p) && ti < (int)sizeof(term) - 1) term[ti++] = *p++;
        } else {
            while (*p && !isspace((unsigned char)*p) && *p != '+' && *p != '-' && ti < (int)sizeof(term) - 1) term[ti++] = *p++;
        }
        term[ti] = '\0';
        if (!term[0]) {
            asm_error(a, line_no, "invalid expression near '%s'", p);
            return false;
        }
        long value = 0;
        if (!ci_cmp(term, "$")) {
            asm_error(a, line_no, "'$' is not supported; use labels or numeric constants");
            return false;
        } else if (parse_int_literal(term, &value)) {
            /* ok */
        } else {
            int idx = find_label(a, term);
            if (idx < 0) {
                asm_error(a, line_no, "unknown symbol '%s'", term);
                return false;
            }
            value = a->labels[idx].value;
        }
        total += sign * value;
        sign = +1;
    }
    *out = total;
    return true;
}

static int split_operands(char *s, char *out[], int max_out) {
    int count = 0;
    char *p = s;
    while (*p) {
        while (isspace((unsigned char)*p) || *p == ',') p++;
        if (!*p) break;
        if (count >= max_out) return count;
        out[count++] = p;
        int bracket_depth = 0;
        while (*p) {
            if (*p == '[') bracket_depth++;
            else if (*p == ']') bracket_depth--;
            else if (*p == ',' && bracket_depth == 0) break;
            p++;
        }
        if (*p == ',') { *p = '\0'; p++; }
    }
    for (int i = 0; i < count; i++) trim_inplace(out[i]);
    return count;
}

static bool parse_condition(const char *s, int *out_cond) {
    struct { const char *name; int value; } conds[] = {
        {"al",0},{"cs",1},{"ae",1},{"cc",2},{"bl",2},{"zs",3},{"eq",3},
        {"zc",4},{"ne",4},{"sc",5},{"ge",5},{"ss",6},{"ls",6}
    };
    for (size_t i = 0; i < sizeof(conds)/sizeof(conds[0]); i++) {
        if (ci_cmp(s, conds[i].name) == 0) { *out_cond = conds[i].value; return true; }
    }
    return false;
}

static bool parse_register(const char *s, int *out_reg) {
    if (!s) return false;
    if ((s[0] == 'R' || s[0] == 'r') && isdigit((unsigned char)s[1])) {
        char *end = NULL;
        long v = strtol(s + 1, &end, 10);
        if (*end == '\0' && v >= 0 && v <= 15) { *out_reg = (int)v; return true; }
    }
    struct { const char *name; int reg; } aliases[] = {
        {"ZERO",12},{"ONE",13},{"FLAG",14},{"PC",15}
    };
    for (size_t i = 0; i < sizeof(aliases)/sizeof(aliases[0]); i++) {
        if (ci_cmp(s, aliases[i].name) == 0) { *out_reg = aliases[i].reg; return true; }
    }
    return false;
}

typedef struct {
    bool is_mem;
    int reg;
} Operand;

static bool parse_operand_regmem(Asm *a, int line_no, const char *s, Operand *op) {
    char buf[128];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim_inplace(buf);
    size_t n = strlen(buf);
    if (n >= 3 && buf[0] == '[' && buf[n - 1] == ']') {
        buf[n - 1] = '\0';
        char *inner = buf + 1;
        trim_inplace(inner);
        if (!parse_register(inner, &op->reg)) {
            asm_error(a, line_no, "expected register inside memory operand: '%s'", s);
            return false;
        }
        op->is_mem = true;
        return true;
    }
    if (!parse_register(buf, &op->reg)) {
        asm_error(a, line_no, "expected register or [register]: '%s'", s);
        return false;
    }
    op->is_mem = false;
    return true;
}

static bool parse_word_value(Asm *a, int line_no, const char *expr, uint16_t *out) {
    long v;
    if (!eval_expr(a, line_no, expr, &v)) return false;
    if (v < -32768 || v > 0xFFFF) {
        asm_error(a, line_no, "word value out of range: %ld", v);
        return false;
    }
    *out = (uint16_t)(v & 0xFFFF);
    return true;
}

static bool emit_word(Asm *a, int line_no, uint16_t addr, uint16_t value) {
    if (a->used[addr]) {
        asm_error(a, line_no, "address 0x%04X written more than once", addr);
        return false;
    }
    a->image[addr] = value;
    a->used[addr] = true;
    return true;
}

static int generic_opcode(const char *mn) {
    struct { const char *name; int op; } ops[] = {
        {"CPY",0},{"NOT",1},{"ADD",2},{"SUB",3},{"AND",4},{"OR",5},{"SHL",6},{"SHR",7}
    };
    for (size_t i = 0; i < sizeof(ops)/sizeof(ops[0]); i++) if (ci_cmp(mn, ops[i].name) == 0) return ops[i].op;
    return -1;
}

static bool encode_instruction(Asm *a, Line *ln, uint16_t *out_word) {
    char buf[MAX_LINE_LEN];
    strncpy(buf, ln->text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *s = buf;
    trim_inplace(s);
    if (!*s) return false;

    char *mn = s;
    while (*s && !isspace((unsigned char)*s)) s++;
    if (*s) *s++ = '\0';
    trim_inplace(s);

    int cond = 0;
    char *dot = strrchr(mn, '.');
    if (dot) {
        *dot++ = '\0';
        if (!parse_condition(dot, &cond)) {
            asm_error(a, ln->line_no, "unknown condition suffix '%s'", dot);
            return false;
        }
    }

    char *ops[MAX_TOKENS];
    int opcount = split_operands(s, ops, MAX_TOKENS);

    int gop = generic_opcode(mn);
    if (gop >= 0) {
        if (opcount != 2) {
            asm_error(a, ln->line_no, "%s expects 2 operands", mn);
            return false;
        }
        Operand d, src;
        if (!parse_operand_regmem(a, ln->line_no, ops[0], &d)) return false;
        if (!parse_operand_regmem(a, ln->line_no, ops[1], &src)) return false;

        if ((gop == 0 || gop == 1) && (d.reg == 12 || d.reg == 13)) {
            asm_error(a, ln->line_no, "%s with destination R12/R13 or [R12]/[R13] is reserved for special opcodes; use the dedicated mnemonic", mn);
            return false;
        }
        if ((gop == 6 || gop == 7) && (d.reg == 12 || d.reg == 13)) {
            asm_error(a, ln->line_no, "%s with destination R12/R13 or [R12]/[R13] is reserved for JMP", mn);
            return false;
        }
        *out_word = (uint16_t)((cond << 13) | (gop << 10) | ((d.is_mem ? 1 : 0) << 9) |
                     (d.reg << 5) | ((src.is_mem ? 1 : 0) << 4) | src.reg);
        return true;
    }

    struct { const char *name; int base7; int subop; bool reg_arg; bool implicit_r0_source; } spec[] = {
        {"SAR",0b0000110,0,true,true}, {"XOR",0b0000110,1,true,true},
        {"SIL",0b0000110,2,false,false}, {"SIR",0b0000110,3,false,false},
        {"R2C",0b0010110,0,false,false}, {"C2R",0b0010110,1,false,false},
        {"SB0",0b0010110,2,false,false}, {"CB0",0b0010110,3,false,false},
        {"NEG",0b0001110,0,true,false}, {"B2W",0b0001110,1,true,false},
        {"W2B",0b0001110,2,true,false}, {"SWP",0b0001110,3,true,false},
        {"SL4",0b0011110,0,true,false}, {"SL8",0b0011110,1,true,false},
        {"SR4",0b0011110,2,true,false}, {"SR8",0b0011110,3,true,false}
    };
    for (size_t i = 0; i < sizeof(spec)/sizeof(spec[0]); i++) {
        if (ci_cmp(mn, spec[i].name) == 0) {
            int low = 0;
            if (spec[i].reg_arg) {
                if (spec[i].implicit_r0_source) {
                    if (opcount == 1) {
                        if (!parse_register(ops[0], &low)) {
                            asm_error(a, ln->line_no, "%s expects a register", mn);
                            return false;
                        }
                    } else if (opcount == 2) {
                        int r0;
                        if (!parse_register(ops[0], &r0) || r0 != 0) {
                            asm_error(a, ln->line_no, "%s accepts either '%s Rx' or '%s R0, Rx'", mn, mn, mn);
                            return false;
                        }
                        if (!parse_register(ops[1], &low)) {
                            asm_error(a, ln->line_no, "%s expects a register as second operand", mn);
                            return false;
                        }
                    } else {
                        asm_error(a, ln->line_no, "%s expects 1 operand (or 2 with explicit R0)", mn);
                        return false;
                    }
                } else {
                    if (opcount != 1 || !parse_register(ops[0], &low)) {
                        asm_error(a, ln->line_no, "%s expects exactly 1 register operand", mn);
                        return false;
                    }
                }
            } else {
                long v;
                if (opcount != 1 || !eval_expr(a, ln->line_no, ops[0], &v)) return false;
                if (v < 0 || v > 15) {
                    asm_error(a, ln->line_no, "%s immediate must be in [0,15]", mn);
                    return false;
                }
                low = (int)v;
            }
            *out_word = (uint16_t)((cond << 13) | (spec[i].base7 << 6) | (spec[i].subop << 4) | low);
            return true;
        }
    }

    if (ci_cmp(mn, "JMP") == 0) {
        if (opcount != 1) {
            asm_error(a, ln->line_no, "JMP expects 1 operand");
            return false;
        }
        long target;
        if (!eval_expr(a, ln->line_no, ops[0], &target)) return false;
        long rel = target - ((long)ln->addr + 1L);
        if (rel < -128 || rel > 127) {
            asm_error(a, ln->line_no, "JMP target out of range: relative offset %ld not in [-128,127]", rel);
            return false;
        }
        uint8_t u = (uint8_t)(int8_t)rel;
        *out_word = (uint16_t)((cond << 13) | 0x18C0u | (((u >> 7) & 1u) << 10) |
                     (((u >> 6) & 1u) << 9) | (u & 0x3Fu));
        return true;
    }

    if (ci_cmp(mn, "AR0") == 0) {
        if (cond != 0) {
            asm_error(a, ln->line_no, "AR0 has no condition field; do not add a condition suffix");
            return false;
        }
        if (opcount != 1 && opcount != 2) {
            asm_error(a, ln->line_no, "AR0 expects 'AR0 value' or 'AR0 value, shift'");
            return false;
        }
        long val;
        if (!eval_expr(a, ln->line_no, ops[0], &val)) return false;
        long shift = -1;
        if (opcount == 2) {
            if (!eval_expr(a, ln->line_no, ops[1], &shift)) return false;
            if (shift != 0 && shift != 1) {
                asm_error(a, ln->line_no, "AR0 shift must be 0 or 1");
                return false;
            }
        }
        long imm = val;
        if (shift < 0) {
            if (imm >= 0 && imm <= 0x0FFF) shift = 0;
            else if ((imm & 0xF) == 0 && imm >= 0 && imm <= 0xFFF0) shift = 1, imm >>= 4;
            else {
                asm_error(a, ln->line_no, "AR0 value 0x%lX is not representable (need 12 bits, optionally shifted left by 4)", val);
                return false;
            }
        } else {
            if (shift == 1) {
                if ((imm & 0xF) != 0) {
                    asm_error(a, ln->line_no, "AR0 with shift 1 requires the value's low nibble to be zero");
                    return false;
                }
                imm >>= 4;
            }
            if (imm < 0 || imm > 0x0FFF) {
                asm_error(a, ln->line_no, "AR0 immediate out of range after shift selection");
                return false;
            }
        }
        *out_word = (uint16_t)(0xE000u | ((shift & 1) << 12) | (imm & 0x0FFF));
        return true;
    }

    asm_error(a, ln->line_no, "unknown mnemonic '%s'", mn);
    return false;
}

static int count_words_for_directive(const char *mn, char *rest) {
    if (ci_cmp(mn, ".org") == 0 || ci_cmp(mn, "ORG") == 0 || ci_cmp(mn, ".end") == 0 || ci_cmp(mn, "END") == 0) return 0;
    if (ci_cmp(mn, ".word") == 0 || ci_cmp(mn, "DW") == 0 || ci_cmp(mn, "WORD") == 0) {
        char *ops[MAX_TOKENS];
        return split_operands(rest, ops, MAX_TOKENS);
    }
    if (ci_cmp(mn, ".fill") == 0 || ci_cmp(mn, "FILL") == 0) {
        char *ops[MAX_TOKENS];
        int n = split_operands(rest, ops, MAX_TOKENS);
        (void)n;
        return -1; /* resolved later */
    }
    return 1;
}

static bool first_pass(Asm *a) {
    FILE *f = fopen(a->infile, "r");
    if (!f) fatal("cannot open input file");
    char raw[MAX_LINE_LEN];
    uint32_t pc = 0;
    int line_no = 0;
    while (fgets(raw, sizeof(raw), f)) {
        line_no++;
        char line[MAX_LINE_LEN];
        strncpy(line, raw, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        strip_comment(line);
        trim_inplace(line);
        if (!line[0]) continue;

        char *p = line;
        while (1) {
            char *colon = strchr(p, ':');
            if (!colon) break;
            bool valid_label = true;
            for (char *q = p; q < colon; q++) {
                if (isspace((unsigned char)*q)) { valid_label = false; break; }
            }
            if (!valid_label) break;
            *colon = '\0';
            trim_inplace(p);
            if (!p[0]) break;
            add_label(a, line_no, p, (uint16_t)pc);
            p = colon + 1;
            trim_inplace(p);
            if (!*p) break;
        }
        if (!*p) continue;

        if (a->line_count >= MAX_LINES) fatal("too many source lines");
        a->lines[a->line_count].line_no = line_no;
        a->lines[a->line_count].text = xstrdup(p);
        a->lines[a->line_count].addr = (uint16_t)pc;
        a->lines[a->line_count].words = 0;

        char tmpbuf[MAX_LINE_LEN];
        strncpy(tmpbuf, p, sizeof(tmpbuf) - 1);
        tmpbuf[sizeof(tmpbuf) - 1] = '\0';
        char *mn = tmpbuf;
        char *restp = tmpbuf;
        while (*restp && !isspace((unsigned char)*restp)) restp++;
        if (*restp) *restp++ = '\0';
        trim_inplace(restp);

        int words = count_words_for_directive(mn, restp);
        if (words == 0) {
            if (ci_cmp(mn, ".org") == 0 || ci_cmp(mn, "ORG") == 0) {
                long new_pc;
                if (!eval_expr(a, line_no, restp, &new_pc)) continue;
                if (new_pc < 0 || new_pc >= MAX_WORDS) {
                    asm_error(a, line_no, ".org address out of range");
                    continue;
                }
                pc = (uint32_t)new_pc;
                a->lines[a->line_count].addr = (uint16_t)pc;
            }
            a->line_count++;
            continue;
        }
        if (words == -1) {
            char *ops[MAX_TOKENS];
            int n = split_operands(restp, ops, MAX_TOKENS);
            if (n < 1 || n > 2) {
                asm_error(a, line_no, ".fill expects count[, value]");
                words = 0;
            } else {
                long count;
                if (!eval_expr(a, line_no, ops[0], &count) || count < 0 || count > MAX_WORDS) {
                    asm_error(a, line_no, "invalid .fill count");
                    words = 0;
                } else {
                    words = (int)count;
                }
            }
        }
        a->lines[a->line_count].words = words;
        if (pc + (uint32_t)words > MAX_WORDS) {
            asm_error(a, line_no, "program exceeds 64K words of address space");
        } else {
            pc += (uint32_t)words;
        }
        a->line_count++;
    }
    fclose(f);
    return a->error_count == 0;
}

static bool second_pass(Asm *a) {
    for (int i = 0; i < a->line_count; i++) {
        Line *ln = &a->lines[i];
        char buf[MAX_LINE_LEN];
        strncpy(buf, ln->text, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        trim_inplace(buf);
        if (!buf[0]) continue;
        char *mn = buf;
        char *rest = buf;
        while (*rest && !isspace((unsigned char)*rest)) rest++;
        if (*rest) *rest++ = '\0';
        trim_inplace(rest);

        if (ci_cmp(mn, ".org") == 0 || ci_cmp(mn, "ORG") == 0 || ci_cmp(mn, ".end") == 0 || ci_cmp(mn, "END") == 0) {
            continue;
        }
        if (ci_cmp(mn, ".word") == 0 || ci_cmp(mn, "DW") == 0 || ci_cmp(mn, "WORD") == 0) {
            char *ops[MAX_TOKENS];
            int n = split_operands(rest, ops, MAX_TOKENS);
            for (int k = 0; k < n; k++) {
                uint16_t v;
                if (!parse_word_value(a, ln->line_no, ops[k], &v)) continue;
                emit_word(a, ln->line_no, (uint16_t)(ln->addr + k), v);
            }
            continue;
        }
        if (ci_cmp(mn, ".fill") == 0 || ci_cmp(mn, "FILL") == 0) {
            char *ops[MAX_TOKENS];
            int n = split_operands(rest, ops, MAX_TOKENS);
            if (n < 1 || n > 2) {
                asm_error(a, ln->line_no, ".fill expects count[, value]");
                continue;
            }
            long count;
            if (!eval_expr(a, ln->line_no, ops[0], &count) || count < 0 || count > MAX_WORDS) {
                asm_error(a, ln->line_no, "invalid .fill count");
                continue;
            }
            uint16_t fill_value = 0;
            if (n == 2 && !parse_word_value(a, ln->line_no, ops[1], &fill_value)) continue;
            for (long k = 0; k < count; k++) emit_word(a, ln->line_no, (uint16_t)(ln->addr + k), fill_value);
            continue;
        }
        uint16_t word;
        if (encode_instruction(a, ln, &word)) emit_word(a, ln->line_no, ln->addr, word);
    }
    return a->error_count == 0;
}

static bool write_output(Asm *a) {
    int highest = -1;
    for (int i = 0; i < MAX_WORDS; i++) if (a->used[i]) highest = i;
    if (highest < 0) highest = -1;

    FILE *f = fopen(a->outfile, "wb");
    if (!f) {
        fprintf(stderr, "cannot open output '%s'\n", a->outfile);
        return false;
    }
    for (int i = 0; i <= highest; i++) {
        uint16_t w = a->used[i] ? a->image[i] : 0;
        uint8_t b[2];
        if (a->big_endian) { b[0] = (uint8_t)(w >> 8); b[1] = (uint8_t)(w & 0xFF); }
        else { b[0] = (uint8_t)(w & 0xFF); b[1] = (uint8_t)(w >> 8); }
        if (fwrite(b, 1, 2, f) != 2) {
            fclose(f);
            fprintf(stderr, "write failed\n");
            return false;
        }
    }
    fclose(f);
    return true;
}

static void usage(FILE *f) {
    fprintf(f,
        "bslash9asm - assembler for the synthetic BackSlash 9 ISA\n\n"
        "Usage:\n"
        "  bslash9asm [--be|--le] input.asm output.bin\n\n"
        "Options:\n"
        "  --le   write output words little-endian (default)\n"
        "  --be   write output words big-endian\n"
        "  -h     show this help\n");
}

int main(int argc, char **argv) {
    Asm a;
    memset(&a, 0, sizeof(a));
    a.big_endian = false;

    const char *pos[2] = {0};
    int pc = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--be")) a.big_endian = true;
        else if (!strcmp(argv[i], "--le")) a.big_endian = false;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(stdout); return 0; }
        else if (pc < 2) pos[pc++] = argv[i];
        else { usage(stderr); return 1; }
    }
    if (pc != 2) { usage(stderr); return 1; }
    a.infile = pos[0];
    a.outfile = pos[1];

    if (!first_pass(&a) || !second_pass(&a)) return 1;
    if (!write_output(&a)) return 1;
    return 0;
}
