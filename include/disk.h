#ifndef DISK_H
#define DISK_H

#include <stdint.h>

void ata_read_sectors(uint32_t lba, uint8_t count, uint16_t *buffer);

#endif
