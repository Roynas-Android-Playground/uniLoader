// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 * Copyright (c) 2026, Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <lib/debug.h>
#include <lib/console.h>
#include <main/boot.h>
#include <string.h>

#define ARM64_IMAGE_MAGIC_OFFSET 0x38
#define ARM64_IMAGE_MAGIC 0x644d5241U
#define SCTLR_A (1UL << 1)

static unsigned long disable_alignment_checks(void)
{
	unsigned long sctlr;

	__asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr));
	if (sctlr & SCTLR_A) {
		__asm__ volatile("msr SCTLR_EL1, %0\n\tisb"
				 : : "r"(sctlr & ~SCTLR_A) : "memory");
	}
	return sctlr;
}

static void restore_alignment_checks(unsigned long sctlr)
{
	__asm__ volatile("msr SCTLR_EL1, %0\n\tisb"
			 : : "r"(sctlr) : "memory");
}

static void clean_dcache_range(const void *start, unsigned long size)
{
	unsigned long ctr, line_size, address, end;

	if (!size)
		return;
	__asm__ volatile("mrs %0, CTR_EL0" : "=r"(ctr));
	line_size = 4UL << ((ctr >> 16) & 0xf);
	address = (unsigned long)start & ~(line_size - 1);
	end = (unsigned long)start + size;
	for (; address < end; address += line_size)
		__asm__ volatile("dc cvac, %0" : : "r"(address) : "memory");
}

static void sync_exec_range(const void *start, unsigned long size)
{
	unsigned long ctr, line_size, address, end;

	clean_dcache_range(start, size);
	__asm__ volatile("dsb sy" : : : "memory");

	__asm__ volatile("mrs %0, CTR_EL0" : "=r"(ctr));
	line_size = 4UL << (ctr & 0xf);
	address = (unsigned long)start & ~(line_size - 1);
	end = (unsigned long)start + size;
	for (; address < end; address += line_size)
		__asm__ volatile("ic ivau, %0" : : "r"(address) : "memory");

	__asm__ volatile("dsb sy\n\tisb" : : : "memory");
}

#ifdef CONFIG_KERNEL_DECOMPRESS
#include <lib/decompress.h>

static unsigned int read_le32(const unsigned char *p)
{
	return (unsigned int)p[0] |
	       ((unsigned int)p[1] << 8) |
	       ((unsigned int)p[2] << 16) |
	       ((unsigned int)p[3] << 24);
}

static int is_arm64_linux_image(const void *image, unsigned long size)
{
	const unsigned char *p = image;

	return size >= ARM64_IMAGE_MAGIC_OFFSET + 4 &&
	       read_le32(p + ARM64_IMAGE_MAGIC_OFFSET) == ARM64_IMAGE_MAGIC;
}
#endif

static void dump_handoff_status(const char *stage, void *dt,
				unsigned long image_size)
{
	const unsigned int *image = (void *)CONFIG_PAYLOAD_ENTRY;
	unsigned long current_el, daif, sctlr_el1, sp;
	unsigned int dt_magic = dt ? *(const volatile unsigned int *)dt : 0;

	__asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
	__asm__ volatile("mrs %0, DAIF" : "=r"(daif));
	__asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr_el1));
	__asm__ volatile("mov %0, sp" : "=r"(sp));

	printk(KERN_INFO,
	       "%s: el=%lx daif=%lx sctlr_el1=%lx sp=%lx size=%lu\n",
	       stage, current_el >> 2, daif, sctlr_el1, sp, image_size);
	printk(KERN_INFO,
	       "%s: image[0]=%x image_magic=%x dt=%lx dt_magic=%x\n",
	       stage, image[0], image[ARM64_IMAGE_MAGIC_OFFSET / 4],
	       (unsigned long)dt, dt_magic);
#ifdef CONFIG_EXYNOS_8890
	printk(KERN_INFO,
	       "%s: rst_stat=%x uart_utrstat=%x uart_ufstat=%x\n",
	       stage, *(const volatile unsigned int *)0x105c0404UL,
	       *(const volatile unsigned int *)0x14c50010UL,
	       *(const volatile unsigned int *)0x14c50018UL);
#endif
}

void arch_load_kernel(void* kernel, void* dt, void* ramdisk)
{
	unsigned long loaded_size = (unsigned long)&kernel_size;
	int sync_decompressed_image = 0;
#ifdef CONFIG_KERNEL_DECOMPRESS
	enum kernel_payload_format format = KERNEL_PAYLOAD_RAW;
	unsigned long input_size = (unsigned long)&kernel_size;
	size_t output_size = 0;
	unsigned long saved_sctlr;
	int ret;

	saved_sctlr = disable_alignment_checks();
	printk(KERN_INFO, "Kernel loader: temporary SCTLR.A clear (saved=%lx)\n",
	       saved_sctlr);
	console_flush();
	ret = kernel_payload_load((void *)CONFIG_PAYLOAD_ENTRY,
				  CONFIG_KERNEL_DECOMPRESS_MAX_SIZE,
				  kernel, input_size, &output_size, &format);
	restore_alignment_checks(saved_sctlr);
	if (ret) {
		printk(KERN_ERR, "Kernel %s payload load failed: %d\n",
		       kernel_payload_format_name(format), ret);
		return;
	}

	if (format != KERNEL_PAYLOAD_RAW &&
	    !is_arm64_linux_image((void *)CONFIG_PAYLOAD_ENTRY, output_size)) {
		printk(KERN_ERR, "Decompressed %s payload is not an arm64 Image\n",
		       kernel_payload_format_name(format));
		return;
	}

	printk(KERN_INFO, "Kernel payload: %s %lu -> %lu bytes\n",
	       kernel_payload_format_name(format), input_size,
	       (unsigned long)output_size);
	loaded_size = output_size;
	sync_decompressed_image = format != KERNEL_PAYLOAD_RAW;
#else
	memcpy((void*)CONFIG_PAYLOAD_ENTRY, kernel, (unsigned long)&kernel_size);
#endif

#ifndef CONFIG_RAMDISK_NO_COPY
	__optimized_memcpy((void*)CONFIG_RAMDISK_ENTRY, ramdisk,
			   (unsigned long)&ramdisk_size);
#endif
	if (sync_decompressed_image) {
		printk(KERN_INFO, "Synchronizing decompressed kernel caches\n");
		console_flush();
		sync_exec_range((void *)CONFIG_PAYLOAD_ENTRY, loaded_size);
		printk(KERN_INFO, "Decompressed kernel cache sync complete\n");
		printk(KERN_INFO,
		       "%s compressed kernel image decompression done, jumping to kernel\n",
		       kernel_payload_format_name(format));
	}
	printk(KERN_INFO,
	       "Starting kernel: entry=%lx dt=%lx initrd=%lx\n",
	       (unsigned long)CONFIG_PAYLOAD_ENTRY, (unsigned long)dt,
	       (unsigned long)CONFIG_RAMDISK_ENTRY);
	dump_handoff_status("pre-jump", dt, loaded_size);
	console_flush();
	load_kernel_and_jump(dt, 0, 0, 0, (void*)CONFIG_PAYLOAD_ENTRY);
	printk(KERN_EMERG, "Kernel returned to uniLoader\n");
	dump_handoff_status("post-return", dt, loaded_size);
	console_flush();
}
