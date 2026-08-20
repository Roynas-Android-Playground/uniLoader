/* SPDX-License-Identifier: GPL-2.0-only */
#include "asmdefs.h"

.macro exception_vector id
	.align 7
	mov	x0, #\id
	b	exception_common
.endm

.section .text
.align 11
	.global exception_vectors
	.type exception_vectors, %function
exception_vectors:
	exception_vector 0
	exception_vector 1
	exception_vector 2
	exception_vector 3
	exception_vector 4
	exception_vector 5
	exception_vector 6
	exception_vector 7
	exception_vector 8
	exception_vector 9
	exception_vector 10
	exception_vector 11
	exception_vector 12
	exception_vector 13
	exception_vector 14
	exception_vector 15
	.size exception_vectors, . - exception_vectors

ENTRY(install_exception_vectors)
	adrp	x0, exception_vectors
	add	x0, x0, #:lo12:exception_vectors
	mrs	x1, CurrentEL
	cmp	x1, #0xc
	b.eq	1f
	cmp	x1, #0x8
	b.eq	2f
	cmp	x1, #0x4
	b.eq	3f
	ret
1:
	msr	VBAR_EL3, x0
	b	4f
2:
	msr	VBAR_EL2, x0
	b	4f
3:
	msr	VBAR_EL1, x0
4:
	isb
	ret
END(install_exception_vectors)

ENTRY(exception_common)
	mrs	x1, CurrentEL
	cmp	x1, #0xc
	b.eq	1f
	cmp	x1, #0x8
	b.eq	2f
	cmp	x1, #0x4
	b.eq	3f
	b	.
1:
	mrs	x2, ESR_EL3
	mrs	x3, ELR_EL3
	mrs	x4, FAR_EL3
	mrs	x5, SPSR_EL3
	b	4f
2:
	mrs	x2, ESR_EL2
	mrs	x3, ELR_EL2
	mrs	x4, FAR_EL2
	mrs	x5, SPSR_EL2
	b	4f
3:
	mrs	x2, ESR_EL1
	mrs	x3, ELR_EL1
	mrs	x4, FAR_EL1
	mrs	x5, SPSR_EL1
4:
	bl	aarch64_exception_handler
	b	.
END(exception_common)
