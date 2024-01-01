// fat32.c
#include "fat32.h"
#include "vga.h"

#ifdef UEFI_BUILD
#include "../boot/efi/efi.h"

// UEFI Implementation
void fat32_init(struct fat32_bpb *bpb) {
    // UEFI file system is initialized by the firmware
    (void)bpb;
}

int fat32_read_file(const char *filename, void *dest) {
    if (!g_SystemTable || !g_ImageHandle) return -1;

    // 1. Get LoadedImage Protocol to find the DeviceHandle
    EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_STATUS status = g_SystemTable->BootServices->HandleProtocol(g_ImageHandle, &loaded_image_guid, (void**)&loaded_image);
    
    if (status != 0) {
        vga_put_string("FS: Failed to get LoadedImage\n", 0x1F);
        return -1;
    }

    // 2. Get SimpleFileSystem Protocol from DeviceHandle
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = g_SystemTable->BootServices->HandleProtocol(loaded_image->DeviceHandle, &fs_guid, (void**)&fs);
    
    if (status != 0) {
        vga_put_string("FS: Failed to get FileSystem\n", 0x1F);
        return -1;
    }

    // 3. Open Volume (Root Dir)
    EFI_FILE_PROTOCOL *root;
    status = fs->OpenVolume(fs, &root);
    if (status != 0) {
        vga_put_string("FS: Failed to open volume\n", 0x1F);
        return -1;
    }

    // 4. Convert ASCII filename to Unicode (short*)
    short name_buffer[256];
    int i = 0;
    while(filename[i] && i < 255) {
        name_buffer[i] = (short)filename[i];
        i++;
    }
    name_buffer[i] = 0;

    // 5. Open File
    EFI_FILE_PROTOCOL *file;
    status = root->Open(root, &file, name_buffer, EFI_FILE_MODE_READ, 0);
    root->Close(root); 

    if (status != 0) {
        vga_put_string("FS: Open failed for: '", 0x1F);
        vga_put_string(filename, 0x1F);
        vga_put_string("'\n", 0x1F);
        
        vga_put_string("FS: Status: ", 0x1F);
        // Simple hex printer
        char hex[] = "0123456789ABCDEF";
        for(int k=60; k>=0; k-=4) {
             char c[2] = { hex[(status >> k) & 0xF], 0 };
             vga_put_string(c, 0x1F);
        }
        vga_put_string("\n", 0x1F);

        vga_put_string("FS: Filename Hex: ", 0x1F);
        for(int k=0; filename[k]; k++) {
             char c2[3] = { hex[(filename[k] >> 4) & 0xF], hex[filename[k] & 0xF], 0 };
             vga_put_string(c2, 0x1F);
             vga_put_string(" ", 0x1F);
        }
        vga_put_string("\n", 0x1F);
        return -1;
    }

    // 6. Read File
    // Assume max file size is reasonable for the buffer. 
    // In a real OS we'd get file info first, but here we assume 'dest' is large enough (legacy constraint).
    UINTN read_size = 10 * 1024 * 1024; // 10MB Buffer Limit
    status = file->Read(file, &read_size, dest);
    file->Close(file);

    if (status != 0) {
         vga_put_string("FS: Failed to read file\n", 0x1F);
         return -1;
    }
    
    return 0; // Success
}

#else
// Legacy BIOS Implementation
#include "disk.h"
#include "mem.h"

static struct fat32_bpb g_bpb;
static uint32_t g_data_lba;

void fat32_init(struct fat32_bpb *bpb) {
    // Manual copy to avoid unaligned access or memcpy issues
    g_bpb.bytes_per_sector = bpb->bytes_per_sector;
    g_bpb.sectors_per_cluster = bpb->sectors_per_cluster;
    g_bpb.reserved_sectors = bpb->reserved_sectors;
    g_bpb.fat_count = bpb->fat_count;
    g_bpb.sectors_per_fat_32 = bpb->sectors_per_fat_32;
    g_bpb.root_cluster = bpb->root_cluster;

    g_data_lba = g_bpb.reserved_sectors + (g_bpb.fat_count * g_bpb.sectors_per_fat_32);
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return g_data_lba + (cluster - 2) * g_bpb.sectors_per_cluster;
}

static void format_83(const char *src, char *dst) {
    for (int i = 0; i < 11; i++) dst[i] = ' ';
    int i = 0;
    while (src[i] && src[i] != '.' && i < 8) {
        dst[i] = src[i];
        if (dst[i] >= 'a' && dst[i] <= 'z') dst[i] -= 32;
        i++;
    }
    while (src[i] && src[i] != '.') i++;
    if (src[i] == '.') {
        i++;
        int j = 0;
        while (src[i] && (src[i] != ' ' && src[i] != '\0') && j < 3) {
            dst[8 + j] = src[i];
            if (dst[8 + j] >= 'a' && dst[8 + j] <= 'z') dst[8 + j] -= 32;
            i++; j++;
        }
    }
}

int fat32_read_file(const char *filename, void *dest) {
    char target[12];
    format_83(filename, target);
    target[11] = '\0';

    vga_put_string("\nFS: Searching for [", 0x1F);
    vga_put_string(target, 0x1F);
    vga_put_string("]", 0x1F);

    uint8_t buffer[512];
    uint32_t cluster = g_bpb.root_cluster;

    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        for (int s = 0; s < g_bpb.sectors_per_cluster; s++) {
            ata_read_sectors(lba + s, 1, (uint16_t *)buffer);
            for (int i = 0; i < 512; i += 32) {
                if (buffer[i] == 0) {
                    vga_put_string("\nFS: End of directory reached.", 0x1F);
                    return -1;
                }
                if (buffer[i] == 0xE5) continue;
                
                char entry_name[12];
                for(int k=0; k<11; k++) entry_name[k] = buffer[i+k];
                entry_name[11] = '\0';
                vga_put_string("\nFound: ", 0x1F);
                vga_put_string(entry_name, 0x1F);

                int match = 1;
                for (int j = 0; j < 11; j++) {
                    if (buffer[i + j] != target[j]) { match = 0; break; }
                }

                if (match) {
                    uint32_t start_cluster = (*(uint16_t *)&buffer[i + 20] << 16) | *(uint16_t *)&buffer[i + 26];
                    vga_put_string(" -> OK!", 0x1F);
                    
                    uint32_t current = start_cluster;
                    uint8_t *ptr = (uint8_t *)dest;
                    while (current < 0x0FFFFFF8) {
                        uint32_t file_lba = cluster_to_lba(current);
                        for (int fs = 0; fs < g_bpb.sectors_per_cluster; fs++) {
                            ata_read_sectors(file_lba + fs, 1, (uint16_t *)ptr);
                            ptr += 512;
                        }
                        
                        uint32_t fat_sector = current / 128;
                        uint32_t fat_offset = current % 128;
                        uint32_t fat_lba = g_bpb.reserved_sectors + fat_sector;
                        uint32_t fat_buf[128];
                        ata_read_sectors(fat_lba, 1, (uint16_t *)fat_buf);
                        current = fat_buf[fat_offset] & 0x0FFFFFFF;
                    }
                    return 0;
                }
            }
        }
        
        // Move to next cluster in the directory chain
        uint32_t fat_sector = cluster / 128;
        uint32_t fat_offset = cluster % 128;
        uint32_t fat_lba = g_bpb.reserved_sectors + fat_sector;
        uint32_t fat_buf[128];
        ata_read_sectors(fat_lba, 1, (uint16_t *)fat_buf);
        cluster = fat_buf[fat_offset] & 0x0FFFFFFF;
    }
    return -1;
}
#endif
