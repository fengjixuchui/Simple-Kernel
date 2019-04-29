//==================================================================================================================================
//  Simple Kernel: Interrupt Handlers
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
// This file provides interrupt handlers for x86-64 CPUs.
//

.extern AVX_ISR_handler
.extern ISR_handler
.extern AVX_EXC_handler
.extern EXC_handler

.global avx_isr_caller
.global isr_caller
.global avx_exc_caller
.global exc_caller

.align 8

.section .text

//----------------------------------------------------------------------------------------------------------------------------------
//  avx_isr_caller: ISR Handler with Instruction Set Extensions
//----------------------------------------------------------------------------------------------------------------------------------
//
// Interrupt handler with instruction set extensions (depends on CPU features passed to compiler)
//

#ifdef __AVX512F__
avx_isr_caller:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rax
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  andq $-64, %rsp
  subq $2240, %rsp # imm = 0x8C0
  kmovw %k7, -74(%rbp) # 2-byte Spill
  kmovw %k6, -76(%rbp) # 2-byte Spill
  kmovw %k5, -78(%rbp) # 2-byte Spill
  kmovw %k4, -80(%rbp) # 2-byte Spill
  kmovw %k3, -82(%rbp) # 2-byte Spill
  kmovw %k2, -84(%rbp) # 2-byte Spill
  kmovw %k1, -86(%rbp) # 2-byte Spill
  kmovw %k0, -88(%rbp) # 2-byte Spill
  vmovaps %zmm31, -192(%rbp) # 64-byte Spill
  vmovaps %zmm30, -272(%rbp) # 64-byte Spill
  vmovaps %zmm29, -368(%rbp) # 64-byte Spill
  vmovaps %zmm28, -432(%rbp) # 64-byte Spill
  vmovaps %zmm27, -496(%rbp) # 64-byte Spill
  vmovaps %zmm26, -560(%rbp) # 64-byte Spill
  vmovaps %zmm25, -624(%rbp) # 64-byte Spill
  vmovaps %zmm24, -688(%rbp) # 64-byte Spill
  vmovaps %zmm23, -752(%rbp) # 64-byte Spill
  vmovaps %zmm22, -816(%rbp) # 64-byte Spill
  vmovaps %zmm21, -880(%rbp) # 64-byte Spill
  vmovaps %zmm20, -944(%rbp) # 64-byte Spill
  vmovaps %zmm19, -1008(%rbp) # 64-byte Spill
  vmovaps %zmm18, -1072(%rbp) # 64-byte Spill
  vmovaps %zmm17, -1136(%rbp) # 64-byte Spill
  vmovaps %zmm16, -1200(%rbp) # 64-byte Spill
  vmovaps %zmm15, -1264(%rbp) # 64-byte Spill
  vmovaps %zmm14, -1328(%rbp) # 64-byte Spill
  vmovaps %zmm13, -1392(%rbp) # 64-byte Spill
  vmovaps %zmm12, -1456(%rbp) # 64-byte Spill
  vmovaps %zmm11, -1520(%rbp) # 64-byte Spill
  vmovaps %zmm10, -1584(%rbp) # 64-byte Spill
  vmovaps %zmm9, -1648(%rbp) # 64-byte Spill
  vmovaps %zmm8, -1712(%rbp) # 64-byte Spill
  vmovaps %zmm7, -1776(%rbp) # 64-byte Spill
  vmovaps %zmm6, -1840(%rbp) # 64-byte Spill
  vmovaps %zmm5, -1904(%rbp) # 64-byte Spill
  vmovaps %zmm4, -1968(%rbp) # 64-byte Spill
  vmovaps %zmm3, -2032(%rbp) # 64-byte Spill
  vmovaps %zmm2, -2096(%rbp) # 64-byte Spill
  vmovaps %zmm1, -2160(%rbp) # 64-byte Spill
  vmovaps %zmm0, -2224(%rbp) # 64-byte Spill
  cld
  leaq 8(%rbp), %rax
  movq %rax, 56(%rsp)
  vzeroupper
  callq AVX_ISR_handler
  vmovaps -2224(%rbp), %zmm0 # 64-byte Reload
  vmovaps -2160(%rbp), %zmm1 # 64-byte Reload
  vmovaps -2096(%rbp), %zmm2 # 64-byte Reload
  vmovaps -2032(%rbp), %zmm3 # 64-byte Reload
  vmovaps -1968(%rbp), %zmm4 # 64-byte Reload
  vmovaps -1904(%rbp), %zmm5 # 64-byte Reload
  vmovaps -1840(%rbp), %zmm6 # 64-byte Reload
  vmovaps -1776(%rbp), %zmm7 # 64-byte Reload
  vmovaps -1712(%rbp), %zmm8 # 64-byte Reload
  vmovaps -1648(%rbp), %zmm9 # 64-byte Reload
  vmovaps -1584(%rbp), %zmm10 # 64-byte Reload
  vmovaps -1520(%rbp), %zmm11 # 64-byte Reload
  vmovaps -1456(%rbp), %zmm12 # 64-byte Reload
  vmovaps -1392(%rbp), %zmm13 # 64-byte Reload
  vmovaps -1328(%rbp), %zmm14 # 64-byte Reload
  vmovaps -1264(%rbp), %zmm15 # 64-byte Reload
  vmovaps -1200(%rbp), %zmm16 # 64-byte Reload
  vmovaps -1136(%rbp), %zmm17 # 64-byte Reload
  vmovaps -1072(%rbp), %zmm18 # 64-byte Reload
  vmovaps -1008(%rbp), %zmm19 # 64-byte Reload
  vmovaps -944(%rbp), %zmm20 # 64-byte Reload
  vmovaps -880(%rbp), %zmm21 # 64-byte Reload
  vmovaps -816(%rbp), %zmm22 # 64-byte Reload
  vmovaps -752(%rbp), %zmm23 # 64-byte Reload
  vmovaps -688(%rbp), %zmm24 # 64-byte Reload
  vmovaps -624(%rbp), %zmm25 # 64-byte Reload
  vmovaps -560(%rbp), %zmm26 # 64-byte Reload
  vmovaps -496(%rbp), %zmm27 # 64-byte Reload
  vmovaps -432(%rbp), %zmm28 # 64-byte Reload
  vmovaps -368(%rbp), %zmm29 # 64-byte Reload
  vmovaps -272(%rbp), %zmm30 # 64-byte Reload
  vmovaps -192(%rbp), %zmm31 # 64-byte Reload
  kmovw -88(%rbp), %k0 # 2-byte Reload
  kmovw -86(%rbp), %k1 # 2-byte Reload
  kmovw -84(%rbp), %k2 # 2-byte Reload
  kmovw -82(%rbp), %k3 # 2-byte Reload
  kmovw -80(%rbp), %k4 # 2-byte Reload
  kmovw -78(%rbp), %k5 # 2-byte Reload
  kmovw -76(%rbp), %k6 # 2-byte Reload
  kmovw -74(%rbp), %k7 # 2-byte Reload
  leaq -72(%rbp), %rsp
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rax
  popq %rbp
  iretq

