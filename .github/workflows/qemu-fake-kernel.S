/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Synthetic arm64 "kernel" used only by qemu_boot_test.sh to verify that
 * uniLoader's kernel loader (raw/gzip/lz4/xz) can locate, decompress and
 * jump into a payload under QEMU. This is not a real kernel: it wears a
 * minimal arm64 Linux Image header (so the post-decompress magic check in
 * arch/aarch64/load-kernel.c accepts it), prints a fixed marker string to
 * the QEMU virt PL011 UART at 0x09000000, and then spins in WFI so the test
 * harness can capture the marker and kill QEMU after a timeout.
 *
 * Built with: aarch64-linux-gnu-{as,ld,objcopy}, linked at CONFIG_PAYLOAD_ENTRY.
 */
.section .text
.global _start
_start:
	b	code_start	/* code0: arm64 Image header requires real code here */
	.word	0		/* code1 */
	.quad	0		/* text_offset (unused; we execute from offset 0) */
	.quad	_end - _start	/* image_size */
	.quad	0		/* flags */
	.quad	0		/* res2 */
	.quad	0		/* res3 */
	.quad	0		/* res4 */
	.word	0x644d5241	/* magic: "ARM\x64" */
	.word	0		/* res5 */

code_start:
	movz	x2, #0x0000
	movk	x2, #0x0900, lsl #16	/* x2 = 0x09000000 (QEMU virt PL011) */
	adr	x0, msg
1:	ldrb	w1, [x0], #1
	cbz	w1, 2f
	strb	w1, [x2]
	b	1b
2:	wfi
	b	2b

.align 3
msg:
	.asciz "UNILOADER-QEMU-BOOT-OK\n"
.align 3
_end:
