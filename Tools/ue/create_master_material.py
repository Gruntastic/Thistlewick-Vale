"""Builds the Thistlewick Vale master materials and imports the palette atlas.

Headless:  UnrealEditor-Cmd.exe <ThistlewickVale.uproject> -run=pythonscript -script=<this file>
Idempotent: existing assets are cleared and rebuilt in place (asset references stay valid).

Parameter names are code-facing (ART_CONTRACT section 3); do not rename.
Run and checked against the 5.8.4 editor Python API (2026-11-10). Every connect is checked; a wrong pin name raises.
Authoring conventions (see Docs/STYLE_LOCK.md): atlas UVs, secondary region = vertex alpha (UseVertexColor on)
or UV1.x > 0.5 (atlas mode), glow-only cells 1 / 11 / 15.
"""
import os
import unreal

MEL = unreal.MaterialEditingLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

MAT_DIR = "/Game/MMO/Core/Materials"
TEX_DIR = "/Game/MMO/Core/Textures"
ATLAS_NAME = "T_Palette_Atlas"
ATLAS_ASSET = f"{TEX_DIR}/{ATLAS_NAME}"
ATLAS_SRC_REL = "SourceArt/Core/T_Palette_Atlas.png"
EMISSIVE_CELLS = (1, 11, 15)  # Ember Gold, Moss Glow, Hollow Violet


# ---------------------------------------------------------------- texture
def import_atlas():
    src = os.path.join(unreal.SystemLibrary.get_project_directory(), ATLAS_SRC_REL)
    if not os.path.isfile(src):
        raise RuntimeError(f"Palette atlas PNG not found: {src} (run SourceArt/Core/make_palette_atlas.py)")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", TEX_DIR)
    task.set_editor_property("destination_name", ATLAS_NAME)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    ASSET_TOOLS.import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(ATLAS_ASSET)
    if tex is None:
        raise RuntimeError("Atlas import failed")
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


# ---------------------------------------------------------------- helpers
def get_or_create_material(name):
    path = f"{MAT_DIR}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mat = unreal.EditorAssetLibrary.load_asset(path)
        MEL.delete_all_material_expressions(mat)
        return mat
    return ASSET_TOOLS.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())


