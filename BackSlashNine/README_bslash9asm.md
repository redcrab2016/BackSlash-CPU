# BackSlash 9 Assembler (`bslash9asm`)

This is a small two-pass assembler, written in C, for the synthetic **BackSlash 9** 16-bit ISA described in your specification.

It reads a text assembly file and writes a **raw binary image** made of 16-bit words.

## What the assembler supports

- All documented instruction families:
  - Generic: `CPY`, `ADD`, `SUB`, `NOT`, `AND`, `OR`, `SHL`, `SHR`
  - Special encodings: `SAR`, `XOR`, `SIL`, `SIR`, `R2C`, `C2R`, `SB0`, `CB0`, `NEG`, `B2W`, `W2B`, `SWP`, `SL4`, `SL8`, `SR4`, `SR8`, `JMP`, `AR0`
- Condition suffixes such as `.eq`, `.ne`, `.cs`, `.ge`, etc.
- Labels
- Numeric expressions with `+` and `-`
- Data directives:
  - `.org expr`
  - `.word expr[, expr ...]`
  - `.fill count[, value]`
  - `.end`
- Register aliases: `ZERO`, `ONE`, `FLAG`, `PC`
- Memory-register operands such as `[R8]`

## Output format

The output file is a raw sequence of 16-bit words.

- Default output endianness: **little-endian**
- Optional big-endian output: `--be`

If the source uses `.org` and leaves holes in the address space, the assembler fills the skipped words with `0x0000` in the output file.

The output spans from address `0x0000` up to the highest address written by the source.

## Build

```bash
gcc -std=c11 -Wall -Wextra -O2 bslash9asm.c -o bslash9asm
```

## Command line

```bash
bslash9asm [--le|--be] input.asm output.bin
```

Examples:

```bash
bslash9asm program.asm program.bin
bslash9asm --be program.asm program_be.bin
```

## Assembly syntax

### Comments

These comment styles are accepted:

```asm
; comment
# comment
// comment
```

### Labels

A label ends with `:`.

```asm
start:
loop:
```

Multiple labels may appear before one instruction.

### Registers

You may write either `R0`..`R15` or these aliases:

- `ZERO` = `R12`
- `ONE` = `R13`
- `FLAG` = `R14`
- `PC` = `R15`

### Memory operands

Only register-indirect memory operands are supported, exactly as in the ISA:

```asm
[R0]
[R8]
[PC]    ; valid syntax, even if unusual
```

### Conditions

Conditions are written as a suffix on the mnemonic:

```asm
ADD.eq R1, R2
SUB.ne R3, R4
CPY.cs R5, [R8]
JMP.ge target
```

Accepted condition names:

- `al`
- `cs`, `ae`
- `cc`, `bl`
- `zs`, `eq`
- `zc`, `ne`
- `sc`, `ge`
- `ss`, `ls`

`AR0` does **not** accept a condition suffix, because its encoding always uses `ccc = 111` as the immediate-assignment marker.

## Instruction forms

### 1. Generic two-operand instructions

Supported mnemonics:

- `CPY`
- `NOT`
- `ADD`
- `SUB`
- `AND`
- `OR`
- `SHL`
- `SHR`

Form:

```asm
MNEMONIC[.cond] dest, src
```

Examples:

```asm
CPY R1, R2
CPY R1, [R8]
CPY [R9], R2
CPY [R9], [R8]

ADD R1, R2
SUB.eq R3, R4
AND R5, [R6]
OR [R9], R0
SHL R1, ONE
SHR.ne [R8], R2
```

### Important restriction enforced by the assembler

The ISA reuses some otherwise-invalid encodings as different mnemonics.
To avoid accidental silent remapping, the assembler **rejects** these generic forms:

- `CPY` or `NOT` with destination `R12`, `R13`, `[R12]`, `[R13]`
- `SHL` or `SHR` with destination `R12`, `R13`, `[R12]`, `[R13]`

Use the dedicated special mnemonic instead:

- Use `SAR`, `XOR`, `SIL`, `SIR`, `R2C`, `C2R`, `SB0`, `CB0`, `NEG`, `B2W`, `W2B`, `SWP`, `SL4`, `SL8`, `SR4`, `SR8`, or `JMP`

