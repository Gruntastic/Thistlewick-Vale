"""Creates the 4 class MIs and 9 gear-tier MIs (paths from Docs/ASSET_MANIFEST.csv).

Headless:  UnrealEditor-Cmd.exe <ThistlewickVale.uproject> -run=pythonscript -script=<this file>
Run create_master_material.py first. Idempotent: existing MIs are updated in place.

Tint convention (Docs/STYLE_LOCK.md): tinted regions are authored on atlas cell 14 (Bone), so
tint = target palette color / Bone, which lands on the target color after the multiply.
T3 trim is authored on atlas cell 1 (Ember Gold) and glows through EmissiveMaskFromAtlas = 1.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MASTER = "/Game/MMO/Core/Materials/M_Master_LowPoly"

HEX = {
    "HearthRed": "#C8553D", "EmberGold": "#FFB347", "Wheat": "#F2CC8F", "Clay": "#B5835A",
    "Timber": "#7F5539", "DarkBark": "#4A3428", "DuskBlue": "#3D5A80", "MossGreen": "#6A994E",
    "DeepPine": "#386641", "StoneGrey": "#8D99AE", "Slate": "#4A5568", "Bone": "#EDE6D6",
}


def srgb_to_lin(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lin(hexstr):
    return [srgb_to_lin(int(hexstr[i:i + 2], 16)) for i in (1, 3, 5)]


def tint_of(name):
    """palette color / Bone, per channel, in linear space."""
    t, b = lin(HEX[name]), lin(HEX["Bone"])
    return [t[i] / b[i] for i in range(3)]


def color(rgb):
    return unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)


# (asset path, base tint, secondary tint, emissive color or None, emissive strength, mask from atlas)
CLASS_MIS = [
    ("/Game/MMO/Characters/Warrior/MI_Class_Warrior", "Bone", "HearthRed"),
    ("/Game/MMO/Characters/Mage/MI_Class_Mage", "Bone", "DuskBlue"),
    ("/Game/MMO/Characters/Cleric/MI_Class_Cleric", "Bone", "Wheat"),
    ("/Game/MMO/Characters/Ranger/MI_Class_Ranger", "Bone", "DeepPine"),
]

# Tier looks (ART_CONTRACT section 2). Silhouette families share the tier look.
TIERS = {
    "T1_Wanderers": ("Clay", "Timber", None),
    "T2_ValeGuard": ("StoneGrey", "MossGreen", None),
    "T3_HollowCrown": ("Slate", "Slate", "EmberGold"),
}
FAMILIES = ("Plate", "Leather", "Robe")
GEAR_DIR = "/Game/MMO/Gear/Materials"


def get_or_create_mi(path, parent):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.EditorAssetLibrary.load_asset(path)
    else:
        folder, name = path.rsplit("/", 1)
        unreal.EditorAssetLibrary.make_directory(folder)
        mi = ASSET_TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    return mi


def _require(names, wanted, kind, parent):
    missing = [w for w in wanted if w not in [str(n) for n in names]]
    if missing:
        raise RuntimeError(f"{parent.get_name()} has no {kind} parameter(s) {missing}")


def check_parent(parent):
    """Fail early if the master is missing a parameter. The 5.8 setters cannot be trusted for this: the vector setter
    always returns False (MaterialEditingLibrary.cpp never sets bResult) and an unknown name is silently ignored."""
    _require(MEL.get_vector_parameter_names(parent), ("BaseTint", "SecondaryTint", "EmissiveColor"), "vector", parent)
    _require(MEL.get_scalar_parameter_names(parent), ("EmissiveStrength", "EmissiveMaskFromAtlas", "Roughness"), "scalar", parent)
    _require(MEL.get_static_switch_parameter_names(parent), ("FlatShaded",), "static switch", parent)


def configure(mi, base, secondary, emissive=None, strength=0.0, mask_atlas=0.0, roughness=0.8):
    vectors = (("BaseTint", color(base)), ("SecondaryTint", color(secondary)), ("EmissiveColor", color(emissive or [0, 0, 0])))
    scalars = (("EmissiveStrength", strength), ("EmissiveMaskFromAtlas", mask_atlas), ("Roughness", roughness))
    for name, value in vectors:
        MEL.set_material_instance_vector_parameter_value(mi, name, value)
    for name, value in scalars:
        MEL.set_material_instance_scalar_parameter_value(mi, name, value)
    # Default already matches the parent; set explicitly so the MI documents it.
    MEL.set_material_instance_static_switch_parameter_value(mi, "FlatShaded", True)
    MEL.update_material_instance(mi)
    # Read back: the setters' return values are not reliable in 5.8.
    for name, value in vectors:
        got = MEL.get_material_instance_vector_parameter_value(mi, name)
        if not got.is_near_equal(value, 1e-4):
            raise RuntimeError(f"{mi.get_name()}: {name} is {got}, expected {value}")
    for name, value in scalars:
        got = MEL.get_material_instance_scalar_parameter_value(mi, name)
        if abs(got - value) > 1e-4:
            raise RuntimeError(f"{mi.get_name()}: {name} is {got}, expected {value}")
    if not MEL.get_material_instance_static_switch_parameter_value(mi, "FlatShaded"):
        raise RuntimeError(f"{mi.get_name()}: FlatShaded is off")
    unreal.EditorAssetLibrary.save_loaded_asset(mi)


def main():
    parent = unreal.EditorAssetLibrary.load_asset(MASTER)
    if parent is None:
        raise RuntimeError("M_Master_LowPoly missing: run Tools/ue/create_master_material.py first")
    check_parent(parent)

    for path, base, sec in CLASS_MIS:
        mi = get_or_create_mi(path, parent)
        configure(mi, tint_of(base), tint_of(sec))
        unreal.log(f"[ArtDirector] {path}")

    for tier, (prim, sec, glow) in TIERS.items():
        for fam in FAMILIES:
            mi = get_or_create_mi(f"{GEAR_DIR}/MI_Gear_{tier}_{fam}", parent)
            if glow:
                # trim authored on atlas cell 1; only glow cells (1, 11, 15) emit
                configure(mi, tint_of(prim), tint_of(sec), emissive=lin(HEX[glow]), strength=3.0, mask_atlas=1.0)
            else:
                configure(mi, tint_of(prim), tint_of(sec))
            unreal.log(f"[ArtDirector] MI_Gear_{tier}_{fam}")
    unreal.log("[ArtDirector] material instances done")


main()
