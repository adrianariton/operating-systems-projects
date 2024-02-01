// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>
#include <stdlib.h>
char *strcpy(char *destination, const char *source)
{
	size_t length = strlen(source);
	char *dest = strncpy(destination, source, length);
	*(dest + length) = '\0';
	return dest;
}

char *strncpy(char *destination, const char *source, size_t len)
{
	char *olddest = destination;
	while (len--) {
		*destination++ = *source++;
	}
	return olddest;
}

char *strcat(char *destination, const char *source)
{
	return strncat(destination, source, strlen(source));
}

char *strncat(char *destination, const char *source, size_t len)
{
	char *olddest = destination;
	destination = destination + strlen(destination) ;
	while (*source != '\0' && len--) {
        *destination++ = *source++;
    }
    *destination = '\0';
	return olddest;
}

int strcmp(const char *str1, const char *str2)
{
	while (*str1 != '\0' && *str2 != '\0') {
		if (*str1 != *str2) {
			return (*str1) - (*str2);
		} else {
			if (*(str1 + 1) == '\0' && *(str2 + 1) != '\0')
				return -1;
			if (*(str2 + 1) == '\0' && *(str1 + 1) != '\0')
				return 1;
		}
		str1++;
		str2++;
	}

	return 0;
}

int strncmp(const char *str1, const char *str2, size_t len)
{
	size_t i = 0;
	while (*str1 != '\0' && *str2 != '\0' && i < len) {
		if (*str1 != *str2) {
			return (*str1) - (*str2);
		} else {
			if (i + 1 >= len)
				break;
			if (*(str1 + 1) == '\0' && *(str2 + 1) != '\0')
				return -1;
			if (*(str2 + 1) == '\0' && *(str1 + 1) != '\0')
				return 1;
		}
		i++;
		str1++;
		str2++;
	}

	return 0;
}

size_t strlen(const char *str)
{
	size_t i = 0;

	for (; *str != '\0'; str++, i++)
		;

	return i;
}

char *strchr(const char *str, int c)
{
	size_t i = 0;
	for(;*str != '\0'; str++, i++) {
		if (*str == (char)c)
			return (char *)str;
	}

	return NULL;
}

char *strrchr(const char *str, int c)
{
	size_t len = strlen(str);
	while(*str != '\0')
		str++;
	for (int i = len - 1; i >= 0; --i) {
		str--;
		if (*str == c)
			return (char *)str;
	}
	return NULL;
}

char *strstr(const char *haystack, const char *needle)
{
	/* TODO: Implement strstr(). */
	size_t len = strlen(haystack);
	size_t needlen =  strlen(needle);
	for (size_t i = 0; i < len - needlen; ++i) {
		if (strncmp(haystack + i, needle, strlen(needle)) == 0)
			return (char *)haystack + i;
	}
	return NULL;
}

char *strrstr(const char *haystack, const char *needle)
{
	char *p;
	p = (char *)haystack;
	size_t len = strlen(haystack);
	size_t needlen =  strlen(needle);
	for (int i = len - needlen; i >= 0; --i) {
		if (strncmp(haystack + i, needle, strlen(needle)) == 0)
			return p + i;
	}
	return NULL;
}

void *memcpy(void *destination, const void *source, size_t num)
{
	return strncpy(destination, source, num);;
}

void *memmove(void *destination, const void *source, size_t num)
{
	void* buffer = malloc(num);
	for (size_t i = 0 ; i < num; ++i)
		((char *)buffer)[i] = ((char *)source)[i];
	for (size_t i = 0 ; i < num; ++i)
		((char *)destination)[i] = ((char *)buffer)[i];
	return destination;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	// return strncmp(ptr1, ptr2, num);

	size_t i = 0;
	while (i < num) {
		if (*(char *)ptr1 != *(char *)ptr2)
			return (*(char *)ptr1) - (*(char *)ptr2);
		i++;
		ptr1++;
		ptr2++;
	}
	return 0;
}

void *memset(void *source, int value, size_t num)
{
	while (num--) {
		*(char *)source = (char)value;
		source++;
	}

	return source;
}
