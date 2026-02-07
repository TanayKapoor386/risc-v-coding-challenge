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
