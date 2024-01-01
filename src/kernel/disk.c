#include "disk.h"
#include "port.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LOW      0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HIGH     0x1F5
#define ATA_PRIMARY_DRIVE_SEL    0x1F6
#define ATA_PRIMARY_COMMAND      0x1F7
#define ATA_PRIMARY_STATUS       0x1F7

#define ATA_CMD_READ_PIO         0x20

void ata_wait_bsy() {
    while (inb(ATA_PRIMARY_STATUS) & 0x80);
}

void ata_wait_drq() {
    while (!(inb(ATA_PRIMARY_STATUS) & 0x08));
}

void ata_read_sectors(uint32_t lba, uint8_t count, uint16_t *buffer) {
    ata_wait_bsy();

    outb(ATA_PRIMARY_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LOW, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);

    for (int i = 0; i < count; i++) {
        ata_wait_bsy();
        ata_wait_drq();

        for (int j = 0; j < 256; j++) {
            buffer[i * 256 + j] = inw(ATA_PRIMARY_DATA);
        }
    }
}
