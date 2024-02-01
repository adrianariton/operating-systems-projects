#include <internal/syscall.h>
#include <stdlib.h>
#include <time.h>
unsigned int sleep(unsigned int seconds)
{
    struct timespec ts = (struct timespec) { .tv_sec = seconds, .tv_nsec = 0};
	return nanosleep(&ts, NULL);
}