#elif __AVX2__
avx_isr_caller:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rax
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  andq $-32, %rsp
  subq $608, %rsp # imm = 0x260
  vmovaps %ymm15, -128(%rbp) # 32-byte Spill
  vmovaps %ymm14, -176(%rbp) # 32-byte Spill
  vmovaps %ymm13, -208(%rbp) # 32-byte Spill
  vmovaps %ymm12, -240(%rbp) # 32-byte Spill
  vmovaps %ymm11, -272(%rbp) # 32-byte Spill
  vmovaps %ymm10, -304(%rbp) # 32-byte Spill
  vmovaps %ymm9, -336(%rbp) # 32-byte Spill
  vmovaps %ymm8, -368(%rbp) # 32-byte Spill
  vmovaps %ymm7, -400(%rbp) # 32-byte Spill
  vmovaps %ymm6, -432(%rbp) # 32-byte Spill
  vmovaps %ymm5, -464(%rbp) # 32-byte Spill
  vmovaps %ymm4, -496(%rbp) # 32-byte Spill
  vmovaps %ymm3, -528(%rbp) # 32-byte Spill
  vmovaps %ymm2, -560(%rbp) # 32-byte Spill
  vmovaps %ymm1, -592(%rbp) # 32-byte Spill
  vmovaps %ymm0, -624(%rbp) # 32-byte Spill
  cld
  leaq 8(%rbp), %rax
  movq %rax, 24(%rsp)
  vzeroupper
  callq AVX_ISR_handler
  vmovaps -624(%rbp), %ymm0 # 32-byte Reload
  vmovaps -592(%rbp), %ymm1 # 32-byte Reload
  vmovaps -560(%rbp), %ymm2 # 32-byte Reload
  vmovaps -528(%rbp), %ymm3 # 32-byte Reload
  vmovaps -496(%rbp), %ymm4 # 32-byte Reload
  vmovaps -464(%rbp), %ymm5 # 32-byte Reload
  vmovaps -432(%rbp), %ymm6 # 32-byte Reload
  vmovaps -400(%rbp), %ymm7 # 32-byte Reload
  vmovaps -368(%rbp), %ymm8 # 32-byte Reload
  vmovaps -336(%rbp), %ymm9 # 32-byte Reload
  vmovaps -304(%rbp), %ymm10 # 32-byte Reload
  vmovaps -272(%rbp), %ymm11 # 32-byte Reload
  vmovaps -240(%rbp), %ymm12 # 32-byte Reload
  vmovaps -208(%rbp), %ymm13 # 32-byte Reload
  vmovaps -176(%rbp), %ymm14 # 32-byte Reload
  vmovaps -128(%rbp), %ymm15 # 32-byte Reload
  leaq -72(%rbp), %rsp
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rax
  popq %rbp
  iretq

