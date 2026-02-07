# RISC-V Audiomark – Q15 AXPY (RVV)

This project implements a **Q15 saturating AXPY kernel**  
(`y[i] = sat_q15(a[i] + alpha * b[i])`) and a **RISC-V Vector (RVV)–accelerated version** of the same function.

The goal is to produce a **bit-for-bit identical** vectorized implementation and evaluate its performance.

---

## Why Docker Is Used

This project targets **RISC-V Linux with the RVV (Vector) extension**.

macOS does not natively support:
- RISC-V cross-compilation toolchains
- RVV intrinsics
- RISC-V execution environments

To address this, **Docker** is used to run a lightweight **Ubuntu Linux userspace locally**, where the official RISC-V GNU toolchain and QEMU emulator are installed.

### Key Idea
- The source code lives on macOS  
- Linux tools run inside Docker  
- Files are shared in real time via a mounted directory  

No virtual machine setup is required.

---

## Running the Docker Environment

From the project root directory on macOS, start an Ubuntu container and install all required tools inside it:

```bash
docker run --rm -it -v "$PWD":/work -w /work ubuntu:24.04 bash

apt-get update
apt-get install -y gcc-riscv64-linux-gnu qemu-user
```
This installs:
riscv64-linux-gnu-gcc: RISC-V cross-compiler used to build RV64 + RVV binaries on x86/ARM hosts
qemu-riscv64: User-mode emulation for running RISC-V Linux executables without a full virtual machine

Build and run:

```bash
riscv64-linux-gnu-gcc -O2 -march=rv64gcv -mabi=lp64d -static src/q15_axpy_challenge.c -o q15_axpy
qemu-riscv64 ./q15_axpy
```

---

## RVV Implementation Overview

The scalar reference implementation computes the AXPY operation in 32-bit integer space and then saturates the result to the Q15 range:
```bash

acc = (int32)a[i] + (int32)alpha * (int32)b[i]
y[i] = clamp(acc, -32768, 32767)

```


The RVV implementation performs the exact same computation, but operates on multiple elements at once using the RISC-V Vector Extension. All arithmetic semantics are preserved so that the output is bit-for-bit identical to the scalar reference.

---

## Vector-Length Agnostic Design

The RVV kernel follows the standard vector-length agnostic programming model:

- A vector length (`vl`) is chosen dynamically each iteration based on how many elements remain.
- All vector operations operate on exactly `vl` elements.
- The loop advances by `vl` until all elements are processed.

This design avoids hardcoding a vector width and allows the same binary to run correctly on RISC-V implementations with different vector register sizes (VLEN).

---

## Widening Arithmetic and Saturation

To match scalar semantics exactly:

- Input arrays `a` and `b` are loaded as 16-bit integers.
- Multiplication is performed using a widening multiply, producing 32-bit results.
- Addition is done in 32-bit space to avoid overflow.
- Results are clamped to the Q15 range `[-32768, 32767]`.
- Values are narrowed back to 16-bit integers before storing.

This mirrors the scalar implementation and prevents wraparound or precision loss.

---

## Performance Results (QEMU)

Measured using the `rdcycle` instruction under QEMU user-mode emulation:

```bash
Cycles ref: 177041
Verify RVV: OK (max diff = 0)
Cycles RVV: 70625
```



These results show:

- Correctness verified (bit-for-bit match with the scalar reference)
- Approximately **2.5× speedup** from RVV vectorization

Note: Absolute cycle counts under QEMU are not representative of real hardware performance. However, relative performance trends are still meaningful for evaluating vectorization benefits.

---

## Final Notes

This implementation:

- Is fully compliant with RVV v1.0
- Uses only standard RVV intrinsics
- Preserves exact scalar arithmetic semantics
- Demonstrates practical vectorization of a DSP-style kernel using RISC-V vectors