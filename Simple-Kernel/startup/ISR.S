//==================================================================================================================================
//  Simple Kernel: Interrupt Handlers
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
// This file provides interrupt handlers for x86-64 CPUs. It is inspired by what Clang/LLVM and GCC use for functions marked with
// __attribute__((interrupt)). Unfortunately they do not both make the same interrupt code (nor do they support the same features for
// interrupt functions, e.g. GCC does not support interrupts with SSE/AVX), so doing it this way ensures the best of both worlds and a
// consistent inter-compiler experience. I have also incorporated a full register dump into interrupt routines, which neither compiler
// does since they only preserve CPU state with the minimum possible set of registers (interesting since the minimum possible methods are
// only about 5 registers short of a full register dump onto the stack).
//
// This file does what GCC does and refers to interrupts without error codes 'interrupts' (designated by 'isr') and ones with error
// codes 'exceptions' (designated by 'exc'). Intel names the first 0-31 entries in the IDT (the predefined entries) exceptions, but names
// 32-255 interrupts. Note that only some of the first 32 entries actually have error codes, meaning most of the first 32 entries will be
// handled by the interrupt code in this file, despite technically being called exceptions. This file has been set up this way such that
// only 2 kinds of functions are needed (interrupt and exception) instead of 4: interrupt, interrupt with error code, exception, and
// exception with error code... And that does not even account for the extra naming complexity brought on by faults, aborts, and traps.
// Using only 2 names instead of all of that, I think, keeps it about as simple as it needs to be. Plus, working in 64-bit mode means
// inter-privilege level changes do not even apply--great since those could easily double the number of functions needed--and that the
// stack frame is auto-aligned by the CPU to 16-bytes since %rsp is pushed unconditionally.
//
// ..Ok, technically there are 4 kinds of interrupt/exception callers here, since each one has an AVX/SSE and non-AVX/SSE version.
//

.extern AVX_ISR_handler
.extern ISR_handler
.extern AVX_EXC_handler
.extern EXC_handler

// Hardware gets their own handlers, like mouse and keyboard
//.extern Mouse_handler
//.extern KB_handler

.section .text

//----------------------------------------------------------------------------------------------------------------------------------
//  SAVE/RESTORE_ISR_REGISTERS: General Register Only ISR Handler
//----------------------------------------------------------------------------------------------------------------------------------
//
// Bog-standard interrupt handler macros with no instruction set extensions
//

.macro SAVE_ISR_REGISTERS offs:req
  pushq %rbp
  leaq \offs(%rsp), %rbp // 40 for interrupt, 56 for exception with error code + isr_num
  pushq %r15
  pushq %r14
  pushq %r13
  pushq %r12
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %rbx
  pushq %rax
  cld
.endm

.macro RESTORE_ISR_REGISTERS
  popq %rax
  popq %rbx
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %r12
  popq %r13
  popq %r14
  popq %r15
  popq %rbp
.endm

//----------------------------------------------------------------------------------------------------------------------------------
//  SAVE/RESTORE_AVX_ISR_REGISTERS: ISR Handler with Instruction Set Extensions
//----------------------------------------------------------------------------------------------------------------------------------
//
// Interrupt handler macros with instruction set extensions (depends on CPU features passed to the compiler)
//

#ifdef __AVX512F__
.macro SAVE_AVX_ISR_REGISTERS offs:req
  pushq %rbp
  leaq \offs(%rsp), %rbp // 40 for interrupt, 56 for exception with error code + isr_num
  pushq %r15
  pushq %r14
  pushq %r13
  pushq %r12
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %rbx
  pushq %rax
#ifdef __AVX512BW__
  subq $2112, %rsp
  kmovq %k7, 2104(%rsp)
  kmovq %k6, 2096(%rsp)
  kmovq %k5, 2088(%rsp)
  kmovq %k4, 2080(%rsp)
  kmovq %k3, 2072(%rsp)
  kmovq %k2, 2064(%rsp)
  kmovq %k1, 2056(%rsp)
  kmovq %k0, 2048(%rsp)
#else // AVX512F
  subq $2064, %rsp
  kmovw %k7, 2062(%rsp)
  kmovw %k6, 2060(%rsp)
  kmovw %k5, 2058(%rsp)
  kmovw %k4, 2056(%rsp)
  kmovw %k3, 2054(%rsp)
  kmovw %k2, 2052(%rsp)
  kmovw %k1, 2050(%rsp)
  kmovw %k0, 2048(%rsp)
