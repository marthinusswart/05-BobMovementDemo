#pragma once

#ifndef PACMAN_H
#define PACMAN_H

#include <exec/types.h>
#include <hardware/custom.h>
#include "support/gcc8_c_support.h"

typedef enum
{
    LEFT,
    UP,
    RIGHT,
    DOWN
} Direction;

typedef struct Sprite
{
    int x;
    int y;
    int width;
    int height;
    const UBYTE *spriteData;
} Sprite;

typedef struct Pacman
{
    int x;
    int y;
    int prevX;
    int prevY;
    int width;
    int height;
    Direction direction;
    Sprite downSprite;
    Sprite upSprite;
    Sprite leftSprite;
    Sprite rightSprite;

    // Function pointer to emulate a method call
    void (*movePacman)(struct Pacman *p, Direction dir);
    void (*addSprite)(struct Pacman *p, Direction dir, int spriteX, int spriteY, int width, int height);
    Sprite *(*getSprite)(struct Pacman *p, Direction dir);
} Pacman;

Pacman *createPacman(int x, int y, int width, int height);

#endif // PACMAN_H