//==================================================================================================================================
//  Simple Kernel: Interrupt Structures Header
//==================================================================================================================================
//
// Version 0.z
//
// Author:
//  KNNSpeed
//
// Source Code:
//  https://github.com/KNNSpeed/Simple-Kernel
//
// This file provides structures for interrupt handlers, and they reflect the stack as set up in ISR.S
//

#ifndef _ISR_H
#define _ISR_H

// Intel Architecture Manual Vol. 3A, Fig. 6-4 (Stack Usage on Transfers to Interrupt and Exception-Handling Routines)
// and Fig. 6-8 (IA-32e Mode Stack Usage After Privilege Level Change)
// Note the order of these structs with respect to the stack. Note that 64-bit pushes SS:RSP unconditionally.
typedef struct __attribute__ ((packed)) {
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} INTERRUPT_FRAME_X64;

// Exception codes are pushed before rip (and so get popped first)
typedef struct __attribute__ ((packed)) {
  UINT64 error_code;
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} EXCEPTION_FRAME_X64;

// The below interrupt frames are set up by ISR.S

// All-in-one structure for interrupts
// ISRs save the state up to where the ISR was called, so a regdump is accessible
// Though it might not always be needed, a minimal ISR is only 5 registers away from a full dump anyways, so might as well just get the whole thing.
typedef struct __attribute__ ((packed)) {
  // ISR identification number pushed by ISR.S
  UINT64 isr_num;

  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} INTERRUPT_FRAME;

// All-in-one structure for exceptions
typedef struct __attribute__ ((packed)) {
  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // ISR identification number pushed by ISR.S
  UINT64 isr_num;
  // Exception error code pushed by CPU
  UINT64 error_code;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} EXCEPTION_FRAME;

// All-in-one structures for AVX interrupts, depending on target CPU
#ifdef __AVX512F__

typedef struct __attribute__ ((packed)) {
  // ISR identification number pushed by ISR.S
  UINT64 isr_num;

  // AVX register save from ISR.S
  UINT64 zmm0[8];
  UINT64 zmm1[8];
  UINT64 zmm2[8];
  UINT64 zmm3[8];
  UINT64 zmm4[8];
  UINT64 zmm5[8];
  UINT64 zmm6[8];
  UINT64 zmm7[8];
  UINT64 zmm8[8];
  UINT64 zmm9[8];
  UINT64 zmm10[8];
  UINT64 zmm11[8];
  UINT64 zmm12[8];
  UINT64 zmm13[8];
  UINT64 zmm14[8];
  UINT64 zmm15[8];
  UINT64 zmm16[8];
  UINT64 zmm17[8];
  UINT64 zmm18[8];
  UINT64 zmm19[8];
  UINT64 zmm20[8];
  UINT64 zmm21[8];
  UINT64 zmm22[8];
  UINT64 zmm23[8];
  UINT64 zmm24[8];
  UINT64 zmm25[8];
  UINT64 zmm26[8];
  UINT64 zmm27[8];
  UINT64 zmm28[8];
  UINT64 zmm29[8];
  UINT64 zmm30[8];
  UINT64 zmm31[8];

#ifdef __AVX512BW__
  UINT64 k0;
  UINT64 k1;
  UINT64 k2;
  UINT64 k3;
  UINT64 k4;
  UINT64 k5;
  UINT64 k6;
  UINT64 k7;
#else
  UINT16 k0;
  UINT16 k1;
  UINT16 k2;
  UINT16 k3;
  UINT16 k4;
  UINT16 k5;
  UINT16 k6;
  UINT16 k7;
#endif

  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} AVX_INTERRUPT_FRAME;