#elif __AVX__
avx_isr_caller:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rax
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  andq $-32, %rsp
  subq $608, %rsp # imm = 0x260
  vmovaps %ymm15, -128(%rbp) # 32-byte Spill
  vmovaps %ymm14, -176(%rbp) # 32-byte Spill
  vmovaps %ymm13, -208(%rbp) # 32-byte Spill
  vmovaps %ymm12, -240(%rbp) # 32-byte Spill
  vmovaps %ymm11, -272(%rbp) # 32-byte Spill
  vmovaps %ymm10, -304(%rbp) # 32-byte Spill
  vmovaps %ymm9, -336(%rbp) # 32-byte Spill
  vmovaps %ymm8, -368(%rbp) # 32-byte Spill
  vmovaps %ymm7, -400(%rbp) # 32-byte Spill
  vmovaps %ymm6, -432(%rbp) # 32-byte Spill
  vmovaps %ymm5, -464(%rbp) # 32-byte Spill
  vmovaps %ymm4, -496(%rbp) # 32-byte Spill
  vmovaps %ymm3, -528(%rbp) # 32-byte Spill
  vmovaps %ymm2, -560(%rbp) # 32-byte Spill
  vmovaps %ymm1, -592(%rbp) # 32-byte Spill
  vmovaps %ymm0, -624(%rbp) # 32-byte Spill
  cld
  leaq 8(%rbp), %rax
  movq %rax, 24(%rsp)
  vzeroupper
  callq AVX_ISR_handler
  vmovaps -624(%rbp), %ymm0 # 32-byte Reload
  vmovaps -592(%rbp), %ymm1 # 32-byte Reload
  vmovaps -560(%rbp), %ymm2 # 32-byte Reload
  vmovaps -528(%rbp), %ymm3 # 32-byte Reload
  vmovaps -496(%rbp), %ymm4 # 32-byte Reload
  vmovaps -464(%rbp), %ymm5 # 32-byte Reload
  vmovaps -432(%rbp), %ymm6 # 32-byte Reload
  vmovaps -400(%rbp), %ymm7 # 32-byte Reload
  vmovaps -368(%rbp), %ymm8 # 32-byte Reload
  vmovaps -336(%rbp), %ymm9 # 32-byte Reload
  vmovaps -304(%rbp), %ymm10 # 32-byte Reload
  vmovaps -272(%rbp), %ymm11 # 32-byte Reload
  vmovaps -240(%rbp), %ymm12 # 32-byte Reload
  vmovaps -208(%rbp), %ymm13 # 32-byte Reload
  vmovaps -176(%rbp), %ymm14 # 32-byte Reload
  vmovaps -128(%rbp), %ymm15 # 32-byte Reload
  leaq -72(%rbp), %rsp
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rax
  popq %rbp
  iretq

