#include "blitter.h"
#include <hardware/blit.h>
#include <hardware/dmabits.h>

void blitBob1x(int x, int y, int width, int height, const UBYTE *tileset, int tileset_width, int tileset_height, int sprite_location_x, int sprite_location_y, UBYTE *screen_buffer, volatile struct Custom *custom)
{
    WaitBlt(custom); // Always wait for the blitter before setting registers

    // Calculate source offset based on sprite location
    // Source is interleaved with a mask after EVERY plane (5 image + 5 mask = 10 planes total)
    int src_y_offset = sprite_location_y * (tileset_width / 8) * 10;
    int src_x_offset = sprite_location_x / 8; // 8 pixels = 1 byte
    const UBYTE *src_start = tileset + src_y_offset + src_x_offset;

    int shift = x & 15;
    int draw_words = width / 16;

    // If the bob is shifted, it spills over into an extra 16-pixel word.
    // Because this is a 1x padded bob, the source asset MUST have an empty 16px word to the right!
    if (shift > 0)
    {
        draw_words += 1;
    }
    int draw_bytes = draw_words * 2;

    // Standard cookie-cutter minterm: (A AND B) OR (C AND (NOT B)) -> 0xE2
    custom->bltcon0 = 0xe2 | SRCA | SRCB | SRCC | DEST | (shift << ASHIFTSHIFT);
    custom->bltcon1 = (shift << BSHIFTSHIFT);

    // Calculate stride logic dynamically based on the source image size vs blit size
    int src_modulo = (tileset_width / 8) * 2 - draw_bytes;
    int dest_modulo = (320 / 8) - draw_bytes;
    int mask_offset = (tileset_width / 8);

    custom->bltapt = (APTR)src_start;
    custom->bltamod = src_modulo;
    custom->bltbpt = (APTR)(src_start + mask_offset);
    custom->bltbmod = src_modulo;

    UBYTE *dest = screen_buffer + (320 / 8) * 5 * y + (x / 16) * 2;
    custom->bltcpt = dest;
    custom->bltdpt = dest;

    custom->bltcmod = dest_modulo;
    custom->bltdmod = dest_modulo;

    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltsize = ((height * 5) << HSIZEBITS) | draw_words; // height * planes
}

void blitBob2x(int x, int y, int width, int height, const UBYTE *tileset, int tileset_width, int tileset_height, int sprite_location_x, int sprite_location_y, UBYTE *screen_buffer, volatile struct Custom *custom)
{
    WaitBlt(custom); // Always wait for the blitter before setting registers

    int shift = x & 15;
    int draw_words = width / 16;

    // If the bob is shifted, it spills over into an extra 16-pixel word.
    if (shift > 0)
    {
        draw_words += 1;
    }
    int draw_bytes = draw_words * 2;

    // Calculate source offset based on sprite location
    // Source is interleaved with a mask after EVERY plane (5 image + 5 mask = 10 planes total)
    int src_y_offset = sprite_location_y * (tileset_width / 8) * 10;
    int src_x_offset = sprite_location_x / 8; // 8 pixels = 1 byte
    const UBYTE *src_start = tileset + src_y_offset + src_x_offset;

    // If we read an extra word to accommodate the shift, it contains the neighboring
    // sprite in the sheet. We mask it to 0 so it doesn't bleed onto the screen.
    UWORD lwm = (shift > 0) ? 0x0000 : 0xffff;

    int mask_offset = (tileset_width / 8);

    // Modulos must account for the actual number of bytes blitted
    int src_modulo = (tileset_width / 8) * 2 - draw_bytes;
    int dest_modulo = (320 / 8) - draw_bytes;

    UBYTE *dest = screen_buffer + (320 / 8) * 5 * y + (x / 16) * 2;

    // --- PASS 1: Cut the hole (Minterm 0x0A = C & ~A) ---
    // We point Channel A to the MASK data.
    custom->bltcon0 = 0x0a | SRCA | SRCC | DEST | (shift << ASHIFTSHIFT);
    custom->bltcon1 = 0; // Only A is shifted, B is unused

    custom->bltafwm = 0xffff;
    custom->bltalwm = lwm;

    custom->bltapt = (APTR)(src_start + mask_offset);
    custom->bltamod = src_modulo;

    custom->bltcpt = dest;
    custom->bltcmod = dest_modulo;
    custom->bltdpt = dest;
    custom->bltdmod = dest_modulo;

    custom->bltsize = ((height * 5) << HSIZEBITS) | draw_words;

    // --- PASS 2: Draw the Image (Minterm 0xFA = C | A) ---
    WaitBlt(custom);
    // We point Channel A to the IMAGE data.
    custom->bltcon0 = 0xfa | SRCA | SRCC | DEST | (shift << ASHIFTSHIFT);
    custom->bltcon1 = 0;

    // FWM/LWM remain the same
    custom->bltapt = (APTR)src_start;
    custom->bltcpt = dest;
    custom->bltdpt = dest;

    custom->bltsize = ((height * 5) << HSIZEBITS) | draw_words;
}

void restoreBackground(int x, int y, int width, int height, const UBYTE *clean_bg, UBYTE *screen_buffer, volatile struct Custom *custom)
{
    WaitBlt(custom);

    int shift = x & 15;
    int draw_words = width / 16;
    if (shift > 0)
        draw_words += 1;

    int draw_bytes = draw_words * 2;

    // Minterm 0xF0 means D = A (Direct Copy).
    // No shifts are needed because the clean background and screen perfectly align!
    custom->bltcon0 = SRCA | DEST | 0xf0;
    custom->bltcon1 = 0;

    int byte_offset = (320 / 8) * 5 * y + (x / 16) * 2;
    int modulo = (320 / 8) - draw_bytes;

    custom->bltapt = (APTR)(clean_bg + byte_offset);
    custom->bltamod = modulo;

    custom->bltdpt = screen_buffer + byte_offset;
    custom->bltdmod = modulo;

    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltsize = ((height * 5) << HSIZEBITS) | draw_words;
}

// Calculates the X and Y pixel coordinates of a sprite within a tileset
void calculateSpriteLocation(int row, int col, int sprite_width, int sprite_height, int tileset_width, int tileset_height, int *sprite_x, int *sprite_y)
{
    // Multiply row/col by dimensions, and use modulo to safely wrap if out of bounds
    *sprite_x = (col * sprite_width) % tileset_width;
    *sprite_y = (row * sprite_height) % tileset_height;
}