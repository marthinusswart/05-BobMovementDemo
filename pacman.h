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

// Forward declaration (Opaque type)
typedef struct Pacman Pacman;

Pacman *createPacman(int x, int y, int width, int height, Direction direction);

#endif // PACMAN_H