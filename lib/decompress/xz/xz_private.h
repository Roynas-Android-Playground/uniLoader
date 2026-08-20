/* SPDX-License-Identifier: 0BSD */

/*
 * Private includes and definitions
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 */
#ifndef XZ_PRIVATE_H
#define XZ_PRIVATE_H

#include "xz_config.h"

#if !defined(XZ_DEC_SINGLE) && !defined(XZ_DEC_PREALLOC) && \
    !defined(XZ_DEC_DYNALLOC)
#define XZ_DEC_SINGLE
#define XZ_DEC_PREALLOC
#define XZ_DEC_DYNALLOC
#endif

#ifdef XZ_DEC_SINGLE
#define DEC_IS_SINGLE(mode) ((mode) == XZ_SINGLE)
#else
#define DEC_IS_SINGLE(mode) (false)
#endif

#ifdef XZ_DEC_PREALLOC
#define DEC_IS_PREALLOC(mode) ((mode) == XZ_PREALLOC)
#else
#define DEC_IS_PREALLOC(mode) (false)
#endif

#ifdef XZ_DEC_DYNALLOC
#define DEC_IS_DYNALLOC(mode) ((mode) == XZ_DYNALLOC)
#else
#define DEC_IS_DYNALLOC(mode) (false)
#endif

#if !defined(XZ_DEC_SINGLE)
#define DEC_IS_MULTI(mode) (true)
#elif defined(XZ_DEC_PREALLOC) || defined(XZ_DEC_DYNALLOC)
#define DEC_IS_MULTI(mode) ((mode) != XZ_SINGLE)
#else
#define DEC_IS_MULTI(mode) (false)
#endif

#ifndef XZ_DEC_BCJ
#if defined(XZ_DEC_X86) || defined(XZ_DEC_POWERPC) || defined(XZ_DEC_ARM) || \
    defined(XZ_DEC_ARMTHUMB) || defined(XZ_DEC_SPARC) || \
    defined(XZ_DEC_ARM64) || defined(XZ_DEC_RISCV)
#define XZ_DEC_BCJ
#endif
#endif

struct xz_dec_lzma2 *xz_dec_lzma2_create(enum xz_mode mode, uint32_t dict_max);
enum xz_ret xz_dec_lzma2_reset(struct xz_dec_lzma2 *s, uint8_t props);
enum xz_ret xz_dec_lzma2_run(struct xz_dec_lzma2 *s, struct xz_buf *b);
void xz_dec_lzma2_end(struct xz_dec_lzma2 *s);

#ifdef XZ_DEC_BCJ
struct xz_dec_bcj *xz_dec_bcj_create(bool single_call);
enum xz_ret xz_dec_bcj_reset(struct xz_dec_bcj *s, uint8_t id);
enum xz_ret xz_dec_bcj_run(struct xz_dec_bcj *s, struct xz_dec_lzma2 *lzma2,
			   struct xz_buf *b);
#define xz_dec_bcj_end(s) kfree(s)
#endif

#endif
