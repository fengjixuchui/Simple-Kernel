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
// These are the 3 pathways for handlers, depending on which outcome is desired (just replace "(num)" with a number 32-255, since 0-31 are architecturally reserved):
//
// For user-defined notifications:
//  a. USER_ISR_MACRO (num) --> extern void user_isr_pusher(num) --> set_interrupt_entry( (num), (uint64_t)user_isr_pusher(num) ) --> "case (num):" in User_ISR_handler()
//
// For CPU architectural notifications:
//  b. CPU_ISR_MACRO (num) --> extern void cpu_isr_pusher(num) --> set_interrupt_entry( (num), (uint64_t)cpu_isr_pusher(num) ) --> "case (num):" in CPU_ISR_handler()
//  c. CPU_EXC_MACRO (num) --> extern void cpu_exc_pusher(num) --> set_interrupt_entry( (num), (uint64_t)cpu_exc_pusher(num) ) --> "case (num):" in CPU_EXC_handler()
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

//
// Using XSAVE & XRSTOR:
//
// Per 13.7, XSAVE is invoked like this:
// XSAVE (address of first byte of XSAVE area)
// EAX and EDX should correspond to 0xE7 to save AVX512, AVX, SSE, x87
// EDX:EAX is an AND mask for XCR0.
//
// Per 13.8, XRSTOR is invoked in exactly the same way.
//
// Intel Architecture Manual Vol. 1, Section 13.4 (XSAVE Area)
typedef struct __attribute__((aligned(64), packed)) {
// Legacy region (first 512 bytes)
  // Legacy FXSAVE header
  UINT16 fcw;
  UINT16 fsw;
  UINT8 ftw;
  UINT8 Reserved1;
  UINT16 fop;
  UINT64 fip; // FCS is only for 32-bit
  UINT64 fdp; // FDS is only for 32-bit
  UINT32 mxcsr;
  UINT32 mxcsr_mask;

  // Legacy x87/MMX registers
  UINT64 st_mm_0[2];
  UINT64 st_mm_1[2];
  UINT64 st_mm_2[2];
  UINT64 st_mm_3[2];
  UINT64 st_mm_4[2];
  UINT64 st_mm_5[2];
  UINT64 st_mm_6[2];
  UINT64 st_mm_7[2];

  // SSE registers
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

  UINT8 Reserved2[48];// (463:416) are reserved.
  UINT8 pad[48]; // XSAVE doesn't use (511:464).

// AVX region
  // XSAVE header
  UINT64 xstate_bv; // CPU uses this to track what is saved in XSAVE area--init to 0 with the rest of the region and then don't modify it after.
  UINT64 xcomp_bv; // Only support for xcomp_bv = 0 is expressly provided here (it's the standard form of XSAVE/XRSTOR that all AVX CPUs must support)

  UINT64 Reserved3[6];

  // XSAVE Extended region
  // Only standard format support is provided here
  UINT8 extended_region[1]; // This depends on the values in EBX & EAX after cpuid EAX=0Dh, ECX=[state comp]

} XSAVE_AREA_LAYOUT;
// Note that XSAVES/XRSTORS, used for supervisor components, only includes Process Trace in addition to the standard "user" XSAVE features at the moment.
// The standard user XSAVE states include x87/SSE/AVX/AVX-512 and MPX/PKRU.

//------------------------------------------//
// References to functions defined in ISR.h //
//------------------------------------------//

//
// Predefined System Interrupts and Exceptions
//

extern void cpu_isr_pusher0(); // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
extern void cpu_isr_pusher1(); // Fault/Trap #DB: Debug Exception
extern void cpu_isr_pusher2(); // NMI (Nonmaskable External Interrupt)
extern void cpu_isr_pusher3(); // Trap #BP: Breakpoint (INT3 instruction)
extern void cpu_isr_pusher4(); // Trap #OF: Overflow (INTO instruction)
extern void cpu_isr_pusher5(); // Fault #BR: BOUND Range Exceeded (BOUND instruction)
extern void cpu_isr_pusher6(); // Fault #UD: Invalid or Undefined Opcode
extern void cpu_isr_pusher7(); // Fault #NM: Device Not Available Exception

extern void cpu_exc_pusher8(); // Abort #DF: Double Fault (error code is always 0)

extern void cpu_isr_pusher9(); // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

extern void cpu_exc_pusher10(); // Fault #TS: Invalid TSS
extern void cpu_exc_pusher11(); // Fault #NP: Segment Not Present
extern void cpu_exc_pusher12(); // Fault #SS: Stack Segment Fault
extern void cpu_exc_pusher13(); // Fault #GP: General Protection
extern void cpu_exc_pusher14(); // Fault #PF: Page Fault

