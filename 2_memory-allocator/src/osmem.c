// SPDX-License-Identifier: BSD-3-Clause
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "osmem.h"
#include "linked_list.h"
#include "printf.h"

void *__os_malloc(size_t size, size_t limit)
{
	if (size == 0)
		return NULL;
	void *chunk;
	size_t block_size = METADATA_SIZE + pad(size);

	if (block_size >= limit) {
		chunk = mmap(0, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

		DIE(chunk == NULL, "Error mapping block");
	} else {
		if (os_isfirst(OS_HEAP_ALLOCATIONS)) {
			chunk = sbrk(MMAP_THRESHOLD);
			os_memlist_prealloc(chunk);
		}

		chunk = os_memlist_do_the_monster_mash(size);
		if (chunk == NULL) {
			chunk = sbrk(block_size);
		} else {
			showlist();
			return getpayload(chunk);
		}
		DIE(chunk == NULL, "Error allocating block on heap");
	}

	struct block_meta *block = os_memlist_wrap(chunk, size, limit);
	int position;

	os_memlist_insert(block, &position);
	return getpayload(chunk);
}

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;
	return __os_malloc(size, MMAP_THRESHOLD);
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;
	void *chunk = ptr - METADATA_SIZE;
	struct block_meta *block = (struct block_meta *)chunk;

	if (block->status == STATUS_FREE)
		return;

	if (block->status == STATUS_MAPPED) {
		int removed_position;
		void *removed = os_memlist_bremove(chunk, &removed_position,
							0, OS_MEM_FIND_NOCHECK);

		DIE((long)removed != (long)chunk, "Tried to free block that was not in the memlist.");
		long success = munmap(chunk, METADATA_SIZE + pad(block->size));

		DIE(success != 0, "Error occured during the munmap syscall");
	} else {
		block->status = STATUS_FREE;
		os_memlist_coalesce(block, 0, -1);
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (nmemb * size == 0)
		return NULL;
	void *payload = __os_malloc(nmemb * size, getpagesize());

	for (size_t i = 0 ; i < nmemb * size; ++i)
		*(char *)(payload + i) = (char)0;
	return payload;
}

void *os_memcpy(void *dest, void *src, size_t size)
{
	for (size_t i = 0; i < size; ++i)
		*((char *)(dest) + i) = *((char *)(src) + i);
	return dest;
}

void *os_realloc(void *ptr, size_t size)
{
	printf_("Realloc %p %d\n => ", ptr, size);
	if (size == 0 && ptr != NULL) {
		os_free(ptr);
		return NULL;
	}

	if (ptr == NULL)
		return os_malloc(size);

	struct block_meta *block = ptr - METADATA_SIZE;

	DIE(!block, "Tried to realloc unallocated start-value");

	if (block->status == STATUS_FREE)
		return NULL;

	if (block->status == STATUS_MAPPED) {
		void *new_ptr = os_malloc(size);

		os_memcpy(new_ptr, ptr, pad(block->size) > pad(size) ? pad(size) : pad(block->size));
		os_free(getpayload(block));
		return new_ptr;
	}

	if ((METADATA_SIZE + pad(size) >= MMAP_THRESHOLD)) {
		void *new_ptr = os_malloc(size);

		os_memcpy(new_ptr, ptr, pad(block->size) > pad(size) ? pad(size) : pad(block->size));
		block->status = STATUS_ALLOC;
		os_free(getpayload(block));
		return new_ptr;
	}

	struct block_meta *refit_block = os_memlist_refit(block, pad(size));

	if (refit_block) {
		refit_block->status = STATUS_ALLOC;
		return getpayload(refit_block);
	}

	block->status = STATUS_FREE;
	block = os_memlist_coalesce(block, 1, pad(size));
	if (pad(block->size) >= pad(size)) {
		block->status = STATUS_FREE;
		struct block_meta *splitter = __os_memlist_split_nocheck(block, pad(size));

		block->status = STATUS_ALLOC;
		if (splitter)
			os_memlist_coalesce(splitter, 1, -1);
		block->status = STATUS_ALLOC;
		showlist();

		return getpayload(block);
	}

	block->status = STATUS_ALLOC;
	void *new_ptr = os_malloc(size);

	os_memcpy(new_ptr, ptr, pad(block->size) > pad(size) ? pad(size) : pad(block->size));
	os_free(getpayload(block));

	return new_ptr;
}
