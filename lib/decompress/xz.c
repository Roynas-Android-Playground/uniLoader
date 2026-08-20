// SPDX-License-Identifier: GPL-2.0-only
/* Boot-time wrapper for the 0BSD XZ Embedded decoder. */

#include <lib/decompress.h>
#include <stddef.h>
#include <stdint.h>

#include "xz/xz.h"

/* XZ_SINGLE needs < 30 KiB of state; leave comfortable headroom for BCJ. */
#define XZ_WORKSPACE_SIZE (64 * 1024)

static unsigned long xz_workspace[XZ_WORKSPACE_SIZE / sizeof(unsigned long)];
static size_t xz_workspace_pos;

void *xz_alloc(size_t size)
{
	const size_t align = sizeof(unsigned long);
	unsigned char *base = (unsigned char *)xz_workspace;
	void *ptr;

	if (size > (size_t)-1 - (align - 1))
		return NULL;

	size = (size + align - 1) & ~(align - 1);
	if (size > XZ_WORKSPACE_SIZE - xz_workspace_pos)
		return NULL;

	ptr = base + xz_workspace_pos;
	xz_workspace_pos += size;
	return ptr;
}

void *xz_memmove(void *dst, const void *src, size_t size)
{
	unsigned char *d = dst;
	const unsigned char *s = src;

	if (d == s || !size)
		return dst;

	if (d < s) {
		while (size--)
			*d++ = *s++;
	} else {
		d += size;
		s += size;
		while (size--)
			*--d = *--s;
	}

	return dst;
}

void *xz_memcpy(void *dst, const void *src, size_t size)
{
	unsigned char *d = dst;
	const unsigned char *s = src;

	while (size--)
		*d++ = *s++;
	return dst;
}

void *xz_memset(void *dst, int value, size_t size)
{
	unsigned char *d = dst;

	while (size--)
		*d++ = (unsigned char)value;
	return dst;
}

int kernel_decompress_xz(void *dst, size_t dst_size, const void *src,
			 size_t src_size, size_t *out_size)
{
	struct xz_buf buffer = {
		.in = src,
		.in_pos = 0,
		.in_size = src_size,
		.out = dst,
		.out_pos = 0,
		.out_size = dst_size,
	};
	struct xz_dec *decoder;
	enum xz_ret ret;

	xz_workspace_pos = 0;
	xz_crc32_init();

	decoder = xz_dec_init(XZ_SINGLE, 0);
	if (!decoder)
		return KERNEL_PAYLOAD_NOMEM;

	ret = xz_dec_run(decoder, &buffer);
	xz_dec_end(decoder);

	if (ret == XZ_BUF_ERROR)
		return KERNEL_PAYLOAD_NOSPACE;
	if (ret != XZ_STREAM_END)
		return KERNEL_PAYLOAD_INVALID;

	*out_size = buffer.out_pos;
	return KERNEL_PAYLOAD_OK;
}
