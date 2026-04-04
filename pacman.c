#include "pacman.h"
#include <proto/exec.h>

INCBIN_CHIP(pacman_tiles2, "bpl/pacman_tiles.bpl")

// Private forward declaration
static void movePacman(Pacman *p, Direction direction);
static void addSprite(Pacman *p, Direction direction, int spriteX, int spriteY, int width, int height);
static Sprite *getSprite(Pacman *p, Direction direction);

Pacman *createPacman(int x, int y, int width, int height)
{
    Pacman *p = (Pacman *)AllocMem(sizeof(Pacman), MEMF_CHIP | MEMF_CLEAR);
    p->x = x;
    p->y = y;
    p->width = width;
    p->height = height;
    p->prevX = x;
    p->prevY = y;
    p->direction = RIGHT;
    p->movePacman = movePacman; // Assign the function pointer
    p->addSprite = addSprite;   // Assign the function pointer
    p->getSprite = getSprite;   // Assign the function pointer
    return p;
}

static void movePacman(Pacman *p, Direction direction)
{
    p->prevX = p->x;
    p->prevY = p->y;
    p->direction = direction;
    switch (direction)
    {
    case LEFT:
        p->x -= 1;
        break;
    case UP:
        p->y -= 1;
        break;
    case RIGHT:
        p->x += 1;
        break;
    case DOWN:
        p->y += 1;
        break;
    default:
        break;
    }
}

static void addSprite(Pacman *p, Direction direction, int spriteX, int spriteY, int width, int height)
{
    Sprite *sprite = NULL;
    switch (direction)
    {
    case LEFT:
        sprite = &p->leftSprite;
        break;
    case UP:
        sprite = &p->upSprite;
        break;
    case RIGHT:
        sprite = &p->rightSprite;
        break;
    case DOWN:
        sprite = &p->downSprite;
        break;
    default:
        return; // Invalid direction
    }
    sprite->x = spriteX;
    sprite->y = spriteY;
    sprite->width = width;
    sprite->height = height;
    sprite->spriteData = (const UBYTE *)pacman_tiles2; // Example: all sprites use the same data for now
}

static Sprite *getSprite(Pacman *p, Direction direction)
{
    switch (direction)
    {
    case LEFT:
        return &p->leftSprite;
    case UP:
        return &p->upSprite;
    case RIGHT:
        return &p->rightSprite;
    case DOWN:
        return &p->downSprite;
    default:
        return NULL; // Invalid direction
    }
}
