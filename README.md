# ZonVM — Virtual Machine (v2.4.0) "The Function Update 2.0"

ZonVM is the high-performance execution engine for the Zonetic programming language. It implements a virtual register-based architecture inspired by **RISC-V (RV64I + M Extension + F Extension + D Extension)**, optimized for mathematical precision, robotics simulations, and low-latency control.

## What's New in v2.4.0

- **Function Call Convention:** Full support for `JAL` (Jump and Link) and `JALR` (Jump and Link Register) with proper `ra` (x1) management for nested calls.
- **Dedicated Frame Pointer:** Register `x8` is now exclusively the Frame Pointer (`fp`), no longer shared with `s0`. This provides stable stack frame tracking across function calls.
- **Stack Memory Operations:** Native `SD` (Store Doubleword) and `LD` (Load Doubleword) for integer stack management; `FSD` and `FLD` for floating-point stack management.
- **Register Spilling Support:** Registers `x31` (t6) and `f31` (ft11) are now **reserved system scratch registers**, dedicated to temporary spilling during register pressure. They are never allocated by the register allocator.
- **Stack Overflow Protection:** Hardware-enforced 128KB stack limit with guard pages using `mmap`/`mprotect` (POSIX) or `VirtualAlloc`/`VirtualProtect` (Windows).
- **Tail Call Optimization (TCO):** The compiler now detects tail recursion and converts it to iterative loops, preventing unnecessary stack growth.
- **Double Precision (D Extension):** Full support for 64-bit floating-point operations: `FADD.D`, `FSUB.D`, `FMUL.D`, `FDIV.D`, `FCVT.D.S`, `FCVT.S.D`.

## Architecture Specifications

The virtual machine operates under a **64-bit register model**. Integer registers (`x`) handle 64-bit signed values, while floating-point registers (`f`) handle IEEE 754 double precision.

### Integer Registers (x0 - x31)

| Register | Name | Description |
| :--- | :--- | :--- |
| **x0** | zero | Constant zero, cannot be modified. |
| **x1** | ra | Return Address for function calls (`JAL`/`JALR`). |
| **x2** | sp | Stack Pointer for RAM memory management. |
| **x3** | gp | Global Pointer for `.data` segment access. |
| **x4** | tp | Thread Pointer (reserved for future use). |
| **x5 - x7** | t0 - t2 | Volatile temporaries (caller-saved). |
| **x8** | fp | **Frame Pointer** (dedicated, not shared with s0). |
| **x9** | s1 | Saved register (callee-saved). |
| **x10 - x17** | a0 - a7 | Argument passing and return values (`a7` for syscalls). |
| **x18 - x27** | s2 - s11 | Additional saved registers (callee-saved). |
| **x28 - x30** | t3 - t5 | Volatile temporaries (caller-saved). |
| **x31** | **scratch** | **System scratch register** for spilling (reserved, never allocated). |

### Floating-Point Registers (f0 - f31)

| Register | Name | Description |
| :--- | :--- | :--- |
| **f0 - f7** | ft0 - ft7 | Volatile FP temporaries (caller-saved). |
| **f8 - f9** | fs0 - fs1 | Saved FP registers (callee-saved). |
| **f10 - f11** | fa0 - fa1 | FP Arguments and Return values. |
| **f12 - f17** | fa2 - fa7 | FP Arguments. |
| **f18 - f27** | fs2 - fs11 | Additional saved FP registers (callee-saved). |
| **f28 - f30** | ft8 - ft10 | Volatile FP temporaries (caller-saved). |
| **f31** | **scratch_f** | **System scratch register** for FP spilling (reserved, never allocated). |

## Instruction Set (ISA)

### Memory Operations (Stack & Heap)

| Instruction | Description |
| :--- | :--- |
| **LD** | Load Doubleword (8 bytes) from memory into integer register. |
| **SD** | Store Doubleword (8 bytes) from integer register to memory. |
| **FLD** | Load Doubleword from memory into FP register. |
| **FSD** | Store Doubleword from FP register to memory. |

### Control Flow (Function Calls)