#elif __SSE2__
avx_isr_caller:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rax
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  subq $280, %rsp # imm = 0x118
  movaps %xmm15, -96(%rbp) # 16-byte Spill
  movaps %xmm14, -112(%rbp) # 16-byte Spill
  movaps %xmm13, -128(%rbp) # 16-byte Spill
  movaps %xmm12, -144(%rbp) # 16-byte Spill
  movaps %xmm11, -160(%rbp) # 16-byte Spill
  movaps %xmm10, -176(%rbp) # 16-byte Spill
  movaps %xmm9, -192(%rbp) # 16-byte Spill
  movaps %xmm8, -208(%rbp) # 16-byte Spill
  movaps %xmm7, -224(%rbp) # 16-byte Spill
  movaps %xmm6, -240(%rbp) # 16-byte Spill
  movaps %xmm5, -256(%rbp) # 16-byte Spill
  movaps %xmm4, -272(%rbp) # 16-byte Spill
  movaps %xmm3, -288(%rbp) # 16-byte Spill
  movaps %xmm2, -304(%rbp) # 16-byte Spill
  movaps %xmm1, -320(%rbp) # 16-byte Spill
  movaps %xmm0, -336(%rbp) # 16-byte Spill
  cld
  leaq 8(%rbp), %rax
  movq %rax, -344(%rbp)
  callq AVX_ISR_handler
  movaps -336(%rbp), %xmm0 # 16-byte Reload
  movaps -320(%rbp), %xmm1 # 16-byte Reload
  movaps -304(%rbp), %xmm2 # 16-byte Reload
  movaps -288(%rbp), %xmm3 # 16-byte Reload
  movaps -272(%rbp), %xmm4 # 16-byte Reload
  movaps -256(%rbp), %xmm5 # 16-byte Reload
  movaps -240(%rbp), %xmm6 # 16-byte Reload
  movaps -224(%rbp), %xmm7 # 16-byte Reload
  movaps -208(%rbp), %xmm8 # 16-byte Reload
  movaps -192(%rbp), %xmm9 # 16-byte Reload
  movaps -176(%rbp), %xmm10 # 16-byte Reload
  movaps -160(%rbp), %xmm11 # 16-byte Reload
  movaps -144(%rbp), %xmm12 # 16-byte Reload
  movaps -128(%rbp), %xmm13 # 16-byte Reload
  movaps -112(%rbp), %xmm14 # 16-byte Reload
  movaps -96(%rbp), %xmm15 # 16-byte Reload
  addq $280, %rsp # imm = 0x118
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rax
  popq %rbp
  iretq

#endif

//----------------------------------------------------------------------------------------------------------------------------------
//  isr_caller: General Register Only ISR Handler
//----------------------------------------------------------------------------------------------------------------------------------
//
// Bog-standard interrupt handler with no instruction set extensions
//

isr_caller:
  pushq %rbp
  movq %rsp, %rbp
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rcx
  pushq %rdx
  pushq %rax
  subq $8, %rsp
  leaq 8(%rbp), %rax
  movq %rax, -80(%rbp)
  movl $0, %eax
  cld
  callq ISR_handler
  nop
  addq $8, %rsp
  popq %rax
  popq %rdx
  popq %rcx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rbp
  iretq

//----------------------------------------------------------------------------------------------------------------------------------
//  avx_exc_caller: Exception Handler with Instruction Set Extensions
//----------------------------------------------------------------------------------------------------------------------------------
//
// Exception handler with instruction set extensions (depends on CPU features passed to compiler)
//

