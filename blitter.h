#pragma once

#include <exec/types.h>
#include <hardware/custom.h>

__attribute__((always_inline)) inline void WaitBlt(volatile struct Custom *custom)
{
    UWORD tst = *(volatile UWORD *)&custom->dmaconr; // for compatiblity a1000
    (void)tst;
    while (*(volatile UWORD *)&custom->dmaconr & (1 << 14))
    {
    } // blitter busy wait
}

// Blits a 5-plane interleaved masked bob to the 320-pixel wide Interleaved screen.
void blitBob1x(int x, int y, int width, int height, const UBYTE *tileset, int tileset_width, int tileset_height, int sprite_location_x, int sprite_location_y, UBYTE *screen_buffer, volatile struct Custom *custom);

void blitBob2x(int x, int y, int width, int height, const UBYTE *tileset, int tileset_width, int tileset_height, int sprite_location_x, int sprite_location_y, UBYTE *screen_buffer, volatile struct Custom *custom);

// Restores a rectangular block from a clean background to the active screen buffer.
void restoreBackground(int x, int y, int width, int height, const UBYTE *clean_bg, UBYTE *screen_buffer, volatile struct Custom *custom);

// Calculates the X and Y pixel coordinates of a sprite within a tileset
void calculateSpriteLocation(int row, int col, int sprite_width, int sprite_height, int tileset_width, int tileset_height, int *sprite_x, int *sprite_y);