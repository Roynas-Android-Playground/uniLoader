// SPDX-License-Identifier: GPL-2.0-only
/*
 * Embedded Linux kernel payload decompression.
 *
 * Keep format detection here so architecture handoff code only needs to care
 * about the final, uncompressed Image at CONFIG_PAYLOAD_ENTRY.
 */

#include <lib/decompress.h>
#include <lib/debug.h>
#include <string.h>

#include "internal.h"

static int payload_has_magic(const unsigned char *src, size_t src_size,
			     const unsigned char *magic, size_t magic_size)
{
	return src_size >= magic_size && !memcmp(src, magic, magic_size);
}

static enum kernel_payload_format kernel_payload_detect(const void *src,
							 size_t src_size)
{
	static const unsigned char gzip_magic[] = { 0x1f, 0x8b };
	static const unsigned char lz4_magic[] = { 0x02, 0x21, 0x4c, 0x18 };
	static const unsigned char xz_magic[] = { 0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00 };
	const unsigned char *bytes = src;

	if (payload_has_magic(bytes, src_size, gzip_magic, sizeof(gzip_magic)))
		return KERNEL_PAYLOAD_GZIP;

	/* Linux' Image.lz4 target uses the legacy LZ4 framing format. */
	if (payload_has_magic(bytes, src_size, lz4_magic, sizeof(lz4_magic)))
		return KERNEL_PAYLOAD_LZ4;

	if (payload_has_magic(bytes, src_size, xz_magic, sizeof(xz_magic)))
		return KERNEL_PAYLOAD_XZ;

	return KERNEL_PAYLOAD_RAW;
}

const char *kernel_payload_format_name(enum kernel_payload_format format)
{
	switch (format) {
	case KERNEL_PAYLOAD_GZIP:
		return "GZIP";
	case KERNEL_PAYLOAD_LZ4:
		return "LZ4";
	case KERNEL_PAYLOAD_XZ:
		return "XZ";
	case KERNEL_PAYLOAD_RAW:
	default:
		return "raw";
	}
}

const char *kernel_payload_error_name(int error)
{
	switch (error) {
	case KERNEL_PAYLOAD_OK:
		return "OK";
	case KERNEL_PAYLOAD_INVALID:
		return "invalid/corrupt payload";
	case KERNEL_PAYLOAD_NOSPACE:
		return "decompressed image exceeds CONFIG_KERNEL_DECOMPRESS_MAX_SIZE";
	case KERNEL_PAYLOAD_UNSUPPORTED:
		return "format support not compiled in";
	case KERNEL_PAYLOAD_NOMEM:
		return "out of memory";
	default:
		return "unknown error";
	}
}

int kernel_payload_load(void *dst, size_t dst_size, const void *src,
			size_t src_size, size_t *out_size,
			enum kernel_payload_format *format)
{
	enum kernel_payload_format detected;

	if (!dst || !src || !out_size)
		return KERNEL_PAYLOAD_INVALID;

	detected = kernel_payload_detect(src, src_size);
	if (format)
		*format = detected;
	if (detected != KERNEL_PAYLOAD_RAW)
		printk(KERN_INFO, "%s compressed kernel image, decompressing...\n",
		       kernel_payload_format_name(detected));

	switch (detected) {
	case KERNEL_PAYLOAD_GZIP:
#ifdef CONFIG_KERNEL_DECOMPRESS_GZIP
		return kernel_decompress_gzip(dst, dst_size, src, src_size,
					      out_size);
#else
		return KERNEL_PAYLOAD_UNSUPPORTED;
#endif

	case KERNEL_PAYLOAD_LZ4:
#ifdef CONFIG_KERNEL_DECOMPRESS_LZ4
		return kernel_decompress_lz4(dst, dst_size, src, src_size,
					     out_size);
#else
		return KERNEL_PAYLOAD_UNSUPPORTED;
#endif

	case KERNEL_PAYLOAD_XZ:
#ifdef CONFIG_KERNEL_DECOMPRESS_XZ
		return kernel_decompress_xz(dst, dst_size, src, src_size,
					    out_size);
#else
		return KERNEL_PAYLOAD_UNSUPPORTED;
#endif

	case KERNEL_PAYLOAD_RAW:
		/* Preserve the old behavior for an uncompressed Image. */
		memcpy(dst, src, src_size);
		*out_size = src_size;
		return KERNEL_PAYLOAD_OK;
	}

	return KERNEL_PAYLOAD_INVALID;
}
