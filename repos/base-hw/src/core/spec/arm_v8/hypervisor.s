/*
 * \brief  Transition between virtual/host mode
 * \author Alexander Boettcher
 * \author Stefan Kalkowski
 * \date   2019-06-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


.section .text

/*
 * see D1.10.2 Exception vectors chapter
 */
.p2align 12
.global hypervisor_exception_vector
hypervisor_exception_vector:
	.rept 16
	add sp, sp, #-16   /* push x29, x30 to stack */
	stp x29, x30, [sp]
	mrs x30, hcr_el2   /* read HCR register */
	tst x30, #1        /* check VM bit */
	beq _from_host     /* if VM bit is not set, its a host call */
	ldr x29, [sp, #32] /* otherwise, load vcpu_state pointer */
	adr x30, .         /* hold exception vector offset in x30 */
	and x30, x30, #0xf80
	b   _from_vm
	.balign 128
	.endr

_from_host:
	ldp x29, x30, [sp], #2*8  /* pop x29, x30 from stack */
	cmp x0, #0
	beq _to_vm
	cmp x0, #1
	beq _invalidate_tlb
	eret

_to_vm:
	add sp, sp, #-16    /* push arg1/2 (vm/host state to stack */
	stp x1, x2, [sp]
	add sp, sp, #-16    /* push arg3 (vm pic state) to stack */
	str x3, [sp]

	msr vttbr_el2, x4   /* stage2 table pointer was arg4 */
	mov x0, x1

	add  x0, x0, #31*8  /* skip x0...x30, loaded later */

	ldp  x1, x2,  [x0], #2*8  /* sp, ip          */
	ldp  x3, x4,  [x0], #2*8  /* esr_el1, pstate */
	msr  sp_el0,   x1
	msr  elr_el2,  x2
	msr  esr_el1,  x3
	msr  spsr_el2, x4

	add  x0, x0, #2*8 /* skip exception_type and esr_el2 */

	/** FPU register **/
	ldp  w1,  w2,  [x0], #2*4
	msr  fpcr, x1
	msr  fpsr, x2
	ldp  q0,  q1,  [x0], #2*16
	ldp  q2,  q3,  [x0], #2*16
	ldp  q4,  q5,  [x0], #2*16
	ldp  q6,  q7,  [x0], #2*16
	ldp  q8,  q9,  [x0], #2*16
	ldp  q10, q11, [x0], #2*16
	ldp  q12, q13, [x0], #2*16
	ldp  q14, q15, [x0], #2*16
	ldp  q16, q17, [x0], #2*16
	ldp  q18, q19, [x0], #2*16
	ldp  q20, q21, [x0], #2*16
	ldp  q22, q23, [x0], #2*16
	ldp  q24, q25, [x0], #2*16
	ldp  q26, q27, [x0], #2*16
	ldp  q28, q29, [x0], #2*16
	ldp  q30, q31, [x0], #2*16

	/** system register **/
	ldp  x1,  x2,  [x0], #2*8  /* elr_el1,   sp_el1          */
	ldp  x3,  x4,  [x0], #2*8  /* spsr_el1, sctlr_el1        */
	ldp  x5,  x6,  [x0], #2*8  /* actlr_el1, vbar_el1        */
	ldp  w7,  w8,  [x0], #2*4  /* cpacr_el1, afsr0_el1       */
	ldp  w9,  w10, [x0], #2*4  /* afsr1_el1, contextidr_el1  */
	ldp  x11, x12, [x0], #2*8  /* ttbr0_el1, ttbr1_el1       */
	ldp  x13, x14, [x0], #2*8  /* tcr_el1,   mair_el1        */
	ldp  x15, x16, [x0], #2*8  /* amair_el1, far_el1         */
	ldp  x17, x18, [x0], #2*8  /* par_el1,   tpidrro_el0     */
	ldp  x19, x20, [x0], #2*8  /* tpidr_el0, tpidr_el1       */
	ldr  x21,      [x0], #3*8  /* vmpidr_el2                 */
	msr  elr_el1,        x1
	msr  sp_el1,         x2
	msr  spsr_el1,       x3
	msr  sctlr_el1,      x4
	msr  actlr_el1,      x5
	msr  vbar_el1,       x6
	msr  cpacr_el1,      x7
	msr  afsr0_el1,      x8
	msr  afsr1_el1,      x9
	msr  contextidr_el1, x10
	msr  ttbr0_el1,      x11
	msr  ttbr1_el1,      x12
	msr  tcr_el1,        x13
	msr  mair_el1,       x14
	msr  amair_el1,      x15
	msr  far_el1,        x16
	msr  par_el1,        x17
	msr  tpidrro_el0,    x18
	msr  tpidr_el0,      x19
	msr  tpidr_el1,      x20
	msr  vmpidr_el2,     x21


	/**********************
	 ** load timer state **
	 **********************/

	ldp  x22, x23, [x0], #2*8  /* timer.offset, timer.compare */
	ldp  w24, w25, [x0]        /* timer.control, kcontrol    */
	msr  cntvoff_el2,    x22
	msr  cntv_cval_el0,  x23
	msr  cntv_ctl_el0,   x24
	msr  cntkctl_el1,    x25

	mov  x0, #0b100
	msr  cnthctl_el2, x0


	/************************
	 ** debug/perfm access **
	 ************************/

	mrs  x0, mdcr_el2
	movz x1, #0b111101100000
	orr  x0, x0, x1
	msr  mdcr_el2, x0


	/**********************
	 ** Load pic context **
	 **********************/

	ldr x0, [sp]

	ldr x1,     [x0], #8    /* lr0        */
	ldp w2, w3, [x0]        /* apr, vmcr  */

	msr S3_4_C12_C12_0, x1
	msr S3_4_C12_C9_0,  x2
	msr S3_4_C12_C11_7, x3

	mov  x0, #1              /* enable PIC virtualization */
	msr  S3_4_C12_C11_0, x0

	/** enable VM mode **/
	movz x1, #0b1110000000111001
	movk x1, #0b10111, lsl 16
	mrs  x0, hcr_el2
	orr  x0, x0, x1
	msr  hcr_el2, x0

	ldr  x30, [sp, #16]  /* load head of Vcpu_state again */

	/** general-purpose registers **/
	ldp   x0,  x1, [x30], #2*8
	ldp   x2,  x3, [x30], #2*8
	ldp   x4,  x5, [x30], #2*8
	ldp   x6,  x7, [x30], #2*8
	ldp   x8,  x9, [x30], #2*8
	ldp  x10, x11, [x30], #2*8
	ldp  x12, x13, [x30], #2*8
	ldp  x14, x15, [x30], #2*8
	ldp  x16, x17, [x30], #2*8
	ldp  x18, x19, [x30], #2*8
	ldp  x20, x21, [x30], #2*8
	ldp  x22, x23, [x30], #2*8
	ldp  x24, x25, [x30], #2*8
	ldp  x26, x27, [x30], #2*8
	ldp  x28, x29, [x30], #2*8
	ldr  x30, [x30]

	eret


_invalidate_tlb:
	msr  vttbr_el2, x1
	tlbi vmalle1is
	msr  vttbr_el2, xzr
	eret


_from_vm:

	/*********************
	 ** Save vm context **
	 *********************/

	/** general-purpose register **/
	stp   x0,  x1, [x29], #2*8
	stp   x2,  x3, [x29], #2*8
	stp   x4,  x5, [x29], #2*8
	stp   x6,  x7, [x29], #2*8
	stp   x8,  x9, [x29], #2*8
	stp  x10, x11, [x29], #2*8
	stp  x12, x13, [x29], #2*8
	stp  x14, x15, [x29], #2*8
	stp  x16, x17, [x29], #2*8
	stp  x18, x19, [x29], #2*8
	stp  x20, x21, [x29], #2*8
	stp  x22, x23, [x29], #2*8
	stp  x24, x25, [x29], #2*8
	stp  x26, x27, [x29], #2*8
	mov  x0,  x29
	mov  x1,  x30
	ldp  x29, x30, [sp], #2*8  /* pop x29, x30 from stack */
	stp  x28, x29, [x0], #2*8
	str       x30, [x0], #1*8

	/** save sp, ip, esr_el1, pstate and exception reason **/
	mrs  x2, sp_el0
	mrs  x3, elr_el2
	mrs  x4, esr_el1
	mrs  x5, spsr_el2
	mrs  x6, esr_el2
	stp  x2, x3, [x0], #2*8 /* sp, ip                  */
	stp  x4, x5, [x0], #2*8 /* esr_el1, pstate         */
	stp  x1, x6, [x0], #2*8 /* exception_type, esr_el2 */

	/** fpu registers **/
	mrs  x1, fpcr
	mrs  x2, fpsr
	stp  w1,  w2,  [x0], #2*4
	stp  q0,  q1,  [x0], #32
	stp  q2,  q3,  [x0], #32
	stp  q4,  q5,  [x0], #32
	stp  q6,  q7,  [x0], #32
	stp  q8,  q9,  [x0], #32
	stp  q10, q11, [x0], #32
	stp  q12, q13, [x0], #32
	stp  q14, q15, [x0], #32
	stp  q16, q17, [x0], #32
	stp  q18, q19, [x0], #32
	stp  q20, q21, [x0], #32
	stp  q22, q23, [x0], #32
	stp  q24, q25, [x0], #32
	stp  q26, q27, [x0], #32
	stp  q28, q29, [x0], #32
	stp  q30, q31, [x0], #32

	mrs  x3, elr_el1
	mrs  x4, sp_el1
	mrs  x5, spsr_el1
	mrs  x6, sctlr_el1
	mrs  x7, actlr_el1
	mrs  x8, vbar_el1
	mrs  x9, cpacr_el1
	mrs x10, afsr0_el1
	mrs x11, afsr1_el1
	mrs x12, contextidr_el1
	mrs x13, ttbr0_el1
	mrs x14, ttbr1_el1
	mrs x15, tcr_el1
	mrs x16, mair_el1
	mrs x17, amair_el1
	mrs x18, far_el1
	mrs x19, par_el1
	mrs x20, tpidrro_el0
	mrs x21, tpidr_el0
	mrs x22, tpidr_el1
	mrs x23, far_el2
	mrs x24, hpfar_el2
	stp  x3,  x4, [x0], #2*8
	stp  x5,  x6, [x0], #2*8
	stp  x7,  x8, [x0], #2*8
	stp  w9, w10, [x0], #2*4
	stp w11, w12, [x0], #2*4
	stp x13, x14, [x0], #2*8
	stp x15, x16, [x0], #2*8
	stp x17, x18, [x0], #2*8
	stp x19, x20, [x0], #2*8
	stp x21, x22, [x0], #3*8
	stp x23, x24, [x0], #2*8


	/**********************
	 ** save timer state **
	 **********************/

	mrs x25, cntvoff_el2
	mrs x26, cntv_cval_el0
	mrs x27, cntv_ctl_el0
	mrs x28, cntkctl_el1
	stp x25, x26, [x0], #2*8
	stp w27, w28, [x0], #2*4
	sub x0,  x0,  #0x3f0 /* overall vcpu state size */

	mov  x1, #0b111
	msr  cnthctl_el2, x1

	ldr x29,     [sp], #2*8  /* pop vm pic state from stack       */
	ldp x2, x30, [sp], #2*8  /* pop vm, and host state from stack */


	/**********************
	 ** Save pic context **
	 **********************/

	mrs x10, S3_4_C12_C12_0
	mrs x11, S3_4_C12_C9_0
	mrs x12, S3_4_C12_C11_7
	mrs x13, S3_4_C12_C11_2
	mrs x14, S3_4_C12_C11_3
	mrs x15, S3_4_C12_C11_5

	str x10,      [x29], #8    /* lr0        */
	stp w11, w12, [x29], #2*4  /* apr, vmcr  */
	stp w13, w14, [x29], #2*4  /* misr, eisr */
	str w15,      [x29]        /* elrsr      */

	msr  S3_4_C12_C11_0, xzr   /* disable PIC virtualization */


	/***********************
	 ** Load host context **
	 ***********************/

	add x30, x30,        #32*8    /* skip general-purpose regs, sp       */
	ldr  x1,      [x30], #2*8     /* host state ip, skip esr_el1         */
	ldr  x2,      [x30], #3*8     /* host state pstate,
	                                 skip exception_type and esr_el2     */
	ldp  w3,  w4, [x30], #2*4     /* fpcr and fpsr                       */
	add x30, x30,        #32*16+8 /* skip remaining fpu regs and elr_el1 */
	ldr       x5, [x30], #2*8     /* sp_el1                              */
	ldp  x6,  x7, [x30], #2*8     /* sctlr_el1, actlr_el1                */
	ldr       x8, [x30], #1*8     /* vbar_el1                            */
	ldr       w9, [x30], #4*4     /* cpacr_el1                           */
	ldp x10, x11, [x30], #2*8     /* ttbr0_el1, ttbr1_el1                */
	ldp x12, x13, [x30], #2*8     /* tcr_el1, mair_el1                   */
	ldr      x14, [x30]           /* amair_el1                           */

	msr elr_el2,    x1
	msr spsr_el2,   x2
	msr fpcr,       x3
	msr fpsr,       x4
	msr sp_el1,     x5
	msr sctlr_el1,  x6
	msr actlr_el1,  x7
	msr vbar_el1,   x8
	msr cpacr_el1,  x9
	msr ttbr0_el1,  x10
	msr ttbr1_el1,  x11
	msr tcr_el1,    x12
	msr mair_el1,   x13
	msr amair_el1,  x14
	mrs x1, mpidr_el1
	msr vmpidr_el2, x1


	/************************
	 ** debug/perfm access **
	 ************************/

	mrs  x1, mdcr_el2
	movz x2, #0b111101100000
	bic  x1, x1, x2
	msr  mdcr_el2, x1

	/** disable VM mode **/
	movz x2, #0b1110000000111001
	movk x2, #0b10111, lsl 16
	mrs  x1, hcr_el2
	msr vttbr_el2, xzr   /* stage2 table pointer zeroing */
	bic  x1, x1, x2
	msr  hcr_el2, x1

	eret

/* host kernel must jump to this point to switch to a vm */
.global hypervisor_call
hypervisor_call:
	hvc #0
	ret
