//==================================================================================================================================
//  Simple Kernel: Interrupt Structures Header
//==================================================================================================================================
//
// Version 0.9
//
// Author:
//  KNNSpeed
//
// Source Code:
//  https://github.com/KNNSpeed/Simple-Kernel
//
// This file provides structures for interrupt handlers, and they reflect the stack as set up in ISR.S.
//
// *How to Add/Modify a User-Defined Interrupt:*
//
// To add an interrupt or exception (here, exception refers to interrupt with error code--not "the first 32 entries in the IDT"):
//
//  1) Ensure the macro used in ISR.S is correct for the desired interrupt number
//  2) Ensure the extern references the correct function at the bottom of this file
//  3) In the Setup_IDT() function in System.c, ensure set_interrupt_entry() is correct for the desired interrupt number (if you want
//     a trap instead, call set_trap_entry() instead of set_interrupt_entry() with the same arguments)
//  4) Add a "case (interrupt number):" statement in the correct handler function in System.c
//
// These are the 4 pathways for handlers, depending on which outcome is desired (just replace "(num)" with a number 32-255, since 0-31 are architecturally reserved):
//
//  a. AVX_ISR_MACRO (num) --> extern void avx_isr_pusher(num) --> set_interrupt_entry( (num), (uint64_t)avx_isr_pusher(num) ) --> "case (num):" in AVX_ISR_handler()
//  b. ISR_MACRO (num) --> extern void isr_pusher(num) --> set_interrupt_entry( (num), (uint64_t)isr_pusher(num) ) --> "case (num):" in ISR_handler()
//  c. AVX_EXC_MACRO (num) --> extern void avx_exc_pusher(num) --> set_interrupt_entry( (num), (uint64_t)avx_exc_pusher(num) ) --> "case (num):" in AVX_EXC_handler()
//  d. EXC_MACRO (num) --> extern void exc_pusher(num) --> set_interrupt_entry( (num), (uint64_t)exc_pusher(num) ) --> "case (num):" in EXC_handler()
//
// Note again that set_interrupt_entry() can be replaced by set_trap_entry() if desired. The difference is that traps don't clear IF in
// %rflags, which allows maskable interrupts to trigger during other interrupts instead of double-faulting.
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

//------------------------------------------//
// References to functions defined in ISR.h //
//------------------------------------------//

// Don't have anything using non-AVX currently
//extern void isr_pusher0();
//extern void exc_pusher8();

//
// Predefined System Interrupts and Exceptions
//

extern void avx_isr_pusher0(); // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
extern void avx_isr_pusher1(); // Fault/Trap #DB: Debug Exception
extern void avx_isr_pusher2(); // NMI (Nonmaskable External Interrupt)
extern void avx_isr_pusher3(); // Trap #BP: Breakpoint (INT3 instruction)
extern void avx_isr_pusher4(); // Trap #OF: Overflow (INTO instruction)
extern void avx_isr_pusher5(); // Fault #BR: BOUND Range Exceeded (BOUND instruction)
extern void avx_isr_pusher6(); // Fault #UD: Invalid or Undefined Opcode
extern void avx_isr_pusher7(); // Fault #NM: Device Not Available Exception

extern void avx_exc_pusher8(); // Abort #DF: Double Fault (error code is always 0)

extern void avx_isr_pusher9(); // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

extern void avx_exc_pusher10(); // Fault #TS: Invalid TSS
extern void avx_exc_pusher11(); // Fault #NP: Segment Not Present
extern void avx_exc_pusher12(); // Fault #SS: Stack Segment Fault
extern void avx_exc_pusher13(); // Fault #GP: General Protection
extern void avx_exc_pusher14(); // Fault #PF: Page Fault

extern void avx_isr_pusher16(); // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

extern void avx_exc_pusher17(); // Fault #AC: Alignment Check (error code is always 0)

extern void avx_isr_pusher18(); // Abort #MC: Machine Check
extern void avx_isr_pusher19(); // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
extern void avx_isr_pusher20(); // Fault #VE: Virtualization Exception

extern void avx_exc_pusher30(); // Fault #SX: Security Exception

//
// These are system reserved, if they trigger they will go to unhandled interrupt error
//

extern void avx_isr_pusher15();

extern void avx_isr_pusher21();
extern void avx_isr_pusher22();
extern void avx_isr_pusher23();
extern void avx_isr_pusher24();
extern void avx_isr_pusher25();
extern void avx_isr_pusher26();
extern void avx_isr_pusher27();
extern void avx_isr_pusher28();
extern void avx_isr_pusher29();