#ifdef __AVX512F__
avx_exc_caller:
  pushq %rax
  pushq %rbp
  movq %rsp, %rbp
  pushq %rax
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  andq $-64, %rsp
  subq $2240, %rsp # imm = 0x8C0
  kmovw %k7, -74(%rbp) # 2-byte Spill
  kmovw %k6, -76(%rbp) # 2-byte Spill
  kmovw %k5, -78(%rbp) # 2-byte Spill
  kmovw %k4, -80(%rbp) # 2-byte Spill
  kmovw %k3, -82(%rbp) # 2-byte Spill
  kmovw %k2, -84(%rbp) # 2-byte Spill
  kmovw %k1, -86(%rbp) # 2-byte Spill
  kmovw %k0, -88(%rbp) # 2-byte Spill
  vmovaps %zmm31, -192(%rbp) # 64-byte Spill
  vmovaps %zmm30, -272(%rbp) # 64-byte Spill
  vmovaps %zmm29, -368(%rbp) # 64-byte Spill
  vmovaps %zmm28, -432(%rbp) # 64-byte Spill
  vmovaps %zmm27, -496(%rbp) # 64-byte Spill
  vmovaps %zmm26, -560(%rbp) # 64-byte Spill
  vmovaps %zmm25, -624(%rbp) # 64-byte Spill
  vmovaps %zmm24, -688(%rbp) # 64-byte Spill
  vmovaps %zmm23, -752(%rbp) # 64-byte Spill
  vmovaps %zmm22, -816(%rbp) # 64-byte Spill
  vmovaps %zmm21, -880(%rbp) # 64-byte Spill
  vmovaps %zmm20, -944(%rbp) # 64-byte Spill
  vmovaps %zmm19, -1008(%rbp) # 64-byte Spill
  vmovaps %zmm18, -1072(%rbp) # 64-byte Spill
  vmovaps %zmm17, -1136(%rbp) # 64-byte Spill
  vmovaps %zmm16, -1200(%rbp) # 64-byte Spill
  vmovaps %zmm15, -1264(%rbp) # 64-byte Spill
  vmovaps %zmm14, -1328(%rbp) # 64-byte Spill
  vmovaps %zmm13, -1392(%rbp) # 64-byte Spill
  vmovaps %zmm12, -1456(%rbp) # 64-byte Spill
  vmovaps %zmm11, -1520(%rbp) # 64-byte Spill
  vmovaps %zmm10, -1584(%rbp) # 64-byte Spill
  vmovaps %zmm9, -1648(%rbp) # 64-byte Spill
  vmovaps %zmm8, -1712(%rbp) # 64-byte Spill
  vmovaps %zmm7, -1776(%rbp) # 64-byte Spill
  vmovaps %zmm6, -1840(%rbp) # 64-byte Spill
  vmovaps %zmm5, -1904(%rbp) # 64-byte Spill
  vmovaps %zmm4, -1968(%rbp) # 64-byte Spill
  vmovaps %zmm3, -2032(%rbp) # 64-byte Spill
  vmovaps %zmm2, -2096(%rbp) # 64-byte Spill
  vmovaps %zmm1, -2160(%rbp) # 64-byte Spill
  vmovaps %zmm0, -2224(%rbp) # 64-byte Spill
  cld
  movq 16(%rbp), %rax
  leaq 24(%rbp), %rcx
  movq %rcx, 64(%rsp)
  movq %rax, 56(%rsp) # 8-byte Spill
  vzeroupper
  callq AVX_EXC_handler
  vmovaps -2224(%rbp), %zmm0 # 64-byte Reload
  vmovaps -2160(%rbp), %zmm1 # 64-byte Reload
  vmovaps -2096(%rbp), %zmm2 # 64-byte Reload
  vmovaps -2032(%rbp), %zmm3 # 64-byte Reload
  vmovaps -1968(%rbp), %zmm4 # 64-byte Reload
  vmovaps -1904(%rbp), %zmm5 # 64-byte Reload
  vmovaps -1840(%rbp), %zmm6 # 64-byte Reload
  vmovaps -1776(%rbp), %zmm7 # 64-byte Reload
  vmovaps -1712(%rbp), %zmm8 # 64-byte Reload
  vmovaps -1648(%rbp), %zmm9 # 64-byte Reload
  vmovaps -1584(%rbp), %zmm10 # 64-byte Reload
  vmovaps -1520(%rbp), %zmm11 # 64-byte Reload
  vmovaps -1456(%rbp), %zmm12 # 64-byte Reload
  vmovaps -1392(%rbp), %zmm13 # 64-byte Reload
  vmovaps -1328(%rbp), %zmm14 # 64-byte Reload
  vmovaps -1264(%rbp), %zmm15 # 64-byte Reload
  vmovaps -1200(%rbp), %zmm16 # 64-byte Reload
  vmovaps -1136(%rbp), %zmm17 # 64-byte Reload
  vmovaps -1072(%rbp), %zmm18 # 64-byte Reload
  vmovaps -1008(%rbp), %zmm19 # 64-byte Reload
  vmovaps -944(%rbp), %zmm20 # 64-byte Reload
  vmovaps -880(%rbp), %zmm21 # 64-byte Reload
  vmovaps -816(%rbp), %zmm22 # 64-byte Reload
  vmovaps -752(%rbp), %zmm23 # 64-byte Reload
  vmovaps -688(%rbp), %zmm24 # 64-byte Reload
  vmovaps -624(%rbp), %zmm25 # 64-byte Reload
  vmovaps -560(%rbp), %zmm26 # 64-byte Reload
  vmovaps -496(%rbp), %zmm27 # 64-byte Reload
  vmovaps -432(%rbp), %zmm28 # 64-byte Reload
  vmovaps -368(%rbp), %zmm29 # 64-byte Reload
  vmovaps -272(%rbp), %zmm30 # 64-byte Reload
  vmovaps -192(%rbp), %zmm31 # 64-byte Reload
  kmovw -88(%rbp), %k0 # 2-byte Reload
  kmovw -86(%rbp), %k1 # 2-byte Reload
  kmovw -84(%rbp), %k2 # 2-byte Reload
  kmovw -82(%rbp), %k3 # 2-byte Reload
  kmovw -80(%rbp), %k4 # 2-byte Reload
  kmovw -78(%rbp), %k5 # 2-byte Reload
  kmovw -76(%rbp), %k6 # 2-byte Reload
  kmovw -74(%rbp), %k7 # 2-byte Reload
  leaq -72(%rbp), %rsp
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rax
  popq %rbp
  addq $16, %rsp
  iretq

