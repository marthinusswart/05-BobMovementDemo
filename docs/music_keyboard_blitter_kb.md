# Music, Keyboard, and Blitter Integration Issue

## Problem Statement

The demo had music playback configured but no audio was playing, despite:
- The P61 music player being initialized correctly with `p61Init(module)`
- The music player being called in the VBlank interrupt with `p61Music()`
- The `MUSIC` define being enabled
- The identical setup working in the 03-BobDemo project

## Root Cause Analysis

The issue was **interrupt configuration conflicts** between multiple systems competing for hardware resources:

### Amiga Interrupt Levels (Motorola 68000)
The Amiga uses prioritized interrupt levels:
- **Level 1**: Software/TBE (not used in this demo)
- **Level 2**: CIA (keyboard, serial, timers) - `INTF_PORTS` - vector 0x68
- **Level 3**: Copper, VBlank, Blitter - `INTF_VERTB` - vector 0x6c
- **Level 4**: Audio channels - `INTF_AUD0-3` (not used in this demo)
- **Level 5**: Disk sync - `INTF_DSKSYN` (not used in this demo)
- **Level 6**: External/CIA-B - `INTF_EXTER` - vector 0x70
- **Level 7**: Non-maskable interrupt (not used)

### The Problem

The original code at line 537 was:
```c
custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB | INTF_PORTS;
```

This enabled **both** Level 2 (INTF_PORTS for keyboard) and Level 3 (INTF_VERTB for VBlank) interrupts. However:

1. **Keyboard system** requires Level 2 interrupts (`INTF_PORTS`) with its own ISR at vector 0x68
2. **VBlank/Music system** requires Level 3 interrupts (`INTF_VERTB`) with ISR at vector 0x6c  
3. **P61 Music Player** requires Level 6 interrupts (`INTF_EXTER`) for its internal timing
4. The keyboard ISR was being installed **AFTER** interrupts were enabled (line 553)

### Why 03-BobDemo Worked

Comparing to the working 03-BobDemo revealed:
- It did **NOT** use `INTF_PORTS` because it used a simpler polling-based keyboard system
- It initialized keyboard **before** enabling interrupts
- The interrupt line was simply: `custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB;`

## The Solution

### Step 1: Understand the Interrupt Architecture

Three separate interrupt handlers need to coexist:

1. **keyboard_isr** (Level 2, vector 0x68) - handles CIA keyboard interrupts
   - Reads keyboard scancodes from CIA Serial Data Register
   - Performs hardware handshake
   - Updates keyboard state array

2. **interruptHandler** (Level 3, vector 0x6c) - handles VBlank
   - Calls `p61Music()` every frame to drive music playback
   - Updates frame counter
   - Manages copper list modifications

3. **P61 Player internal handler** (Level 6, vector 0x70) - music timing
   - Managed internally by the P61 player
   - Requires `INTF_EXTER` enabled
   - Handles precise audio timing and Paula audio DMA

### Step 2: Correct Initialization Order

**CRITICAL**: Interrupt handlers must be installed **BEFORE** their interrupts are enabled.

Changed from:
```c
// DEMO
SetInterruptHandler((APTR)interruptHandler);
custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB | INTF_PORTS;
#ifdef MUSIC
custom->intena = INTF_SETCLR | INTF_EXTER;
#endif

// ... later ...
keyboard_init();  // TOO LATE! INTF_PORTS already enabled
```

Changed to:
```c
// Initialize keyboard BEFORE enabling interrupts
keyboard_init();  // Installs Level 2 handler at 0x68

// DEMO
SetInterruptHandler((APTR)interruptHandler);  // Installs Level 3 handler at 0x6c
custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB | INTF_PORTS;
#ifdef MUSIC
custom->intena = INTF_SETCLR | INTF_EXTER; // ThePlayer needs INTF_EXTER for Level 6
#endif
```

### Step 3: DMA Configuration

The DMA setup at line 533 enables all required DMA channels:
```c
custom->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER | DMAF_COPPER | DMAF_BLITTER;
```

**Note**: Audio DMA (`DMAF_AUDIO`) is **NOT** explicitly enabled here. The P61 music player manages audio DMA channels internally when `p61Music()` is called. This is why 03-BobDemo works without explicit audio DMA setup.

**CRITICAL**: Never use a separate `custom->dmacon = ...` line for audio DMA without `DMAF_SETCLR`, as this will **replace** all DMA settings and disable blitter, copper, and raster DMA, causing the entire display and blitting system to stop.

### Step 4: Interrupt Enable Sequence

