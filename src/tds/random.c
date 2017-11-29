/*
 * Copyright (c) 2012-2016 Devin Smith <devin@devinsmith.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>

#include <config.h>

void
tds_random_buffer(unsigned char *out, int len)
{
	int i;

#if defined(HAVE_OPENSSL)
	if (RAND_bytes(out, len) == 1)
		return;
	if (RAND_pseudo_bytes(out, len) >= 0)
		return;
#endif

#if defined(HAVE_ARC4RANDOM_BUF)
	arc4random_buf(out, len);
#else
	/* TODO find a better random... */
	for (i = 0; i < len; ++i)
		out[i] = rand() / (RAND_MAX / 256);
#endif
}