#endif
  vmovdqu64 %zmm31, 1984(%rsp)
  vmovdqu64 %zmm30, 1920(%rsp)
  vmovdqu64 %zmm29, 1856(%rsp)
  vmovdqu64 %zmm28, 1792(%rsp)
  vmovdqu64 %zmm27, 1728(%rsp)
  vmovdqu64 %zmm26, 1664(%rsp)
  vmovdqu64 %zmm25, 1600(%rsp)
  vmovdqu64 %zmm24, 1536(%rsp)
  vmovdqu64 %zmm23, 1472(%rsp)
  vmovdqu64 %zmm22, 1408(%rsp)
  vmovdqu64 %zmm21, 1344(%rsp)
  vmovdqu64 %zmm20, 1280(%rsp)
  vmovdqu64 %zmm19, 1216(%rsp)
  vmovdqu64 %zmm18, 1152(%rsp)
  vmovdqu64 %zmm17, 1088(%rsp)
  vmovdqu64 %zmm16, 1024(%rsp)
  vmovdqu64 %zmm15, 960(%rsp)
  vmovdqu64 %zmm14, 896(%rsp)
  vmovdqu64 %zmm13, 832(%rsp)
  vmovdqu64 %zmm12, 768(%rsp)
  vmovdqu64 %zmm11, 704(%rsp)
  vmovdqu64 %zmm10, 640(%rsp)
  vmovdqu64 %zmm9, 576(%rsp)
  vmovdqu64 %zmm8, 512(%rsp)
  vmovdqu64 %zmm7, 448(%rsp)
  vmovdqu64 %zmm6, 384(%rsp)
  vmovdqu64 %zmm5, 320(%rsp)
  vmovdqu64 %zmm4, 256(%rsp)
  vmovdqu64 %zmm3, 192(%rsp)
  vmovdqu64 %zmm2, 128(%rsp)
  vmovdqu64 %zmm1, 64(%rsp)
  vmovdqu64 %zmm0, (%rsp)
  vzeroupper
  cld
.endm

.macro RESTORE_AVX_ISR_REGISTERS
  vmovdqu64 (%rsp), %zmm0
  vmovdqu64 64(%rsp), %zmm1
  vmovdqu64 128(%rsp), %zmm2
  vmovdqu64 192(%rsp), %zmm3
  vmovdqu64 256(%rsp), %zmm4
  vmovdqu64 320(%rsp), %zmm5
  vmovdqu64 384(%rsp), %zmm6
  vmovdqu64 448(%rsp), %zmm7
  vmovdqu64 512(%rsp), %zmm8
  vmovdqu64 576(%rsp), %zmm9
  vmovdqu64 640(%rsp), %zmm10
  vmovdqu64 704(%rsp), %zmm11
  vmovdqu64 768(%rsp), %zmm12
  vmovdqu64 832(%rsp), %zmm13
  vmovdqu64 896(%rsp), %zmm14
  vmovdqu64 960(%rsp), %zmm15
  vmovdqu64 1024(%rsp), %zmm16
  vmovdqu64 1088(%rsp), %zmm17
  vmovdqu64 1152(%rsp), %zmm18
  vmovdqu64 1216(%rsp), %zmm19
  vmovdqu64 1280(%rsp), %zmm20
  vmovdqu64 1344(%rsp), %zmm21
  vmovdqu64 1408(%rsp), %zmm22
  vmovdqu64 1472(%rsp), %zmm23
  vmovdqu64 1536(%rsp), %zmm24
  vmovdqu64 1600(%rsp), %zmm25
  vmovdqu64 1664(%rsp), %zmm26
  vmovdqu64 1728(%rsp), %zmm27
  vmovdqu64 1792(%rsp), %zmm28
  vmovdqu64 1856(%rsp), %zmm29
  vmovdqu64 1920(%rsp), %zmm30
  vmovdqu64 1984(%rsp), %zmm31
#ifdef __AVX512BW__
  kmovw 2048(%rsp), %k0
  kmovw 2056(%rsp), %k1
  kmovw 2064(%rsp), %k2
  kmovw 2072(%rsp), %k3
  kmovw 2080(%rsp), %k4
  kmovw 2088(%rsp), %k5
  kmovw 2096(%rsp), %k6
  kmovw 2104(%rsp), %k7
  addq $2112, %rsp
#else // AVX512F
  kmovw 2048(%rsp), %k0
  kmovw 2050(%rsp), %k1
  kmovw 2052(%rsp), %k2
  kmovw 2054(%rsp), %k3
  kmovw 2056(%rsp), %k4
  kmovw 2058(%rsp), %k5
  kmovw 2060(%rsp), %k6
  kmovw 2062(%rsp), %k7
  addq $2064, %rsp
#endif
  popq %rax
  popq %rbx
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %r12
  popq %r13
  popq %r14
  popq %r15
  popq %rbp
.endm

#elif __AVX__
.macro SAVE_AVX_ISR_REGISTERS offs:req
  pushq %rbp
  leaq \offs(%rsp), %rbp // 40 for interrupt, 56 for exception with error code + isr_num
  pushq %r15
  pushq %r14
  pushq %r13
  pushq %r12
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %rbx
  pushq %rax
  subq $512, %rsp
  vmovdqu %ymm15, 480(%rsp) // AVX wants 32-byte alignment. We can only guarantee 16 without making a mess.
  vmovdqu %ymm14, 448(%rsp)
  vmovdqu %ymm13, 416(%rsp)
  vmovdqu %ymm12, 384(%rsp)
  vmovdqu %ymm11, 352(%rsp)
  vmovdqu %ymm10, 320(%rsp)
  vmovdqu %ymm9, 288(%rsp)
  vmovdqu %ymm8, 256(%rsp)
  vmovdqu %ymm7, 224(%rsp)
  vmovdqu %ymm6, 192(%rsp)
  vmovdqu %ymm5, 160(%rsp)
  vmovdqu %ymm4, 128(%rsp)
  vmovdqu %ymm3, 96(%rsp)
  vmovdqu %ymm2, 64(%rbp)
  vmovdqu %ymm1, 32(%rsp)
  vmovdqu %ymm0, (%rsp)
  vzeroupper
  cld
