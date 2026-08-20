/*
 * tinfgzip - tiny gzip decompressor
 *
 * Copyright (c) 2003-2019 Joergen Ibsen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must
 *      not claim that you wrote the original software.
 *   2. Altered source versions must be plainly marked as such, and must
 *      not be misrepresented as being the original software.
 *   3. This notice may not be removed or altered from any source
 *      distribution.
 *
 * Adapted for uniLoader: formatting/comments were reduced for the freestanding
 * build; gzip validation and inflate behavior are unchanged.
 */

#include "tinf.h"

typedef enum {
	FTEXT    = 1,
	FHCRC    = 2,
	FEXTRA   = 4,
	FNAME    = 8,
	FCOMMENT = 16
} tinf_gzip_flag;

static unsigned int read_le16(const unsigned char *p)
{
	return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8);
}

static unsigned int read_le32(const unsigned char *p)
{
	return ((unsigned int)p[0]) |
	       ((unsigned int)p[1] << 8) |
	       ((unsigned int)p[2] << 16) |
	       ((unsigned int)p[3] << 24);
}

int tinf_gzip_uncompress(void *dest, unsigned int *destLen,
			 const void *source, unsigned int sourceLen)
{
	const unsigned char *src = source;
	unsigned char *dst = dest;
	const unsigned char *start;
	unsigned int dlen, crc32;
	unsigned char flg;
	int res;

	if (sourceLen < 18)
		return TINF_DATA_ERROR;
	if (src[0] != 0x1f || src[1] != 0x8b || src[2] != 8)
		return TINF_DATA_ERROR;

	flg = src[3];
	if (flg & 0xe0)
		return TINF_DATA_ERROR;

	start = src + 10;

	if (flg & FEXTRA) {
		unsigned int xlen;

		if ((unsigned int)(start - src) > sourceLen - 2)
			return TINF_DATA_ERROR;
		xlen = read_le16(start);
		if (xlen > sourceLen - 12)
			return TINF_DATA_ERROR;
		start += xlen + 2;
	}

	if (flg & FNAME) {
		do {
			if ((unsigned int)(start - src) >= sourceLen)
				return TINF_DATA_ERROR;
		} while (*start++);
	}

	if (flg & FCOMMENT) {
		do {
			if ((unsigned int)(start - src) >= sourceLen)
				return TINF_DATA_ERROR;
		} while (*start++);
	}

	if (flg & FHCRC) {
		unsigned int hcrc;

		if ((unsigned int)(start - src) > sourceLen - 2)
			return TINF_DATA_ERROR;
		hcrc = read_le16(start);
		if (hcrc != (tinf_crc32(src, start - src) & 0xffff))
			return TINF_DATA_ERROR;
		start += 2;
	}

	if ((unsigned int)(start - src) > sourceLen - 8)
		return TINF_DATA_ERROR;

	crc32 = read_le32(src + sourceLen - 8);
	dlen = read_le32(src + sourceLen - 4);
	if (dlen > *destLen)
		return TINF_BUF_ERROR;

	res = tinf_uncompress(dst, destLen, start,
			      (src + sourceLen) - start - 8);
	if (res != TINF_OK || *destLen != dlen)
		return TINF_DATA_ERROR;
	if (crc32 != tinf_crc32(dst, dlen))
		return TINF_DATA_ERROR;

	return TINF_OK;
}
