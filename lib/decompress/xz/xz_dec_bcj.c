// SPDX-License-Identifier: 0BSD
/*
 * ARM/ARM64 Branch/Call/Jump filter decoders from XZ Embedded.
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <https://7-zip.org/>
 *
 * uniLoader only targets ARM and AArch64, so unrelated BCJ implementations
 * are omitted from this copy.
 */

#include "xz_private.h"

#ifdef XZ_DEC_BCJ

struct xz_dec_bcj {
	enum {
		BCJ_ARM = 7,
		BCJ_ARMTHUMB = 8,
		BCJ_ARM64 = 10,
	} type;
	enum xz_ret ret;
	bool single_call;
	uint32_t pos;
	uint8_t *out;
	size_t out_pos;
	size_t out_size;
	struct {
		size_t filtered;
		size_t size;
		uint8_t buf[16];
	} temp;
};

#ifdef XZ_DEC_ARM
static size_t bcj_arm(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t addr;

	size &= ~(size_t)3;
	for (i = 0; i < size; i += 4) {
		if (buf[i + 3] == 0xeb) {
			addr = (uint32_t)buf[i] | ((uint32_t)buf[i + 1] << 8) |
			       ((uint32_t)buf[i + 2] << 16);
			addr <<= 2;
			addr -= s->pos + (uint32_t)i + 8;
			addr >>= 2;
			buf[i] = (uint8_t)addr;
			buf[i + 1] = (uint8_t)(addr >> 8);
			buf[i + 2] = (uint8_t)(addr >> 16);
		}
	}
	return i;
}
#endif

#ifdef XZ_DEC_ARMTHUMB
static size_t bcj_armthumb(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t addr;

	if (size < 4)
		return 0;
	size -= 4;

	for (i = 0; i <= size; i += 2) {
		if ((buf[i + 1] & 0xf8) == 0xf0 &&
		    (buf[i + 3] & 0xf8) == 0xf8) {
			addr = (((uint32_t)buf[i + 1] & 0x07) << 19) |
			       ((uint32_t)buf[i] << 11) |
			       (((uint32_t)buf[i + 3] & 0x07) << 8) |
			       (uint32_t)buf[i + 2];
			addr <<= 1;
			addr -= s->pos + (uint32_t)i + 4;
			addr >>= 1;
			buf[i + 1] = (uint8_t)(0xf0 | ((addr >> 19) & 0x07));
			buf[i] = (uint8_t)(addr >> 11);
			buf[i + 3] = (uint8_t)(0xf8 | ((addr >> 8) & 0x07));
			buf[i + 2] = (uint8_t)addr;
			i += 2;
		}
	}
	return i;
}
#endif

#ifdef XZ_DEC_ARM64
static size_t bcj_arm64(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t instr;
	uint32_t addr;

	size &= ~(size_t)3;
	for (i = 0; i < size; i += 4) {
		instr = get_unaligned_le32(buf + i);

		if ((instr >> 26) == 0x25) {
			addr = instr - ((s->pos + (uint32_t)i) >> 2);
			instr = 0x94000000 | (addr & 0x03ffffff);
			put_unaligned_le32(instr, buf + i);
		} else if ((instr & 0x9f000000) == 0x90000000) {
			addr = ((instr >> 29) & 3) | ((instr >> 3) & 0x1ffffc);
			if ((addr + 0x020000) & 0x1c0000)
				continue;

			addr -= (s->pos + (uint32_t)i) >> 12;
			instr &= 0x9000001f;
			instr |= (addr & 3) << 29;
			instr |= (addr & 0x03fffc) << 3;
			instr |= (0U - (addr & 0x020000)) & 0xe00000;
			put_unaligned_le32(instr, buf + i);
		}
	}
	return i;
}
#endif