#elif __AVX2__
avx_exc_caller:
  pushq %rax
  pushq %rbp
  movq %rsp, %rbp
  pushq %rax
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  andq $-32, %rsp
  subq $608, %rsp # imm = 0x260
  vmovaps %ymm15, -128(%rbp) # 32-byte Spill
  vmovaps %ymm14, -176(%rbp) # 32-byte Spill
  vmovaps %ymm13, -208(%rbp) # 32-byte Spill
  vmovaps %ymm12, -240(%rbp) # 32-byte Spill
  vmovaps %ymm11, -272(%rbp) # 32-byte Spill
  vmovaps %ymm10, -304(%rbp) # 32-byte Spill
  vmovaps %ymm9, -336(%rbp) # 32-byte Spill
  vmovaps %ymm8, -368(%rbp) # 32-byte Spill
  vmovaps %ymm7, -400(%rbp) # 32-byte Spill
  vmovaps %ymm6, -432(%rbp) # 32-byte Spill
  vmovaps %ymm5, -464(%rbp) # 32-byte Spill
  vmovaps %ymm4, -496(%rbp) # 32-byte Spill
  vmovaps %ymm3, -528(%rbp) # 32-byte Spill
  vmovaps %ymm2, -560(%rbp) # 32-byte Spill
  vmovaps %ymm1, -592(%rbp) # 32-byte Spill
  vmovaps %ymm0, -624(%rbp) # 32-byte Spill
  cld
  movq 16(%rbp), %rax
  leaq 24(%rbp), %rcx
  movq %rcx, 32(%rsp)
  movq %rax, 24(%rsp) # 8-byte Spill
  vzeroupper
  callq AVX_EXC_handler
  vmovaps -624(%rbp), %ymm0 # 32-byte Reload
  vmovaps -592(%rbp), %ymm1 # 32-byte Reload
  vmovaps -560(%rbp), %ymm2 # 32-byte Reload
  vmovaps -528(%rbp), %ymm3 # 32-byte Reload
  vmovaps -496(%rbp), %ymm4 # 32-byte Reload
  vmovaps -464(%rbp), %ymm5 # 32-byte Reload
  vmovaps -432(%rbp), %ymm6 # 32-byte Reload
  vmovaps -400(%rbp), %ymm7 # 32-byte Reload
  vmovaps -368(%rbp), %ymm8 # 32-byte Reload
  vmovaps -336(%rbp), %ymm9 # 32-byte Reload
  vmovaps -304(%rbp), %ymm10 # 32-byte Reload
  vmovaps -272(%rbp), %ymm11 # 32-byte Reload
  vmovaps -240(%rbp), %ymm12 # 32-byte Reload
  vmovaps -208(%rbp), %ymm13 # 32-byte Reload
  vmovaps -176(%rbp), %ymm14 # 32-byte Reload
  vmovaps -128(%rbp), %ymm15 # 32-byte Reload
  leaq -72(%rbp), %rsp
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rax
  popq %rbp
  addq $16, %rsp
  iretq

