#include "pacman.h"
#include <proto/exec.h>

INCBIN_CHIP(pacman_tiles2, "bpl/pacman_tiles.bpl")

struct Pacman
{
    int x;
    int y;
    int prev_x;
    int prev_y;
    int width;
    int height;
    Direction direction;
    const UBYTE *spriteData;
};

Pacman *createPacman(int x, int y, int width, int height, Direction direction)
{
    Pacman *p = (Pacman *)AllocMem(sizeof(Pacman), MEMF_CHIP | MEMF_CLEAR);
    p->x = x;
    p->y = y;
    p->prev_x = x;
    p->prev_y = y;
    p->width = width;
    p->height = height;
    p->direction = direction;
    p->spriteData = (const UBYTE *)pacman_tiles2; // Point to the sprite data
    return p;
}
