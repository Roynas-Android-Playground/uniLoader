// SPDX-License-Identifier: GPL-2.0-only
/* gzip wrapper around the small zlib-licensed tinf inflater. */

#include <lib/decompress.h>
#include <stddef.h>

#include "tinf/tinf.h"

int kernel_decompress_gzip(void *dst, size_t dst_size, const void *src,
			   size_t src_size, size_t *out_size)
{
	unsigned int output_len;
	int ret;

	/* tinf's public interface is intentionally 32-bit sized. */
	if (src_size > (unsigned int)-1)
		return KERNEL_PAYLOAD_INVALID;

	output_len = dst_size > (unsigned int)-1 ? (unsigned int)-1 :
		     (unsigned int)dst_size;
	ret = tinf_gzip_uncompress(dst, &output_len, src,
				   (unsigned int)src_size);
	if (ret == TINF_BUF_ERROR)
		return KERNEL_PAYLOAD_NOSPACE;
	if (ret != TINF_OK)
		return KERNEL_PAYLOAD_INVALID;

	*out_size = output_len;
	return KERNEL_PAYLOAD_OK;
}