The interrupt enable sequence uses the `INTF_SETCLR` bit (0x8000) to **OR** new interrupt enables with existing ones:

```c
custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB | INTF_PORTS;  // Enable Level 2 & 3
#ifdef MUSIC
custom->intena = INTF_SETCLR | INTF_EXTER;  // Also enable Level 6
#endif
```

This results in three active interrupt levels:
- **INTF_PORTS** (Level 2): Keyboard via keyboard_isr
- **INTF_VERTB** (Level 3): VBlank + music update via interruptHandler  
- **INTF_EXTER** (Level 6): P61 player internal timing

## Final Working Configuration

### Initialization Order (lines 529-540)
```c
custom->cop1lc = (ULONG)copper1;
custom->cop2lc = (ULONG)copper2;
custom->dmacon = DMAF_BLITTER;  // disable blitter dma for copjmp bug
custom->copjmp1 = 0x7fff;       // start copper
custom->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER | DMAF_COPPER | DMAF_BLITTER;

// Initialize keyboard BEFORE enabling interrupts
keyboard_init();  // Installs vector 0x68 (Level 2)

// DEMO
SetInterruptHandler((APTR)interruptHandler);  // Installs vector 0x6c (Level 3)
custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB | INTF_PORTS;
#ifdef MUSIC
custom->intena = INTF_SETCLR | INTF_EXTER;  // ThePlayer needs INTF_EXTER (Level 6)
#endif

custom->intreq = (1 << INTB_VERTB);  // reset vbl req
```

### VBlank Interrupt Handler (lines 326-344)
```c
static __attribute__((interrupt)) void interruptHandler()
{
    custom->intreq = (1 << INTB_VERTB);
    custom->intreq = (1 << INTB_VERTB);  // reset vbl req. twice for a4000 bug.

    // modify scrolling in copper list
    if (scroll)
    {
        // int sin = sinus15[frameCounter & 63];
        // *scroll = sin | (sin << 4);
    }

#ifdef MUSIC
    // DEMO - ThePlayer
    p61Music();  // Update music player every frame
#endif
    // DEMO - increment frameCounter
    frameCounter++;
}
```

### Keyboard ISR (keyboard.c lines 14-50)
```c
__attribute__((interrupt)) void keyboard_isr(void)
{
    UBYTE icr = CIAAICR;  // Read ICR to clear CIA-A interrupt status
    
    if (icr & 0x08)  // Bit 3 = CIA-A Serial Port (Keyboard)
    {
        UBYTE raw = CIAASDR;
        UBYTE keycode = ~raw;
        
        // Decode Amiga keyboard protocol
        UBYTE rotated = (keycode >> 1) | (keycode << 7);
        UBYTE code = rotated & 0x7F;
        UBYTE is_up = rotated & 0x80;
        
        if (is_up)
            g_keyboard.curr[code] = 0;
        else
            g_keyboard.curr[code] = 1;
        
        // Hardware handshake
        CIACRA |= 0x40;
        for (volatile int i = 0; i < 4; i++)
            (void)*(volatile ULONG *)0xDFF004;  // Delay
        CIACRA &= ~0x40;
    }
    
    // Clear Paula Level 2 interrupt request
    INTREQ = 0x0008;
}
```

## Key Takeaways

1. **Install interrupt handlers BEFORE enabling their corresponding interrupt bits**
2. **Multiple interrupt levels can coexist** as long as they use different vectors
3. **The P61 player manages its own audio DMA** - don't enable DMAF_AUDIO explicitly
4. **Always use INTF_SETCLR/DMAF_SETCLR** when modifying interrupt/DMA registers to OR with existing settings
5. **Never call `custom->dmacon = ...` without DMAF_SETCLR** as it replaces all DMA settings
6. **INTF_PORTS is required** for CIA-based keyboard input to work
7. **INTF_EXTER is required** for P61 music player timing
8. **INTF_VERTB is required** for calling p61Music() every frame via VBlank

## Testing Results

After implementing these changes:
- ✅ Music plays correctly
- ✅ Keyboard input works (W/S for up/down, ESC to exit)
- ✅ Bob blitting and animation works
- ✅ Background restoration works
- ✅ Copper effects continue working
- ✅ All three interrupt handlers coexist without conflicts

## References

- ThePlayer 6.1a: https://www.pouet.net/prod.php?which=19922
- Amiga Hardware Reference Manual (interrupt system)
- CIA 8520 Complex Interface Adapter documentation
- Comparison with working 03-BobDemo implementation
