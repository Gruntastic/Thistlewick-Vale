"""Generates T_Palette_Atlas.png (256x256, 4x4 cells of 64 px) and palette.json per ART_CONTRACT section 2.
Run: python3 make_palette_atlas.py   (outputs next to this script). Uses PIL if present, else stdlib PNG writer."""
import json, os, struct, zlib

PALETTE = [
    ("Hearth Red", "#C8553D"), ("Ember Gold", "#FFB347"), ("Wheat", "#F2CC8F"), ("Clay", "#B5835A"),
    ("Timber", "#7F5539"), ("Dark Bark", "#4A3428"), ("Sky Teal", "#5FA8A0"), ("Dusk Blue", "#3D5A80"),
    ("Moss Green", "#6A994E"), ("Deep Pine", "#386641"), ("Meadow Lime", "#A7C957"), ("Moss Glow", "#9CFF7A"),
    ("Stone Grey", "#8D99AE"), ("Slate", "#4A5568"), ("Bone", "#EDE6D6"), ("Hollow Violet", "#7A5CFF"),
]
SIZE, CELL, GRID = 256, 64, 4
HERE = os.path.dirname(os.path.abspath(__file__))


def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def write_png(path):
    try:
        from PIL import Image
        img = Image.new("RGB", (SIZE, SIZE))
        for i, (_, h) in enumerate(PALETTE):
            cx, cy = (i % GRID) * CELL, (i // GRID) * CELL
            img.paste(rgb(h), (cx, cy, cx + CELL, cy + CELL))
        img.save(path, optimize=True)
        return
    except ImportError:
        pass
    rows = []
    for y in range(SIZE):
        row = bytearray(b"\x00")
        for x in range(SIZE):
            row += bytes(rgb(PALETTE[(y // CELL) * GRID + (x // CELL)][1]))
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    write_png(os.path.join(HERE, "T_Palette_Atlas.png"))
    cells = []
    for i, (name, h) in enumerate(PALETTE):
        col, row = i % GRID, i // GRID
        cells.append({"index": i, "name": name, "hex": h, "row": row, "col": col,
                      "uv_center": [(col * CELL + CELL / 2) / SIZE, (row * CELL + CELL / 2) / SIZE]})
    with open(os.path.join(HERE, "palette.json"), "w") as f:
        json.dump({"texture": "T_Palette_Atlas", "size": SIZE, "cell_px": CELL, "grid": GRID,
                   "index_order": "row-major from top-left; UV origin top-left (v down)",
                   "emissive_cells": [1, 11, 15], "cells": cells}, f, indent=2)
    print("wrote atlas + palette.json")


if __name__ == "__main__":
    main()
