// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <internal/syscall.h>
#include <errno.h>

off_t lseek(int fd, off_t offset, int whence)
{
	/* TODO: Implement lseek(). */
	long ret = syscall(8, fd, offset, whence);

	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
