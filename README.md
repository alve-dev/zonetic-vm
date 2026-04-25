# ZonVM — Virtual Machine (v2.2.0) "The 64-bit Update"

ZonVM is the high-performance execution engine for the Zonetic programming language. It implements a virtual register-based architecture inspired by **RISC-V (RV64I + M Extension + F Extension)**, optimized for mathematical precision, robotics simulations, and low-latency control.

## What's New in v2.2.0
- **64-bit Core:** Full transition to a 64-bit architecture for integer registers and memory addressing.
- **Floating Point Unit (FPU):** Implementation of the **F Extension** for single-precision (32-bit) floating-point operations, mapped to internal double-precision storage for future compatibility.
- **Hardware-Linked Ops:** Dedicated operators for speed, including power (`**`) and modulo (`%`) leveraging C++ intrinsics.
- **System Calls (ABI):** Standardized `ECALL` services for I/O and execution control.

## Architecture Specifications

The virtual machine operates under a **64-bit register model**. Integer registers (`x`) handle 64-bit signed values, while floating-point registers (`f`) handle IEEE 754 precision.

### Integer Registers (x0 - x31)

|### Registers and ABI

| Register | Name | Description |
| :--- | :--- | :--- |
| **x0** | zero | Constant zero, cannot be modified. |
| **x1** | ra | Return Address for function calls. |
| **x2** | sp | Stack Pointer for RAM memory management. |
| **x3** | gp | Global Pointer for global variables access. |
| **x4** | tp | Thread Pointer for thread-local storage. |
| **x5 - x7** | t0 - t2 | Volatile temporals for intermediate calculations. |
| **x8 - x9** | s0 - s1 | Saved registers for user variables (`inmut`/`mut`). |
| **x10 - x17** | a0 - a7 | Argument passing and return values (`a7` for syscalls). |
| **x18 - x27** | s2 - s11 | Additional saved registers. |
| **x28 - x31** | t3 - t6 | Additional temporal registers. |

### Floating-Point Registers (f0 - f31)

| Register | Name | Description |
| :--- | :--- | :--- |
| **f0 - f7** | ft0 - ft7 | FP Temporaries. |
| **f8 - f9** | fs0 - fs1 | FP Saved registers. |
| **f10 - f11** | fa0 - fa1 | FP Arguments and Return values. |
| **f12 - f17** | fa2 - fa7 | FP Arguments. |
| **f18 - f27** | fs2 - fs11 | FP Additional saved registers. |
| **f28 - f31** | ft8 - ft11 | FP Additional temporaries. |

## Instruction Set (ISA)

### Floating-Point Extension (RV32F)
ZonVM now supports native floating-point arithmetic for robotics:
- **Standard Ops:** `FADD.S`, `FSUB.S`, `FMUL.S`, `FDIV.S`.
- **Sign Manipulation:** `FSGNJ.S` (Move/Copy), `FSGNJN.S` (Negate).
- **Conversions:** `FCVT.W.S` (Float to Int), `FCVT.S.W` (Int to Float).
- **Movement:** `FMV.S` (Move between FP registers), `FMV.W.X` (Int to Float bits).
- **Comparisons:** `FEQ.S` (Equal), `FLT.S` (Less Than), `FLE.S` (Less or Equal).

### Standard Arithmetic & Logic
- **M-Extension:** `MUL`, `DIV`, `REM` (Integer).
- **I-Type / R-Type:** `ADD`, `SUB`, `AND`, `OR`, `XOR`, `SLT`, `SLTU`.
- **Immediate Ops:** `ADDI`, `ANDI`, `ORI`, `XORI`, `SLTI`, `LUI`.

### Flow Control (B-Type & J-Type)
- **Branches:** `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU`.
- **Jumps:** `JAL` (Jump and Link) for function calls and loops.

## System Services (ECALL)

System services are invoked by loading the Service ID into register **a7** (x17) and executing `ECALL`.

| Service ID | Name | Description | Register Input |
| :--- | :--- | :--- | :--- |
| **93** | EXIT | Terminates VM execution. | - |
| **1000** | IPRINT | Prints an integer. | `a0` (x10) |
| **1001** | FPRINT | Prints a float. | `fa0` (f10) |
| **1002** | BPRINT | Prints a boolean (`true`/`false`). | `a0` (x10) |

## Example: Robotics Logic in Zonetic

'''zon
-- Logic for drone stability (v2.2.0)
inmut target_alt: float = 12.5
mut current_alt: float = sensor_read()

if current_alt < target_alt {
    inmut thrust = 1.5 ** 2.0  -- Power operation
    print(thrust)
}
'''

## Performance Optimization

1. **Computed Gotos:** The VM uses a dispatch table for O(1) instruction decoding, significantly faster than traditional switch-case blocks.
2. **Zero-Overhead Numbers:** Integers and Floats are handled directly in hardware registers.
3. **C++ Intrinsics:** Operations like `%` (float modulo) and `**` (pow) use `std::fmod` and `std::pow` for maximum speed and IEEE 754 compliance.

---
**ZonVM v2.2.0** - "Simulating the future, one byte at a time."