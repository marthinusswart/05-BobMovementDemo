#pragma once

#include <exec/types.h>

// Common Amiga raw keycodes
#define KEY_ESCAPE 0x45
#define KEY_SPACE 0x40
#define KEY_UP 0x4c
#define KEY_DOWN 0x4d
#define KEY_RIGHT 0x4e
#define KEY_LEFT 0x4f

// Polls the hardware to see if a new key has been pressed or released.
// Returns the raw keycode (0x00 to 0x7F), or -1 if no new key event occurred.
short PollKeyboard();