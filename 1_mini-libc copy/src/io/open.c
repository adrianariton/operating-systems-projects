// SPDX-License-Identifier: BSD-3-Clause

#include <fcntl.h>
#include <internal/syscall.h>
#include <stdarg.h>
#include <errno.h>

int open(const char *filename, int flags, ...)
{
	va_list valist;
    va_start(valist, flags);
	int a, b, c;
	a = flags;
	b = va_arg(valist, int);
	c = va_arg(valist, int);
	va_end(valist);
	long ret = syscall(2, filename, a, b, c);

	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
