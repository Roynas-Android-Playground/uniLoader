// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal safe decoder for the legacy LZ4 stream used by Linux Image.lz4.
 *
 * The arm/arm64 kernel build invokes `lz4 -l`, whose stream consists of the
 * legacy magic followed by independently-compressed blocks prefixed with a
 * little-endian compressed size. This avoids pulling the full LZ4 library into
 * a small secondary loader.
 */

#include <lib/decompress.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LZ4_LEGACY_MAGIC 0x184c2102U
#define LZ4_MIN_MATCH 4U

static uint32_t lz4_read_le32(const unsigned char *p)
{
	return (uint32_t)p[0] |
	       ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static int lz4_extend_length(const unsigned char **input,
			     const unsigned char *input_end,
			     size_t *length)
{
	unsigned int value;

	do {
		if (*input >= input_end)
			return KERNEL_PAYLOAD_INVALID;

		value = *(*input)++;
		if ((size_t)value > (size_t)-1 - *length)
			return KERNEL_PAYLOAD_INVALID;

		*length += value;
	} while (value == 255);

	return KERNEL_PAYLOAD_OK;
}

static int lz4_decode_block(void *dst, size_t dst_size, const void *src,
			    size_t src_size, size_t *produced)
{
	const unsigned char *ip = src;
	const unsigned char *iend = ip + src_size;
	unsigned char *op = dst;
	unsigned char *block_start = op;
	unsigned char *oend = op + dst_size;

	while (ip < iend) {
		unsigned int token = *ip++;
		size_t literal_len = token >> 4;
		size_t match_len;
		size_t offset;
		unsigned char *match;
		int ret;

		if (literal_len == 15) {
			ret = lz4_extend_length(&ip, iend, &literal_len);
			if (ret)
				return ret;
		}

		if (literal_len > (size_t)(iend - ip))
			return KERNEL_PAYLOAD_INVALID;
		if (literal_len > (size_t)(oend - op))
			return KERNEL_PAYLOAD_NOSPACE;

		for (size_t i = 0; i < literal_len; i++)
			op[i] = ip[i];
		op += literal_len;
		ip += literal_len;

		/* A valid LZ4 block ends with a literal-only sequence. */
		if (ip == iend)
			break;

		if ((size_t)(iend - ip) < 2)
			return KERNEL_PAYLOAD_INVALID;

		offset = (size_t)ip[0] | ((size_t)ip[1] << 8);
		ip += 2;

		if (!offset || offset > (size_t)(op - block_start))
			return KERNEL_PAYLOAD_INVALID;

		match_len = token & 0x0f;
		if (match_len == 15) {
			ret = lz4_extend_length(&ip, iend, &match_len);
			if (ret)
				return ret;
		}

		if (match_len > (size_t)-1 - LZ4_MIN_MATCH)
			return KERNEL_PAYLOAD_INVALID;
		match_len += LZ4_MIN_MATCH;

		if (match_len > (size_t)(oend - op))
			return KERNEL_PAYLOAD_NOSPACE;

		/* Bytewise copy deliberately handles overlapping matches. */
		match = op - offset;
		while (match_len--)
			*op++ = *match++;
	}

	*produced = op - block_start;
	return KERNEL_PAYLOAD_OK;
}

int kernel_decompress_lz4(void *dst, size_t dst_size, const void *src,
			  size_t src_size, size_t *out_size)
{
	const unsigned char *ip = src;
	const unsigned char *iend = ip + src_size;
	unsigned char *op = dst;
	size_t total = 0;
	int saw_stream = 0;

	if (src_size < 4 || lz4_read_le32(ip) != LZ4_LEGACY_MAGIC)
		return KERNEL_PAYLOAD_INVALID;

	ip += 4;
	saw_stream = 1;

	while (ip < iend) {
		uint32_t chunk_size;
		size_t chunk_output;
		int ret;

		if ((size_t)(iend - ip) < 4)
			return KERNEL_PAYLOAD_INVALID;

		chunk_size = lz4_read_le32(ip);
		ip += 4;

		/* The Linux decoder also permits concatenated legacy streams. */
		if (chunk_size == LZ4_LEGACY_MAGIC)
			continue;

		if (!chunk_size)
			break;

		if ((size_t)chunk_size > (size_t)(iend - ip))
			return KERNEL_PAYLOAD_INVALID;
		if (total >= dst_size)
			return KERNEL_PAYLOAD_NOSPACE;

		ret = lz4_decode_block(op, dst_size - total, ip, chunk_size,
				       &chunk_output);
		if (ret)
			return ret;

		op += chunk_output;
		total += chunk_output;
		ip += chunk_size;
	}

	if (!saw_stream || !total)
		return KERNEL_PAYLOAD_INVALID;

	*out_size = total;
	return KERNEL_PAYLOAD_OK;
}
