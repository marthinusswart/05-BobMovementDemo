# Amiga BPL Tile Viewer Utility

This document contains the prompt recipe and source code for a Python utility designed to visualize Amiga `.bpl` tilesets that have been exported via Kingcon.

Keeping Python tooling separate from the Amiga C codebase ensures the build environment remains clean.

---

## Asset Export Context

The tileset is generated from a 320x320 `.png` image using `kingcon` with the following flags:
```bat
kingcon packman_tiles.png ..\packman_tiles -W=16 -H=16 -Interleaved -Format=5 -RawPalette -Mask
```

**Data Structure:**
Because of the `-W=16 -H=16` flags, Kingcon slices the 320x320 image into a sequential array of 400 individual 16x16 tiles.
*   **Dimensions:** 16x16 pixels per tile.
*   **Planes:** 5 bitplanes.
*   **Format:** Interleaved image data and mask data.
*   **Size:** Each tile is exactly 320 bytes in memory.

---

## Prompt for LLM Regeneration

If you lose the script below and need Gemini to rebuild it, you can paste the following prompt:

> "I have an Amiga project where I export a 320x320 PNG sprite sheet into a `.bpl` and `.pal` file using Kingcon with the following command:
> `kingcon packman_tiles.png ..\packman_tiles -W=16 -H=16 -Interleaved -Format=5 -RawPalette -Mask`
> 
> This creates a tileset of 16x16 bobs. Because it is 5 planes, interleaved, and includes a mask, each 16x16 tile is exactly 320 bytes long sequentially in memory. 
> 
> Please write a Python script using the Pillow library that takes the `.bpl` and `.pal` files as input, reconstructs the 320x320 image, and overlays a grid with the 0-indexed `row,col` on top of each tile so I can easily find the array indices for my Amiga C code."

---

## Python Script (`tile_viewer.py`)

Save the following code as `tile_viewer.py` in your `tools` directory. 

**Requirements:**
```sh
pip install Pillow
```

**Usage:**
```sh
python tile_viewer.py ../packman_tiles.bpl ../packman_tiles.pal output.png
```

**Source Code:**
```python
#!/usr/bin/env python3
import sys
from PIL import Image, ImageDraw, ImageFont

# --- Constants based on Kingcon export ---
TILE_WIDTH = 16
TILE_HEIGHT = 16
SHEET_WIDTH_IN_TILES = 20  # 320 / 16
SHEET_HEIGHT_IN_TILES = 20 # 320 / 16
NUM_PLANES = 5
BYTES_PER_TILE = 320 # 16x16, 5 planes, with mask

def parse_palette(pal_path):
    """Reads an Amiga .pal file and returns a list of (R, G, B) tuples."""
    palette = []
    with open(pal_path, 'rb') as f:
        while True:
            word_bytes = f.read(2)
            if not word_bytes:
                break
            word = int.from_bytes(word_bytes, 'big')
            # Amiga color format is 12-bit: 0x0RGB
            r = (word >> 8) & 0xF
            g = (word >> 4) & 0xF
            b = word & 0xF
            # Scale 4-bit color components to 8-bit (0-15 -> 0-255)
            palette.append((r * 17, g * 17, b * 17))
    return palette

def render_bpl(bpl_path, palette):
    """Renders a .bpl tileset into a Pillow Image object."""
    img_width = TILE_WIDTH * SHEET_WIDTH_IN_TILES
    img_height = TILE_HEIGHT * SHEET_HEIGHT_IN_TILES
    output_image = Image.new('RGB', (img_width, img_height))
    pixels = output_image.load()

    with open(bpl_path, 'rb') as f:
        bpl_data = f.read()

    for tile_y in range(SHEET_HEIGHT_IN_TILES):
        for tile_x in range(SHEET_WIDTH_IN_TILES):
            tile_index = tile_y * SHEET_WIDTH_IN_TILES + tile_x
            tile_data_start = tile_index * BYTES_PER_TILE
            tile_data = bpl_data[tile_data_start : tile_data_start + BYTES_PER_TILE]

            for y in range(TILE_HEIGHT):
                for x in range(TILE_WIDTH):
                    color_index = 0
                    for plane_idx in range(NUM_PLANES):
                        plane_offset = plane_idx * (TILE_HEIGHT * 4)
                        line_offset = y * 4
                        byte_offset_in_line = x // 8
                        byte_pos = plane_offset + line_offset + byte_offset_in_line
                        
                        if byte_pos < len(tile_data):
                            byte_val = tile_data[byte_pos]
                            bit_pos = 7 - (x % 8)
                            if (byte_val >> bit_pos) & 1:
                                color_index |= (1 << plane_idx)

                    rgb_color = palette[color_index]
                    dest_x = tile_x * TILE_WIDTH + x
                    dest_y = tile_y * TILE_HEIGHT + y
                    pixels[dest_x, dest_y] = rgb_color
                    
    return output_image

def add_grid_and_indices(image):
    """Draws a grid and row/col indices on the image."""
    draw = ImageDraw.Draw(image)
    try:
        font = ImageFont.truetype("Arial.ttf", 10)
    except IOError:
        font = ImageFont.load_default()

    for tile_y in range(SHEET_HEIGHT_IN_TILES):
        for tile_x in range(SHEET_WIDTH_IN_TILES):
            text = f"{tile_y},{tile_x}"
            pos_x = tile_x * TILE_WIDTH + 1
            pos_y = tile_y * TILE_HEIGHT
            draw.rectangle([pos_x, pos_y, pos_x + 22, pos_y + 10], fill=(0,0,0,128))
            draw.text((pos_x, pos_y), text, font=font, fill=(255, 255, 0))

    return image

def main():
    if len(sys.argv) != 4:
        print(f"Usage: python {sys.argv[0]} <input.bpl> <input.pal> <output.png>")
        sys.exit(1)

    bpl_path, pal_path, output_path = sys.argv[1], sys.argv[2], sys.argv[3]
    palette = parse_palette(pal_path)
    rendered_image = render_bpl(bpl_path, palette)
    final_image = add_grid_and_indices(rendered_image)
    final_image.save(output_path)

if __name__ == "__main__":
    main()
```