class Graph:
    """Small wrapper so the node wiring below stays readable."""

    def __init__(self, mat):
        self.mat = mat
        self.x = -1400
        self.y = 0

    def node(self, cls, col=0, row=0, **props):
        e = MEL.create_material_expression(self.mat, cls, -1400 + col * 260, row * 180)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, src, dst, dst_in, src_out=""):
        # The connect calls return False (no exception) on a wrong pin name, so check them.
        if not MEL.connect_material_expressions(src, src_out, dst, dst_in):
            raise RuntimeError(f"{self.mat.get_name()}: could not connect {src.get_class().get_name()}.{src_out or '<default>'}"
                               f" -> {dst.get_class().get_name()}.{dst_in}")
        return dst

    def to_prop(self, src, prop, src_out=""):
        if not MEL.connect_material_property(src, src_out, prop):
            raise RuntimeError(f"{self.mat.get_name()}: could not connect {src.get_class().get_name()} to {prop}")

    # parameter nodes -------------------------------------------------
    def scalar(self, name, default, col=0, row=0):
        return self.node(unreal.MaterialExpressionScalarParameter, col, row, parameter_name=name, default_value=default)

    def vector(self, name, rgb, col=0, row=0):
        return self.node(unreal.MaterialExpressionVectorParameter, col, row, parameter_name=name,
                         default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def switch(self, name, default, col=0, row=0):
        return self.node(unreal.MaterialExpressionStaticSwitchParameter, col, row, parameter_name=name,
                         default_value=default)

    # math ------------------------------------------------------------
    def mul(self, a, b, col=0, row=0, a_out="", b_out=""):
        n = self.node(unreal.MaterialExpressionMultiply, col, row)
        if isinstance(a, float) or isinstance(a, int):
            n.set_editor_property("const_a", float(a))
        else:
            self.link(a, n, "A", a_out)
        if isinstance(b, float) or isinstance(b, int):
            n.set_editor_property("const_b", float(b))
        else:
            self.link(b, n, "B", b_out)
        return n

    def add(self, a, b, col=0, row=0, a_out="", b_out=""):
        n = self.node(unreal.MaterialExpressionAdd, col, row)
        self.link(a, n, "A", a_out)
        self.link(b, n, "B", b_out)
        return n

    def sub_const(self, a, const_b, col=0, row=0, a_out=""):
        n = self.node(unreal.MaterialExpressionSubtract, col, row, const_b=float(const_b))
        self.link(a, n, "A", a_out)
        return n

    def lerp(self, a, b, alpha, col=0, row=0, a_out="", b_out="", alpha_out=""):
        n = self.node(unreal.MaterialExpressionLinearInterpolate, col, row)
        self.link(a, n, "A", a_out)
        self.link(b, n, "B", b_out)
        self.link(alpha, n, "Alpha", alpha_out)
        return n

    def unary(self, cls, a, col=0, row=0, a_out="", in_name=""):
        # Single-input nodes: "" = first input (5.8 MaterialEditingLibrary GetExpressionInputByName); their pins have no
        # matchable name, so "Input"/"Value"/"VectorInput" do not resolve.
        n = self.node(cls, col, row)
        self.link(a, n, in_name, a_out)
        return n

    def mask(self, src, channel, col=0, row=0, src_out=""):
        n = self.node(unreal.MaterialExpressionComponentMask, col, row,
                      r=("r" in channel), g=("g" in channel or channel == "rgb"), b=("b" in channel or channel == "rgb"), a=(channel == "a"))
        self.link(src, n, "", src_out)  # single input, see unary()
        return n


# ---------------------------------------------------------------- shared sub-graphs
def atlas_color_and_glowmask(g, atlas_tex, y0=0):
    """Returns (atlas_rgb_node, glow_mask_node) where glow_mask is 1 only for cells 1, 11, 15."""
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, 0, y0 + 0, coordinate_index=0)
    atlas = g.node(unreal.MaterialExpressionTextureSampleParameter2D, 1, y0 + 0,
                   parameter_name="PaletteAtlas", texture=atlas_tex)
    # nearest/clamped lookup: sampler type is color, filtering comes from the texture asset itself
    atlas.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    g.link(uv0, atlas, "UVs")
    # cell index = floor(u*4) + 4*floor(v*4)
    u = g.mask(uv0, "r", 1, y0 + 1)
    v = g.mask(uv0, "g", 1, y0 + 2)
    col = g.unary(unreal.MaterialExpressionFloor, g.mul(u, 4.0, 2, y0 + 1), 3, y0 + 1)
    row = g.unary(unreal.MaterialExpressionFloor, g.mul(v, 4.0, 2, y0 + 2), 3, y0 + 2)
    idx = g.add(col, g.mul(row, 4.0, 3, y0 + 3), 4, y0 + 2)
    glow = None
    for i, cell in enumerate(EMISSIVE_CELLS):
        d = g.sub_const(idx, cell, 5, y0 + 4 + i)
        hit = g.unary(unreal.MaterialExpressionOneMinus,
                      g.unary(unreal.MaterialExpressionSaturate,
                              g.unary(unreal.MaterialExpressionAbs, d, 6, y0 + 4 + i), 7, y0 + 4 + i),
                      8, y0 + 4 + i)
        glow = hit if glow is None else g.add(glow, hit, 9, y0 + 4 + i)
    return atlas, glow


