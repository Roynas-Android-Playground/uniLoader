// SPDX-License-Identifier: GPL-2.0-only
#ifndef _UNILOADER_DECOMPRESS_H_
#define _UNILOADER_DECOMPRESS_H_

#include <stddef.h>

enum kernel_payload_format {
	KERNEL_PAYLOAD_RAW = 0,
	KERNEL_PAYLOAD_GZIP,
	KERNEL_PAYLOAD_LZ4,
	KERNEL_PAYLOAD_XZ,
};

enum kernel_payload_error {
	KERNEL_PAYLOAD_OK = 0,
	KERNEL_PAYLOAD_INVALID = -1,
	KERNEL_PAYLOAD_NOSPACE = -2,
	KERNEL_PAYLOAD_UNSUPPORTED = -3,
	KERNEL_PAYLOAD_NOMEM = -4,
};

/*
 * Detect and unpack an embedded Linux kernel image.
 *
 * Raw images are copied exactly as before and aren't constrained by dst_size;
 * dst_size is the safety ceiling for decompressed payloads. This preserves the
 * historical raw Image behavior while bounding malformed compressed streams.
 */
int kernel_payload_load(void *dst, size_t dst_size, const void *src,
			size_t src_size, size_t *out_size,
			enum kernel_payload_format *format);

const char *kernel_payload_format_name(enum kernel_payload_format format);
const char *kernel_payload_error_name(int error);

#endif