#elif __AVX__
avx_exc_caller:
  pushq %rax
  pushq %rbp
  movq %rsp, %rbp
  pushq %rax
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  andq $-32, %rsp
  subq $608, %rsp # imm = 0x260
  vmovaps %ymm15, -128(%rbp) # 32-byte Spill
  vmovaps %ymm14, -176(%rbp) # 32-byte Spill
  vmovaps %ymm13, -208(%rbp) # 32-byte Spill
  vmovaps %ymm12, -240(%rbp) # 32-byte Spill
  vmovaps %ymm11, -272(%rbp) # 32-byte Spill
  vmovaps %ymm10, -304(%rbp) # 32-byte Spill
  vmovaps %ymm9, -336(%rbp) # 32-byte Spill
  vmovaps %ymm8, -368(%rbp) # 32-byte Spill
  vmovaps %ymm7, -400(%rbp) # 32-byte Spill
  vmovaps %ymm6, -432(%rbp) # 32-byte Spill
  vmovaps %ymm5, -464(%rbp) # 32-byte Spill
  vmovaps %ymm4, -496(%rbp) # 32-byte Spill
  vmovaps %ymm3, -528(%rbp) # 32-byte Spill
  vmovaps %ymm2, -560(%rbp) # 32-byte Spill
  vmovaps %ymm1, -592(%rbp) # 32-byte Spill
  vmovaps %ymm0, -624(%rbp) # 32-byte Spill
  cld
  movq 16(%rbp), %rax
  leaq 24(%rbp), %rcx
  movq %rcx, 32(%rsp)
  movq %rax, 24(%rsp) # 8-byte Spill
  vzeroupper
  callq AVX_EXC_handler
  vmovaps -624(%rbp), %ymm0 # 32-byte Reload
  vmovaps -592(%rbp), %ymm1 # 32-byte Reload
  vmovaps -560(%rbp), %ymm2 # 32-byte Reload
  vmovaps -528(%rbp), %ymm3 # 32-byte Reload
  vmovaps -496(%rbp), %ymm4 # 32-byte Reload
  vmovaps -464(%rbp), %ymm5 # 32-byte Reload
  vmovaps -432(%rbp), %ymm6 # 32-byte Reload
  vmovaps -400(%rbp), %ymm7 # 32-byte Reload
  vmovaps -368(%rbp), %ymm8 # 32-byte Reload
  vmovaps -336(%rbp), %ymm9 # 32-byte Reload
  vmovaps -304(%rbp), %ymm10 # 32-byte Reload
  vmovaps -272(%rbp), %ymm11 # 32-byte Reload
  vmovaps -240(%rbp), %ymm12 # 32-byte Reload
  vmovaps -208(%rbp), %ymm13 # 32-byte Reload
  vmovaps -176(%rbp), %ymm14 # 32-byte Reload
  vmovaps -128(%rbp), %ymm15 # 32-byte Reload
  leaq -72(%rbp), %rsp
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rax
  popq %rbp
  addq $16, %rsp
  iretq

