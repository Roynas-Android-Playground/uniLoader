// SPDX-License-Identifier: GPL-2.0-only

#include <lib/console.h>
#include <lib/debug.h>

void aarch64_exception_handler(unsigned long vector, unsigned long current_el,
			       unsigned long esr, unsigned long elr,
			       unsigned long far, unsigned long spsr)
{
	printk(KERN_EMERG,
	       "AArch64 exception: vector=%lu EL=%lu ESR=%lx ELR=%lx\n",
	       vector, current_el >> 2, esr, elr);
	printk(KERN_EMERG, "AArch64 exception: FAR=%lx SPSR=%lx\n", far, spsr);
	console_flush();

	for (;;)
		__asm__ volatile("wfe");
}
