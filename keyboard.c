#include "keyboard.h"

short PollKeyboard()
{
    volatile UBYTE *ciaa_icr = (volatile UBYTE *)0xbfed01; // Interrupt Control Register
    volatile UBYTE *ciaa_sdr = (volatile UBYTE *)0xbfec01; // Serial Data Register
    volatile UBYTE *ciaa_cra = (volatile UBYTE *)0xbfef01; // Control Register A

    // Reading the Interrupt Control Register (ICR) clears its flags.
    // Bit 3 (0x08) is the Serial Data Register (SDR) full flag.
    // It is set by the hardware when the keyboard finishes sending a new byte.
    if (*ciaa_icr & 0x08)
    {
        // Read the raw keyboard data.
        // We MUST cast to UBYTE before shifting to prevent C integer promotion bugs!
        UBYTE rawKey = ((UBYTE) ~(*ciaa_sdr)) >> 1;

        // Perform the keyboard handshake to acknowledge receipt.
        // Without this pulse, the Amiga keyboard will stop sending keys!
        UBYTE cra = *ciaa_cra;
        *ciaa_cra = cra | 0x40; // Set SPMODE (bit 6) to 1 (Output)

        for (volatile int i = 0; i < 1000; i++)
        {
        } // Wait ~85 microseconds

        *ciaa_cra = cra & ~0x40; // Set SPMODE back to 0 (Input)

        // The handshake itself triggers the CIA's "serial transmission complete"
        // interrupt flag. This takes a few hundred microseconds. We must wait for
        // this flag to appear and then clear it to prevent a phantom keypress on
        // the next poll.
        while (!(*ciaa_icr & 0x08))
        {
            // Busy-wait for the flag to be set by the hardware.
        }
        // Now that the flag is set, this dummy read will clear it.
        volatile UBYTE dummy = *ciaa_icr;
        (void)dummy;

        return rawKey; // Return the keycode (if rawKey > 0x7F, it's a key release event)
    }

    return -1; // No new key event
}