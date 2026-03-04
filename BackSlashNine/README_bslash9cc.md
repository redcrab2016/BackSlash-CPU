# BackSlash 9 Tiny C Compiler (`bslash9cc`)

This is a small educational compiler for the synthetic **BackSlash 9** 16-bit CPU.
It compiles a deliberately limited C-like language to:

- **BackSlash 9 assembly** (`-S`)
- **raw binary**, by invoking the BackSlash 9 assembler

This version adds three embedded-style features:

- **fixed-address globals** with `__at(...)`
- **arbitrary 16-bit memory access** with `__load16(...)` and `__store16(...)`
- **startup stack-pointer control** with `--sp=ADDR`

---

## Supported language subset

Supported:

- `int` only
  - all values are treated as **16-bit words**
- function definitions
- up to **4 parameters** per function
- local variables
- statements:
  - block `{ ... }`
  - declaration: `int x;`
  - declaration with initializer: `int x = expr;`
  - assignment: `x = expr;`
  - expression statement: `expr;`
  - `return expr;`
  - `if (...) stmt`
  - `if (...) stmt else stmt`
  - `while (...) stmt`
- expressions:
  - integer literals (decimal and `0x` hexadecimal)
  - variable references
  - function calls
  - unary `+`, `-`, `~`, `!`
  - binary `+`, `-`, `&`, `|`, `^`
  - comparisons `==`, `!=`, `<`, `<=`, `>`, `>=`
  - parentheses

---

## Non-standard extensions added in this version

### 1. Fixed-address global variables

Syntax:

```c
__at(0x2000) int video_reg;
__at(0x2001) int status_reg;
```

Meaning:

- the variable is bound to the exact 16-bit memory address given
- every read or write becomes a memory access at that address

Rules:

- only supported at **top level**
- only supported for type `int`
- initializers are **not** supported

Example:

```c
__at(0x2000) int data_port;
__at(0x2001) int control_port;

int main() {
    data_port = 0x55;
    control_port = data_port + 1;
    return control_port;
}
```

Notes:

- addresses are 16-bit word addresses
- a local variable with the same name as a fixed-address global will shadow it inside that function

---

### 2. Arbitrary memory access intrinsics

Two built-in intrinsics are available.

#### `__load16(address)`

Reads one 16-bit word from memory.

```c
int x;
x = __load16(0x2000);
```

The address is a normal expression and is evaluated at run time.

#### `__store16(address, value)`

Writes one 16-bit word to memory.

```c
__store16(0x2001, 123);
```

The address and value are both normal expressions and are evaluated at run time.

`__store16(...)` also leaves the stored value in the result register, so it can be used in an expression if desired:

```c
int x;
x = __store16(0x2002, 7);
```

Example:

```c
int main() {
    __store16(0x2100, 0x1234);
    return __load16(0x2100);
}
```

These intrinsics are the recommended way to access arbitrary memory in this tiny compiler, because the compiler still does **not** implement pointers.

---

### 3. Configurable startup stack pointer

The startup code initializes `R10` (the stack pointer) before calling `main`.

Default:

- `R10 = 0xFFFE`

You can override it with:

```bash
./bslash9cc --sp=0xE000 input.c output.bin
./bslash9cc -S --sp=0x8000 input.c output.asm
```

The value may be written in decimal, hexadecimal, or any format accepted by `strtoul(..., 0)`.
The final value is truncated to 16 bits.

This is a **compiler option**, not a source-language construct.

---

## What is still not supported

Still not supported:

- pointers
- arrays
- address-of (`&x`) and dereference (`*p`)
- global variables other than `__at(...) int name;`
- string literals
- `for`, `do`, `switch`, `break`, `continue`, `goto`
- `*`, `/`, `%`
- `&&`, `||`
- preprocessor handling (`#include`, `#define`, etc.)
- varargs
- more than 4 function arguments

---

## ABI used by the compiler

The compiler uses this simple ABI:

- `R0` : return value
- `R1`..`R4` : first 4 function arguments
- `R9` : frame pointer
- `R10` : stack pointer
- `R11` : link / saved return address
- stack grows **downward**

### Register convention

- caller-saved: `R0`..`R8`, `R11`
- callee-saved: `R9`
- `R10` must be restored by the callee before returning

### Call sequence

A function call is implemented as an absolute branch using `CPY PC, R0` after loading the callee address into `R0`.
Because `CPY` to `PC` saves the old `PC` into `R11`, `R11` acts as a link register.

### Function prologue

Each function:

1. pushes old `R9`
2. pushes incoming `R11`
3. sets `R9 = R10`
4. allocates stack slots for parameters and locals
5. spills incoming argument registers into stack slots

### Function epilogue

Each function:

1. restores `R10` from `R9`
2. restores saved `R11`
3. restores saved `R9`
4. returns with `CPY PC, R11`

### Runtime start

The generated program begins at address `0` with a tiny startup stub that:

1. initializes `SP` (`R10`) to `0xFFFE`, or to the value passed with `--sp=...`
2. calls `main`
3. halts by setting `PC = 0xFFFF`

---

## Build

```bash
gcc -std=c11 -Wall -Wextra -O2 bslash9cc.c -o bslash9cc
```

---

## Command line

```bash
bslash9cc [-S] [--le|--be] [--sp=0xNNNN] input.c output
```

Options:

- `-S` : write BackSlash 9 assembly instead of raw binary
- `--le` : little-endian raw binary output (default)
- `--be` : big-endian raw binary output
- `--sp=ADDR` : initialize `R10`/`SP` to `ADDR` in the startup stub

### Binary output dependency

For raw binary output, `bslash9cc` writes temporary assembly and then invokes the assembler.

It looks for the assembler in this order:

1. environment variable `BSLASH9ASM`
2. executable name `bslash9asm` in `PATH`

Examples:

```bash
./bslash9cc -S program.c program.asm
BSLASH9ASM=./bslash9asm ./bslash9cc program.c program.bin
BSLASH9ASM=./bslash9asm ./bslash9cc --sp=0xE000 program.c program.bin
```

---

## Examples

### Fixed-address variables

```c
__at(0x2000) int data;
__at(0x2001) int ctrl;

int main() {
    data = 0x0041;
    ctrl = data + 1;
    return ctrl;
}
```

### Arbitrary memory access

```c
int main() {
    __store16(0x2100, 0x1234);
    return __load16(0x2100);
}
```

### Mixed use

```c
__at(0x2000) int reg;

int main() {
    reg = 5;
    __store16(0x2001, reg + 1);
    return __load16(0x2001);
}
```

---

## Notes on generated assembly

The generated assembly is intentionally low level and verbose.
It is designed to be re-assembled reliably and to follow the chosen ABI, not to be optimized.

Typical patterns:

- constants are built in `R0` with `AR0` plus `OR`
- locals live in stack slots addressed from `R9`
- fixed-address globals are accessed by materializing their address into a register and using `CPY R0, [R8]` or `CPY [R8], R0`
- `__load16` and `__store16` evaluate addresses dynamically at run time
- control flow often uses absolute branches synthesized from constant loads plus `CPY PC, R0`

---

## Practical limitations

This compiler is deliberately small.

Practical consequences:

- code size is not optimized
- diagnostics are basic
- fixed-address globals have no initializer syntax
- arbitrary memory access is available only through `__load16` and `__store16`
- if you need full C pointer semantics, this compiler must be extended further

---

## Files

- `bslash9cc.c` : compiler source code
- `README_bslash9cc.md` : this document