def build_lowpoly(name, masked, atlas_tex):
    mat = get_or_create_material(name)
    g = Graph(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED if masked else unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", masked)
    # world-space normal output so DDX/DDY normals need no tangent transform
    mat.set_editor_property("tangent_space_normal", False)

    atlas, glow = atlas_color_and_glowmask(g, atlas_tex, 0)

    # --- one UseVertexColor switch carries RGBA: True = vertex color RGBA, False = (atlas RGB, UV1.x>0.5 step)
    vc = g.node(unreal.MaterialExpressionVertexColor, 0, 9)
    uv1 = g.node(unreal.MaterialExpressionTextureCoordinate, 0, 11, coordinate_index=1)
    uv1x = g.mask(uv1, "r", 1, 11)
    step = g.unary(unreal.MaterialExpressionFloor, g.mul(uv1x, 2.0, 2, 11), 3, 11)  # floor(2x) = 1 when x >= 0.5
    step = g.unary(unreal.MaterialExpressionSaturate, step, 4, 11)
    atlas_rgba = g.node(unreal.MaterialExpressionAppendVector, 5, 11)
    g.link(atlas, atlas_rgba, "A", "RGB")
    g.link(step, atlas_rgba, "B")
    # VertexColor output 0 is RGB only (5.8 MaterialExpressions.cpp: mask 1,1,1,0); alpha is the separate "A" output.
    vc_rgba = g.node(unreal.MaterialExpressionAppendVector, 5, 9)
    g.link(vc, vc_rgba, "A")
    g.link(vc, vc_rgba, "B", "A")
    sw_vc = g.switch("UseVertexColor", False, 10, 0)
    g.link(vc_rgba, sw_vc, "True")        # static switch pins are "True"/"False" (checked in 5.8)
    g.link(atlas_rgba, sw_vc, "False")
    sw_rgb = g.mask(sw_vc, "rgb", 11, 0)
    sw_mask = g.mask(sw_vc, "a", 11, 3)

    base_t = g.vector("BaseTint", (1, 1, 1), 10, 5)
    sec_t = g.vector("SecondaryTint", (1, 1, 1), 10, 6)
    tint = g.lerp(base_t, sec_t, sw_mask, 11, 5)
    color = g.mul(sw_rgb, tint, 12, 2)

    # --- hit flash: lerp to white
    hit_flash = g.scalar("HitFlash", 0.0, 12, 7)
    white = g.node(unreal.MaterialExpressionConstant3Vector, 12, 8, constant=unreal.LinearColor(1, 1, 1, 1))
    flashed = g.lerp(color, white, hit_flash, 13, 2)

    # --- dissolve (masked: real clip; opaque: darken toward Dark Bark so the parameter is live, swap to Masked MI for a true dissolve)
    dissolve = g.scalar("DissolveAmount", 0.0, 12, 9)
    wp = g.node(unreal.MaterialExpressionWorldPosition, 12, 10)
    noise = g.node(unreal.MaterialExpressionNoise, 13, 10, scale=0.08, quality=1, levels=2,
                   output_min=0.0, output_max=1.0, turbulence=False)
    g.link(wp, noise, "World Position")  # 5.8 pin name (MaterialEditingLibrary.get_material_expression_input_names)
    if masked:
        # opacity = saturate((noise + 0.001 - dissolve * 1.001) * 1000)
        d_scaled = g.mul(dissolve, 1.001, 14, 9)
        diff = g.node(unreal.MaterialExpressionSubtract, 15, 10, const_b=-0.001)
        g.link(noise, diff, "A")
        diff2 = g.node(unreal.MaterialExpressionSubtract, 16, 10)
        g.link(diff, diff2, "A")
        g.link(d_scaled, diff2, "B")
        clip = g.unary(unreal.MaterialExpressionSaturate, g.mul(diff2, 1000.0, 17, 10), 18, 10)
        g.to_prop(clip, unreal.MaterialProperty.MP_OPACITY_MASK)
        mat.set_editor_property("opacity_mask_clip_value", 0.5)
        final_color = flashed
    else:
        dark = g.node(unreal.MaterialExpressionConstant3Vector, 14, 9,
                      constant=unreal.LinearColor(0.07, 0.03, 0.02, 1))  # ~Dark Bark linear
        final_color = g.lerp(flashed, dark, dissolve, 14, 2)
    g.to_prop(final_color, unreal.MaterialProperty.MP_BASE_COLOR)

    # --- emissive: color * strength * (lerp(1, atlasGlowMask, EmissiveMaskFromAtlas)) + white * HitFlash
    em_col = g.vector("EmissiveColor", (0, 0, 0), 10, 12)
    em_str = g.scalar("EmissiveStrength", 0.0, 10, 13)
    em_mask_amt = g.scalar("EmissiveMaskFromAtlas", 0.0, 10, 14)
    one = g.node(unreal.MaterialExpressionConstant, 10, 15, r=1.0)
    mask_mix = g.lerp(one, glow, em_mask_amt, 11, 14)
    em = g.mul(g.mul(em_col, em_str, 11, 12), mask_mix, 12, 12)
    em = g.add(em, g.mul(white, hit_flash, 12, 13), 13, 12)
    g.to_prop(em, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # --- roughness / flat shading
    rough = g.scalar("Roughness", 0.8, 14, 14)
    g.to_prop(rough, unreal.MaterialProperty.MP_ROUGHNESS)

    wp2 = g.node(unreal.MaterialExpressionWorldPosition, 0, 16)
    ddx = g.unary(unreal.MaterialExpressionDDX, wp2, 1, 16)
    ddy = g.unary(unreal.MaterialExpressionDDY, wp2, 1, 17)
    cr = g.node(unreal.MaterialExpressionCrossProduct, 2, 16)
    g.link(ddx, cr, "A")
    g.link(ddy, cr, "B")
    flat_n = g.unary(unreal.MaterialExpressionNormalize, cr, 3, 16)
    vnorm = g.node(unreal.MaterialExpressionVertexNormalWS, 3, 18)
    sw_flat = g.switch("FlatShaded", True, 4, 16)
    g.link(flat_n, sw_flat, "True")
    g.link(vnorm, sw_flat, "False")
    # cross(ddx, ddy) is the order the 5.8 engine shaders use for world normals (DebugViewModePixelShader.usf).
    g.to_prop(sw_flat, unreal.MaterialProperty.MP_NORMAL)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"[ArtDirector] built {name}")
    return mat


def build_unlit_fx(atlas_tex):
    mat = get_or_create_material("M_Master_Unlit_FX")
    g = Graph(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)

    atlas, _ = atlas_color_and_glowmask(g, atlas_tex, 0)
    vc = g.node(unreal.MaterialExpressionVertexColor, 0, 9)  # particle color / alpha
    sw = g.switch("UseVertexColor", False, 10, 0)
    g.link(vc, sw, "True")  # VertexColor output 0 = RGB
    g.link(atlas, sw, "False", "RGB")
    base_t = g.vector("BaseTint", (1, 1, 1), 10, 2)
    em_col = g.vector("EmissiveColor", (0, 0, 0), 10, 4)
    em_str = g.scalar("EmissiveStrength", 0.0, 10, 5)
    color = g.add(g.mul(sw, base_t, 11, 1), g.mul(em_col, em_str, 11, 4), 12, 2)
    g.to_prop(color, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    dissolve = g.scalar("DissolveAmount", 0.0, 10, 8)
    wp = g.node(unreal.MaterialExpressionWorldPosition, 10, 9)
    noise = g.node(unreal.MaterialExpressionNoise, 11, 9, scale=0.08, quality=1, levels=2,
                   output_min=0.0, output_max=1.0, turbulence=False)
    g.link(wp, noise, "World Position")  # 5.8 pin name (MaterialEditingLibrary.get_material_expression_input_names)
    diff = g.node(unreal.MaterialExpressionSubtract, 12, 9, const_b=-0.001)
    g.link(noise, diff, "A")
    diff2 = g.node(unreal.MaterialExpressionSubtract, 13, 9)
    g.link(diff, diff2, "A")
    g.link(g.mul(dissolve, 1.001, 12, 8), diff2, "B")
    clip = g.unary(unreal.MaterialExpressionSaturate, g.mul(diff2, 1000.0, 14, 9), 15, 9)
    opacity = g.mul(vc, clip, 16, 9, a_out="A")  # vertex alpha
    g.to_prop(opacity, unreal.MaterialProperty.MP_OPACITY)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log("[ArtDirector] built M_Master_Unlit_FX")
    return mat


def main():
    unreal.EditorAssetLibrary.make_directory(MAT_DIR)
    unreal.EditorAssetLibrary.make_directory(TEX_DIR)
    atlas_tex = import_atlas()
    build_lowpoly("M_Master_LowPoly", False, atlas_tex)
    build_lowpoly("M_Master_LowPoly_Masked", True, atlas_tex)
    build_unlit_fx(atlas_tex)
    unreal.log("[ArtDirector] master materials done")


main()
