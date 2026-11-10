"""Greybox placeholders for every SkeletalMesh / StaticMesh row of Docs/ASSET_MANIFEST.csv.

Output: SourceArt/Placeholders/<ManifestID>.fbx (skeletal rows are exported as STATIC meshes for now).

Run headless:   python Tools/blender/make_placeholders.py [ID ...]      (needs `pip install bpy`)
          or:   blender -b -P Tools/blender/make_placeholders.py -- [ID ...]

Conventions: centimeters, +X forward, +Z up. Characters/mobs/mounts have the pivot at the feet centre,
weapons at the grip with the blade along +Z, gear pieces are authored in character space (pivot at the
feet of the 180 cm body) so they line up with the body when attached. Tintable regions use palette
cell 14 (Bone) per STYLE_LOCK.md; everything else uses real palette colours.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ue5_export as ex  # noqa: E402
from tv_geo import MeshBuilder  # noqa: E402
from ue5_export import (BONE, CLAY, DARK_BARK, DEEP_PINE, DUSK_BLUE, EMBER_GOLD, HEARTH_RED,  # noqa: E402
                        HOLLOW_VIOLET, MEADOW_LIME, MOSS_GLOW, MOSS_GREEN, SLATE, SKY_TEAL, STONE_GREY,
                        TIMBER, WHEAT)

OUT_DIR = os.path.join(ex.REPO_ROOT, "SourceArt", "Placeholders")


def fit_height(mb, target):
    """Uniformly scale (about the origin) so the highest vertex is at `target` cm."""
    zmax = max(v[2] for v in mb.verts)
    k = target / zmax
    mb.verts = [(x * k, y * k, z * k) for x, y, z in mb.verts]


# ------------------------------------------------------------------------------------------------
# humanoid body, authored for 180 cm then rescaled with fit_height
# ------------------------------------------------------------------------------------------------
def humanoid(mb, skin=CLAY, torso=TIMBER, legs=DARK_BARK, boots=DARK_BARK, hunch=0.0, bulk=1.0,
             torso_sec=False, head_scale=1.0):
    for s in (1, -1):
        mb.box((6, s * 11, 5), (28, 14, 10), boots)                       # oversized feet, toes to +X
        mb.box((0, s * 10, 48), (15 * bulk, 15 * bulk, 78), legs)         # legs
        mb.box((0, s * 26 * bulk, 117), (12 * bulk, 12 * bulk, 54), torso, sec=torso_sec)  # arms
        mb.box((2, s * 27 * bulk, 87), (16, 15, 15), skin)                # oversized hands
    mb.box((0, 0, 94), (25 * bulk, 36 * bulk, 14), legs)                  # pelvis
    mb.box((hunch * 4, 0, 124), (23 * bulk, 42 * bulk, 46), torso, rot=(0, hunch * 12, 0), sec=torso_sec)
    hs = head_scale
    mb.box((hunch * 9, 0, 166), (27 * hs, 25 * hs, 28 * hs), skin)        # head
    mb.box((hunch * 9 + 15 * hs, 0, 163), (7, 7, 7), skin)                # nose marks facing +X
    mb.box((hunch * 6, 0, 149.5), (9, 9, 5), skin)                        # neck


def build_class(name, h, torso, accent, kind):
    mb = MeshBuilder(name)
    humanoid(mb, skin=CLAY, torso=BONE if kind != "mage" else BONE, legs=TIMBER, boots=DARK_BARK, torso_sec=True)
    if kind == "warrior":
        for s in (1, -1):
            mb.box((0, s * 30, 150), (20, 18, 12), BONE, sec=True)       # pauldrons
        mb.box((0, 0, 177), (30, 28, 12), STONE_GREY)                     # helm top
        mb.box((16, 0, 168), (4, 22, 4), SLATE)                           # visor slit
    elif kind == "mage":
        mb.frustum((0, 0, 60), 25, 18, 55, 8, BONE, sec=True)             # robe skirt
        mb.frustum((0, 0, 178), 22, 0, 36, 6, DUSK_BLUE)                  # pointed hat
        mb.frustum((0, 0, 177), 24, 24, 3, 6, DUSK_BLUE)
        mb.box((14, 0, 128), (6, 8, 8), EMBER_GOLD)                       # glowing amulet
    elif kind == "cleric":
        mb.frustum((0, 0, 40), 24, 19, 70, 8, BONE, sec=True)             # long tabard
        mb.box((-14, 0, 150), (8, 40, 10), WHEAT)                         # mantle
        mb.box((0, 0, 179), (28, 26, 5), EMBER_GOLD)                      # halo ring slab
    elif kind == "ranger":
        mb.box((0, 0, 177), (30, 28, 10), DEEP_PINE)                      # hood
        mb.box((-16, 0, 150), (8, 8, 8), DEEP_PINE)
        mb.box((-16, 6, 135), (9, 12, 50), TIMBER, rot=(0, 0, 0))         # quiver on back
        mb.box((-15, 0, 127), (5, 30, 6), DARK_BARK)                      # strap
    fit_height(mb, h)
    return mb


def build_npc(name, kind):
    mb = MeshBuilder(name)
    if kind == "wren":    # elder: robe, white beard, staff
        humanoid(mb, skin=CLAY, torso=SKY_TEAL, legs=SKY_TEAL, bulk=0.95, hunch=0.5)
        mb.frustum((0, 0, 35), 25, 19, 75, 8, SKY_TEAL)
        mb.box((14, 0, 156), (8, 18, 18), BONE)                           # beard
        mb.box((0, 0, 178), (29, 27, 8), BONE)
        mb.box((14, 40, 100), (6, 6, 180), TIMBER)
    elif kind == "bram":  # innkeeper: apron, broad
        humanoid(mb, skin=CLAY, torso=WHEAT, legs=TIMBER, bulk=1.2)
        mb.box((13, 0, 112), (4, 32, 40), BONE)
        mb.box((0, 0, 178), (28, 28, 5), HEARTH_RED)
    elif kind == "ysolde":  # ranger-captain: green cloak
        humanoid(mb, skin=CLAY, torso=DEEP_PINE, legs=DARK_BARK)
        mb.box((-14, 0, 125), (6, 44, 62), MOSS_GREEN)
        mb.box((0, 0, 178), (29, 27, 6), DARK_BARK)
    else:                 # brother cael: cleric, tonsure, mace
        humanoid(mb, skin=CLAY, torso=WHEAT, legs=BONE)
        mb.frustum((0, 0, 40), 24, 19, 70, 8, WHEAT)
        mb.box((0, 0, 179), (22, 20, 3), BONE)
        mb.box((14, -40, 95), (6, 6, 70), TIMBER)
        mb.blob((14, -40, 135), (12, 12, 12), EMBER_GOLD, seed=3)
    fit_height(mb, 180)
    return mb


# ------------------------------------------------------------------------------------------------
# creatures
# ------------------------------------------------------------------------------------------------
def build_mossback(name, h, shaman=False, warchief=False):
    mb = MeshBuilder(name)
    humanoid(mb, skin=MOSS_GREEN, torso=TIMBER, legs=DARK_BARK, hunch=1.0, bulk=1.25, head_scale=1.3)
    mb.blob((-10, 0, 150), (22, 30, 12), MOSS_GREEN, seed=7)             # moss hump on the back
    mb.blob((-12, 8, 158), (10, 10, 8), MEADOW_LIME, seed=8)
    for s in (1, -1):
        mb.box((14, s * 6, 175), (3, 5, 4), EMBER_GOLD)                   # eyes
    if shaman:
        mb.box((18, 36, 90), (6, 6, 150), TIMBER)                         # staff
        mb.blob((18, 36, 170), (11, 11, 11), MOSS_GLOW, seed=2)
        mb.frustum((0, 0, 178), 20, 0, 30, 6, DEEP_PINE)                  # hood
    elif warchief:
        mb.box((16, 38, 80), (12, 12, 110), DARK_BARK)                    # big club
        mb.blob((16, 38, 150), (20, 20, 26), STONE_GREY, seed=4)
        mb.box((0, 0, 182), (30, 20, 10), TIMBER)                         # horned crown base
        for s in (1, -1):
            mb.limb((0, s * 12, 186), (0, s * 22, 206), 5, 2, 4, BONE)
    else:
        mb.box((18, 36, 100), (7, 7, 60), TIMBER)                         # club
        mb.blob((18, 36, 138), (10, 10, 14), STONE_GREY, seed=5)
    fit_height(mb, h)
    return mb


def quadruped(mb, length, shoulder_h, body, belly=None, legcell=None, leg_w=14, neck_up=0.0, head_len=0.28,
              head_scale=1.0):
    """Four-legged body, +X forward. Shoulder (top of the back) is at shoulder_h."""
    legcell = body if legcell is None else legcell
    leg_h = shoulder_h * 0.5
    bh = shoulder_h - leg_h
    bw = length * 0.3
    mb.box((0, 0, leg_h + bh / 2), (length * 0.7, bw, bh), body)
    mb.box((0, 0, leg_h + bh * 0.2), (length * 0.6, bw * 0.9, bh * 0.4), belly if belly is not None else body)
    for sx in (1, -1):
        for sy in (1, -1):
            mb.box((sx * length * 0.27, sy * bw * 0.38, leg_h / 2), (leg_w, leg_w, leg_h), legcell)
            mb.box((sx * length * 0.27 + 3, sy * bw * 0.38, 4), (leg_w + 6, leg_w + 2, 8), DARK_BARK)  # hooves
    hs = head_scale
    nx = length * 0.35
    mb.box((nx + length * 0.08, 0, shoulder_h + neck_up * 0.5), (length * 0.18, bw * 0.55, bh * 0.7 + neck_up), body,
           rot=(0, -25 if neck_up else 0, 0))                                     # neck
    hx = nx + length * 0.2
    hz = shoulder_h + neck_up
    mb.box((hx, 0, hz), (length * head_len * hs, bw * 0.5 * hs, bh * 0.55 * hs), body)    # head
    mb.box((hx + length * head_len * 0.5 * hs, 0, hz - 6), (length * 0.12, bw * 0.3, bh * 0.3), body)  # snout
    mb.box((-length * 0.38, 0, shoulder_h - 6), (length * 0.18, 10, 10), body, rot=(0, -30, 0))  # tail
    return hx, hz


def build_boar():
    mb = MeshBuilder("MOB_THORNBACK_BOAR")
    hx, hz = quadruped(mb, 150, 90, CLAY, belly=WHEAT, legcell=TIMBER, leg_w=16, head_len=0.3, head_scale=1.1)
    for i in range(5):
        mb.pyramid((-40 + i * 22, 0, 88), 16, 16, 28, MOSS_GREEN)             # thorn ridge
    for s in (1, -1):
        mb.limb((hx + 28, s * 12, hz - 14), (hx + 44, s * 14, hz + 12), 5, 2, 4, BONE)   # tusks
        mb.box((hx + 32, s * 8, hz + 6), (3, 3, 3), EMBER_GOLD)
    return mb


def build_wisp():
    mb = MeshBuilder("MOB_BARROW_WISP")
    mb.blob((0, 0, 95), (34, 34, 38), HOLLOW_VIOLET, seed=1, sub=1)
    for s in (1, -1):
        mb.box((30, s * 12, 100), (6, 8, 8), BONE)                            # eyes face +X
    for i, a in enumerate((0, 120, 240)):
        x, y = 16 * math.cos(math.radians(a)), 16 * math.sin(math.radians(a))
        mb.limb((x, y, 62), (x * 1.6, y * 1.6, 10), 12, 3, 5, HOLLOW_VIOLET)  # wavering tails
    return mb


def build_footman():
    mb = MeshBuilder("MOB_HOLLOWED_FOOTMAN")
    humanoid(mb, skin=BONE, torso=SLATE, legs=SLATE, boots=SLATE, bulk=1.15)
    for s in (1, -1):
        mb.box((0, s * 32, 150), (22, 18, 12), SLATE)
        mb.box((15, s * 6, 166), (3, 6, 4), HOLLOW_VIOLET)                    # glowing eyes
    mb.box((0, 0, 178), (32, 30, 10), SLATE)                                  # helm
    mb.box((16, 36, 80), (4, 8, 80), STONE_GREY)                              # sword
    mb.box((16, 36, 122), (4, 26, 5), HOLLOW_VIOLET)
    mb.box((12, -38, 110), (6, 36, 52), SLATE)                                # tattered shield
    fit_height(mb, 200)
    return mb


def build_morrow():
    mb = MeshBuilder("MOB_SISTER_MORROW")
    humanoid(mb, skin=BONE, torso=HOLLOW_VIOLET, legs=SLATE)
    mb.frustum((0, 0, 20), 30, 20, 100, 8, SLATE)                             # tattered robe
    mb.box((-14, 0, 150), (8, 42, 16), HOLLOW_VIOLET)
    mb.frustum((0, 0, 176), 18, 0, 28, 6, SLATE)                              # hood point
    mb.box((16, 38, 90), (5, 5, 150), BONE)                                   # grave-keeper staff
    mb.blob((16, 38, 170), (10, 10, 10), HOLLOW_VIOLET, seed=9)
    fit_height(mb, 190)
    return mb


def build_gristlehorn():
    mb = MeshBuilder("MOB_GRISTLEHORN")
    hx, hz = quadruped(mb, 300, 190, STONE_GREY, belly=SLATE, legcell=SLATE, leg_w=34, head_len=0.26)
    for i, (x, y) in enumerate(((-60, 20), (20, -30), (70, 15), (-20, -10))):
        mb.blob((x, y, 195), (40, 34, 26), MOSS_GREEN, seed=i, sub=0)         # moss on the boulder back
    for s in (1, -1):
        mb.limb((hx + 10, s * 20, hz + 20), (hx + 40, s * 50, hz + 90), 10, 4, 5, BONE)   # horns
    # Mossback warchief rider
    r = MeshBuilder("rider")
    humanoid(r, skin=MOSS_GREEN, torso=TIMBER, legs=DARK_BARK, hunch=1.0, bulk=1.25, head_scale=1.3)
    mb.box((0, 0, 205), (60, 60, 10), DARK_BARK)                              # saddle slab
    fit_height(r, 150)
    r.verts = [(x + 10, y, z + 200) for x, y, z in r.verts]
    base = len(mb.verts)
    mb.verts += r.verts
    mb.faces += [tuple(base + i for i in f) for f in r.faces]
    mb.cells += r.cells
    mb.secondary += r.secondary
    return mb


def build_maldrek():
    mb = MeshBuilder("MOB_MALDREK")
    humanoid(mb, skin=BONE, torso=SLATE, legs=SLATE, boots=SLATE, bulk=1.4)
    for s in (1, -1):
        mb.box((0, s * 36, 152), (30, 24, 16), SLATE)
        mb.box((15, s * 6, 166), (3, 6, 4), HOLLOW_VIOLET)
    mb.box((0, 0, 184), (34, 32, 6), SLATE)
    for i in range(6):                                                         # Ember Crown spikes
        a = i * 60
        x, y = 15 * math.cos(math.radians(a)), 15 * math.sin(math.radians(a))
        mb.frustum((x, y, 188), 5, 0, 22, 4, EMBER_GOLD)
    mb.box((18, 40, 70), (6, 10, 110), STONE_GREY)                             # greatsword
    mb.box((18, 40, 128), (6, 34, 6), HOLLOW_VIOLET)
    mb.box((-18, 0, 125), (6, 60, 80), HOLLOW_VIOLET)                          # tattered cape
    fit_height(mb, 320)
    return mb


def build_dawnstag():
    mb = MeshBuilder("MNT_DAWNSTAG")
    hx, hz = quadruped(mb, 240, 190, WHEAT, belly=BONE, legcell=CLAY, leg_w=16, neck_up=28, head_len=0.2)
    mb.box((0, 0, 194), (60, 46, 8), HEARTH_RED)                              # saddle = mount_seat
    for s in (1, -1):
        base = (hx - 6, s * 8, hz + 22)
        mb.limb(base, (hx - 20, s * 30, hz + 55), 5, 3.5, 5, BONE)           # main beams
        mb.limb((hx - 20, s * 30, hz + 55), (hx + 10, s * 40, hz + 82), 4, 2.5, 4, BONE)
        mb.limb((hx - 12, s * 20, hz + 40), (hx + 20, s * 28, hz + 55), 4, 2.5, 4, BONE)
        mb.box((hx + 14, s * 7, hz + 4), (3, 3, 3), EMBER_GOLD)
    mb.blob((hx + 30, 0, hz - 4), (5, 5, 5), DARK_BARK, seed=1)               # nose
    return mb


# ------------------------------------------------------------------------------------------------
# gear shells in character space (pivot at feet of the 180 cm body)
# ------------------------------------------------------------------------------------------------
def build_gear(name, family, slot):
    mb = MeshBuilder(name)
    T = BONE
    if slot == "HEAD":
        if family == "PLATE":
            mb.box((0, 0, 166), (31, 29, 30), T, sec=True)
            mb.box((16, 0, 166), (3, 22, 4), SLATE)
            mb.box((0, 0, 183), (10, 6, 8), T)                               # crest
        elif family == "LEATHER":
            mb.blob((0, 0, 168), (17, 16, 17), T, seed=1)
            mb.box((-12, 0, 160), (8, 24, 20), T, sec=True)
        else:
            mb.frustum((0, 0, 178), 18, 0, 36, 6, T, sec=True)
            mb.frustum((0, 0, 177), 21, 21, 3, 6, T)
    elif slot == "CHEST":
        if family == "PLATE":
            mb.box((1, 0, 124), (27, 46, 50), T)
            for s in (1, -1):
                mb.box((0, s * 30, 150), (22, 18, 14), T, sec=True)
                mb.box((0, s * 28, 120), (15, 15, 40), T)
            mb.box((14, 0, 128), (3, 20, 24), T, sec=True)
        elif family == "LEATHER":
            mb.box((1, 0, 123), (26, 44, 46), T)
            mb.box((14, 0, 125), (3, 10, 46), T, sec=True)
            mb.box((0, 0, 148), (24, 46, 5), T, sec=True)
        else:
            mb.box((1, 0, 123), (26, 44, 48), T)
            mb.frustum((0, 0, 60), 25, 18, 44, 8, T, sec=True)
            for s in (1, -1):
                mb.box((0, s * 27, 112), (16, 16, 50), T)
    elif slot == "LEGS":
        if family == "ROBE":
            mb.frustum((0, 0, 28), 26, 21, 74, 8, T, sec=True)
        else:
            for s in (1, -1):
                mb.box((0, s * 10, 50), (19, 18, 80), T)
                if family == "PLATE":
                    mb.box((4, s * 10, 62), (4, 14, 16), T, sec=True)
            mb.box((0, 0, 95), (27, 38, 16), T, sec=True)
    else:  # BOOTS
        h = 14 if family == "ROBE" else 26
        for s in (1, -1):
            mb.box((5, s * 11, h / 2), (34, 17, h), T, sec=(family != "ROBE"))
            if family == "PLATE":
                mb.box((12, s * 11, 21), (10, 15, 6), T)
    return mb


# ------------------------------------------------------------------------------------------------
# weapons: grip at the origin, blade/shaft along +Z, forward/striking face toward +X
# ------------------------------------------------------------------------------------------------
def build_weapon(name):
    mb = MeshBuilder(name)
    if name == "WPN_SWORDSHIELD":
        mb.frustum((0, 0, -9), 3.5, 3.5, 18, 6, DARK_BARK)                    # grip
        mb.box((0, 0, -11), (9, 9, 5), EMBER_GOLD)
        mb.box((0, 0, 10), (6, 26, 5), STONE_GREY)                            # guard
        mb.box((0, 0, 58), (4, 12, 94), STONE_GREY)
        mb.pyramid((0, 0, 105), 12, 4, 14, STONE_GREY)
        mb.frustum((10, -42, -10), 26, 26, 6, 8, HEARTH_RED, rot=(0, 90, 0))   # round shield beside the grip
        mb.frustum((16, -42, -10), 9, 7, 6, 8, EMBER_GOLD, rot=(0, 90, 0))
    elif name == "WPN_STAFF":
        mb.frustum((0, 0, -90), 3.2, 3.2, 190, 6, TIMBER)
        mb.blob((0, 0, 108), (11, 11, 11), EMBER_GOLD, seed=2)
        for a in range(4):
            x, y = 11 * math.cos(math.radians(a * 90)), 11 * math.sin(math.radians(a * 90))
            mb.limb((x, y, 96), (x * 1.4, y * 1.4, 122), 4, 2.5, 4, TIMBER)
    elif name == "WPN_MACETOME":
        mb.frustum((0, 0, -12), 3.5, 3.5, 70, 6, TIMBER)
        mb.blob((0, 0, 70), (13, 13, 13), STONE_GREY, seed=6)
        for a in range(4):
            x, y = 13 * math.cos(math.radians(a * 90)), 13 * math.sin(math.radians(a * 90))
            mb.pyramid((x, y, 70), 7, 7, 9, STONE_GREY, rot=(0, 0, 0))
        mb.box((6, -34, 0), (8, 26, 34), HEARTH_RED)                          # tome
        mb.box((11, -34, 0), (2, 12, 16), EMBER_GOLD)
    else:  # WPN_BOWDAGGERS
        pts = [(0, 0, -55), (4, 0, -35), (10, 0, -12), (10, 0, 12), (4, 0, 35), (0, 0, 55)]
        for a, b in zip(pts, pts[1:]):
            mb.limb(a, b, 3, 3, 4, TIMBER)
        mb.box((6, 0, 0), (6, 7, 20), DARK_BARK)                              # grip wrap
        mb.box((-2, 0, 0), (4, 4, 108), BONE)                                 # string (4 cm)
        for y in (-40, -62):                                                  # paired daggers beside the bow
            mb.box((0, y, -10), (4, 8, 34), STONE_GREY)
            mb.box((0, y, 8), (4, 14, 4), DARK_BARK)
    return mb


# ------------------------------------------------------------------------------------------------
# ENV / PRP greyboxes (table driven, size in cm; the real kit pieces replace these)
# ------------------------------------------------------------------------------------------------
ENV_GREYBOX = {
    # id: (kind, w, d, h, pivot)
    "ENV_TREE_OAK_A": ("tree", 360, 360, 620, "bottom_center"),
    "ENV_TREE_OAK_B": ("tree", 320, 320, 560, "bottom_center"),
    "ENV_TREE_PINE_A": ("pine", 260, 260, 700, "bottom_center"),
    "ENV_TREE_HOLLOW_DEAD": ("deadtree", 260, 260, 520, "bottom_center"),
    "ENV_BUSH_A": ("blob", 140, 120, 90, "bottom_center"),
    "ENV_GLOWMOSS_CLUMP": ("blob", 70, 70, 40, "bottom_center"),
    "ENV_ROCK_LARGE_A": ("blob", 260, 220, 200, "bottom_center"),
    "ENV_ROCK_LARGE_B": ("blob", 300, 240, 170, "bottom_center"),
    "ENV_ROCK_SMALL_A": ("blob", 90, 80, 60, "bottom_center"),
    "ENV_COTTAGE_A": ("house", 500, 400, 480, "bottom_center"),
    "ENV_COTTAGE_B": ("house", 450, 450, 460, "bottom_center"),
    "ENV_INN_LANTERNREST": ("house", 1000, 700, 750, "bottom_center"),
    "ENV_WATCHTOWER": ("tower", 400, 400, 1100, "bottom_center"),
    "ENV_FENCE_STRAIGHT": ("box", 400, 14, 110, "corner"),
    "ENV_FENCE_CORNER": ("box", 400, 400, 110, "corner"),
    "ENV_RUIN_WALL_A": ("box", 400, 60, 300, "corner"),
    "ENV_RUIN_ARCH": ("box", 400, 60, 400, "corner"),
    "ENV_RUIN_PILLAR": ("pillar", 100, 100, 400, "corner"),
    "ENV_STAG_SHRINE": ("box", 600, 600, 400, "bottom_center"),
    "ENV_BARROW_ENTRANCE": ("house", 1000, 800, 600, "bottom_center"),
    "ENV_DGN_WALL": ("box", 400, 50, 400, "corner"),
    "ENV_DGN_WALL_DOOR": ("box", 400, 50, 400, "corner"),
    "ENV_DGN_FLOOR": ("box", 400, 400, 20, "corner"),
    "ENV_DGN_PILLAR": ("pillar", 100, 100, 400, "corner"),
    "ENV_DGN_STAIRS": ("wedge", 400, 400, 200, "corner"),
    "ENV_DGN_THRONE": ("box", 200, 200, 300, "corner"),
    "ENV_TORCH_WALL": ("box", 14, 14, 60, "bottom_center"),
    "PRP_LANTERN": ("box", 24, 24, 40, "bottom_center"),
    "PRP_CRATE": ("box", 80, 80, 80, "bottom_center"),
    "PRP_BARREL": ("pillar", 60, 60, 90, "bottom_center"),
    "PRP_SIGNPOST": ("box", 60, 14, 170, "bottom_center"),
    "PRP_CAMPFIRE": ("blob", 100, 100, 40, "bottom_center"),
    "PRP_CHEST_QUEST": ("box", 90, 60, 60, "bottom_center"),
    "PRP_BIND_STONE": ("pillar", 100, 100, 220, "bottom_center"),
    "PRP_SHARD_PILLAR": ("pillar", 90, 90, 260, "bottom_center"),
    "PRP_EMBER_CROWN": ("pillar", 60, 60, 40, "bottom_center"),
    "PRP_CROWN_SHARD": ("pillar", 24, 24, 50, "bottom_center"),
}


def build_greybox(mid):
    kind, w, d, h, _ = ENV_GREYBOX[mid]
    mb = MeshBuilder(mid)
    if kind == "tree":
        mb.frustum((0, 0, 0), 28, 20, h * 0.45, 6, TIMBER)
        mb.blob((0, 0, h * 0.72), (w / 2, d / 2, h * 0.28), MOSS_GREEN, seed=1)
    elif kind == "pine":
        mb.frustum((0, 0, 0), 20, 14, h * 0.2, 6, TIMBER)
        mb.frustum((0, 0, h * 0.15), w / 2, 0, h * 0.85, 8, DEEP_PINE)
    elif kind == "deadtree":
        mb.frustum((0, 0, 0), 32, 18, h, 6, DARK_BARK)
        mb.limb((0, 0, h * 0.7), (w / 2, 0, h), 12, 4, 4, DARK_BARK)
        mb.limb((0, 0, h * 0.6), (-w / 2, 20, h * 0.9), 12, 4, 4, DARK_BARK)
    elif kind == "blob":
        mb.blob((0, 0, h / 2), (w / 2, d / 2, h / 2), STONE_GREY if "ROCK" in mid else MOSS_GREEN, seed=3)
    elif kind == "house":
        wall = h * 0.55
        mb.box((0, 0, wall / 2), (w, d, wall), WHEAT)
        mb.gable((-w / 2, -d / 2, wall), (w / 2, d / 2, wall), h - wall, HEARTH_RED, overhang=15)
    elif kind == "tower":
        mb.frustum((0, 0, 0), w / 2, w / 2.6, h * 0.7, 8, STONE_GREY)
        mb.frustum((0, 0, h * 0.7), w / 2.2, 0, h * 0.3, 8, HEARTH_RED)
    elif kind == "pillar":
        mb.frustum((0, 0, 0), w / 2, w / 2.4, h, 8, STONE_GREY)
    elif kind == "wedge":
        mb.wedge((0, 0, 0), (w, d, h), STONE_GREY)
    else:
        mb.box((w / 2, d / 2, h / 2), (w, d, h), STONE_GREY)
    return mb


# ------------------------------------------------------------------------------------------------
def make(row):
    mid = row["ID"]
    if mid.startswith("CHR_") and mid.endswith("_BODY"):
        cls = mid.split("_")[1].lower()
        accent = {"warrior": HEARTH_RED, "mage": DUSK_BLUE, "cleric": WHEAT, "ranger": DEEP_PINE}[cls]
        return build_class(mid, 180, BONE, accent, cls), "bottom_center_keep"
    if mid.startswith("CHR_NPC_"):
        return build_npc(mid, {"ELDERWREN": "wren", "BRAMTALLOW": "bram", "YSOLDETHORN": "ysolde",
                               "BROTHERCAEL": "cael"}[mid[8:]]), "origin"
    table = {
        "MOB_MOSSBACK_SCRAPPER": lambda: build_mossback(mid, 120),
        "MOB_MOSSBACK_SHAMAN": lambda: build_mossback(mid, 120, shaman=True),
        "MOB_THORNBACK_BOAR": build_boar,
        "MOB_BARROW_WISP": build_wisp,
        "MOB_HOLLOWED_FOOTMAN": build_footman,
        "MOB_ROTFANG": lambda: build_mossback(mid, 240, warchief=True),
        "MOB_SISTER_MORROW": build_morrow,
        "MOB_GRISTLEHORN": build_gristlehorn,
        "MOB_MALDREK": build_maldrek,
        "MNT_DAWNSTAG": build_dawnstag,
    }
    if mid in table:
        return table[mid](), "origin"
    if mid.startswith("GEAR_"):
        _, fam, slot = mid.split("_")
        return build_gear(mid, fam, slot), "origin"
    if mid.startswith("WPN_"):
        return build_weapon(mid), "origin"
    if mid in ENV_GREYBOX:
        return build_greybox(mid), ENV_GREYBOX[mid][4]
    return None, None


def main(ids=None):
    rows = ex.load_manifest()
    done, failed = [], []
    for mid, row in rows.items():
        if row["Type"] not in ("SkeletalMesh", "StaticMesh"):
            continue
        if ids and mid not in ids:
            continue
        ex.clear_scene()
        mb, pivot = make(row)
        if mb is None:
            failed.append((mid, "no placeholder generator"))
            continue
        # characters/mobs are authored with the pivot at the feet centre already
        pivot = "origin" if pivot == "bottom_center_keep" else pivot
        try:
            tris = ex.export_asset(mb.build(), mid, OUT_DIR, pivot=pivot)
            done.append((mid, tris))
            print("OK  %-28s %5d tris" % (mid, tris))
        except ex.ExportError as e:
            failed.append((mid, str(e)))
            print("FAIL", e)
    print("%d exported, %d failed" % (len(done), len(failed)))
    return 1 if failed else 0


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(set(args) or None))
