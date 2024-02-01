#include <fcntl.h>
#include <internal/syscall.h>
#include <internal/io.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifndef EOF
# define EOF (-1)
#endif
int puts(const char *str)
{
    size_t l = write(1, str, strlen(str));
    write(1, "\n", 1);
    return l + 1;
}
