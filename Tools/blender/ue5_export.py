"""Shared Blender -> Unreal Engine 5 export (ART_CONTRACT section 4). Nobody exports by hand.

Works headless (`pip install bpy`, `blender -b -P script.py`) and inside a live Blender session.

Public API
    PALETTE, cell_uv(i)
    init_uvs(obj)
    assign_palette(obj, face_selector, cell_index, secondary=False)
    export_asset(objs, manifest_id, out_dir, pivot="origin", budget=None) -> int   (raises ExportError)
    tri_count(obj)

Conventions enforced here
    * 1 Blender unit == 1 cm (scene unit scale 0.01), FBX written with global_scale 1, so 1 UU == 1 cm.
    * +X forward, +Z up. Blender's FBX exporter is given its default basis (-Z forward, Y up), which
      Unreal imports with X and Z unchanged (Y is mirrored by the left/right-handed conversion).
      # VERIFY-5.8: a 180 cm humanoid must measure 180 cm facing +X after import.
    * Transforms applied, mesh triangulated, flat (faceted) normals, one material slot.
    * Triangle budget comes from Docs/ASSET_MANIFEST.csv (TriBudget); over budget fails the export.
"""
import csv
import json
import math
import os

import bpy
import bmesh

PALETTE_HEX = [
    "#C8553D", "#FFB347", "#F2CC8F", "#B5835A",
    "#7F5539", "#4A3428", "#5FA8A0", "#3D5A80",
    "#6A994E", "#386641", "#A7C957", "#9CFF7A",
    "#8D99AE", "#4A5568", "#EDE6D6", "#7A5CFF",
]
EMISSIVE_CELLS = (1, 11, 15)
# Named cells used by the generators
HEARTH_RED, EMBER_GOLD, WHEAT, CLAY, TIMBER, DARK_BARK, SKY_TEAL, DUSK_BLUE = range(8)
MOSS_GREEN, DEEP_PINE, MEADOW_LIME, MOSS_GLOW, STONE_GREY, SLATE, BONE, HOLLOW_VIOLET = range(8, 16)
TINT_CELL = BONE  # regions tinted by material instances (STYLE_LOCK.md)

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
MANIFEST_CSV = os.path.join(REPO_ROOT, "Docs", "ASSET_MANIFEST.csv")


class ExportError(RuntimeError):
    pass


