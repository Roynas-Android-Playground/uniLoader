// SPDX-License-Identifier: GPL-2.0-only
#ifndef _UNILOADER_DECOMPRESS_INTERNAL_H_
#define _UNILOADER_DECOMPRESS_INTERNAL_H_

#include <stddef.h>

#ifdef CONFIG_KERNEL_DECOMPRESS_GZIP
int kernel_decompress_gzip(void *dst, size_t dst_size, const void *src,
			   size_t src_size, size_t *out_size);
#endif

#ifdef CONFIG_KERNEL_DECOMPRESS_LZ4
int kernel_decompress_lz4(void *dst, size_t dst_size, const void *src,
			  size_t src_size, size_t *out_size);
#endif

#ifdef CONFIG_KERNEL_DECOMPRESS_XZ
int kernel_decompress_xz(void *dst, size_t dst_size, const void *src,
			 size_t src_size, size_t *out_size);
#endif

#endif
