"""Procedural low-poly environment and prop kit for every ENV_ and PRP_ row of the asset manifest.

Run headless:   python Tools/blender/env_kit.py [ID ...]       (needs `pip install bpy`)
          or:   blender -b -P Tools/blender/env_kit.py -- [ID ...]

Output: SourceArt/Env/<ID>.fbx for ENV_ rows, SourceArt/Props/<ID>.fbx for PRP_ rows.
Every generator is `gen(seed, **params) -> MeshBuilder`, deterministic for a given seed. The REGISTRY
at the bottom lists the default seed/params per manifest ID (variants A/B differ by seed and params).

Pivots: dungeon pieces, ruins and fences sit on the 100 cm grid with the pivot at the bottom-left-back
corner (min x/y/z = 0); everything else has its pivot at the bottom centre. Dungeon modules are
400 cm; walls are 400 x 50 x 400, floors 400 x 400. Glow motif: cells 1 (ember gold), 11 (moss glow)
and 15 (hollow violet) are the emissive palette cells.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ue5_export as ex  # noqa: E402
from tv_geo import MeshBuilder  # noqa: E402
from ue5_export import (BONE, CLAY, DARK_BARK, DEEP_PINE, DUSK_BLUE, EMBER_GOLD, HEARTH_RED,  # noqa: E402
                        HOLLOW_VIOLET, MEADOW_LIME, MOSS_GLOW, MOSS_GREEN, SLATE, SKY_TEAL, STONE_GREY,
                        TIMBER, WHEAT)


# ------------------------------------------------------------------------------------------------
# nature
# ------------------------------------------------------------------------------------------------
def tree_oak(seed=1, height=300, trunk_r=26, canopy=165, blobs=4, leaf=MOSS_GREEN, leaf2=MEADOW_LIME):
    r = random.Random(seed)
    mb = MeshBuilder("oak")
    mb.frustum((0, 0, 0), trunk_r * 1.35, trunk_r, 40, 6, TIMBER)                       # root flare
    mb.frustum((0, 0, 40), trunk_r, trunk_r * 0.7, height - 40, 6, TIMBER, rz=r.uniform(0, 30))
    for i in range(3):                                                                  # branches
        a = r.uniform(0, 360) + i * 120
        d = (math.cos(math.radians(a)), math.sin(math.radians(a)))
        z = height * r.uniform(0.6, 0.85)
        mb.limb((0, 0, z), (d[0] * canopy * 0.7, d[1] * canopy * 0.7, z + 60), 14, 7, 5, DARK_BARK)
    for i in range(blobs):
        a = 360 * i / blobs + r.uniform(-20, 20)
        rad = canopy * (0.35 if blobs > 1 else 0)
        c = (rad * math.cos(math.radians(a)), rad * math.sin(math.radians(a)),
             height + r.uniform(0, 70) + (40 if i % 2 else 0))
        s = canopy * r.uniform(0.75, 1.0)
        mb.blob(c, (s, s, s * 0.8), leaf if i % 2 == 0 else leaf2, seed=seed * 10 + i, sub=1 if i == 0 else 0)
    mb.blob((0, 0, height + 110), (canopy * 0.6,) * 2 + (canopy * 0.5,), leaf2, seed=seed + 99, sub=0)
    return mb


def tree_pine(seed=1, height=620, trunk_r=18, tiers=5, spread=130):
    r = random.Random(seed)
    mb = MeshBuilder("pine")
    mb.frustum((0, 0, 0), trunk_r * 1.2, trunk_r * 0.8, height * 0.3, 6, DARK_BARK)
    for i in range(tiers):
        t = i / (tiers - 1)
        z = height * 0.16 + t * height * 0.66
        rad = spread * (1.0 - t * 0.78) * r.uniform(0.92, 1.08)
        mb.frustum((0, 0, z), rad, 0, height * 0.22 * (1 - t * 0.3) + 40, 8, DEEP_PINE if i % 2 == 0 else MOSS_GREEN,
                   rz=r.uniform(0, 45))
    return mb


def tree_dead_hollow(seed=3, height=480, trunk_r=42):
    r = random.Random(seed)
    mb = MeshBuilder("hollowdead")
    mb.frustum((0, 0, 0), trunk_r * 1.4, trunk_r, 60, 8, DARK_BARK)
    # split trunk: two halves leaving a hollow, with violet glow inside
    mb.frustum((0, 0, 60), trunk_r, trunk_r * 0.6, height - 60, 8, DARK_BARK)
    mb.box((trunk_r * 0.78, 0, 150), (14, 50, 110), HOLLOW_VIOLET)                       # glowing hollow facing +X
    mb.box((trunk_r * 0.62, 0, 150), (6, 62, 124), DARK_BARK)                            # dark surround
    for i, a in enumerate((20, 150, 250, 330)):
        z = height * (0.6 + 0.1 * i)
        d = (math.cos(math.radians(a)), math.sin(math.radians(a)))
        e = (d[0] * 150, d[1] * 150, z + r.uniform(40, 110))
        mb.limb((d[0] * 20, d[1] * 20, z), e, 14, 5, 5, DARK_BARK)
        mb.limb(e, (e[0] + d[0] * 50, e[1] + d[1] * 50, e[2] + 60), 6, 3, 4, DARK_BARK)
    for i in range(3):                                                                   # surface roots
        a = i * 120 + 40
        d = (math.cos(math.radians(a)), math.sin(math.radians(a)))
        mb.limb((d[0] * 30, d[1] * 30, 20), (d[0] * 110, d[1] * 110, 0), 14, 6, 5, DARK_BARK)
    return mb


def bush(seed=1, size=70, berries=6):
    r = random.Random(seed)
    mb = MeshBuilder("bush")
    for i in range(4):
        a = i * 90 + r.uniform(-25, 25)
        mb.blob((size * 0.45 * math.cos(math.radians(a)), size * 0.45 * math.sin(math.radians(a)), size * 0.45),
                (size * 0.62,) * 2 + (size * 0.5,), MOSS_GREEN if i % 2 else MEADOW_LIME, seed=seed * 7 + i, sub=1 if i == 0 else 0)
    mb.blob((0, 0, size * 0.65), (size * 0.6,) * 2 + (size * 0.5,), MOSS_GREEN, seed=seed + 40)
    for i in range(berries):
        a = r.uniform(0, 360)
        mb.box((size * 0.85 * math.cos(math.radians(a)), size * 0.85 * math.sin(math.radians(a)),
                size * r.uniform(0.35, 0.75)), (5, 5, 5), HEARTH_RED)
    return mb


def glowmoss(seed=2, size=40, tufts=14):
    r = random.Random(seed)
    mb = MeshBuilder("glowmoss")
    mb.blob((0, 0, size * 0.22), (size * 0.8, size * 0.7, size * 0.3), MOSS_GREEN, seed=seed, sub=1)
    mb.blob((size * 0.5, size * 0.2, size * 0.18), (size * 0.4, size * 0.4, size * 0.2), STONE_GREY, seed=seed + 1)
    for i in range(tufts):
        a = r.uniform(0, 360)
        d = r.uniform(0, size * 0.65)
        x, y = d * math.cos(math.radians(a)), d * math.sin(math.radians(a))
        mb.frustum((x, y, size * 0.18), 4.5, 0, r.uniform(size * 0.3, size * 0.7), 4, MOSS_GLOW, rot=(r.uniform(-12, 12), r.uniform(-12, 12), 0))
    return mb


def rock(seed=1, size=(120, 100, 90), sub=1, flat=0.0, moss=True):
    mb = MeshBuilder("rock")
    sx, sy, sz = size
    mb.blob((0, 0, sz * (0.45 - flat)), (sx / 2, sy / 2, sz / 2 * (1 - flat)), STONE_GREY, seed=seed, sub=sub, jitter=0.2)
    if sub == 1 and sz > 100:
        mb.blob((sx * 0.3, sy * 0.15, sz * 0.2), (sx * 0.22, sy * 0.25, sz * 0.2), SLATE, seed=seed + 5, jitter=0.18)
    return mb


# ------------------------------------------------------------------------------------------------
# buildings
# ------------------------------------------------------------------------------------------------
def cottage(seed=1, w=500, d=400, wall_h=250, roof_h=170, wall=WHEAT, roof=HEARTH_RED, porch=True, chimney_side=1):
    r = random.Random(seed)
    mb = MeshBuilder("cottage")
    mb.box((0, 0, 20), (w + 20, d + 20, 40), STONE_GREY)                                  # foundation
    mb.box((0, 0, 40 + wall_h / 2), (w, d, wall_h), wall)
    for sx in (-1, 1):                                                                    # corner timbers
        for sy in (-1, 1):
            mb.box((sx * w / 2, sy * d / 2, 40 + wall_h / 2), (18, 18, wall_h), TIMBER)
    mb.box((0, d / 2 + 2, 40 + wall_h - 8), (w, 8, 16), TIMBER)
    mb.box((0, -d / 2 - 2, 40 + wall_h - 8), (w, 8, 16), TIMBER)
    mb.gable((-w / 2, -d / 2, 40 + wall_h), (w / 2, d / 2, 40 + wall_h), roof_h, roof, overhang=30)
    mb.box((0, 0, 40 + wall_h + roof_h - 6), (w + 70, 16, 14), TIMBER)                    # ridge cap
    # door on +X (front), windows glow ember gold
    mb.box((w / 2 + 3, 0, 40 + 100), (10, 90, 200), DARK_BARK)
    mb.box((w / 2 + 6, 0, 40 + 110), (6, 70, 180), TIMBER)
    for y in (-d / 3, d / 3):
        mb.box((w / 2 + 3, y, 40 + 140), (8, 50, 50), EMBER_GOLD)
        mb.box((w / 2 + 6, y, 40 + 140), (4, 62, 6), TIMBER)
    mb.box((0, d / 2 + 3, 40 + 140), (60, 8, 50), EMBER_GOLD)
    mb.box((0, -d / 2 - 3, 40 + 140), (60, 8, 50), EMBER_GOLD)
    # chimney
    cy = chimney_side * d * 0.28
    mb.box((-w * 0.25, cy, 40 + wall_h + roof_h * 0.7), (50, 50, roof_h * 1.1), STONE_GREY)
    mb.box((-w * 0.25, cy, 40 + wall_h + roof_h * 1.28), (62, 62, 14), SLATE)
    if porch:
        mb.box((w / 2 + 50, 0, 45), (100, 150, 10), TIMBER)                               # step
        for y in (-60, 60):
            mb.box((w / 2 + 90, y, 40 + 90), (14, 14, 180), TIMBER)
        mb.gable((w / 2 + 10, -75, 40 + 185), (w / 2 + 100, 75, 40 + 185), 40, roof, overhang=6, ridge_axis="y")
    return mb


def inn(seed=1, w=1000, d=700):
    mb = cottage(seed, w=w * 0.62, d=d * 0.7, wall_h=330, roof_h=230, wall=WHEAT, roof=HEARTH_RED)
    mb.name = "inn"
    # side wing
    wx, wy = -w * 0.05, d * 0.55
    mb.box((wx, wy, 190), (w * 0.4, d * 0.4, 300), WHEAT)
    mb.gable((wx - w * 0.2, wy - d * 0.2, 340), (wx + w * 0.2, wy + d * 0.2, 340), 150, CLAY, overhang=25, ridge_axis="y")
    for sx in (-1, 1):
        mb.box((wx + sx * w * 0.2, wy - d * 0.2, 190), (18, 18, 300), TIMBER)
    # hanging lantern sign (the hub's glow landmark)
    ox = w * 0.62 / 2
    mb.box((ox + 30, -d * 0.35 - 20, 260), (8, 8, 160), DARK_BARK)
    mb.box((ox + 70, -d * 0.35 - 20, 330), (90, 8, 8), DARK_BARK)
    mb.box((ox + 105, -d * 0.35 - 20, 290), (30, 24, 40), EMBER_GOLD)
    mb.box((ox + 105, -d * 0.35 - 20, 315), (36, 30, 8), DARK_BARK)
    for y in (-d * 0.2, d * 0.2):                                                         # porch lanterns
        mb.box((ox + 14, y, 250), (18, 18, 26), EMBER_GOLD)
    # extra chimney stack + barrel stack + bench
    mb.box((-w * 0.2, 0, 560), (60, 60, 160), STONE_GREY)
    mb.box((ox + 70, 0, 70), (100, 340, 14), TIMBER)
    return mb


def watchtower(seed=1, base=320, height=900):
    mb = MeshBuilder("tower")
    mb.frustum((0, 0, 0), base / 2, base / 2.5, height * 0.55, 8, STONE_GREY)
    for i in range(3):
        z = 70 + i * 130
        mb.box((base / 2.4, 0, z), (12, 40, 70), SLATE)                                   # arrow slits face +X
    mb.frustum((0, 0, height * 0.55), base / 2.1, base / 2.1, 24, 8, TIMBER)              # gallery floor
    for i in range(8):
        a = 2 * math.pi * i / 8 + math.pi / 8
        mb.box((base / 2.2 * math.cos(a), base / 2.2 * math.sin(a), height * 0.55 + 70), (16, 16, 140), TIMBER)
    mb.frustum((0, 0, height * 0.55 + 140), base / 2.0, base / 2.0, 14, 8, TIMBER)
    mb.frustum((0, 0, height * 0.55 + 154), base / 2.15, 0, height * 0.4 - 100, 8, HEARTH_RED)
    mb.box((base / 2.0 + 6, 0, height * 0.55 + 90), (14, 20, 24), EMBER_GOLD)             # beacon lantern
    mb.box((base / 2.3, 0, 100), (30, 80, 190), DARK_BARK)                                # door
    mb.box((base / 2.3 + 14, 0, 10), (50, 130, 16), STONE_GREY)
    return mb


def fence(seed=1, length=400, height=110, corner=False):
    r = random.Random(seed)
    mb = MeshBuilder("fence")

    def run(x0, y0, x1, y1):
        n = max(2, int(math.hypot(x1 - x0, y1 - y0) // 100) + 1)
        for i in range(n):
            t = i / (n - 1)
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            mb.box((x, y, height / 2), (14, 14, height + r.uniform(-6, 6)), TIMBER)
        for z in (height * 0.35, height * 0.75):
            mb.limb((x0, y0, z), (x1, y1, z), 5, 5, 4, DARK_BARK)

    if not corner:
        run(7, 7, length - 7, 7)
    else:
        run(7, 7, length - 7, 7)
        run(7, 7, 7, length - 7)
    return mb


def ruin_wall(seed=1, w=400, h=300, d=60, gaps=0.3):
    r = random.Random(seed)
    mb = MeshBuilder("ruinwall")
    cols, rows = 4, 6
    bw, bh = w / cols, h / rows
    for row in range(rows):
        for c in range(cols):
            top_missing = row >= 3 and r.random() < gaps + (row - 3) * 0.2
            if top_missing:
                continue
            off = (bw / 2 if row % 2 else 0)
            x0, x1 = c * bw - off + 0, c * bw - off + bw
            x0, x1 = max(0, x0), min(w, x1)
            if x1 - x0 < 12:
                continue
            mb.box_min((x0 + 1, 0, row * bh + 1), (x1 - 1, d, (row + 1) * bh - 1), STONE_GREY if (row + c) % 3 else SLATE)
    mb.blob((w * 0.3, d / 2, h * 0.52), (70, 40, 14), MOSS_GREEN, seed=seed, jitter=0.2)
    return mb


def ruin_arch(seed=1, w=400, h=400, d=60):
    mb = MeshBuilder("ruinarch")
    pw = 90
    for x0 in (0, w - pw):
        for row in range(5):
            mb.box_min((x0 + 1, 0, row * 62 + 1), (x0 + pw - 1, d, row * 62 + 61), STONE_GREY if row % 2 else SLATE)
    n = 7
    for i in range(n):  # semicircular arch ring from stone wedges
        a0, a1 = math.pi * i / n, math.pi * (i + 1) / n
        r_o, r_i, cx, cz = (w / 2), (w / 2 - 70), w / 2, 270
        pts = []
        for a, rr in ((a0, r_i), (a0, r_o), (a1, r_o), (a1, r_i)):
            pts.append((cx + rr * math.cos(a), cz + rr * math.sin(a)))
        base = len(mb.verts)
        for (x, z) in pts:
            mb.verts.append((x, 0, z))
        for (x, z) in pts:
            mb.verts.append((x, d, z))
        for f in ((0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)):
            mb.faces.append(tuple(base + k for k in f))
            mb.cells.append(STONE_GREY if i % 2 else SLATE)
            mb.secondary.append(False)
    mb.blob((w * 0.12, d / 2, 335), (60, 36, 12), MOSS_GREEN, seed=seed)
    return mb


def ruin_pillar(seed=1, h=400, r=45, broken=0.0):
    rr = random.Random(seed)
    mb = MeshBuilder("ruinpillar")
    cx = cy = 50
    top = h * (1 - broken)
    mb.frustum((cx, cy, 0), 48, 42, 40, 8, STONE_GREY)                                      # plinth
    mb.frustum((cx, cy, 40), r, r * 0.85, top - 80, 8, SLATE)
    if not broken:
        mb.frustum((cx, cy, h - 40), r * 1.1, 48, 40, 8, STONE_GREY)
    mb.blob((cx, cy, top - 40 if broken else h - 48), (r * 0.9, r * 0.9, 14), MOSS_GREEN, seed=seed)
    return mb


def stag_shrine(seed=1, size=600):
    mb = MeshBuilder("shrine")
    mb.frustum((0, 0, 0), size / 2, size / 2.2, 30, 8, STONE_GREY)                         # steps
    mb.frustum((0, 0, 30), size / 2.8, size / 3.1, 30, 8, SLATE)
    for a in (-1, 1):
        for b in (-1, 1):                                                                   # four moss posts
            mb.box((a * size * 0.3, b * size * 0.3, 190), (40, 40, 260), STONE_GREY)
            mb.blob((a * size * 0.3, b * size * 0.3, 330), (34, 34, 14), MOSS_GREEN, seed=a * 3 + b)
    for b in (-1, 1):
        mb.box((0, b * size * 0.3, 345), (size * 0.66, 36, 36), SLATE)
    # stag statue
    sx = 0
    mb.box((sx, 0, 120), (110, 44, 50), STONE_GREY)
    for x in (-40, 40):
        for y in (-14, 14):
            mb.box((sx + x, y, 80), (14, 14, 60), STONE_GREY)
    mb.box((sx + 55, 0, 175), (30, 28, 70), STONE_GREY, rot=(0, -20, 0))
    mb.box((sx + 75, 0, 205), (40, 26, 28), STONE_GREY)
    for s in (-1, 1):
        mb.limb((sx + 66, s * 8, 215), (sx + 55, s * 40, 290), 5, 3.5, 5, BONE)
        mb.limb((sx + 55, s * 40, 290), (sx + 80, s * 52, 320), 4, 2.5, 4, BONE)
    for i, (x, y) in enumerate(((120, 90), (120, -90), (-90, 120))):                       # offering bowls with glow
        mb.frustum((x, y, 60), 26, 18, 18, 6, STONE_GREY)
        mb.blob((x, y, 82), (14, 14, 10), EMBER_GOLD, seed=i)
    mb.blob((0, 0, 330), (24, 24, 24), MOSS_GLOW, seed=7)                                   # floating glow orb
    return mb


def barrow_entrance(seed=1, w=1000, d=800):
    mb = MeshBuilder("barrow")
    mb.blob((-w * 0.1, 0, 120), (w * 0.52, d * 0.5, 230), MOSS_GREEN, seed=seed, sub=1, jitter=0.07)   # grassy mound
    mb.blob((-w * 0.2, d * 0.2, 210), (w * 0.2, d * 0.2, 80), MEADOW_LIME, seed=seed + 2)
    fx = w * 0.3
    for y in (-130, 130):
        mb.box((fx, y, 170), (80, 70, 340), STONE_GREY)                                     # door posts
    mb.box((fx, 0, 355), (90, 380, 70), SLATE)                                              # lintel
    mb.box((fx - 28, 0, 150), (20, 190, 300), HOLLOW_VIOLET)                                # glowing seal
    for i in range(4):                                                                      # steps
        mb.box((fx + 70 + i * 60, 0, 10 + (3 - i) * 12 + 6), (60, 320 + i * 30, 20 + (3 - i) * 12), STONE_GREY)
    for y in (-300, 300):                                                                   # brazier posts with violet fire
        mb.frustum((fx + 90, y, 0), 24, 16, 120, 6, SLATE)
        mb.blob((fx + 90, y, 140), (18, 18, 24), HOLLOW_VIOLET, seed=int(y))
    for i, (x, y) in enumerate(((fx + 20, -240), (fx + 20, 250), (fx - 90, -190))):         # fallen standing stones
        mb.box((x, y, 100), (50, 50, 200), STONE_GREY, rot=(0, 0, 18 * i))
    return mb


# ------------------------------------------------------------------------------------------------
# dungeon kit: 400 cm modules on the 100 cm grid, pivot bottom-left-back corner (x 0..400, y 0..50)
# ------------------------------------------------------------------------------------------------
def dgn_wall(seed=1, door=False):
    """400 x 50 x 400. Body sits at y 6..44, trim fills out to the 0..50 envelope."""
    mb = MeshBuilder("dgnwall")
    W, D, H = 400, 50, 400
    y0, y1 = 6, 44
    if not door:
        mb.box_min((0, y0, 0), (W, y1, H), SLATE)
        for z in (100, 200, 300):
            mb.box_min((0, 0, z - 6), (W, y0, z + 6), STONE_GREY)                           # mortar-course ribs
        mb.box_min((0, y1, 0), (W, D, 40), STONE_GREY)                                      # skirting
        mb.box_min((0, y1, H - 40), (W, D, H), STONE_GREY)                                  # cornice
    else:
        dw, dh = 200, 300
        x0 = (W - dw) / 2
        mb.box_min((0, y0, 0), (x0, y1, H), SLATE)
        mb.box_min((x0 + dw, y0, 0), (W, y1, H), SLATE)
        mb.box_min((x0, y0, dh), (x0 + dw, y1, H), SLATE)
        mb.box_min((x0 - 16, 0, 0), (x0, D, dh + 16), STONE_GREY)                           # frame
        mb.box_min((x0 + dw, 0, 0), (x0 + dw + 16, D, dh + 16), STONE_GREY)
        mb.box_min((x0 - 16, 0, dh), (x0 + dw + 16, D, dh + 16), STONE_GREY)
        mb.box_min((x0 + 70, 20, dh - 30), (x0 + dw - 70, 30, dh - 10), HOLLOW_VIOLET)      # glowing keystone sigil
        mb.box_min((0, y1, 0), (x0 - 16, D, 40), STONE_GREY)
        mb.box_min((x0 + dw + 16, y1, 0), (W, D, 40), STONE_GREY)
    return mb


def dgn_floor(seed=1, tiles=4):
    r = random.Random(seed)
    mb = MeshBuilder("dgnfloor")
    mb.box_min((0, 0, 0), (400, 400, 14), SLATE)
    t = 400 / tiles
    for i in range(tiles):
        for j in range(tiles):
            mb.box_min((i * t + 4, j * t + 4, 14), (i * t + t - 4, j * t + t - 4, 20),
                       STONE_GREY if r.random() < 0.7 else SLATE)
    return mb


def dgn_pillar(seed=1):
    mb = MeshBuilder("dgnpillar")
    c = 50
    mb.box_min((0, 0, 0), (100, 100, 40), STONE_GREY)
    mb.frustum((c, c, 40), 36, 30, 300, 8, SLATE)
    mb.box_min((0, 0, 360), (100, 100, 400), STONE_GREY)
    mb.box_min((22, 22, 340), (78, 78, 360), STONE_GREY)
    mb.box_min((38, 25, 150), (62, 38, 190), HOLLOW_VIOLET)                                 # glowing rune slab facing -Y
    return mb


def dgn_stairs(seed=1, steps=8):
    mb = MeshBuilder("dgnstairs")
    run = 400 / steps
    rise = 200 / steps
    for i in range(steps):
        mb.box_min((i * run, 0, 0), ((i + 1) * run, 400, (i + 1) * rise), STONE_GREY if i % 2 else SLATE)
    mb.box_min((0, 0, 0), (400, 14, 40), SLATE)
    mb.box_min((0, 386, 0), (400, 400, 40), SLATE)
    return mb


def dgn_throne(seed=1):
    mb = MeshBuilder("dgnthrone")
    mb.box_min((0, 0, 0), (400, 400, 30), SLATE)                                            # lower dais
    mb.box_min((40, 40, 30), (360, 360, 60), STONE_GREY)                                    # upper dais
    mb.box_min((120, 130, 60), (280, 270, 100), SLATE)                                      # seat base
    mb.box_min((120, 130, 100), (280, 270, 130), STONE_GREY)                                # seat
    mb.box_min((120, 130, 130), (160, 270, 300), SLATE)                                     # back (toward -X, throne faces +X)
    for y in (130, 244):
        mb.box_min((160, y, 130), (270, y + 26, 180), SLATE)                                # arms
    for i, y in enumerate((150, 190, 230)):
        mb.frustum((140, y + 10, 300), 18, 0, 70, 4, EMBER_GOLD)                            # crown-like spikes
    mb.box_min((124, 190, 200), (130, 210, 260), HOLLOW_VIOLET)                             # glowing gem on the back face
    for y in (80, 320):
        mb.frustum((250, y, 60), 20, 16, 160, 6, SLATE)
        mb.blob((250, y, 235), (18, 18, 22), HOLLOW_VIOLET, seed=y)
    return mb


def torch_wall(seed=1):
    mb = MeshBuilder("torch")
    mb.box((-4, 0, 30), (8, 16, 60), DARK_BARK)                                             # back plate on the wall (x = 0 wall)
    mb.limb((0, 0, 30), (22, 0, 54), 5, 4, 5, DARK_BARK)                                    # tilted handle
    mb.frustum((22, 0, 54), 8, 11, 22, 6, DARK_BARK)                                         # cup
    mb.blob((22, 0, 90), (13, 13, 22), EMBER_GOLD, seed=seed)                                # flame
    mb.frustum((22, 0, 94), 6, 0, 30, 4, EMBER_GOLD)
    return mb


# ------------------------------------------------------------------------------------------------
# props
# ------------------------------------------------------------------------------------------------
def lantern(seed=1):
    mb = MeshBuilder("lantern")
    mb.frustum((0, 0, 0), 11, 12, 4, 6, DARK_BARK)
    mb.frustum((0, 0, 4), 8, 8, 24, 6, EMBER_GOLD)                                            # glowing core
    for i in range(6):
        a = math.radians(i * 60)
        mb.box((11 * math.cos(a), 11 * math.sin(a), 16), (4, 4, 28), DARK_BARK, rot=(0, 0, i * 60))   # 4 cm frame ribs
    mb.frustum((0, 0, 30), 14, 5, 8, 6, DARK_BARK)
    mb.box((0, 0, 42), (4, 14, 8), DARK_BARK)                                                  # carry handle
    return mb


def crate(seed=1, s=80):
    mb = MeshBuilder("crate")
    mb.box((0, 0, s / 2), (s - 6, s - 6, s - 6), TIMBER)
    for z in (4, s - 4):
        mb.box((0, 0, z), (s, s, 8), DARK_BARK)
    for x in (-1, 1):
        for y in (-1, 1):
            mb.box((x * (s / 2 - 5), y * (s / 2 - 5), s / 2), (10, 10, s), DARK_BARK)
    mb.box((s / 2 - 1, 0, s / 2), (4, 10, s - 16), DARK_BARK, rot=(0, 0, 0))
    return mb


def barrel(seed=1, h=90, r=30):
    mb = MeshBuilder("barrel")
    mb.frustum((0, 0, 0), r * 0.82, r, h * 0.5, 8, TIMBER, caps=False)
    mb.frustum((0, 0, h * 0.5), r, r * 0.82, h * 0.5, 8, TIMBER, caps=True)
    mb.frustum((0, 0, 0), r * 0.82, r * 0.82, 1, 8, DARK_BARK)
    for z in (h * 0.16, h * 0.84):
        rr = r * (0.82 + 0.18 * (1 - abs(z - h / 2) / (h / 2)) ** 0.5)
        mb.frustum((0, 0, z - 4), rr + 2, rr + 2, 8, 8, DARK_BARK)
    return mb


def signpost(seed=1):
    mb = MeshBuilder("signpost")
    mb.box((0, 0, 85), (14, 14, 170), TIMBER)
    mb.box((9, 0, 140), (6, 80, 30), WHEAT)
    mb.pyramid((9, 40, 140), 30, 6, 30, WHEAT, rot=(0, 90, 0))
    mb.box((9, 0, 100), (6, 60, 26), WHEAT, rot=(0, 0, 0))
    mb.box((9, 0, 140), (7, 60, 4), HEARTH_RED)
    mb.box((0, 0, 4), (40, 40, 8), STONE_GREY)
    mb.blob((9, 0, 168), (6, 6, 6), EMBER_GOLD, seed=seed)                                    # tiny lantern bead
    return mb


def campfire(seed=1):
    mb = MeshBuilder("campfire")
    for i in range(8):
        a = math.radians(i * 45)
        mb.blob((46 * math.cos(a), 46 * math.sin(a), 8), (12, 12, 9), STONE_GREY, seed=i)
    for i in range(5):
        a = i * 72
        mb.limb((26 * math.cos(math.radians(a)), 26 * math.sin(math.radians(a)), 6),
                (-4 * math.cos(math.radians(a)), -4 * math.sin(math.radians(a)), 36), 6, 5, 5, DARK_BARK)
    mb.blob((0, 0, 40), (16, 16, 26), EMBER_GOLD, seed=seed)
    mb.frustum((0, 0, 42), 10, 0, 40, 5, EMBER_GOLD)
    mb.blob((0, 0, 12), (28, 28, 6), DARK_BARK, seed=2)
    return mb


def chest_quest(seed=1):
    mb = MeshBuilder("chest")
    mb.box((0, 0, 20), (90, 56, 40), TIMBER)
    mb.gable((-45, -28, 40), (45, 28, 40), 24, TIMBER, overhang=2)                           # rounded-ish lid
    for x in (-34, 34):
        mb.box((x, 0, 30), (10, 60, 62), DARK_BARK)                                           # iron bands
    mb.box((45 + 2, 0, 38), (6, 14, 16), EMBER_GOLD)                                          # glowing lock = quest cue
    mb.blob((0, 0, 70), (14, 14, 8), EMBER_GOLD, seed=seed)                                   # shard glimmer on top
    return mb


def bind_stone(seed=1):
    mb = MeshBuilder("bindstone")
    mb.frustum((0, 0, 0), 70, 62, 24, 8, STONE_GREY)
    mb.frustum((0, 0, 24), 46, 36, 180, 6, SLATE, rz=15)
    mb.box((36, 0, 130), (6, 30, 70), EMBER_GOLD)                                             # glowing rune slab facing +X
    mb.box((38, 0, 170), (6, 54, 8), EMBER_GOLD)
    mb.pyramid((0, 0, 204), 56, 56, 40, STONE_GREY)
    mb.blob((0, 0, 20), (80, 80, 12), MOSS_GREEN, seed=seed)
    return mb


def shard_pillar(seed=1):
    mb = MeshBuilder("shardpillar")
    mb.frustum((0, 0, 0), 55, 45, 20, 6, STONE_GREY)
    mb.frustum((0, 0, 20), 38, 30, 140, 6, SLATE)
    mb.frustum((0, 0, 160), 46, 46, 14, 6, STONE_GREY)
    mb.frustum((0, 0, 190), 24, 0, 70, 5, EMBER_GOLD)                                         # floating crown shard
    mb.frustum((0, 0, 190), 24, 0, 30, 5, EMBER_GOLD, rot=(180, 0, 0))                       # lower point
    for i in range(3):
        a = math.radians(i * 120)
        mb.frustum((52 * math.cos(a), 52 * math.sin(a), 0), 10, 0, 50, 4, HOLLOW_VIOLET)      # corruption spikes
    return mb


def ember_crown(seed=1, r=22, spikes=8):
    mb = MeshBuilder("embercrown")
    mb.frustum((0, 0, 0), r, r * 1.02, 12, 8, EMBER_GOLD)                                     # band
    mb.frustum((0, 0, 12), r * 1.02, r, 4, 8, DARK_BARK)
    for i in range(spikes):
        a = math.radians(i * 360 / spikes)
        h = 28 if i % 2 == 0 else 18
        mb.frustum((r * 0.92 * math.cos(a), r * 0.92 * math.sin(a), 12), 6, 0, h, 4, EMBER_GOLD)
        if i % 2 == 0:
            mb.blob((r * 0.92 * math.cos(a), r * 0.92 * math.sin(a), 12 + h + 3), (4, 4, 4), HEARTH_RED, seed=i)
    return mb


def crown_shard(seed=1, h=50):
    mb = MeshBuilder("crownshard")
    mb.frustum((0, 0, 0), 12, 0, h, 5, EMBER_GOLD)
    mb.frustum((0, 0, 0), 12, 0, h * 0.3, 5, EMBER_GOLD, rot=(180, 0, 0))
    mb.frustum((10, 8, 0), 7, 0, h * 0.6, 4, EMBER_GOLD, rot=(8, 10, 0))
    mb.frustum((-9, -6, 0), 6, 0, h * 0.45, 4, EMBER_GOLD, rot=(-8, -10, 0))
    return mb


# ------------------------------------------------------------------------------------------------
# registry: manifest ID -> (generator, kwargs, pivot, post-process)
# ------------------------------------------------------------------------------------------------
def _moss_cap(obj):
    ex.assign_palette(obj, lambda p: p.normal.z > 0.55 and p.center.z > 0, ex.MOSS_GREEN)


REGISTRY = {
    "ENV_TREE_OAK_A": (tree_oak, dict(seed=1, height=300, canopy=165, blobs=4), "bottom_center", None),
    "ENV_TREE_OAK_B": (tree_oak, dict(seed=5, height=240, trunk_r=30, canopy=190, blobs=3, leaf=MEADOW_LIME, leaf2=MOSS_GREEN), "bottom_center", None),
    "ENV_TREE_PINE_A": (tree_pine, dict(seed=2), "bottom_center", None),
    "ENV_TREE_HOLLOW_DEAD": (tree_dead_hollow, dict(seed=3), "bottom_center", None),
    "ENV_BUSH_A": (bush, dict(seed=1), "bottom_center", None),
    "ENV_GLOWMOSS_CLUMP": (glowmoss, dict(seed=2), "bottom_center", None),
    "ENV_ROCK_LARGE_A": (rock, dict(seed=1, size=(280, 230, 210), sub=1), "bottom_center", _moss_cap),
    "ENV_ROCK_LARGE_B": (rock, dict(seed=8, size=(320, 240, 160), sub=1, flat=0.1), "bottom_center", _moss_cap),
    "ENV_ROCK_SMALL_A": (rock, dict(seed=4, size=(90, 80, 60), sub=0), "bottom_center", _moss_cap),
    "ENV_COTTAGE_A": (cottage, dict(seed=1), "bottom_center", None),
    "ENV_COTTAGE_B": (cottage, dict(seed=2, w=440, d=440, wall_h=230, roof_h=190, wall=CLAY, roof=DUSK_BLUE, chimney_side=-1), "bottom_center", None),
    "ENV_INN_LANTERNREST": (inn, dict(seed=1), "bottom_center", None),
    "ENV_WATCHTOWER": (watchtower, dict(seed=1), "bottom_center", None),
    "ENV_FENCE_STRAIGHT": (fence, dict(seed=1), "corner", None),
    "ENV_FENCE_CORNER": (fence, dict(seed=2, corner=True), "corner", None),
    "ENV_RUIN_WALL_A": (ruin_wall, dict(seed=3), "corner", None),
    "ENV_RUIN_ARCH": (ruin_arch, dict(seed=1), "corner", None),
    "ENV_RUIN_PILLAR": (ruin_pillar, dict(seed=2, broken=0.25), "corner", None),
    "ENV_STAG_SHRINE": (stag_shrine, dict(seed=1), "bottom_center", None),
    "ENV_BARROW_ENTRANCE": (barrow_entrance, dict(seed=2), "bottom_center", None),
    "ENV_DGN_WALL": (dgn_wall, dict(seed=1), "corner", None),
    "ENV_DGN_WALL_DOOR": (dgn_wall, dict(seed=1, door=True), "corner", None),
    "ENV_DGN_FLOOR": (dgn_floor, dict(seed=1), "corner", None),
    "ENV_DGN_PILLAR": (dgn_pillar, dict(seed=1), "corner", None),
    "ENV_DGN_STAIRS": (dgn_stairs, dict(seed=1), "corner", None),
    "ENV_DGN_THRONE": (dgn_throne, dict(seed=1), "corner", None),
    "ENV_TORCH_WALL": (torch_wall, dict(seed=1), "origin", None),
    "PRP_LANTERN": (lantern, dict(seed=1), "bottom_center", None),
    "PRP_CRATE": (crate, dict(seed=1), "bottom_center", None),
    "PRP_BARREL": (barrel, dict(seed=1), "bottom_center", None),
    "PRP_SIGNPOST": (signpost, dict(seed=1), "bottom_center", None),
    "PRP_CAMPFIRE": (campfire, dict(seed=1), "bottom_center", None),
    "PRP_CHEST_QUEST": (chest_quest, dict(seed=1), "bottom_center", None),
    "PRP_BIND_STONE": (bind_stone, dict(seed=1), "bottom_center", None),
    "PRP_SHARD_PILLAR": (shard_pillar, dict(seed=1), "bottom_center", None),
    "PRP_EMBER_CROWN": (ember_crown, dict(seed=1), "bottom_center", None),
    "PRP_CROWN_SHARD": (crown_shard, dict(seed=1), "bottom_center", None),
}

OUT = {"ENV_": os.path.join(ex.REPO_ROOT, "SourceArt", "Env"), "PRP_": os.path.join(ex.REPO_ROOT, "SourceArt", "Props")}


def main(ids=None):
    rows = ex.load_manifest()
    missing = [i for i, r in rows.items() if i.startswith(("ENV_", "PRP_")) and r["Type"] == "StaticMesh" and i not in REGISTRY]
    ok, bad = 0, list(("%s" % m, "no generator") for m in missing)
    for mid, (gen, kw, pivot, post) in REGISTRY.items():
        if ids and mid not in ids:
            continue
        if mid not in rows:
            bad.append((mid, "not in manifest"))
            continue
        ex.clear_scene()
        mb = gen(**kw)
        obj = mb.build()
        if post:
            post(obj)
        try:
            tris = ex.export_asset(obj, mid, OUT[mid[:4]], pivot=pivot)
            ok += 1
            print("OK  %-24s %4d / %s tris" % (mid, tris, rows[mid]["TriBudget"]))
        except ex.ExportError as e:
            bad.append((mid, str(e)))
            print("FAIL", e)
    print("%d exported, %d failed" % (ok, len(bad)))
    for b in bad:
        print("  ", b)
    return 1 if bad else 0


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(set(args) or None))
