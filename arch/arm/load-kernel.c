// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 * Copyright (c) 2026, Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <lib/debug.h>
#include <main/boot.h>
#include <string.h>

#ifdef CONFIG_KERNEL_DECOMPRESS
#include <lib/decompress.h>
#endif

void arch_load_kernel(void* kernel, void* dt, void* ramdisk)
{
#ifdef CONFIG_KERNEL_DECOMPRESS
	enum kernel_payload_format format = KERNEL_PAYLOAD_RAW;
	unsigned long input_size = (unsigned long)kernel_size;
	size_t output_size = 0;
	int ret;

	ret = kernel_payload_load((void *)CONFIG_PAYLOAD_ENTRY,
				  CONFIG_KERNEL_DECOMPRESS_MAX_SIZE,
				  kernel, input_size, &output_size, &format);
	if (ret) {
		printk(KERN_ERR, "Kernel %s payload load failed: %d (%s)\n",
		       kernel_payload_format_name(format), ret,
		       kernel_payload_error_name(ret));
		return;
	}

	printk(KERN_INFO, "Kernel payload: %s %lu -> %lu bytes\n",
	       kernel_payload_format_name(format), input_size,
	       (unsigned long)output_size);
#else
	memcpy((void*)CONFIG_PAYLOAD_ENTRY, kernel, (unsigned long)kernel_size);
#endif

#ifndef CONFIG_RAMDISK_NO_COPY
	memcpy((void*)CONFIG_RAMDISK_ENTRY, ramdisk, (unsigned long)ramdisk_size);
#endif
	load_kernel_and_jump(0, 0, dt, (void*)CONFIG_PAYLOAD_ENTRY);
}
