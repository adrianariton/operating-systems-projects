// SPDX-License-Identifier: BSD-3-Clause
#include "linked_list.h"
#include "printf.h"
struct linked_list mem_list;

size_t pad(size_t data)
{
	if (data % 8 == 0)
		return data;
	return ((data >> 3) + 1) << 3;
}

void *getpayload(void *chunk)
{
	return chunk + METADATA_SIZE;
}

struct block_meta *os_memlist_wrap(void *chunk, size_t size, size_t limit)
{
	struct block_meta *block = (struct block_meta *)chunk;
	size_t block_size = METADATA_SIZE + pad(size);

	if (block_size >= limit)
		block->status = STATUS_MAPPED;
	else
		block->status = STATUS_ALLOC;

	block->size = pad(size);
	return block;
}

size_t fullsize(struct block_meta *block)
{
	return METADATA_SIZE + pad(block->size);
}

size_t predicted(size_t block_size)
{
	return METADATA_SIZE + pad(block_size);
}


size_t os_isfirst(size_t flag)
{
	if (flag == OS_HEAP_ALLOCATIONS)
		return mem_list.heap_allocations == 0 ? 1 : 0;
	else
		return mem_list.mmaps == 0 ? 1 : 0;
}

void mem_list_init(void)
{
	mem_list.head = NULL;
	mem_list.size = 0;
	mem_list.tail = NULL;
	mem_list.heap_allocations = 0;
	mem_list.mmaps = 0;
}

size_t os_memlist_get(size_t flag)
{
	if (flag == OS_HEAP_ALLOCATIONS)
		return mem_list.heap_allocations;
	else
		return mem_list.mmaps;
}

struct block_meta *os_memlist_findblock(long pointer, int *out_position)
{
	struct block_meta *iter = mem_list.head;

	for (size_t i = 0; i < mem_list.size; ++i) {
		if ((long) (iter->next) > pointer) {
			*out_position = i;
			return iter;
		}
#ifdef SEPARATE_MAPPINGS
		if (iter->next->status != STATUS_MAPPED) {
			*out_position = i;
			return iter;
		}
#endif

		iter = iter->next;
	}
	*out_position = mem_list.size;
	return mem_list.tail;
}

struct block_meta *__os_memlist_getblock_nocheck(long pointer, int offset)
{
	struct block_meta *output = (struct block_meta *)(void *)(pointer - offset);
	return output;
}

struct block_meta *os_memlist_getblockstart(void *payload)
{
	return __os_memlist_getblock_nocheck((long)payload, METADATA_SIZE);
}

struct block_meta *os_memlist_getblock(long pointer, int *out_position)
{
	struct block_meta *iter = mem_list.head;

	for (size_t i = 0; i < mem_list.size; ++i) {
		if ((long) (iter) == pointer) {
			*out_position = i;
			return iter;
		}

		iter = iter->next;
	}
	*out_position = mem_list.size;
	return NULL;
}

struct block_meta *os_memlist_binsert(struct block_meta *block, int *out_pos)
{
	if (mem_list.size == 0) {
		mem_list.head = block;
		block->prev = NULL;
		block->next = NULL;
		mem_list.tail = block;
		mem_list.size++;
		*out_pos = 0;
		return block;
	}
	int position;
	struct block_meta *found = os_memlist_findblock((long)(block), &position);
	struct block_meta *nxt = found->next;

	found->next = block;
	block->prev = found;
	if (nxt)
		nxt->prev = block;
	else
		mem_list.tail = block;
	block->next = nxt;
	mem_list.size++;
	*out_pos = position;
	if (mem_list.tail)
		mem_list.tail->next = NULL;
	if (mem_list.head)
		mem_list.head->prev = NULL;
	return block;
}

struct block_meta *os_memlist_insertafter(struct block_meta *block, struct block_meta *before)
{
	if (mem_list.size == 0 || before == NULL) {
		mem_list.head = block;
		block->prev = NULL;
		block->next = NULL;
		mem_list.tail = block;
		mem_list.size++;
		if (mem_list.tail)
			mem_list.tail->next = NULL;
		if (mem_list.head)
			mem_list.head->prev = NULL;
		return block;
	}

