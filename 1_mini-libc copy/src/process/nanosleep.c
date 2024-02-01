#include <internal/syscall.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
    int ret = syscall(__NR_nanosleep, rqtp, rmtp);

	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
