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
// ...Ok, technically there are 4 kinds of interrupt/exception callers here, since each one has an AVX/SSE and non-AVX/SSE version.
//
// See ISR.h for a step-by-step on how to add/modify a user-defined interrupt.
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

.macro SAVE_ISR_REGISTERS
  pushq %rbp
  movq  %rsp, %rbp
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
.macro SAVE_AVX_ISR_REGISTERS
  SAVE_ISR_REGISTERS
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
  RESTORE_ISR_REGISTERS
.endm

#elif __AVX__
.macro SAVE_AVX_ISR_REGISTERS
  SAVE_ISR_REGISTERS
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
  RESTORE_ISR_REGISTERS
.endm

#else // SSE
.macro SAVE_AVX_ISR_REGISTERS
  SAVE_ISR_REGISTERS
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
  RESTORE_ISR_REGISTERS
.endm

#endif

//----------------------------------------------------------------------------------------------------------------------------------
//  isr_pusherX: Push Interrupt Number X Onto Stack and Call Handlers
//----------------------------------------------------------------------------------------------------------------------------------
//
// IDT points to these, which push the interrupt number onto the stack and call the relevant handler. Hard to know what interrupt is
// being handled without these.
//

//----------------------------//
// Function Macro Definitions //
//----------------------------//

//
// Interrupts (AVX, no error code)
//

.macro AVX_ISR_MACRO num:req
.global avx_isr_pusher\num

avx_isr_pusher\num\():
  SAVE_AVX_ISR_REGISTERS
  pushq $\num // Push this after AVX for alignment reasons (so that AVX is always at least 16-byte aligned, which means sometimes it will be 32- or 64-byte aligned)
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
.endm

//
// Interrupts (no AVX, no error code)
//

.macro ISR_MACRO num:req
.global isr_pusher\num

isr_pusher\num\():
  SAVE_ISR_REGISTERS
  pushq $\num
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
.endm

//
// Exceptions (AVX, have error code)
//

.macro AVX_EXC_MACRO num:req
.global avx_exc_pusher\num

avx_exc_pusher\num\():
  pushq $\num // Push isr_num here to maintain 16-byte alignment for AVX
  SAVE_AVX_ISR_REGISTERS
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq AVX_EXC_handler
  RESTORE_AVX_ISR_REGISTERS
  addq $16, %rsp // For isr_num and error code
  iretq
.endm

//
// Exceptions (no AVX, have error code)
//

.macro EXC_MACRO num:req
.global exc_pusher\num

exc_pusher\num\():
  pushq $\num // Staying consistent
  SAVE_ISR_REGISTERS
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq EXC_handler
  RESTORE_ISR_REGISTERS
  addq $16, %rsp // For isr_num and error code
  iretq
.endm

//-----------------------------------------//
// Function Definitions (Macro Invocation) //
//-----------------------------------------//

//
// Predefined System Interrupts and Exceptions
//

AVX_ISR_MACRO 0 // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
AVX_ISR_MACRO 1 // Fault/Trap #DB: Debug Exception
AVX_ISR_MACRO 2 // NMI (Nonmaskable External Interrupt)
AVX_ISR_MACRO 3 // Trap #BP: Breakpoint (INT3 instruction)
AVX_ISR_MACRO 4 // Trap #OF: Overflow (INTO instruction)
AVX_ISR_MACRO 5 // Fault #BR: BOUND Range Exceeded (BOUND instruction)
AVX_ISR_MACRO 6 // Fault #UD: Invalid or Undefined Opcode
AVX_ISR_MACRO 7 // Fault #NM: Device Not Available Exception

AVX_EXC_MACRO 8 // Abort #DF: Double Fault (error code is always 0)

AVX_ISR_MACRO 9 // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

AVX_EXC_MACRO 10 // Fault #TS: Invalid TSS
AVX_EXC_MACRO 11 // Fault #NP: Segment Not Present
AVX_EXC_MACRO 12 // Fault #SS: Stack Segment Fault
AVX_EXC_MACRO 13 // Fault #GP: General Protection
AVX_EXC_MACRO 14 // Fault #PF: Page Fault

AVX_ISR_MACRO 16 // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

AVX_EXC_MACRO 17 // Fault #AC: Alignment Check (error code is always 0)

AVX_ISR_MACRO 18 // Abort #MC: Machine Check
AVX_ISR_MACRO 19 // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
AVX_ISR_MACRO 20 // Fault #VE: Virtualization Exception

AVX_EXC_MACRO 30 // Fault #SX: Security Exception

//
// These are system reserved, if they trigger they will go to unhandled interrupt error
//

