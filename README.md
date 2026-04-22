# ZonVM — Virtual Machine (v2.1.0)

ZonVM is the high-performance execution engine for the Zonetic programming language. It implements a virtual register-based architecture inspired by **RISC-V (RV32I + M Extension)**, optimized for mathematical precision, control logic, and robotics simulations.

## What's New in v2.1.0
- **Flow Control:** Full implementation of conditional branches (B-Type) for `if`, `elif`, and `else` structures.
- **Boolean Logic:** Support for bitwise operations and comparisons (`AND`, `OR`, `XOR`, `SLT`).
- **Scope Management:** Support for `BlockExpr` and dynamic register cleanup via scoped symbol tables.

## Architecture Specifications

The virtual machine operates under a **64-bit register model** (double precision) to ensure accuracy in physical and system-level calculations.

### Registers and ABI

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

### Instruction Set (ISA)

ZonVM processes 32-bit instructions using the following base formats:

#### R-Type (Register-Register)
Arithmetic and logical operations between registers.
- **Arithmetic:** `ADD`, `SUB`, `MUL`, `DIV`, `REM`.
- **Logic:** `AND`, `OR`, `XOR`.
- **Comparison:** `SLT` (Set Less Than), `SLTU` (Unsigned).

#### I-Type (Immediates)
Operations with 12-bit sign-extended constants.
- **Arithmetic:** `ADDI` (used for `LI` and `MV`).
- **Logic:** `ANDI`, `ORI`, `XORI`.
- **Comparison:** `SLTI`, `SLTIU`.

#### B-Type (Conditional Branches)
Enables code branching based on register comparisons.
- **Equality:** `BEQ` (Equal), `BNE` (Not Equal).
- **Magnitude:** `BLT` (Less Than), `BGE` (Greater or Equal).
- **Unsigned Magnitude:** `BLTU`, `BGEU`.

#### System
- **ECALL:** Invokes system services (e.g., `print`) depending on the value in register `a7`.

## Control Structures and Scopes

Version 2.1.0 introduces the capability to handle complex code blocks:

1. **Short-Circuit Evaluation:** The Emitter generates direct jumps to optimize `and` and `or` operations, preventing unnecessary executions.
2. **Jump Calculation:** Branch offsets in B-Type instructions are calculated during compile-time (2-pass assembly) to target precise `Labels`.
3. **Block Context:** Local scope support; variables defined within an `if` or `elif` block are managed by releasing temporal registers upon exiting the context.

'''zon
-- Example of v2.1.0 logic
inmut limit = 100
if sensor < limit and active {
    print(1)
} else {
    print(0)
}
'''

## File Format (.zbc)

The VM only executes valid Zonetic bytecode files meeting the following structure:

- **Magic Number:** `0x5A4F4E21` ("ZON!").
- **Code Segment:** Binary stream of instructions encoded in **Little Endian**.