extern void cpu_isr_pusher16(); // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

extern void cpu_exc_pusher17(); // Fault #AC: Alignment Check (error code is always 0)

extern void cpu_isr_pusher18(); // Abort #MC: Machine Check
extern void cpu_isr_pusher19(); // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
extern void cpu_isr_pusher20(); // Fault #VE: Virtualization Exception

extern void cpu_exc_pusher30(); // Fault #SX: Security Exception

//
// These are system reserved, if they trigger they will go to unhandled interrupt error
//

extern void cpu_isr_pusher15();

extern void cpu_isr_pusher21();
extern void cpu_isr_pusher22();
extern void cpu_isr_pusher23();
extern void cpu_isr_pusher24();
extern void cpu_isr_pusher25();
extern void cpu_isr_pusher26();
extern void cpu_isr_pusher27();
extern void cpu_isr_pusher28();
extern void cpu_isr_pusher29();

extern void cpu_isr_pusher31();

//
// User-Defined Interrupts
//

// By default everything is set to USER_ISR_MACRO.

extern void user_isr_pusher32();
extern void user_isr_pusher33();
extern void user_isr_pusher34();
extern void user_isr_pusher35();
extern void user_isr_pusher36();
extern void user_isr_pusher37();
extern void user_isr_pusher38();
extern void user_isr_pusher39();
extern void user_isr_pusher40();
extern void user_isr_pusher41();
extern void user_isr_pusher42();
extern void user_isr_pusher43();
extern void user_isr_pusher44();
extern void user_isr_pusher45();
extern void user_isr_pusher46();
extern void user_isr_pusher47();
extern void user_isr_pusher48();
extern void user_isr_pusher49();
extern void user_isr_pusher50();
extern void user_isr_pusher51();
extern void user_isr_pusher52();
extern void user_isr_pusher53();
extern void user_isr_pusher54();
extern void user_isr_pusher55();
extern void user_isr_pusher56();
extern void user_isr_pusher57();
extern void user_isr_pusher58();
extern void user_isr_pusher59();
extern void user_isr_pusher60();
extern void user_isr_pusher61();
extern void user_isr_pusher62();
extern void user_isr_pusher63();
extern void user_isr_pusher64();
extern void user_isr_pusher65();
extern void user_isr_pusher66();
extern void user_isr_pusher67();
extern void user_isr_pusher68();
extern void user_isr_pusher69();
extern void user_isr_pusher70();
extern void user_isr_pusher71();
extern void user_isr_pusher72();
extern void user_isr_pusher73();
extern void user_isr_pusher74();
extern void user_isr_pusher75();
extern void user_isr_pusher76();
extern void user_isr_pusher77();
extern void user_isr_pusher78();
extern void user_isr_pusher79();
extern void user_isr_pusher80();
extern void user_isr_pusher81();
extern void user_isr_pusher82();
extern void user_isr_pusher83();
extern void user_isr_pusher84();
extern void user_isr_pusher85();
extern void user_isr_pusher86();
extern void user_isr_pusher87();
extern void user_isr_pusher88();
extern void user_isr_pusher89();
extern void user_isr_pusher90();
extern void user_isr_pusher91();
extern void user_isr_pusher92();
extern void user_isr_pusher93();
extern void user_isr_pusher94();
extern void user_isr_pusher95();
extern void user_isr_pusher96();
extern void user_isr_pusher97();
extern void user_isr_pusher98();
extern void user_isr_pusher99();
extern void user_isr_pusher100();
extern void user_isr_pusher101();
extern void user_isr_pusher102();
extern void user_isr_pusher103();
extern void user_isr_pusher104();
extern void user_isr_pusher105();
extern void user_isr_pusher106();
extern void user_isr_pusher107();
extern void user_isr_pusher108();
extern void user_isr_pusher109();
extern void user_isr_pusher110();
extern void user_isr_pusher111();
extern void user_isr_pusher112();
extern void user_isr_pusher113();
extern void user_isr_pusher114();
extern void user_isr_pusher115();
extern void user_isr_pusher116();
extern void user_isr_pusher117();
extern void user_isr_pusher118();
extern void user_isr_pusher119();
extern void user_isr_pusher120();
extern void user_isr_pusher121();
extern void user_isr_pusher122();
extern void user_isr_pusher123();
extern void user_isr_pusher124();
extern void user_isr_pusher125();
extern void user_isr_pusher126();
extern void user_isr_pusher127();
extern void user_isr_pusher128();
extern void user_isr_pusher129();
extern void user_isr_pusher130();
extern void user_isr_pusher131();
extern void user_isr_pusher132();
extern void user_isr_pusher133();
extern void user_isr_pusher134();
extern void user_isr_pusher135();
extern void user_isr_pusher136();
extern void user_isr_pusher137();
extern void user_isr_pusher138();
extern void user_isr_pusher139();
extern void user_isr_pusher140();
extern void user_isr_pusher141();
extern void user_isr_pusher142();
extern void user_isr_pusher143();
extern void user_isr_pusher144();
extern void user_isr_pusher145();
extern void user_isr_pusher146();
extern void user_isr_pusher147();
extern void user_isr_pusher148();
extern void user_isr_pusher149();
extern void user_isr_pusher150();
extern void user_isr_pusher151();
extern void user_isr_pusher152();
extern void user_isr_pusher153();
extern void user_isr_pusher154();
extern void user_isr_pusher155();
extern void user_isr_pusher156();
extern void user_isr_pusher157();
extern void user_isr_pusher158();
extern void user_isr_pusher159();
extern void user_isr_pusher160();
extern void user_isr_pusher161();
extern void user_isr_pusher162();
extern void user_isr_pusher163();
extern void user_isr_pusher164();
extern void user_isr_pusher165();
extern void user_isr_pusher166();
extern void user_isr_pusher167();
extern void user_isr_pusher168();
extern void user_isr_pusher169();
extern void user_isr_pusher170();
extern void user_isr_pusher171();
extern void user_isr_pusher172();
extern void user_isr_pusher173();
extern void user_isr_pusher174();
extern void user_isr_pusher175();
extern void user_isr_pusher176();
extern void user_isr_pusher177();
extern void user_isr_pusher178();
extern void user_isr_pusher179();
extern void user_isr_pusher180();
extern void user_isr_pusher181();
extern void user_isr_pusher182();
extern void user_isr_pusher183();
extern void user_isr_pusher184();
extern void user_isr_pusher185();
extern void user_isr_pusher186();
extern void user_isr_pusher187();
extern void user_isr_pusher188();
extern void user_isr_pusher189();
extern void user_isr_pusher190();
extern void user_isr_pusher191();
extern void user_isr_pusher192();
extern void user_isr_pusher193();
extern void user_isr_pusher194();
extern void user_isr_pusher195();
extern void user_isr_pusher196();
extern void user_isr_pusher197();
extern void user_isr_pusher198();
extern void user_isr_pusher199();
extern void user_isr_pusher200();
extern void user_isr_pusher201();
extern void user_isr_pusher202();
extern void user_isr_pusher203();
extern void user_isr_pusher204();
extern void user_isr_pusher205();
extern void user_isr_pusher206();
extern void user_isr_pusher207();
extern void user_isr_pusher208();
extern void user_isr_pusher209();
extern void user_isr_pusher210();
extern void user_isr_pusher211();
extern void user_isr_pusher212();
extern void user_isr_pusher213();
extern void user_isr_pusher214();
extern void user_isr_pusher215();
extern void user_isr_pusher216();
extern void user_isr_pusher217();
extern void user_isr_pusher218();
extern void user_isr_pusher219();
extern void user_isr_pusher220();
extern void user_isr_pusher221();
extern void user_isr_pusher222();
extern void user_isr_pusher223();
extern void user_isr_pusher224();
extern void user_isr_pusher225();
extern void user_isr_pusher226();
extern void user_isr_pusher227();
extern void user_isr_pusher228();
extern void user_isr_pusher229();
extern void user_isr_pusher230();
extern void user_isr_pusher231();
extern void user_isr_pusher232();
extern void user_isr_pusher233();
extern void user_isr_pusher234();
extern void user_isr_pusher235();
extern void user_isr_pusher236();
extern void user_isr_pusher237();
extern void user_isr_pusher238();
extern void user_isr_pusher239();
extern void user_isr_pusher240();
extern void user_isr_pusher241();
extern void user_isr_pusher242();
extern void user_isr_pusher243();
extern void user_isr_pusher244();
extern void user_isr_pusher245();
extern void user_isr_pusher246();
extern void user_isr_pusher247();
extern void user_isr_pusher248();
extern void user_isr_pusher249();
extern void user_isr_pusher250();
extern void user_isr_pusher251();
extern void user_isr_pusher252();
extern void user_isr_pusher253();
extern void user_isr_pusher254();
extern void user_isr_pusher255();

#endif /* _ISR_H */
