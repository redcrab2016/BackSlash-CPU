# BackSlash 9 Disassembler (`bslash9dis`)

This is a C disassembler for the synthetic **BackSlash 9** 16-bit ISA.

It reads a raw binary file as produced by `bslash9asm` and writes BackSlash 9 assembly text.

The output is designed to be **round-trippable** with the assembler:

```bash
bslash9dis input.bin output.asm
bslash9asm output.asm rebuilt.bin
```

For binaries produced by the companion assembler, the rebuilt binary should match the original byte-for-byte when the same endianness is used.

## Files

- `bslash9dis.c` — C source code
- `bslash9dis` — compiled executable (if provided)

## Build

```bash
gcc -std=c11 -Wall -Wextra -O2 bslash9dis.c -o bslash9dis
```

## Command line

```bash
bslash9dis [--le|--be] input.bin [output.asm]
```

Options:

- `--le` : input words are little-endian (**default**)
- `--be` : input words are big-endian

Arguments:

- `input.bin` : raw BackSlash 9 binary image
- `output.asm` : optional output text file

If `output.asm` is omitted, the disassembly is written to standard output.

Examples:

```bash
bslash9dis program.bin program.asm
bslash9dis --be program_be.bin program.asm
bslash9dis program.bin > program.asm
```

## What the disassembler emits

It emits canonical assembly text compatible with the assembler syntax used by `bslash9asm`.

### Generic instructions

These are emitted in the normal two-operand form:

```asm
CPY R1, R2
ADD R3, [R8]
SHR [R9], R0
```

### Special encodings

The ISA has several instruction families that reuse encodings that would otherwise be invalid generic forms.

The disassembler prints those using their **dedicated mnemonics**:

- `SAR`, `XOR`, `SIL`, `SIR`
- `R2C`, `C2R`, `SB0`, `CB0`
- `NEG`, `B2W`, `W2B`, `SWP`
- `SL4`, `SL8`, `SR4`, `SR8`
- `JMP`
- `AR0`

This matches the assembler's explicit policy and preserves the exact encoding when reassembled.

### Conditions

The disassembler uses these condition suffixes:

- `.cs`
- `.cc`
- `.eq`
- `.ne`
- `.ge`
- `.ls`

For the unconditional case (`al`), no suffix is printed.

Examples:

```asm
SUB.eq R1, ONE
JMP.ne L_0020
```

### Registers

The disassembler prints:

- `R0` to `R11`
- `ZERO` for `R12`
- `ONE` for `R13`
- `FLAG` for `R14`
- `PC` for `R15`

Examples:

```asm
CPY PC, R8
ADD R1, ONE
```

### Memory operands

Register-indirect memory operands are printed as:

```asm
[R8]
[PC]
```

### `AR0`

`AR0` is always emitted in an explicit form:

```asm
AR0 value, shift
```

Examples:

```asm
AR0 0x123, 0
AR0 0x1230, 1
```

This avoids ambiguity and makes the encoding exact.

### `JMP` labels

For `JMP` targets that point to another decoded word **inside the input file**, the disassembler creates labels of the form:

```asm
L_0003:
L_01AF:
```

and uses them in jumps:

```asm
JMP.ne L_0003
```

If a `JMP` target falls **outside the decoded file range**, the disassembler emits a numeric expression instead of a label so the assembler can still reconstruct the same signed relative branch.

Example:

```asm
JMP.ge -11
```

## Output format notes

The disassembler writes a short comment header like:

```asm
; Disassembly of input.bin
; Word count: 123
; Endianness: little-endian
```

These comments are accepted by the assembler.

## Important limitation: code vs data

A raw BackSlash 9 binary has no metadata telling whether a word is:

- executable code, or
- data emitted with `.word` / `DW`

Because of that, the disassembler performs a **word-by-word decoding** of the entire file as instruction words.

This means:

- the output will reassemble to the **same binary**, and
- words that were originally data may appear as instruction mnemonics in the disassembly

That is expected for a flat raw binary format with no symbol/debug information.

## Round-trip expectation

Typical workflow:

```bash
bslash9dis program.bin program.asm
bslash9asm program.asm rebuilt.bin
cmp program.bin rebuilt.bin
```

With matching endianness, `cmp` should report no differences.

## Error handling

The disassembler reports errors for:

- input file open failures
- output file open failures
- odd-sized input files
- files larger than 65536 words

## Design goal

The primary goal is:

1. produce readable assembly text
2. preserve exact machine encoding on reassembly

So the output is intentionally **canonical**, not necessarily identical to the original source formatting.