extern void avx_isr_pusher31();

//
// User-Defined Interrupts
//

// Use non-AVX ISR/EXC_MACRO if the interrupts are guaranteed not to touch AVX registers,
// otherwise, or if in any doubt, use AVX_ISR/EXC_MACRO. By default everything is set to AVX_ISR_MACRO.

extern void avx_isr_pusher32();
extern void avx_isr_pusher33();
extern void avx_isr_pusher34();
extern void avx_isr_pusher35();
extern void avx_isr_pusher36();
extern void avx_isr_pusher37();
extern void avx_isr_pusher38();
extern void avx_isr_pusher39();
extern void avx_isr_pusher40();
extern void avx_isr_pusher41();
extern void avx_isr_pusher42();
extern void avx_isr_pusher43();
extern void avx_isr_pusher44();
extern void avx_isr_pusher45();
extern void avx_isr_pusher46();
extern void avx_isr_pusher47();
extern void avx_isr_pusher48();
extern void avx_isr_pusher49();
extern void avx_isr_pusher50();
extern void avx_isr_pusher51();
extern void avx_isr_pusher52();
extern void avx_isr_pusher53();
extern void avx_isr_pusher54();
extern void avx_isr_pusher55();
extern void avx_isr_pusher56();
extern void avx_isr_pusher57();
extern void avx_isr_pusher58();
extern void avx_isr_pusher59();
extern void avx_isr_pusher60();
extern void avx_isr_pusher61();
extern void avx_isr_pusher62();
extern void avx_isr_pusher63();
extern void avx_isr_pusher64();
extern void avx_isr_pusher65();
extern void avx_isr_pusher66();
extern void avx_isr_pusher67();
extern void avx_isr_pusher68();
extern void avx_isr_pusher69();
extern void avx_isr_pusher70();
extern void avx_isr_pusher71();
extern void avx_isr_pusher72();
extern void avx_isr_pusher73();
extern void avx_isr_pusher74();
extern void avx_isr_pusher75();
extern void avx_isr_pusher76();
extern void avx_isr_pusher77();
extern void avx_isr_pusher78();
extern void avx_isr_pusher79();
extern void avx_isr_pusher80();
extern void avx_isr_pusher81();
extern void avx_isr_pusher82();
extern void avx_isr_pusher83();
extern void avx_isr_pusher84();
extern void avx_isr_pusher85();
extern void avx_isr_pusher86();
extern void avx_isr_pusher87();
extern void avx_isr_pusher88();
extern void avx_isr_pusher89();
extern void avx_isr_pusher90();
extern void avx_isr_pusher91();
extern void avx_isr_pusher92();
extern void avx_isr_pusher93();
extern void avx_isr_pusher94();
extern void avx_isr_pusher95();
extern void avx_isr_pusher96();
extern void avx_isr_pusher97();
extern void avx_isr_pusher98();
extern void avx_isr_pusher99();
extern void avx_isr_pusher100();
extern void avx_isr_pusher101();
extern void avx_isr_pusher102();
extern void avx_isr_pusher103();
extern void avx_isr_pusher104();
extern void avx_isr_pusher105();
extern void avx_isr_pusher106();
extern void avx_isr_pusher107();
extern void avx_isr_pusher108();
extern void avx_isr_pusher109();
extern void avx_isr_pusher110();
extern void avx_isr_pusher111();
extern void avx_isr_pusher112();
extern void avx_isr_pusher113();
extern void avx_isr_pusher114();
extern void avx_isr_pusher115();
extern void avx_isr_pusher116();
extern void avx_isr_pusher117();
extern void avx_isr_pusher118();
extern void avx_isr_pusher119();
extern void avx_isr_pusher120();
extern void avx_isr_pusher121();
extern void avx_isr_pusher122();
extern void avx_isr_pusher123();
extern void avx_isr_pusher124();
extern void avx_isr_pusher125();
extern void avx_isr_pusher126();
extern void avx_isr_pusher127();
extern void avx_isr_pusher128();
extern void avx_isr_pusher129();
extern void avx_isr_pusher130();
extern void avx_isr_pusher131();
extern void avx_isr_pusher132();
extern void avx_isr_pusher133();
extern void avx_isr_pusher134();
extern void avx_isr_pusher135();
extern void avx_isr_pusher136();
extern void avx_isr_pusher137();
extern void avx_isr_pusher138();
extern void avx_isr_pusher139();
extern void avx_isr_pusher140();
extern void avx_isr_pusher141();
extern void avx_isr_pusher142();
extern void avx_isr_pusher143();
extern void avx_isr_pusher144();
extern void avx_isr_pusher145();
extern void avx_isr_pusher146();
extern void avx_isr_pusher147();
extern void avx_isr_pusher148();
extern void avx_isr_pusher149();
extern void avx_isr_pusher150();
extern void avx_isr_pusher151();
extern void avx_isr_pusher152();
extern void avx_isr_pusher153();
extern void avx_isr_pusher154();
extern void avx_isr_pusher155();
extern void avx_isr_pusher156();
extern void avx_isr_pusher157();
extern void avx_isr_pusher158();
extern void avx_isr_pusher159();
extern void avx_isr_pusher160();
extern void avx_isr_pusher161();
extern void avx_isr_pusher162();
extern void avx_isr_pusher163();
extern void avx_isr_pusher164();
extern void avx_isr_pusher165();
extern void avx_isr_pusher166();
extern void avx_isr_pusher167();
extern void avx_isr_pusher168();
extern void avx_isr_pusher169();
extern void avx_isr_pusher170();
extern void avx_isr_pusher171();
extern void avx_isr_pusher172();
extern void avx_isr_pusher173();
extern void avx_isr_pusher174();
extern void avx_isr_pusher175();
extern void avx_isr_pusher176();
extern void avx_isr_pusher177();
extern void avx_isr_pusher178();
extern void avx_isr_pusher179();
extern void avx_isr_pusher180();
extern void avx_isr_pusher181();
extern void avx_isr_pusher182();
extern void avx_isr_pusher183();
extern void avx_isr_pusher184();
extern void avx_isr_pusher185();
extern void avx_isr_pusher186();
extern void avx_isr_pusher187();
extern void avx_isr_pusher188();
extern void avx_isr_pusher189();
extern void avx_isr_pusher190();
extern void avx_isr_pusher191();
extern void avx_isr_pusher192();
extern void avx_isr_pusher193();
extern void avx_isr_pusher194();
extern void avx_isr_pusher195();
extern void avx_isr_pusher196();
extern void avx_isr_pusher197();
extern void avx_isr_pusher198();
extern void avx_isr_pusher199();
extern void avx_isr_pusher200();
extern void avx_isr_pusher201();
extern void avx_isr_pusher202();
extern void avx_isr_pusher203();
extern void avx_isr_pusher204();
extern void avx_isr_pusher205();
extern void avx_isr_pusher206();
extern void avx_isr_pusher207();
extern void avx_isr_pusher208();
extern void avx_isr_pusher209();
extern void avx_isr_pusher210();
extern void avx_isr_pusher211();
extern void avx_isr_pusher212();
extern void avx_isr_pusher213();
extern void avx_isr_pusher214();
extern void avx_isr_pusher215();
extern void avx_isr_pusher216();
extern void avx_isr_pusher217();
extern void avx_isr_pusher218();
extern void avx_isr_pusher219();
extern void avx_isr_pusher220();
extern void avx_isr_pusher221();
extern void avx_isr_pusher222();
extern void avx_isr_pusher223();
extern void avx_isr_pusher224();
extern void avx_isr_pusher225();
extern void avx_isr_pusher226();
extern void avx_isr_pusher227();
extern void avx_isr_pusher228();
extern void avx_isr_pusher229();
extern void avx_isr_pusher230();
extern void avx_isr_pusher231();
extern void avx_isr_pusher232();
extern void avx_isr_pusher233();
extern void avx_isr_pusher234();
extern void avx_isr_pusher235();
extern void avx_isr_pusher236();
extern void avx_isr_pusher237();
extern void avx_isr_pusher238();
extern void avx_isr_pusher239();
extern void avx_isr_pusher240();
extern void avx_isr_pusher241();
extern void avx_isr_pusher242();
extern void avx_isr_pusher243();
extern void avx_isr_pusher244();
extern void avx_isr_pusher245();
extern void avx_isr_pusher246();
extern void avx_isr_pusher247();
extern void avx_isr_pusher248();
extern void avx_isr_pusher249();
extern void avx_isr_pusher250();
extern void avx_isr_pusher251();
extern void avx_isr_pusher252();
extern void avx_isr_pusher253();
extern void avx_isr_pusher254();
extern void avx_isr_pusher255();

#endif /* _ISR_H */