	struct block_meta *found = before;
	struct block_meta *nxt = found->next;

	found->next = block;
	block->prev = found;
	if (nxt)
		nxt->prev = block;
	else
		mem_list.tail = block;

	block->next = nxt;
	mem_list.size++;
	if (mem_list.tail)
		mem_list.tail->next = NULL;

	if (mem_list.head)
		mem_list.head->prev = NULL;
	return block;
}

struct block_meta *os_memlist_insert(struct block_meta *block, int *out_pos)
{
	if (block->status == STATUS_MAPPED) {
		mem_list.mmaps++;
		return __os_memlist_insert(0, block);
	}

	mem_list.heap_allocations++;
	*out_pos = mem_list.size;
	return  __os_memlist_insert(mem_list.size, block);
}


void *os_memlist_bremove(void *start, int *out_pos, int offset, int flag)
{
	if (mem_list.size == 0)
		return NULL;

	int position;
	struct block_meta *found;

	if (flag == OS_MEM_FIND_NOCHECK)
		found = (struct block_meta *)__os_memlist_getblock_nocheck((long)(start), offset);
	else
		found = (struct block_meta *)os_memlist_getblock((long)(start), &position);

	if (mem_list.size == 1) {
		mem_list.head = NULL;
		mem_list.tail = NULL;
		mem_list.size--;
		return found;
	}

	if (found->next == NULL) {
		struct block_meta *prev = found->prev;

		mem_list.tail->prev->next = NULL;
		mem_list.tail = prev;
		if (mem_list.tail)
			mem_list.tail->next = NULL;
		mem_list.size--;
		return found;
	}

	if (found == NULL)
		return NULL;

	*out_pos = position;
	if (found->prev != NULL) {
		struct block_meta *prev = found->prev;
		struct block_meta *nxt = found->next;

		prev->next = nxt;
		if (nxt)
			nxt->prev = prev;
		else // block is tail
			mem_list.tail = prev;
	} else if (found->next == NULL) {
		struct block_meta *prev = found->prev;

		mem_list.tail = prev;
		if (mem_list.tail)
			mem_list.tail->next = NULL;
	} else {
		mem_list.head = found->next;
		mem_list.head->prev = NULL;
	}
	mem_list.size--;
	return (void *)found;
}

void showlist(void)
{
	printf_("List\n");
}


struct block_meta *os_memlist_get_first(int flag, size_t extra_payload_len)
{
	struct block_meta *iter = mem_list.head;
	size_t min_size = 1e9;
	struct block_meta *best_block = NULL;

	for (size_t i = 0; i < mem_list.size; ++i) {
		if (flag == FREE_BLOCK) {
			if (iter && iter->status == STATUS_FREE && pad(iter->size) >= pad(extra_payload_len))
				return iter;
		}

		if (flag == BEST_FREE_BLOCK) {
			if (iter && iter->status == STATUS_FREE && pad(iter->size) >= pad(extra_payload_len)) {
				if (min_size > pad(iter->size)) {
					min_size = pad(iter->size);
					best_block = iter;
				}
			}
		}

		iter = iter->next;
	}
	if (flag == BEST_FREE_BLOCK)
		return best_block;
	return NULL;
}

struct block_meta *__os_memlist_split_nocheck(struct block_meta *block_addr, size_t new_size)
{
	if (block_addr->status != STATUS_FREE)
		return NULL;
	if (fullsize(block_addr) < predicted(8) + predicted(new_size))
		return NULL;
	void *new_chunk = (void *)(block_addr) + predicted(new_size);
	struct block_meta *new_block = (struct block_meta *)new_chunk;

	new_block->size = fullsize(block_addr) - METADATA_SIZE - predicted(new_size);
	new_block->status = STATUS_FREE;
	block_addr->status = STATUS_ALLOC;
	block_addr->size = pad(new_size);
	os_memlist_insertafter(new_block, block_addr);
	new_block->status = STATUS_FREE;
	return new_block;
}

struct block_meta *os_memlist_fit(size_t size)
{
	if (mem_list.size == 0)
		return NULL;
	struct block_meta *fit = os_memlist_get_first(BEST_FREE_BLOCK, size);

