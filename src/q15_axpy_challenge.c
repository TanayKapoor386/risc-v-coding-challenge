
// src/q15_axpy_challenge.c
// Single-solution RVV challenge: Q15 y = a + alpha * b  (saturating to Q15)
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


// -------------------- Scalar reference (no intrinsics) --------------------
static inline int16_t sat_q15_scalar(int32_t v) {
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

void q15_axpy_ref(const int16_t *a, const int16_t *b,
                  int16_t *y, int n, int16_t alpha)
{
    for (int i = 0; i < n; ++i) {
        int32_t acc = (int32_t)a[i] + (int32_t)alpha * (int32_t)b[i];
        y[i] = sat_q15_scalar(acc);
    }
}

// -------------------- RVV include per ratified v1.0 spec ------------------
#if __riscv_v_intrinsic >= 1000000
  #include <riscv_vector.h>  // v1.0 test macro & header inclusion
#endif

// -------------------- RVV implementation (mentees edit only here) ---------
/*
Optimize a simple math kernel: y[i] = sat_q15 (a[i] + alpha * b[i])

a,b,y are arrays of 16-bit wide integers, alpha is a 16 bit wide scalar
Pattern = multiply + add across arrays
Q15 = fixed-point format stored in int16_t --> represent numbers in the range [-1, 1) with an int scalar factor
clamp in a 16 bit range [-2^15, 2^15 - 1] = [-32768, 32767] --> saturation to avoid garbage wraparound

Scalar looping does one element per iteration --> inefficient and costs a lot of compute
RVV allows me to do many elements per iteration with vector registers

    1. load chunks of a,b
    2. multiply all b values by alpha in a parallel way
    3. add to a in parallel
    4. saturate in parallel
    5. store entire chunk

    Think about this like multithreaded logic --> do multiple threads at once with different hardware resources
*/
void q15_axpy_rvv(const int16_t *a, const int16_t *b,
                  int16_t *y, int n, int16_t alpha)
{
#if !defined(__riscv) || !defined(__riscv_vector) || (__riscv_v_intrinsic < 1000000)
    // Fallback (keeps correctness off-target)
    q15_axpy_ref(a, b, y, n, alpha);
#else
    // TODO: Enter your solution here

    /*

    e16 --> SEW is 16 bits, since Q15 data is 16-bit sized
    m1 --> LMUL = 1, so baseline vector register grouping is used
    With n - i elements remaining, an appropriate vl to fit the machine is chosen

    
    */  
    size_t i = 0;
    while (i < (size_t) n) {

        // picks vl dynamically
        // this function is important because it makes this loop vector-length agnostic, so this code
        // is portable across different VLEN machines
        size_t vl = vsetvl_e16m1( (size_t) n - i);
        
        // printf("i=%zu vl=%zu\n", i, vl); // for debugging

        // vector loads --> loads vl contiguous 16-bit signed integers from &a[i] into va and &b[i] into vb
        // called a unit-stride contiguous load (fastest, most cache friendly)
        vint16m1_t va = vle16_v_i16m1(a + i, vl);
        vint16m1_t vb = vle16_v_i16m1(b + i, vl);

        // prod[j] = (int32) vb[j] * (int32) alpha --> the widening multiply
        // 32-bit lanes result (LMUL grows from m1 to m2)
        vint32m2_t vprod = vwmul_vx_i32m2(vb, alpha, vl);

        // (int32) va[j] + vprod[j]
        // widening of va, adds with 0, so just widening va
        // then does the vadd
        // just sign extending to 32 bits basically
        vint32m2_t va32  = vwadd_vx_i32m2(va, 0, vl);
        vint32m2_t vacc  = vadd_vv_i32m2(va32, vprod, vl);

        // clamping step
        // vacc is the min of itself and 32767 in case of positive overflow
        // make sure it isn't too negative, so it will either be itself of clamped up to -32768 if too small
        vacc = vmin_vx_i32m2(vacc,  32767, vl);
        vacc = vmax_vx_i32m2(vacc, -32768, vl);

        // narrow back to int16_t
        // vnclip with a 0 shift just narrows to 16 bits and won't change the magnitude since it should already be within 16-bit
        vint16m1_t vy = vnclip_wx_i16m1(vacc, 0, vl);

        // store vl lanes into the result array y[i...i+vl-1]
        vse16_v_i16m1(y + i, vy, vl);
        // advance i by how many elements were processed
        i += vl;
    }
#endif
}

// -------------------- Verification & tiny benchmark -----------------------
static int verify_equal(const int16_t *ref, const int16_t *test, int n, int32_t *max_diff) {
    int ok = 1;
    int32_t md = 0;
    for (int i = 0; i < n; ++i) {
        int32_t d = (int32_t)ref[i] - (int32_t)test[i];
        if (d < 0) d = -d;
        if (d > md) md = d;
        if (d != 0) ok = 0;
    }
    *max_diff = md;
    return ok;
}

#if defined(__riscv)
static inline uint64_t rdcycle(void) { uint64_t c; asm volatile ("rdcycle %0" : "=r"(c)); return c; }
#endif

int main(void) {
    int ok = 1;
    const int N = 4096;
    int16_t *a  = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t *b  = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t *y0 = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t *y1 = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));

    // Deterministic integer data (no libm)
    srand(1234);
    for (int i = 0; i < N; ++i) {
        a[i] = (int16_t)((rand() % 65536) - 32768);
        b[i] = (int16_t)((rand() % 65536) - 32768);
    }

    const int16_t alpha = 3; // example scalar gain

    uint32_t c0 = rdcycle();
    q15_axpy_ref(a, b, y0, N, alpha);
    uint32_t c1 = rdcycle();
    printf("Cycles ref: %u\n", c1 - c0);

    int32_t md = 0;

#if defined(__riscv)
    c0 = rdcycle();
    q15_axpy_rvv(a, b, y1, N, alpha);
    c1 = rdcycle();
    ok = verify_equal(y0, y1, N, &md);
    printf("Verify RVV: %s (max diff = %d)\n", ok ? "OK" : "FAIL", md);
    printf("Cycles RVV: %llu\n", (unsigned long long)(c1 - c0));
#endif

    free(a); free(b); free(y0); free(y1);
    return ok ? 0 : 1;
}

