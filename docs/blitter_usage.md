# Amiga Blitter Usage & Best Practices

This document covers the fundamental math and gotchas for drawing interleaved, masked bobs (Blitter Objects) and restoring backgrounds using the Amiga's Custom Chip (Blitter).

## 1. Minterms (Logical Operations)

The `bltcon0` register requires a minterm (a 1-byte logic truth table) to dictate how the A, B, and C DMA channels are combined to produce the Destination (D).

### The Cookie-Cut (Masking)

The standard cookie-cutter formula is: `(Source AND Mask) OR (Background AND (NOT Mask))`

Depending on how you assign your channels, the hex value changes:

- **`0xE2`**: Correct when `A = Source`, `B = Mask`, `C = Background` (Recommended)
- **`0xCA`**: Correct when `A = Mask`, `B = Source`, `C = Background`

### Other useful minterms

- **`0xFA`**: `A OR C` (Ignores B/Mask entirely). Useful for debugging to see if a bob is rendering without its mask.
- **`0xF0`**: `D = A` (Direct copy). Used for restoring the background.

## 2. The "Chained Assignment" Hardware Trap

In C, it is common to chain assignments for variables sharing the same value. **Never do this for Amiga Custom Registers.**

```c
// BAD: This will read open-bus garbage from bltdpt and write it to bltcpt!
custom->bltcpt = custom->bltdpt = dest;
```

Most Blitter registers are **Write-Only**. When the C compiler executes the chain, it writes to the first register, reads the value _back_ from the hardware, and writes it to the next. Reading a write-only register returns random garbage, completely breaking your pointers and modulos.

```c
// GOOD: Assign one by one
custom->bltcpt = dest;
custom->bltdpt = dest;
```

## 3. Modulo Math: Single Images vs. Sprite Sheets

When blitting interleaved bobs, the pointers and modulos (stride) change depending on whether the asset is a standalone image (e.g., a 64x64 PNG) or a tile packed inside a larger sprite sheet (e.g., a 16x16 tile inside a 320x320 PNG).

### Single Image (Exact Fit)

If the image width perfectly matches the blit width, the mask data is located immediately after the plane data.

- **Mask Pointer (`bltbpt`):** `src + (width / 8)`
- **Modulo (`bltamod` / `bltbmod`):** `width / 8`

### Sprite Sheets (Tile Extraction)

If you are extracting a small tile (e.g., 16px wide) from a large sprite sheet (e.g., 320px wide), the blitter must be told how to "skip" the rest of the 320px line to jump down to the next plane.

- **Mask Pointer (`bltbpt`):** The mask is located exactly one _full sheet width_ away. `src + (sheet_width / 8)`
- **Modulo (`bltamod` / `bltbmod`):** Every plane line consists of the image line + mask line. You must stride by `(sheet_width / 8) * 2`, minus the bytes you actually blitted `(blit_width / 8)`.

### The Universal Formula

This math dynamically handles both exact-fit images and sprite sheets:

```c
int src_modulo = (sheet_width / 8) * 2 - (blit_width / 8);
int mask_offset = (sheet_width / 8);

custom->bltapt = src;
custom->bltamod = src_modulo;
custom->bltbpt = src + mask_offset;
custom->bltbmod = src_modulo;
```

## 4. Sub-Word Shifting (Smooth Movement)

The Amiga blitter works in 16-pixel Words. If you want to move a bob to an X coordinate that is not a multiple of 16 (e.g., `x = 97`), you must shift the bits dynamically.

```c
custom->bltcon0 = 0xE2 | ((x & 15) << ASHIFTSHIFT);
custom->bltcon1 = ((x & 15) << BSHIFTSHIFT);
```

_Note:_ When shifting, bits will overflow off the right side of the word. To prevent the right edge of your bob from being cut off, you must pad your graphics with an extra 16-pixel blank word on the right side and increase your `bltsize` width by 1 word.

## 5. Background Restoration

When you cookie-cut a bob onto the screen, the background pixels underneath are permanently destroyed. To animate a bob without leaving a trail:

1. Keep a `clean_bg` buffer in Chip RAM that is never drawn on.
2. Keep a `screen` buffer that the Copper displays.
3. Every frame, use the Blitter to copy (`0xF0` minterm) a block of the `clean_bg` over the bob's _old_ position.
4. Update the bob's coordinates.
5. Blit the bob to the _new_ position.

