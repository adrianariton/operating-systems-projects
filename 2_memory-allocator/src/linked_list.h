#pragma once
// SPDX-License-Identifier: BSD-3-Clause
#include <unistd.h>
#include "block_meta.h"
#include "osmem.h"

#define METADATA_SIZE		(pad(sizeof(struct block_meta)))
#define MMAP_THRESHOLD		(128 * 1024)
#define OS_MEM_FIND_NOCHECK 1
#define OS_MEM_FIND 2
#define OS_HEAP_ALLOCATIONS 1
#define OS_MMAPS 2
#define SPLITABLE_BLOCK 1
#define FREE_BLOCK 2
#define BEST_FREE_BLOCK 3
#define REALLOC_SAFE
#define SEPARATE_MAPPINGS

struct linked_list {
	struct block_meta* head;
	size_t size;
	struct block_meta* tail;
	size_t heap_allocations;
	size_t mmaps;
};
typedef struct linked_list linked_list;

/**
 * @brief Insert a pre-allocated block in the memory list.
 * If the block in STATUS_MAPPED, it is inserted at the front
 * else it is inserted in the back. Speify SEPARATE_MAPPINGS
 * to keep blocks separated and unordered (in the case of MAPPED
 * blocks), or #undef it to keep them ordered by their start
 * 
 * @param block block to be added
 * @param out_pos output: position where block was added
 * @return struct block_meta* 
 */
struct block_meta* os_memlist_insert(struct block_meta* block, int *out_pos);

/**
 * @brief Finds the first block with start-address less
 * than the given pointer. Unused in the current implementation
 * which separates mappinds (#define SEPARATE_MAPPINGS), but
 * usable in the case of freeing sace in the middle of a block
 * which was beyond the scope of the assignment
 * 
 * @param pointer 
 * @param out_position 
 * @return struct block_meta* 
 */
struct block_meta* os_memlist_findblock(long pointer, int* out_position);

/**
 * @brief Remove a block that has start address of metadata at
 * (start - offset)
 * 
 * @param start 
 * @param out_pos position of removed block
 * @param offset 
 * @param flag OS_MEM_FIND_NOCHECK | OS_MEM_FIND, if OS_MEM_FIND_NOCHECK the
 * existanc eof the block is not checked, and will throw an error/segv if
 * not found
 * @return void* 
 */
void *os_memlist_bremove(void* start, int *out_pos, int offset, int flag);

/**
 * @brief Checks if block can be extended and if yes, adds it to the memlist
 * and returns it, if it cannot be extendend, NULL is returned
 * 
 * @param size 
 * @return struct block_meta* 
 */
struct block_meta *os_memlist_fit(size_t size);

/**
 * @brief Get data of memlist: heap/mapped operation number
 * 
 * @param flag 
 * @return size_t 
 */
size_t os_memlist_get(size_t flag);

/**
 * @brief Check if an allocation is the first on of its kind
 * 
 * @param flag 
 * @return size_t 
 */
size_t os_isfirst(size_t flag);

/**
 * @brief Returns the address of the payload of a block
 * 
 * @param chunk 
 * @return void* 
 */
void *getpayload(void *chunk);

/**
 * @brief Size with padding alligned to multiples of 8
 * 
 * @param data 
 * @return size_t 
 */
size_t pad(size_t data);

/**
 * @brief Returns a block with the given params
 * 
 * @param chunk 
 * @param size 
 * @param limit Limit that decides if block is MAPPED of on the heap
 * @return struct block_meta* 
 */
struct block_meta *os_memlist_wrap(void *chunk, size_t size, size_t limit);

/**
 * @brief Prealocates MMAP_THRESHOLD bytes on the heap
 * 
 * @param chunk 
 */
void os_memlist_prealloc(void *chunk);

/**
 * @brief Check if bloc can be extended and if not, check and
 * reuse available free blocks. If none succeded, return NULL
 * 
 * @param size 
 * @return void* 
 */
void *os_memlist_do_the_monster_mash(size_t size);

/**
 * @brief Insert block at position, in the memlist
 * 
 * @param position 
 * @param block 
 * @return struct block_meta* 
 */
struct block_meta* __os_memlist_insert(size_t position, struct block_meta* block);

/**
 * @brief Coalesce list of blocks starting from the given block
 * If direction == 1, only coalesce the next consecutive free blocks
 * starting from block, including it.
 * If direction == 0, coalesce both left and right and return the block
 * If target_size is > 0, the coalesscing stops when the resulting coalesced
 * block is bigger than target_size.
 * 
 * @param block 
 * @param direction 
 * @param target_size 
 * @return struct block_meta* 
 */
struct block_meta * os_memlist_coalesce(struct block_meta *block, int direction, int target_size);

/**
 * @brief Given a valid payload start, return the start of it's
 * block_meta METDATA
 * 
 * @param payload 
 * @return struct block_meta* 
 */
struct block_meta* os_memlist_getblockstart(void *payload);

/**
 * @brief Check if block can be refit (for realloc)
 * 
 * @param block 
 * @param new_size 
 * @return void* 
 */
void *os_memlist_refit(struct block_meta *block, size_t new_size);

/**
 * @brief Split block if its size allows it to fit a block of
 * size = new_size
 * 
 * @param block_addr 
 * @param new_size 
 * @return struct block_meta* 
 */
struct block_meta *__os_memlist_split_nocheck(struct block_meta* block_addr, size_t new_size);
void showlist(void);
