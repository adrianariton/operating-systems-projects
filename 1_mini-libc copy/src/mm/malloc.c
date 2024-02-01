// SPDX-License-Identifier: BSD-3-Clause

#include <internal/mm/mem_list.h>
#include <internal/types.h>
#include <internal/essentials.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

void *malloc(size_t size)
{
	void *chunk = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// memcpy(chunk, &size, sizeof(size_t));
	// return chunk + sizeof(size_t);
	mem_list_add(chunk, size);
	return chunk;
}

void *calloc(size_t nmemb, size_t size)
{
	void *res = malloc(nmemb * size);
	return memset(res, 0, nmemb * size);
}

void free(void *ptr)
{
	// void *chunk = ptr - sizeof(size_t);
	// size_t length = *(size_t *)chunk;
	// munmap(chunk, length + sizeof(size_t));
	struct mem_list *mem = mem_list_find(ptr);
	munmap(mem->start, mem->len);
	mem_list_del(ptr);
}

void *realloc(void *ptr, size_t size)
{
	void* newptr = malloc(size);
	memcpy(newptr, ptr, size);
	free(ptr);
	return newptr;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
	return realloc(ptr, nmemb * size);
}