```c
// Fast D=A Background Restore
custom->bltcon0 = SRCA | DEST | 0xf0;
custom->bltcon1 = 0;

int byte_offset = (320 / 8) * 5 * y + (x / 16) * 2;
int modulo = (320 - width) / 8;

custom->bltapt = clean_bg + byte_offset;
custom->bltamod = modulo;
custom->bltdpt = screen_buffer + byte_offset;
custom->bltdmod = modulo;

custom->bltafwm = 0xffff;
custom->bltalwm = 0xffff;
custom->bltsize = ((height * 5) << HSIZEBITS) | (width / 16);
```

## 6. The Channel B Mask Trap (1-Pass vs. 2-Pass Blitting)

When shifting a bob by a sub-word amount (e.g., 3 pixels), the bits overflow into an extra 16-pixel word in memory. To draw this, you must increase your blit width (`bltsize`) by 1 word.

However, the Amiga hardware **does not have a Last Word Mask for Channel B!** If your sprite sheet is tightly packed, expanding the blit width causes Channel B to suck in the neighboring sprite's mask, shifting it and ripping black holes into your background.

There are two ways to solve this classic memory vs. speed trade-off:

### The Single-Pass Method (Padded Sprite Sheets)

You physically add a 1-word (16px) empty padding column between masked bobs in the sprite sheet asset itself.

- **Pros:** Maximum performance. Uses the standard `0xE2` cookie-cut minterm in a single pass. Costs only 4 DMA cycles per word.
- **Cons:** Wastes Chip RAM memory due to the empty space in the sprite sheet.

**How to pad your sprite sheet:**

- **Width (Right-side only):** You need exactly 16 pixels (1 Word) of completely transparent empty space to the right of every sprite. The Amiga Blitter only has to perform complex bit-shifting for sub-pixel movement on the horizontal X-axis. When shifting right, pixels overflow into the next word.
- **Height (No padding):** Moving a sprite vertically does not require any bit-shifting; you just tell the blitter to start reading from a lower row in memory. Sprites can be tightly packed vertically.

_Example Grid for 16x16 Sprites:_
If your game object is a 16x16 pixel sprite, place it inside a 32x16 bounding box.

- **X (Horizontal):** Space them every 32 pixels (Draw at X: 0, 32, 64, 96...)
- **Y (Vertical):** Pack them tightly every 16 pixels (Draw at Y: 0, 16, 32, 48...)

**Pre-Padding Assets vs. In-Memory Padding:**
It is highly recommended to pad your sprites directly in your graphics program rather than injecting padding dynamically in Chip RAM at runtime.

- **Data Compression:** The empty 16-pixel padding consists of zeroes, which compress extremely well (e.g., using RLE or dictionary compression). A padded 32x16 sprite sheet will compress to almost the exact same size on disk as a tightly packed 16x16 sheet.
- **Code Complexity:** Injecting padding into a 5-plane interleaved image at runtime requires allocating a new memory block, looping through every row and plane, and carefully copying bits while injecting zeroes. This adds unnecessary complexity and load time.
- **Visual Debugging:** Pre-padded assets ensure that what you see in the hardware graphics debugger perfectly matches the data on disk.

### The Two-Pass Method (Tightly Packed Sprite Sheets)

You abandon Channel B entirely and perform the cookie-cut in two passes using Channel A, because Channel A **does** have a Last Word Mask (`bltalwm`).

1. **Pass 1:** Cut the hole using the mask data. (Minterm `0x0A`: `D = C & ~A`)
2. **Pass 2:** Draw the image over the hole. (Minterm `0xFA`: `D = C | A`)

By dynamically setting `bltalwm = 0x0000` when reading the extra word, you cleanly mask out the neighboring sprite's image data before it hits the screen.

- **Pros:** Preserves tight 16x16 tile sizes and saves memory. No asset padding required.
- **Cons:** 50% slower. Costs 6 DMA cycles per word (3 for Pass 1, 3 for Pass 2) instead of 4. Requires the CPU to set up registers twice and call `WaitBlt()` in the middle of drawing a single bob, halting parallel processing between the CPU and the Blitter.