#elif __SSE2__
avx_exc_caller:
  pushq %rax
  pushq %rbp
  movq %rsp, %rbp
  pushq %rax
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  subq $288, %rsp # imm = 0x120
  movaps %xmm15, -96(%rbp) # 16-byte Spill
  movaps %xmm14, -112(%rbp) # 16-byte Spill
  movaps %xmm13, -128(%rbp) # 16-byte Spill
  movaps %xmm12, -144(%rbp) # 16-byte Spill
  movaps %xmm11, -160(%rbp) # 16-byte Spill
  movaps %xmm10, -176(%rbp) # 16-byte Spill
  movaps %xmm9, -192(%rbp) # 16-byte Spill
  movaps %xmm8, -208(%rbp) # 16-byte Spill
  movaps %xmm7, -224(%rbp) # 16-byte Spill
  movaps %xmm6, -240(%rbp) # 16-byte Spill
  movaps %xmm5, -256(%rbp) # 16-byte Spill
  movaps %xmm4, -272(%rbp) # 16-byte Spill
  movaps %xmm3, -288(%rbp) # 16-byte Spill
  movaps %xmm2, -304(%rbp) # 16-byte Spill
  movaps %xmm1, -320(%rbp) # 16-byte Spill
  movaps %xmm0, -336(%rbp) # 16-byte Spill
  cld
  movq 16(%rbp), %rax
  leaq 24(%rbp), %rcx
  movq %rcx, -344(%rbp)
  movq %rax, -352(%rbp) # 8-byte Spill
  callq AVX_EXC_handler
  movaps -336(%rbp), %xmm0 # 16-byte Reload
  movaps -320(%rbp), %xmm1 # 16-byte Reload
  movaps -304(%rbp), %xmm2 # 16-byte Reload
  movaps -288(%rbp), %xmm3 # 16-byte Reload
  movaps -272(%rbp), %xmm4 # 16-byte Reload
  movaps -256(%rbp), %xmm5 # 16-byte Reload
  movaps -240(%rbp), %xmm6 # 16-byte Reload
  movaps -224(%rbp), %xmm7 # 16-byte Reload
  movaps -208(%rbp), %xmm8 # 16-byte Reload
  movaps -192(%rbp), %xmm9 # 16-byte Reload
  movaps -176(%rbp), %xmm10 # 16-byte Reload
  movaps -160(%rbp), %xmm11 # 16-byte Reload
  movaps -144(%rbp), %xmm12 # 16-byte Reload
  movaps -128(%rbp), %xmm13 # 16-byte Reload
  movaps -112(%rbp), %xmm14 # 16-byte Reload
  movaps -96(%rbp), %xmm15 # 16-byte Reload
  addq $288, %rsp # imm = 0x120
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rax
  popq %rbp
  addq $16, %rsp
  iretq

#endif

//----------------------------------------------------------------------------------------------------------------------------------
//  exc_caller: General Register Only Exception Handler
//----------------------------------------------------------------------------------------------------------------------------------
//
// Bog-standard exception handler with no instruction set extensions
//
exc_caller:
  pushq %rbp
  movq %rsp, %rbp
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rcx
  pushq %rdx
  pushq %rax
  subq $16, %rsp
  leaq 16(%rbp), %rax
  movq %rax, -80(%rbp)
  movq 8(%rbp), %rax
  movq %rax, -88(%rbp)
  movl $0, %eax
  cld
  callq EXC_handler
  nop
  addq $16, %rsp
  popq %rax
  popq %rdx
  popq %rcx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %rbp
  addq $8, %rsp
  iretq
