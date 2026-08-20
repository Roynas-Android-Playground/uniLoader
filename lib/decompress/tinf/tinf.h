/*
 * tinf - tiny inflate library (inflate, gzip, zlib)
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
 * Adapted for uniLoader: only the API needed by the gzip kernel decoder is
 * declared here.
 */
#ifndef TINF_H_INCLUDED
#define TINF_H_INCLUDED

typedef enum {
	TINF_OK         = 0,
	TINF_DATA_ERROR = -3,
	TINF_BUF_ERROR  = -5
} tinf_error_code;

void tinf_init(void);
int tinf_uncompress(void *dest, unsigned int *destLen,
		    const void *source, unsigned int sourceLen);
int tinf_gzip_uncompress(void *dest, unsigned int *destLen,
			 const void *source, unsigned int sourceLen);
unsigned int tinf_crc32(const void *data, unsigned int length);

#endif