typedef struct __attribute__ ((packed)) {
  // AVX register save from ISR.S
  UINT64 zmm0[8];
  UINT64 zmm1[8];
  UINT64 zmm2[8];
  UINT64 zmm3[8];
  UINT64 zmm4[8];
  UINT64 zmm5[8];
  UINT64 zmm6[8];
  UINT64 zmm7[8];
  UINT64 zmm8[8];
  UINT64 zmm9[8];
  UINT64 zmm10[8];
  UINT64 zmm11[8];
  UINT64 zmm12[8];
  UINT64 zmm13[8];
  UINT64 zmm14[8];
  UINT64 zmm15[8];
  UINT64 zmm16[8];
  UINT64 zmm17[8];
  UINT64 zmm18[8];
  UINT64 zmm19[8];
  UINT64 zmm20[8];
  UINT64 zmm21[8];
  UINT64 zmm22[8];
  UINT64 zmm23[8];
  UINT64 zmm24[8];
  UINT64 zmm25[8];
  UINT64 zmm26[8];
  UINT64 zmm27[8];
  UINT64 zmm28[8];
  UINT64 zmm29[8];
  UINT64 zmm30[8];
  UINT64 zmm31[8];

#ifdef __AVX512BW__
  UINT64 k0;
  UINT64 k1;
  UINT64 k2;
  UINT64 k3;
  UINT64 k4;
  UINT64 k5;
  UINT64 k6;
  UINT64 k7;
#else
  UINT16 k0;
  UINT16 k1;
  UINT16 k2;
  UINT16 k3;
  UINT16 k4;
  UINT16 k5;
  UINT16 k6;
  UINT16 k7;
#endif

  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // ISR identification number pushed by ISR.S
  UINT64 isr_num;
  // Exception error code pushed by CPU
  UINT64 error_code;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} AVX_EXCEPTION_FRAME;

#elif __AVX__

typedef struct __attribute__ ((packed)) {
  // ISR identification number pushed by ISR.S
  UINT64 isr_num;

  // AVX register save from ISR.S
  UINT64 ymm0[4];
  UINT64 ymm1[4];
  UINT64 ymm2[4];
  UINT64 ymm3[4];
  UINT64 ymm4[4];
  UINT64 ymm5[4];
  UINT64 ymm6[4];
  UINT64 ymm7[4];
  UINT64 ymm8[4];
  UINT64 ymm9[4];
  UINT64 ymm10[4];
  UINT64 ymm11[4];
  UINT64 ymm12[4];
  UINT64 ymm13[4];
  UINT64 ymm14[4];
  UINT64 ymm15[4];

  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} AVX_INTERRUPT_FRAME;

typedef struct __attribute__ ((packed)) {
  // AVX register save from ISR.S
  UINT64 ymm0[4];
  UINT64 ymm1[4];
  UINT64 ymm2[4];
  UINT64 ymm3[4];
  UINT64 ymm4[4];
  UINT64 ymm5[4];
  UINT64 ymm6[4];
  UINT64 ymm7[4];
  UINT64 ymm8[4];
  UINT64 ymm9[4];
  UINT64 ymm10[4];
  UINT64 ymm11[4];
  UINT64 ymm12[4];
  UINT64 ymm13[4];
  UINT64 ymm14[4];
  UINT64 ymm15[4];

  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // ISR identification number pushed by ISR.S
  UINT64 isr_num;
  // Exception error code pushed by CPU
  UINT64 error_code;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} AVX_EXCEPTION_FRAME;

#else // SSE

typedef struct __attribute__ ((packed)) {
  // ISR identification number pushed by ISR.S
  UINT64 isr_num;

  // AVX register save from ISR.S
  UINT64 xmm0[2];
  UINT64 xmm1[2];
  UINT64 xmm2[2];
  UINT64 xmm3[2];
  UINT64 xmm4[2];
  UINT64 xmm5[2];
  UINT64 xmm6[2];
  UINT64 xmm7[2];
  UINT64 xmm8[2];
  UINT64 xmm9[2];
  UINT64 xmm10[2];
  UINT64 xmm11[2];
  UINT64 xmm12[2];
  UINT64 xmm13[2];
  UINT64 xmm14[2];
  UINT64 xmm15[2];

  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} AVX_INTERRUPT_FRAME;

typedef struct __attribute__ ((packed)) {
  // AVX register save from ISR.S
  UINT64 xmm0[2];
  UINT64 xmm1[2];
  UINT64 xmm2[2];
  UINT64 xmm3[2];
  UINT64 xmm4[2];
  UINT64 xmm5[2];
  UINT64 xmm6[2];
  UINT64 xmm7[2];
  UINT64 xmm8[2];
  UINT64 xmm9[2];
  UINT64 xmm10[2];
  UINT64 xmm11[2];
  UINT64 xmm12[2];
  UINT64 xmm13[2];
  UINT64 xmm14[2];
  UINT64 xmm15[2];

  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // ISR identification number pushed by ISR.S
  UINT64 isr_num;
  // Exception error code pushed by CPU
  UINT64 error_code;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} AVX_EXCEPTION_FRAME;

#endif

#endif /* _ISR_H */