def hex_to_linear(h):
    def f(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return tuple(f(int(h[i:i + 2], 16)) for i in (1, 3, 5))


def cell_uv(i):
    """UV centre of palette cell i. Cells are row-major from the top-left; Blender's v is up so flip v."""
    col, row = i % 4, i // 4
    return ((col + 0.5) / 4.0, 1.0 - (row + 0.5) / 4.0)


# ---------------------------------------------------------------------------------------------
# scene / material setup
# ---------------------------------------------------------------------------------------------
def setup_scene():
    s = bpy.context.scene
    s.unit_settings.system = "METRIC"
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit = "CENTIMETERS"


def palette_material():
    """Single material slot. Its name is the slot name Unreal sees (import_meshes.py swaps in the MI)."""
    name = "M_Master_LowPoly"
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
        mat.diffuse_color = (*hex_to_linear(PALETTE_HEX[STONE_GREY]), 1.0)
    return mat


def init_uvs(obj):
    mesh = obj.data
    if "UVMap" not in mesh.uv_layers:
        mesh.uv_layers.new(name="UVMap")
    mat = palette_material()
    if not mesh.materials:
        mesh.materials.append(mat)


def _select(mesh, face_selector):
    if face_selector is None:
        return [p.index for p in mesh.polygons]
    if callable(face_selector):
        return [p.index for p in mesh.polygons if face_selector(p)]
    return list(face_selector)


def assign_palette(obj, face_selector, cell_index, secondary=False):
    """Map selected faces to the centre of palette cell `cell_index`.

    face_selector: None (all faces), an iterable of polygon indices, or a callable(polygon) -> bool,
    e.g. lambda p: p.normal.z > 0.6. `secondary=True` writes UV1.x = 0.75 (else 0.25) so the master
    material applies SecondaryTint to those faces (ART_CONTRACT section 6).
    """
    if not 0 <= cell_index < 16:
        raise ExportError("palette cell out of range: %r" % (cell_index,))
    mesh = obj.data
    init_uvs(obj)
    uv0 = mesh.uv_layers["UVMap"].data
    u, v = cell_uv(cell_index)
    idx = _select(mesh, face_selector)
    for pi in idx:
        for li in mesh.polygons[pi].loop_indices:
            uv0[li].uv = (u, v)
    if secondary or "UV1" in mesh.uv_layers:
        if "UV1" not in mesh.uv_layers:
            mesh.uv_layers.new(name="UV1")
            for d in mesh.uv_layers["UV1"].data:
                d.uv = (0.25, 0.5)
        uv1 = mesh.uv_layers["UV1"].data
        for pi in idx:
            for li in mesh.polygons[pi].loop_indices:
                uv1[li].uv = (0.75 if secondary else 0.25, 0.5)
    mesh.update()


# ---------------------------------------------------------------------------------------------
# manifest / budgets
# ---------------------------------------------------------------------------------------------
def load_manifest(path=MANIFEST_CSV):
    with open(path, newline="", encoding="utf-8") as fh:
        return {r["ID"]: r for r in csv.DictReader(fh)}


def budget_for(manifest_id):
    row = load_manifest().get(manifest_id)
    if row is None:
        raise ExportError("unknown manifest ID %s" % manifest_id)
    b = (row.get("TriBudget") or "").strip()
    return int(b) if b else None


# ---------------------------------------------------------------------------------------------
# geometry finishing
# ---------------------------------------------------------------------------------------------
def tri_count(obj):
    """Triangles after triangulation, measured on a temporary triangulated copy of the mesh."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    n = len(bm.faces)
    bm.free()
    return n


def _join(objs):
    objs = list(objs)
    if len(objs) == 1:
        return objs[0]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    return bpy.context.view_layer.objects.active


def _apply_pivot(obj, pivot):
    """Translate mesh data so the pivot lands on the object origin.
    'origin' keeps the geometry as authored (it is already in pivot space),
    'bottom_center' puts the pivot at bbox centre-x/y, min z (props, trees, characters),
    'corner' puts it at the bbox min corner (modular kit pieces, 100 cm grid)."""
    if pivot == "origin":
        return
    vs = [v.co for v in obj.data.vertices]
    mn = [min(v[i] for v in vs) for i in range(3)]
    mx = [max(v[i] for v in vs) for i in range(3)]
    if pivot == "bottom_center":
        off = ((mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2, mn[2])
    elif pivot == "corner":
        off = tuple(mn)
    else:
        raise ExportError("unknown pivot mode %r" % pivot)
    for v in obj.data.vertices:
        v.co.x -= off[0]; v.co.y -= off[1]; v.co.z -= off[2]
    obj.data.update()


def _finish(obj, pivot):
    """Apply transforms, set pivot, triangulate, flat normals, merge nothing else."""
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    _apply_pivot(obj, pivot)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    # keep UV seams by never merging vertices; triangulate with the beauty method
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(obj.data)
    bm.free()
    for p in obj.data.polygons:
        p.use_smooth = False
    obj.data.update()
    obj.location = (0, 0, 0)


def bounds_cm(obj):
    vs = [v.co for v in obj.data.vertices]
    mn = tuple(min(v[i] for v in vs) for i in range(3))
    mx = tuple(max(v[i] for v in vs) for i in range(3))
    return mn, mx


def export_asset(objs, manifest_id, out_dir, pivot="origin", budget=None, log=True):
    """Export one asset. Returns the measured triangle count. Raises ExportError when over budget.

    objs may be one object or a list (joined into one mesh). The objects are consumed (deleted)."""
    setup_scene()
    if not isinstance(objs, (list, tuple)):
        objs = [objs]
    obj = _join(objs)
    obj.name = manifest_id
    obj.data.name = manifest_id
    _finish(obj, pivot)
    tris = tri_count(obj)
    if budget is None:
        budget = budget_for(manifest_id)
    mn, mx = bounds_cm(obj)
    try:
        if budget is not None and tris > budget:
            raise ExportError("%s: %d triangles exceeds budget %d" % (manifest_id, tris, budget))
        mats = len(obj.data.materials)
        if mats > 2:
            raise ExportError("%s: %d material slots (max 2)" % (manifest_id, mats))
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, manifest_id + ".fbx")
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.fbx(
            filepath=path,
            use_selection=True,
            object_types={"MESH"},
            global_scale=1.0,
            apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_NONE",
            bake_space_transform=False,
            axis_forward="-Z",   # Blender defaults; see module docstring
            axis_up="Y",
            use_mesh_modifiers=True,
            mesh_smooth_type="FACE",
            use_tspace=False,
            use_custom_props=False,
            colors_type="NONE",
            add_leaf_bones=False,
            bake_anim=False,
            path_mode="STRIP",
            embed_textures=False,
            use_triangles=True,
        )
        if log:
            _log(out_dir, manifest_id, tris, mn, mx)
    finally:
        mesh = obj.data
        bpy.data.objects.remove(obj, do_unlink=True)
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    return tris


def _log(out_dir, manifest_id, tris, mn, mx):
    """Measured results live next to the FBX files; sync_manifest.py reads them."""
    p = os.path.join(out_dir, "_tricounts.json")
    data = {}
    if os.path.exists(p):
        with open(p, encoding="utf-8") as fh:
            data = json.load(fh)
    data[manifest_id] = {"tris": tris,
                         "size_cm": [round(mx[i] - mn[i], 1) for i in range(3)],
                         "min_cm": [round(mn[i], 1) for i in range(3)]}
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(dict(sorted(data.items())), fh, indent=1)


def clear_scene():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):
        bpy.data.meshes.remove(m)
