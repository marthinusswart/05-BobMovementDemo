#pragma once

#include <exec/types.h>

// Common Amiga raw keycodes
#define KEY_ESC 0x45
#define KEY_SPACE 0x40
#define KEY_UP 0x4c
#define KEY_DOWN 0x4d
#define KEY_RIGHT 0x4e
#define KEY_LEFT 0x4f

typedef struct
{
    UBYTE curr[128];
    UBYTE prev[128];
} KeyboardDevice;

extern volatile KeyboardDevice g_keyboard;

void keyboard_init(void);
void keyboard_update(void);
int key_is_down(UBYTE keycode);
int key_was_pressed(UBYTE keycode);