static void bcj_apply(struct xz_dec_bcj *s, uint8_t *buf,
		      size_t *pos, size_t size)
{
	size_t filtered = 0;

	buf += *pos;
	size -= *pos;

	switch (s->type) {
#ifdef XZ_DEC_ARM
	case BCJ_ARM:
		filtered = bcj_arm(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_ARMTHUMB
	case BCJ_ARMTHUMB:
		filtered = bcj_armthumb(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_ARM64
	case BCJ_ARM64:
		filtered = bcj_arm64(s, buf, size);
		break;
#endif
	default:
		break;
	}

	*pos += filtered;
	s->pos += filtered;
}

static void bcj_flush(struct xz_dec_bcj *s, struct xz_buf *b)
{
	size_t copy_size = min_t(size_t, s->temp.filtered,
				 b->out_size - b->out_pos);

	memcpy(b->out + b->out_pos, s->temp.buf, copy_size);
	b->out_pos += copy_size;
	s->temp.filtered -= copy_size;
	s->temp.size -= copy_size;
	memmove(s->temp.buf, s->temp.buf + copy_size, s->temp.size);
}

enum xz_ret xz_dec_bcj_run(struct xz_dec_bcj *s, struct xz_dec_lzma2 *lzma2,
			   struct xz_buf *b)
{
	size_t out_start;

	if (s->temp.filtered > 0) {
		bcj_flush(s, b);
		if (s->temp.filtered > 0)
			return XZ_OK;
		if (s->ret == XZ_STREAM_END)
			return XZ_STREAM_END;
	}

	if (s->temp.size < b->out_size - b->out_pos || s->temp.size == 0) {
		out_start = b->out_pos;
		memcpy(b->out + b->out_pos, s->temp.buf, s->temp.size);
		b->out_pos += s->temp.size;

		s->ret = xz_dec_lzma2_run(lzma2, b);
		if (s->ret != XZ_STREAM_END &&
		    (s->ret != XZ_OK || s->single_call))
			return s->ret;

		bcj_apply(s, b->out, &out_start, b->out_pos);
		if (s->ret == XZ_STREAM_END)
			return XZ_STREAM_END;

		s->temp.size = b->out_pos - out_start;
		b->out_pos -= s->temp.size;
		memcpy(s->temp.buf, b->out + b->out_pos, s->temp.size);
		if (b->out_pos + s->temp.size < b->out_size)
			return XZ_OK;
	}

	if (b->out_pos < b->out_size) {
		s->out = b->out;
		s->out_pos = b->out_pos;
		s->out_size = b->out_size;
		b->out = s->temp.buf;
		b->out_pos = s->temp.size;
		b->out_size = sizeof(s->temp.buf);

		s->ret = xz_dec_lzma2_run(lzma2, b);
		s->temp.size = b->out_pos;
		b->out = s->out;
		b->out_pos = s->out_pos;
		b->out_size = s->out_size;

		if (s->ret != XZ_OK && s->ret != XZ_STREAM_END)
			return s->ret;

		bcj_apply(s, s->temp.buf, &s->temp.filtered, s->temp.size);
		if (s->ret == XZ_STREAM_END)
			s->temp.filtered = s->temp.size;

		bcj_flush(s, b);
		if (s->temp.filtered > 0)
			return XZ_OK;
	}

	return s->ret;
}

struct xz_dec_bcj *xz_dec_bcj_create(bool single_call)
{
	struct xz_dec_bcj *s = kmalloc_obj(*s);
	if (s != NULL)
		s->single_call = single_call;
	return s;
}

enum xz_ret xz_dec_bcj_reset(struct xz_dec_bcj *s, uint8_t id)
{
	switch (id) {
#ifdef XZ_DEC_ARM
	case BCJ_ARM:
#endif
#ifdef XZ_DEC_ARMTHUMB
	case BCJ_ARMTHUMB:
#endif
#ifdef XZ_DEC_ARM64
	case BCJ_ARM64:
#endif
		break;
	default:
		return XZ_OPTIONS_ERROR;
	}

	s->type = id;
	s->ret = XZ_OK;
	s->pos = 0;
	s->temp.filtered = 0;
	s->temp.size = 0;
	return XZ_OK;
}

#endif
