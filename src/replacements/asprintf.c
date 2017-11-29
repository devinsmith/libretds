/*
 * asprintf(3)
 * 20020809 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 */

#include <config.h>

#if !HAVE_ASPRINTF

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "tds_sysdep_private.h"
#include "replacements.h"

TDS_RCSID(var, "$Id: asprintf.c,v 1.7 2006/08/23 14:26:20 freddy77 Exp $");

int
asprintf(char **ret, const char *fmt, ...)
{
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vasprintf(ret, fmt, ap);
	va_end(ap);
	return len;
}

#endif /* !HAVE_ASPRINTF */