| Instruction | Description |
| :--- | :--- |
| **JAL** | Jump and Link — unconditional jump, stores PC+4 in `rd`. |
| **JALR** | Jump and Link Register — indirect jump via base address in `rs1`. |
| **BEQ/BNE/BLT/BGE/BLTU/BGEU** | Conditional branches for control flow. |

### Integer Arithmetic (RV64I + M Extension)

| Instruction Set | Operations |
| :--- | :--- |
| **ALU (R-Type)** | `ADD`, `SUB`, `AND`, `OR`, `XOR`, `SLT`, `SLTU` |
| **ALU (I-Type)** | `ADDI`, `ANDI`, `ORI`, `XORI`, `SLTI`, `SLTIU` |
| **Shift** | `SLL`, `SRL`, `SRA`, `SLLI`, `SRLI`, `SRAI` |
| **M-Extension** | `MUL`, `DIV`, `REM`, `MULW`, `DIVW`, `REMW` |
| **Upper Immediate** | `LUI`, `AUIPC` |

### Floating-Point (RV32F + RV64D Extensions)

| Category | Operations |
| :--- | :--- |
| **Arithmetic (D)** | `FADD.D`, `FSUB.D`, `FMUL.D`, `FDIV.D` |
| **Arithmetic (S)** | `FADD.S`, `FSUB.S`, `FMUL.S`, `FDIV.S` |
| **Sign Manipulation** | `FSGNJ.D`, `FSGNJN.D`, `FSGNJX.D` |
| **Conversions** | `FCVT.D.S`, `FCVT.S.D`, `FCVT.W.D`, `FCVT.D.W` |
| **Movement** | `FMV.D` (copy between FP regs) |
| **Comparisons** | `FEQ.D`, `FLT.D`, `FLE.D` |

## System Services (ECALL)

System services are invoked by loading the Service ID into register **a7** (x17) and executing `ECALL`.

| Service ID | Name | Description | Input |
| :--- | :--- | :--- | :--- |
| **93** | EXIT | Terminates VM execution with code in `a0`. | `a0` (exit code) |
| **1000** | IPRINT | Prints a 64-bit signed integer. | `a0` (value) |
| **1001** | FPRINT | Prints a double-precision float. | `fa0` (value) |
| **1002** | BPRINT | Prints a boolean (`true`/`false`). | `a0` (0=false, else true) |

## Stack Management & Function Calling

### Function Prologue (Compiler-generated)

```
addi sp, sp, -frame_size    ; allocate stack frame
sd fp, frame_size-8(sp)     ; save old frame pointer
addi fp, sp, frame_size     ; set new frame pointer
sd ra, frame_size-16(sp)    ; save return address (if has calls)
; ... save callee-saved registers (s1-s11, fs0-fs11)
```

### Function Epilogue

```
; ... restore callee-saved registers
ld ra, frame_size-16(sp)    ; restore return address
ld fp, frame_size-8(sp)     ; restore old frame pointer
addi sp, sp, frame_size     ; deallocate stack frame
jalr x0, 0(ra)              ; return to caller
```

### Register Spilling

When register pressure exceeds available registers (7 integer, 12 FP), the compiler spills temporaries to stack slots using the dedicated scratch registers:

- **x31** (integer scratch) for loading/storing integer temporaries
- **f31** (FP scratch) for loading/storing FP temporaries

These registers are **never allocated by the register allocator**, ensuring they are always available for spilling.

## Performance Optimization

1. **Computed Gotos:** The VM uses a dispatch table for O(1) instruction decoding, significantly faster than traditional switch-case blocks.
2. **Zero-Overhead Numbers:** Integers and Floats are handled directly in hardware registers.
3. **C++ Intrinsics:** Operations like `%` (float modulo) and `**` (pow) use `std::fmod` and `std::pow` for maximum speed and IEEE 754 compliance.
4. **Tail Call Optimization:** Recursive tail calls are eliminated at compile time, reducing stack pressure.
5. **Guard Page Protection:** Stack overflow is caught at the hardware level using OS memory protection, not software checks.

**Link to the Zonetic Compiler repository** -> [click here](https://github.com/alve-dev/zonetic-compiler/tree/main)
