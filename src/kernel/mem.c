// mem.c
#include "mem.h"

#ifdef UEFI_BUILD
#include "../boot/efi/efi.h"

void kheap_init()
{
    // UEFI heap is already initialized by firmware
}

void *kmalloc(uint32_t size)
{
    if (g_SystemTable && g_SystemTable->BootServices) {
        void *ptr;
        // AllocatePool(PoolType, Size, &Buffer)
        // EfiLoaderData = 2
        EFI_STATUS status = g_SystemTable->BootServices->AllocatePool(2, size, &ptr);
        if (status == 0) { // EFI_SUCCESS
            return ptr;
        }
    }
    return 0;
}

void kfree(void *ptr)
{
    if (ptr && g_SystemTable && g_SystemTable->BootServices) {
        g_SystemTable->BootServices->FreePool(ptr);
    }
}

#else

// Legacy Implementation
void kheap_init()
{
    free_list->size = HEAP_SIZE - sizeof(block_header_t);
    free_list->free = 1;
    free_list->next = 0;
}

void *kmalloc(uint32_t size)
{
    block_header_t *curr = free_list;

    while (curr)
    {
        if (curr->free && curr->size >= size)
        {
            // Split block if it's large enough
            if (curr->size >= size + sizeof(block_header_t) + 1)
            {
                block_header_t *new_block = (block_header_t *)((uint8_t *)curr + sizeof(block_header_t) + size);
                new_block->size = curr->size - size - sizeof(block_header_t);
                new_block->free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }

            curr->free = 0;
            return (uint8_t *)curr + sizeof(block_header_t);
        }
        curr = curr->next;
    }

    return 0; // Out of memory
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    block->free = 1;

    // Merge adjacent free blocks
    block_header_t *curr = free_list;
    while (curr && curr->next)
    {
        if (curr->free && curr->next->free)
        {
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
        }
        else
        {
            curr = curr->next;
        }
    }
}
#endif
