#ifndef __TIME_H__
#define __TIME_H__	1

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct timespec {
    long unsigned int tv_sec;	/* seconds */
    long tv_nsec;	/* nanoseconds */
};

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
unsigned int sleep(unsigned int seconds);

#ifdef __cplusplus
}
#endif

#endif
