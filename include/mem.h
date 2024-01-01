#ifndef _MEM_H_
#define _MEM_H_

#include <stdint.h>

#define HEAP_START 0x00100000 // 1 MB
#define HEAP_SIZE 0x00100000  // 1 MB heap

typedef struct block_header
{
    uint32_t size;             // Size of the block (excluding header)
    uint8_t free;              // 1 = free, 0 = used
    struct block_header *next; // Pointer to next block
} block_header_t;

static block_header_t *free_list = (block_header_t *)HEAP_START;

void kheap_init();
void *kmalloc(uint32_t size);
void kfree(void *ptr);

#endif