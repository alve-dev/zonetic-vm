# ZonVM — Virtual Machine

ZonVM is the high-performance execution engine for the Zonetic programming language. It implements a virtual register-based architecture inspired by RISC-V (RV32I + M Extension), optimized for precise mathematical calculations and robotics simulations.

## Architecture Specifications

The virtual machine operates under a 64-bit register model (double precision) to ensure accuracy in physical and system-level calculations.

### Registers and ABI

| Register | Name | Description |
| :--- | :--- | :--- |
| **x0** | zero | Constant zero, cannot be modified. |
| **x1** | ra | Return Address for function calls. |
| **x2** | sp | Stack Pointer for RAM memory management. |
| **x3** | gp | Global Pointer |
| **x4** | tp | Thread Pointer |
| **x5 - x7** | t0 - t2 | Volatile temporals for intermediate calculations. |
| **x8 - x9** | s0 - s1 | Saved registers for user variables (mut/inmut). |
| **x10 - x17** | a0 - a7 | Argument passing and return values. |
| **x18 - x27** | s2 - s11 | Additional saved registers. |
| **x28 - x31** | t3 - t6 | Additional temporals. |

### Instruction Set (v2.0.0)

ZonVM processes 32-bit instructions using the following base formats:

- **I-Type (Immediates):** Instructions operating with 12-bit sign-extended constants.
  - `ADDI`: Adds an immediate to a register. Also used for `LI` (Load Immediate) and `MV` (Move).
- **R-Type (Registers):** Arithmetic operations between two registers.
  - `ADD`, `SUB`: Basic addition and subtraction.
  - `MUL`, `DIV`, `REM`: Multiplication, division, and remainder (M-Extension).
- **Pseudo-instructions:**
  - `NEG`: Internally implemented as `SUB rd, x0, rs2`.

## Execution Pipeline

The life cycle of an instruction in ZonVM follows the standard computing model:
1. **Fetch:** Reads the 32-bit instruction from the bytecode stream.
2. **Decode:** Extracts Opcode, rd, rs1, rs2, funct3, and funct7 fields.
3. **Execute:** Performs the arithmetic or logical operation.
4. **Write-back:** Updates the destination register.

## File Format (.zbc)

The VM only executes valid Zonetic bytecode files meeting the following structure:

- **Magic Number:** `0x5A4F4E21` ("ZON!").
- **Code Segment:** Binary stream of instructions encoded in Little Endian.

## Installation and Execution

The VM is designed to be managed by the `zon_launcher`. The launcher automatically detects if the binary needs to be built or updated.

```bash
# The launcher handles compilation transparently
zon r my_program.zon
```

### Dependencies
To ensure cross-platform consistency, Zonetic uses the GNU Compiler Collection (GCC).

- **Windows:** `MinGW-w64` (g++) with C++20 support.
- **Linux / Termux:** `g++` with C++20 support.