AVX_ISR_MACRO 15

AVX_ISR_MACRO 21
AVX_ISR_MACRO 22
AVX_ISR_MACRO 23
AVX_ISR_MACRO 24
AVX_ISR_MACRO 25
AVX_ISR_MACRO 26
AVX_ISR_MACRO 27
AVX_ISR_MACRO 28
AVX_ISR_MACRO 29

AVX_ISR_MACRO 31

//
// User-Defined Interrupts
//

// Use non-AVX ISR/EXC_MACRO if the interrupts are guaranteed not to touch AVX registers,
// otherwise, or if in any doubt, use AVX_ISR/EXC_MACRO. By default everything is set to AVX_ISR_MACRO.

AVX_ISR_MACRO 32
AVX_ISR_MACRO 33
AVX_ISR_MACRO 34
AVX_ISR_MACRO 35
AVX_ISR_MACRO 36
AVX_ISR_MACRO 37
AVX_ISR_MACRO 38
AVX_ISR_MACRO 39
AVX_ISR_MACRO 40
AVX_ISR_MACRO 41
AVX_ISR_MACRO 42
AVX_ISR_MACRO 43
AVX_ISR_MACRO 44
AVX_ISR_MACRO 45
AVX_ISR_MACRO 46
AVX_ISR_MACRO 47
AVX_ISR_MACRO 48
AVX_ISR_MACRO 49
AVX_ISR_MACRO 50
AVX_ISR_MACRO 51
AVX_ISR_MACRO 52
AVX_ISR_MACRO 53
AVX_ISR_MACRO 54
AVX_ISR_MACRO 55
AVX_ISR_MACRO 56
AVX_ISR_MACRO 57
AVX_ISR_MACRO 58
AVX_ISR_MACRO 59
AVX_ISR_MACRO 60
AVX_ISR_MACRO 61
AVX_ISR_MACRO 62
AVX_ISR_MACRO 63
AVX_ISR_MACRO 64
AVX_ISR_MACRO 65
AVX_ISR_MACRO 66
AVX_ISR_MACRO 67
AVX_ISR_MACRO 68
AVX_ISR_MACRO 69
AVX_ISR_MACRO 70
AVX_ISR_MACRO 71
AVX_ISR_MACRO 72
AVX_ISR_MACRO 73
AVX_ISR_MACRO 74
AVX_ISR_MACRO 75
AVX_ISR_MACRO 76
AVX_ISR_MACRO 77
AVX_ISR_MACRO 78
AVX_ISR_MACRO 79
AVX_ISR_MACRO 80
AVX_ISR_MACRO 81
AVX_ISR_MACRO 82
AVX_ISR_MACRO 83
AVX_ISR_MACRO 84
AVX_ISR_MACRO 85
AVX_ISR_MACRO 86
AVX_ISR_MACRO 87
AVX_ISR_MACRO 88
AVX_ISR_MACRO 89
AVX_ISR_MACRO 90
AVX_ISR_MACRO 91
AVX_ISR_MACRO 92
AVX_ISR_MACRO 93
AVX_ISR_MACRO 94
AVX_ISR_MACRO 95
AVX_ISR_MACRO 96
AVX_ISR_MACRO 97
AVX_ISR_MACRO 98
AVX_ISR_MACRO 99
AVX_ISR_MACRO 100
AVX_ISR_MACRO 101
AVX_ISR_MACRO 102
AVX_ISR_MACRO 103
AVX_ISR_MACRO 104
AVX_ISR_MACRO 105
AVX_ISR_MACRO 106
AVX_ISR_MACRO 107
AVX_ISR_MACRO 108
AVX_ISR_MACRO 109
AVX_ISR_MACRO 110
AVX_ISR_MACRO 111
AVX_ISR_MACRO 112
AVX_ISR_MACRO 113
AVX_ISR_MACRO 114
AVX_ISR_MACRO 115
AVX_ISR_MACRO 116
AVX_ISR_MACRO 117
AVX_ISR_MACRO 118
AVX_ISR_MACRO 119
AVX_ISR_MACRO 120
AVX_ISR_MACRO 121
AVX_ISR_MACRO 122
AVX_ISR_MACRO 123
AVX_ISR_MACRO 124
AVX_ISR_MACRO 125
AVX_ISR_MACRO 126
AVX_ISR_MACRO 127
AVX_ISR_MACRO 128
AVX_ISR_MACRO 129
AVX_ISR_MACRO 130
AVX_ISR_MACRO 131
AVX_ISR_MACRO 132
AVX_ISR_MACRO 133
AVX_ISR_MACRO 134
AVX_ISR_MACRO 135
AVX_ISR_MACRO 136
AVX_ISR_MACRO 137
AVX_ISR_MACRO 138
AVX_ISR_MACRO 139
AVX_ISR_MACRO 140
AVX_ISR_MACRO 141
AVX_ISR_MACRO 142
AVX_ISR_MACRO 143
AVX_ISR_MACRO 144
AVX_ISR_MACRO 145
AVX_ISR_MACRO 146
AVX_ISR_MACRO 147
AVX_ISR_MACRO 148
AVX_ISR_MACRO 149
AVX_ISR_MACRO 150
AVX_ISR_MACRO 151
AVX_ISR_MACRO 152
AVX_ISR_MACRO 153
AVX_ISR_MACRO 154
AVX_ISR_MACRO 155
AVX_ISR_MACRO 156
AVX_ISR_MACRO 157
AVX_ISR_MACRO 158
AVX_ISR_MACRO 159
AVX_ISR_MACRO 160
AVX_ISR_MACRO 161
AVX_ISR_MACRO 162
AVX_ISR_MACRO 163
AVX_ISR_MACRO 164
AVX_ISR_MACRO 165
AVX_ISR_MACRO 166
AVX_ISR_MACRO 167
AVX_ISR_MACRO 168
AVX_ISR_MACRO 169
AVX_ISR_MACRO 170
AVX_ISR_MACRO 171
AVX_ISR_MACRO 172
AVX_ISR_MACRO 173
AVX_ISR_MACRO 174
AVX_ISR_MACRO 175
AVX_ISR_MACRO 176
AVX_ISR_MACRO 177
AVX_ISR_MACRO 178
AVX_ISR_MACRO 179
AVX_ISR_MACRO 180
AVX_ISR_MACRO 181
AVX_ISR_MACRO 182
AVX_ISR_MACRO 183
AVX_ISR_MACRO 184
AVX_ISR_MACRO 185
AVX_ISR_MACRO 186
AVX_ISR_MACRO 187
AVX_ISR_MACRO 188
AVX_ISR_MACRO 189
AVX_ISR_MACRO 190
AVX_ISR_MACRO 191
AVX_ISR_MACRO 192
AVX_ISR_MACRO 193
AVX_ISR_MACRO 194
AVX_ISR_MACRO 195
AVX_ISR_MACRO 196
AVX_ISR_MACRO 197
AVX_ISR_MACRO 198
AVX_ISR_MACRO 199
AVX_ISR_MACRO 200
AVX_ISR_MACRO 201
AVX_ISR_MACRO 202
AVX_ISR_MACRO 203
AVX_ISR_MACRO 204
AVX_ISR_MACRO 205
AVX_ISR_MACRO 206
AVX_ISR_MACRO 207
AVX_ISR_MACRO 208
AVX_ISR_MACRO 209
AVX_ISR_MACRO 210
AVX_ISR_MACRO 211
AVX_ISR_MACRO 212
AVX_ISR_MACRO 213
AVX_ISR_MACRO 214
AVX_ISR_MACRO 215
AVX_ISR_MACRO 216
AVX_ISR_MACRO 217
AVX_ISR_MACRO 218
AVX_ISR_MACRO 219
AVX_ISR_MACRO 220
AVX_ISR_MACRO 221
AVX_ISR_MACRO 222
AVX_ISR_MACRO 223
AVX_ISR_MACRO 224
AVX_ISR_MACRO 225
AVX_ISR_MACRO 226
AVX_ISR_MACRO 227
AVX_ISR_MACRO 228
AVX_ISR_MACRO 229
AVX_ISR_MACRO 230
AVX_ISR_MACRO 231
AVX_ISR_MACRO 232
AVX_ISR_MACRO 233
AVX_ISR_MACRO 234
AVX_ISR_MACRO 235
AVX_ISR_MACRO 236
AVX_ISR_MACRO 237
AVX_ISR_MACRO 238
AVX_ISR_MACRO 239
AVX_ISR_MACRO 240
AVX_ISR_MACRO 241
AVX_ISR_MACRO 242
AVX_ISR_MACRO 243
AVX_ISR_MACRO 244
AVX_ISR_MACRO 245
AVX_ISR_MACRO 246
AVX_ISR_MACRO 247
AVX_ISR_MACRO 248
AVX_ISR_MACRO 249
AVX_ISR_MACRO 250
AVX_ISR_MACRO 251
AVX_ISR_MACRO 252
AVX_ISR_MACRO 253
AVX_ISR_MACRO 254
AVX_ISR_MACRO 255

// Thank you Excel spreadsheet macros...