.endm

.macro RESTORE_AVX_ISR_REGISTERS
  vmovdqu (%rsp), %ymm0
  vmovdqu 32(%rsp), %ymm1
  vmovdqu 64(%rsp), %ymm2
  vmovdqu 96(%rsp), %ymm3
  vmovdqu 128(%rsp), %ymm4
  vmovdqu 160(%rsp), %ymm5
  vmovdqu 192(%rsp), %ymm6
  vmovdqu 224(%rsp), %ymm7
  vmovdqu 256(%rsp), %ymm8
  vmovdqu 288(%rsp), %ymm9
  vmovdqu 320(%rsp), %ymm10
  vmovdqu 352(%rsp), %ymm11
  vmovdqu 384(%rsp), %ymm12
  vmovdqu 416(%rsp), %ymm13
  vmovdqu 448(%rsp), %ymm14
  vmovdqu 480(%rsp), %ymm15
  addq $512, %rsp
  popq %rax
  popq %rbx
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %r12
  popq %r13
  popq %r14
  popq %r15
  popq %rbp
.endm

#else // SSE
.macro SAVE_AVX_ISR_REGISTERS offs:req
  pushq %rbp
  leaq \offs(%rsp), %rbp // 40 for interrupt, 56 for exception with error code + isr_num
  pushq %r15
  pushq %r14
  pushq %r13
  pushq %r12
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %rbx
  pushq %rax
  subq $256, %rsp
  movdqa %xmm15, 240(%rsp)
  movdqa %xmm14, 224(%rsp)
  movdqa %xmm13, 208(%rsp)
  movdqa %xmm12, 192(%rsp)
  movdqa %xmm11, 176(%rsp)
  movdqa %xmm10, 160(%rsp)
  movdqa %xmm9, 144(%rsp)
  movdqa %xmm8, 128(%rsp)
  movdqa %xmm7, 112(%rsp)
  movdqa %xmm6, 96(%rsp)
  movdqa %xmm5, 80(%rsp)
  movdqa %xmm4, 64(%rsp)
  movdqa %xmm3, 48(%rsp)
  movdqa %xmm2, 32(%rsp)
  movdqa %xmm1, 16(%rsp)
  movdqa %xmm0, (%rsp)
  cld
.endm

.macro RESTORE_AVX_ISR_REGISTERS
  movdqa (%rsp), %xmm0
  movdqa 16(%rsp), %xmm1
  movdqa 32(%rsp), %xmm2
  movdqa 48(%rsp), %xmm3
  movdqa 64(%rsp), %xmm4
  movdqa 80(%rsp), %xmm5
  movdqa 96(%rsp), %xmm6
  movdqa 112(%rsp), %xmm7
  movdqa 128(%rsp), %xmm8
  movdqa 144(%rsp), %xmm9
  movdqa 160(%rsp), %xmm10
  movdqa 176(%rsp), %xmm11
  movdqa 192(%rsp), %xmm12
  movdqa 208(%rsp), %xmm13
  movdqa 224(%rsp), %xmm14
  movdqa 240(%rsp), %xmm15
  addq $256, %rsp
  popq %rax
  popq %rbx
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %r12
  popq %r13
  popq %r14
  popq %r15
  popq %rbp
.endm

#endif

//----------------------------------------------------------------------------------------------------------------------------------
//  isr_pusherX: Push Interrupt Number X Onto Stack and Call Handlers
//----------------------------------------------------------------------------------------------------------------------------------
//
// IDT points to these, which push the interrupt number onto the stack and call the relevant handler. Hard to know what interrupt is
// being handled without these.
//

.global isr_pusher0

isr_pusher0:
  SAVE_ISR_REGISTERS 40
  pushq $0
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq ISR_handler
  addq $8, %rsp
  RESTORE_ISR_REGISTERS
  iretq

.global exc_pusher8

exc_pusher8:
  pushq $8
  SAVE_ISR_REGISTERS 56
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq EXC_handler
  RESTORE_ISR_REGISTERS
  addq $8, %rsp
  iretq

.global avx_isr_pusher0

avx_isr_pusher0:
  SAVE_AVX_ISR_REGISTERS 40
  pushq $0 // Push this after AVX for alignment reasons (so that AVX is always at least 16-byte aligned, which means sometimes it will be 32- or 64-byte aligned)
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq AVX_ISR_handler
  addq $8, %rsp
  RESTORE_AVX_ISR_REGISTERS
  iretq

.global avx_exc_pusher8

avx_exc_pusher8:
  pushq $8 // push isr_num here to maintain 16-byte alignment for AVX
  SAVE_AVX_ISR_REGISTERS 56
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq AVX_EXC_handler
  RESTORE_AVX_ISR_REGISTERS
  addq $8, %rsp
  iretq
