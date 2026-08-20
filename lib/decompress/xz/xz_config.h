/* SPDX-License-Identifier: 0BSD */
/* Freestanding configuration glue for Linux' XZ Embedded decoder. */
#ifndef XZ_CONFIG_H
#define XZ_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "xz.h"

#define XZ_DEC_SINGLE
#define XZ_INTERNAL_CRC32 1

#if defined(__aarch64__)
#define XZ_DEC_ARM64
#elif defined(__arm__)
/* Accept either ARM BCJ flavor; the compressed kernel chooses the filter. */
#define XZ_DEC_ARM
#define XZ_DEC_ARMTHUMB
#endif

void *xz_alloc(size_t size);
void *xz_memcpy(void *dst, const void *src, size_t size);
void *xz_memmove(void *dst, const void *src, size_t size);
void *xz_memset(void *dst, int value, size_t size);

#define kmalloc_obj(obj) xz_alloc(sizeof(obj))
#define kfree(ptr) do { (void)(ptr); } while (0)
#define vmalloc(size) xz_alloc(size)
#define vfree(ptr) do { (void)(ptr); } while (0)

#define memeq(a, b, size) (memcmp((a), (b), (size)) == 0)
#define memcpy(dst, src, size) xz_memcpy((dst), (src), (size))
#define memmove(dst, src, size) xz_memmove((dst), (src), (size))
#define memset(dst, value, size) xz_memset((dst), (value), (size))
#define memzero(buf, size) xz_memset((buf), 0, (size))

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#define min_t(type, x, y) ((type)min((x), (y)))

#ifndef __always_inline
#define __always_inline inline __attribute__((__always_inline__))
#endif

#ifndef fallthrough
#define fallthrough do { } while (0)
#endif

static inline uint32_t get_unaligned_le32(const uint8_t *buf)
{
	return (uint32_t)buf[0] |
	       ((uint32_t)buf[1] << 8) |
	       ((uint32_t)buf[2] << 16) |
	       ((uint32_t)buf[3] << 24);
}

static inline uint32_t get_unaligned_be32(const uint8_t *buf)
{
	return ((uint32_t)buf[0] << 24) |
	       ((uint32_t)buf[1] << 16) |
	       ((uint32_t)buf[2] << 8) |
	       (uint32_t)buf[3];
}

static inline void put_unaligned_le32(uint32_t val, uint8_t *buf)
{
	buf[0] = (uint8_t)val;
	buf[1] = (uint8_t)(val >> 8);
	buf[2] = (uint8_t)(val >> 16);
	buf[3] = (uint8_t)(val >> 24);
}

static inline void put_unaligned_be32(uint32_t val, uint8_t *buf)
{
	buf[0] = (uint8_t)(val >> 24);
	buf[1] = (uint8_t)(val >> 16);
	buf[2] = (uint8_t)(val >> 8);
	buf[3] = (uint8_t)val;
}

#define get_le32 get_unaligned_le32

#endif
