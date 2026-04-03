#include "keyboard.h"

// Hardware Registers
#define CIAASDR *(volatile UBYTE *)0xBFEC01
#define CIACRA *(volatile UBYTE *)0xBFEE01
#define CIAAICR *(volatile UBYTE *)0xBFED01
#define INTREQ *(volatile unsigned short *)0xDFF09C

extern volatile APTR VBR;

volatile KeyboardDevice g_keyboard;

// The ISR: Needs to handle the hardware handshake
__attribute__((interrupt)) void keyboard_isr(void)
{
    // Read ICR to clear CIA-A interrupt status AND capture what caused it.
    UBYTE icr = CIAAICR;

    // Bit 3 (0x08) is the CIA-A Serial Port (Keyboard) interrupt
    if (icr & 0x08)
    {
        UBYTE raw = CIAASDR;
        UBYTE keycode = ~raw;

        // The Amiga keyboard sends bits shifted. Rotate right by 1 bit to decode.
        UBYTE rotated = (keycode >> 1) | (keycode << 7);
        UBYTE code = rotated & 0x7F;
        UBYTE is_up = rotated & 0x80;

        if (is_up)
        {
            g_keyboard.curr[code] = 0;
        }
        else
        {
            g_keyboard.curr[code] = 1;
        }

        // Acknowledge receipt to CIA-A (Handshake)
        CIACRA |= 0x40;
        // CPU-independent delay (~85 microseconds).
        // Reading a custom register is tied to the Agnus bus speed, not CPU clock.
        for (volatile int i = 0; i < 4; i++)
            (void)*(volatile ULONG *)0xDFF004; // Read VPOSR
        CIACRA &= ~0x40;
    }

    // Clear Paula Level 2 interrupt request (INTF_PORTS = 0x0008)
    INTREQ = 0x0008;
}

void keyboard_init(void)
{
    for (int i = 0; i < 128; i++)
    {
        g_keyboard.curr[i] = 0;
        g_keyboard.prev[i] = 0;
    }

    // Install Level 2 Interrupt accounting for VBR
    volatile ULONG *vec_level2 = (volatile ULONG *)((UBYTE *)VBR + 0x68);
    *vec_level2 = (ULONG)keyboard_isr;

    // Disable all other CIA-A interrupts
    CIAAICR = 0x7F;
    // Read ICR to clear any pending CIA interrupts
    volatile UBYTE dummy = CIAAICR;
    (void)dummy;

    // Enable CIA-A Serial Port Interrupt
    CIAAICR = 0x08 | 0x80;
}

void keyboard_update(void)
{
    for (int i = 0; i < 128; i++)
    {
        g_keyboard.prev[i] = g_keyboard.curr[i];
    }
}

int key_is_down(UBYTE keycode)
{
    return g_keyboard.curr[keycode];
}

int key_was_pressed(UBYTE keycode)
{
    return g_keyboard.curr[keycode] && !g_keyboard.prev[keycode];
}