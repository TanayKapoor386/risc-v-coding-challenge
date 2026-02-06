# RISC-V Audiomark – Q15 AXPY (RVV)

This project implements a **Q15 saturating AXPY kernel** (`y[i] = sat_q15(a[i] + alpha * b[i])`) and a **RISC-V Vector (RVV)–accelerated version** of the same function.  
The goal is to produce a **bit-for-bit identical** vectorized implementation and evaluate its performance.

---

## Why Docker is used

This project targets **RISC-V Linux with the RVV (Vector) extension**.

Because macOS does not natively support:
- RISC-V cross-compilation toolchains
- RVV intrinsics
- RISC-V execution environments

we use **Docker** to run a lightweight **Ubuntu Linux userspace locally** and install the official RISC-V GNU toolchain and QEMU emulator there.

Key idea:
- The source code lives on macOS
- Linux tools run inside Docker
- Files are shared in real time via a mounted directory

No virtual machine setup is required.

---

## Running the Docker environment

From the project root directory on macOS:

```bash
docker run --rm -it -v "$PWD":/work -w /work ubuntu:24.04 bash