This choice makes the source code explicit and prevents surprising assembly results.

### 2. `SAR` and `XOR`

The ISA defines these as operations on `R0` and another register.

Accepted forms:

```asm
SAR Rx
SAR R0, Rx
XOR Rx
XOR R0, Rx
```

Examples:

```asm
SAR R3
XOR R5
XOR R0, R8
```

### 3. Bit/shift-immediate special instructions

Immediate must be in the range `0..15`.

```asm
SIL imm4
SIR imm4
R2C imm4
C2R imm4
SB0 imm4
CB0 imm4
```

Examples:

```asm
SIL 4
SIR 1
R2C 7
C2R 0
SB0 12
CB0 12
```

### 4. One-register special instructions

Form:

```asm
NEG Rx
B2W Rx
W2B Rx
SWP Rx
SL4 Rx
SL8 Rx
SR4 Rx
SR8 Rx
```

Examples:

```asm
NEG R2
B2W R3
W2B R4
SWP R5
SL4 R6
SR8 R7
```

### 5. `JMP`

Form:

```asm
JMP[.cond] target
```

`target` may be:

- a label
- a numeric address expression

The assembler converts it to the ISA's signed 8-bit relative form.

#### Offset rule used by the assembler

Per your specification, `R15` designates the **next** instruction to execute.
So the assembler computes the branch offset as:

```text
offset = target_address - (address_of_this_JMP + 1)
```

Valid range is `-128..127`.

Examples:

```asm
JMP loop
JMP.eq done
JMP.ne start
```

### 6. `AR0`

`AR0` is encoded as:

```text
R0 = i << (s * 4)
```

The assembler accepts two forms:

```asm
AR0 value
AR0 value, shift
```

Rules:

- `shift` must be `0` or `1`
- In the one-operand form, the assembler auto-selects:
  - `shift = 0` if `value` fits in 12 bits
  - otherwise `shift = 1` if `value` fits as `(imm12 << 4)`
- If the value is not representable, the assembler reports an error

Examples:

```asm
AR0 0x123        ; encodes directly
AR0 0x1230       ; auto-encodes as imm12=0x123, shift=1
AR0 0x1230, 1    ; explicit form
```

## Directives

### `.org expr`

Sets the current word address.

```asm
.org 0x100
```

### `.word expr[, expr ...]`

Emits one or more 16-bit words.

Aliases accepted:

- `.word`
- `WORD`
- `DW`

Examples:

```asm
.word 0x1234
.word 1, 2, 3, 4
DW 0xBEEF
```

### `.fill count[, value]`

Emits `count` words. If `value` is omitted, `0x0000` is used.

```asm
.fill 16
.fill 8, 0xFFFF
```

### `.end`

Accepted for readability. It does not emit data and does not affect output.

## Expressions

The assembler accepts simple expressions made of terms combined with `+` and `-`.

A term may be:

- a decimal number
- a hexadecimal number like `0x1234`
- a binary number like `0b10101100`
- a label

Examples:

```asm
.org 0x100
.word table + 3
.word end - start
JMP loop - 2
```

## Example program

```asm
start:
    AR0 0x123
    CPY R1, R0
    ADD R1, ONE

loop:
    SUB.eq R1, ONE
    JMP.ne loop
    DW 0xBEEF
```

## Notes on raw binary layout

Each machine word corresponds to one BackSlash 9 memory address.

So if your program emits `N` words, the output file size is:

```text
2 * N bytes
```

Address `0x0000` is stored at bytes `0..1`, address `0x0001` at bytes `2..3`, and so on.

## Deliberate design decisions

Because the ISA specification reinterprets some invalid generic encodings as other instructions, this assembler chooses the following policy:

1. **Dedicated mnemonics must be written explicitly.**
2. The ambiguous generic invalid forms are rejected with an error.

This keeps the source readable and avoids bugs such as writing:

```asm
CPY R12, R5
```

and unexpectedly getting `SAR R5`.

## Limitations

This implementation intentionally stays small and simple.
It does **not** implement:

- macros
- include files
- string directives
- relocatable object files
- listings / symbol dumps

It produces only a final flat raw binary image.
