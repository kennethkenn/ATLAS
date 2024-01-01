#ifndef PORT_H
#define PORT_H

#include <stdint.h>

// Read a byte from an I/O port
uint8_t inb(uint16_t port);

// Write a byte to an I/O port
void outb(uint16_t port, uint8_t value);

// Read a word (16-bit) from an I/O port
uint16_t inw(uint16_t port);

// Write a word (16-bit) to an I/O port
void outw(uint16_t port, uint16_t value);

// Read a dword (32-bit) from an I/O port
uint32_t inl(uint16_t port);

// Write a dword (32-bit) to an I/O port
void outl(uint16_t port, uint32_t value);

#endif // PORT_H
