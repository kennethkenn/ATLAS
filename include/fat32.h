#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

struct fat32_bpb {
    uint16_t bytes_per_sector;     // 11
    uint8_t  sectors_per_cluster; // 13
    uint16_t reserved_sectors;    // 14
    uint8_t  fat_count;           // 16
    uint16_t root_entry_count;    // 17
    uint16_t total_sectors_16;    // 19
    uint8_t  media_type;          // 21
    uint16_t sectors_per_fat_16;  // 22
    uint16_t sectors_per_track;   // 24
    uint16_t head_count;          // 26
    uint32_t hidden_sectors;      // 28
    uint32_t total_sectors_32;    // 32
    
    // FAT32 Extended fields
    uint32_t sectors_per_fat_32;  // 36
    uint16_t ext_flags;           // 40
    uint16_t fs_version;          // 42
    uint32_t root_cluster;        // 44
} __attribute__((packed));

void fat32_init(struct fat32_bpb *bpb);
int fat32_read_file(const char *filename, void *dest);

#endif
