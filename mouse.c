#include "mouse.h"

short MouseLeft()
{
    return !((*(volatile UBYTE *)0xbfe001) & 64);
}

short MouseRight()
{
    return !((*(volatile UWORD *)0xdff016) & (1 << 10));
}