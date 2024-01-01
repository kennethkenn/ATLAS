// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void keyboard_handler_c(uint8_t scancode);
#ifdef UEFI_BUILD
    int uefi_get_scancode();
#endif

#ifdef __cplusplus
}
#endif

#endif