	if (fit == NULL)
		return NULL;
	struct block_meta *splittedright = __os_memlist_split_nocheck(fit, size);

	if (splittedright)
		os_memlist_coalesce(splittedright, 1, -1);
	fit->status = STATUS_ALLOC;
	return fit;
}

void *os_memlist_tryexpand(size_t size)
{
	if (mem_list.tail->status != STATUS_FREE
		|| pad(mem_list.tail->size) >= pad(size))
		return NULL;
	void *extra_chunk = sbrk(pad(size) - pad(mem_list.tail->size));

	DIE(extra_chunk == NULL, "Error expanding block on heap");
	mem_list.tail->status = STATUS_ALLOC;
	mem_list.tail->size = pad(size);
	return (void *)mem_list.tail;
}

void os_memlist_prealloc(void *chunk)
{
	struct block_meta *block = (struct block_meta *)chunk;

	block->status = STATUS_FREE;
	block->size = MMAP_THRESHOLD - METADATA_SIZE;

	int position;

	os_memlist_insert(block, &position);
}


struct block_meta *__os_memlist_insert(size_t position, struct block_meta *block)
{
	if (mem_list.size == 0) {
		mem_list.head = block;
		block->prev = NULL;
		block->next = NULL;
		mem_list.tail = block;
		mem_list.size++;
		return block;
	}

	if (position >= mem_list.size) {
		struct block_meta *tail = mem_list.tail;

		tail->next = block;
		block->prev = tail;
		block->next = NULL;
		mem_list.tail = block;
		mem_list.size++;
		return block;
	}

	struct block_meta *iter = mem_list.head;

	for (size_t i = 0; i < position; ++i)
		iter = iter->next;

	if (position == 0) {
		struct block_meta *oldhead = mem_list.head;

		mem_list.head = block;
		mem_list.head->next = oldhead;
		mem_list.head->next->prev = mem_list.head;
		mem_list.head->prev = NULL;
		mem_list.size++;
	} else if (position < mem_list.size) {
		struct block_meta *bef = iter->prev;
		struct block_meta *af = iter;

		bef->next = block;
		block->next = af;
		af->prev = block;
		block->prev = bef;
		mem_list.size++;
	}
	return block;
}

struct block_meta *direct(struct block_meta *block, int direction)
{
	if (!block)
		return block;
	if (direction == -1)
		return block->prev;
	if (direction == 1)
		return block->next;
	return block;
}

struct block_meta *os_memlist_coalesce(struct block_meta *block, int direction, int target_size)
{
	if (!block)
		return NULL;
	if (block->status != STATUS_FREE)
		return NULL;
	struct block_meta *iter = block;

	if (target_size > 0)
		direction = 1;

	if (direction != 1) {
		while (iter && iter->prev && iter->prev->status == STATUS_FREE)
			iter = iter->prev;
	}

	if (!iter)
		iter = mem_list.head;

	struct block_meta *nxt = iter->next;
	size_t coalesced = 0;

	while (1) {
		if (iter && target_size > 0 && pad(iter->size) >= pad(target_size))
			return iter;
		if (!nxt)
			return iter;
		if (nxt->status != STATUS_FREE)
			return iter;

		iter->size = pad(iter->size) + fullsize(nxt);
		int removed_position;

		os_memlist_bremove((void *)nxt, &removed_position, 0, OS_MEM_FIND_NOCHECK);
		coalesced++;
		nxt = iter->next;
	}
}

void *os_memlist_refit(struct block_meta *block, size_t new_size)
{
	if (block->next == NULL) {
		block->status = STATUS_FREE;
		block = os_memlist_tryexpand(new_size);
		if (!block)
			return NULL;
		block->status = STATUS_ALLOC;
		return block;
	}
	return NULL;
}

void *os_memlist_do_the_monster_mash(size_t size)
{
	void *chunk = NULL;

	chunk = os_memlist_fit(size);
	if (chunk != NULL)
		return chunk;

	chunk = os_memlist_tryexpand(size);
	if (chunk != NULL)
		return chunk;

	return NULL;
}
