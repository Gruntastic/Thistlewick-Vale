"""Import every FBX under SourceArt/Placeholders, SourceArt/Env and SourceArt/Props into the manifest path.

Headless:
    UnrealEditor-Cmd.exe <path>/ThistlewickVale.uproject -run=pythonscript -script=<path>/Tools/ue/import_meshes.py

Rules
  * The FBX file name is the manifest ID (<ID>.fbx); the destination is Docs/ASSET_MANIFEST.csv Path.
  * A final kit piece (SourceArt/Env or SourceArt/Props) replaces a placeholder of the same ID.
  * Skeletal rows are imported as STATIC meshes for now (the placeholders are static); the manifest
    path keeps its SK_ name so code resolves the same soft path later when the real skeletal mesh lands.
    # VERIFY-5.8: a StaticMesh asset named SK_* is accepted by the asset registry; if the later
    # skeletal import must reuse the name, delete the placeholder first (replace_existing handles it).
  * Assigns /Game/MMO/Core/Materials/MI_<AssetName> or M_Master_LowPoly if it exists.
  * Idempotent: re-running reimports with replace_existing, and skips files whose size+mtime match
    the stamp stored in /Game/MMO/Data (none needed: Unreal reimport is cheap) -- use --force semantics
    by deleting the stamp file SourceArt/.import_stamp.json.
"""
import csv
import json
import os

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
MANIFEST = os.path.join(ROOT, "Docs", "ASSET_MANIFEST.csv")
# later entries override earlier ones of the same ID
SOURCES = [os.path.join(ROOT, "SourceArt", d) for d in ("Placeholders", "Env", "Props")]
STAMP = os.path.join(ROOT, "SourceArt", ".import_stamp.json")
MASTER_MI = "/Game/MMO/Core/Materials/MI_Master_LowPoly"
MASTER = "/Game/MMO/Core/Materials/M_Master_LowPoly"


def load_manifest():
    with open(MANIFEST, newline="", encoding="utf-8") as fh:
        return {r["ID"]: r for r in csv.DictReader(fh)}


def collect():
    chosen = {}
    for src in SOURCES:
        if not os.path.isdir(src):
            continue
        for f in sorted(os.listdir(src)):
            if f.lower().endswith(".fbx"):
                chosen[os.path.splitext(f)[0]] = os.path.join(src, f)
    return chosen


def build_task(fbx, dest_dir, dest_name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx)
    task.set_editor_property("destination_path", dest_dir)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("automated_import_should_detect_type", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)  # VERIFY-5.8
    sm = opts.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("generate_lightmap_u_vs", True)
    sm.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)  # VERIFY-5.8
    sm.set_editor_property("build_nanite", False)  # VERIFY-5.8: contract section 7, Nanite off
    task.set_editor_property("options", opts)
    return task


def assign_material(asset_path, asset_name):
    mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
    if mesh is None or not isinstance(mesh, unreal.StaticMesh):
        return False
    for cand in (MASTER_MI, MASTER):
        if unreal.EditorAssetLibrary.does_asset_exist(cand):
            mat = unreal.EditorAssetLibrary.load_asset(cand)
            n = len(mesh.static_materials)
            for i in range(n):
                mesh.set_material(i, mat)  # VERIFY-5.8: StaticMesh.set_material(index, material)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            return True
    return False


def main():
    rows = load_manifest()
    files = collect()
    stamp = {}
    if os.path.exists(STAMP):
        with open(STAMP, encoding="utf-8") as fh:
            stamp = json.load(fh)
    unreal.log("import_meshes: %d FBX files" % len(files))
    imported, skipped, bad = 0, 0, []
    for mid, fbx in sorted(files.items()):
        row = rows.get(mid)
        if row is None or row["Type"] not in ("SkeletalMesh", "StaticMesh"):
            bad.append("%s: not a mesh row in the manifest" % mid)
            continue
        key = "%s|%d|%d" % (fbx, os.path.getsize(fbx), int(os.path.getmtime(fbx)))
        dest_dir, dest_name = row["Path"].rsplit("/", 1)
        exists = unreal.EditorAssetLibrary.does_asset_exist(row["Path"])
        if exists and stamp.get(mid) == key:
            skipped += 1
            continue
        task = build_task(fbx, dest_dir, dest_name)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        if not task.get_editor_property("imported_object_paths"):
            bad.append("%s: import failed" % mid)
            continue
        assign_material(row["Path"], dest_name)
        stamp[mid] = key
        imported += 1
        unreal.log("imported %s -> %s" % (mid, row["Path"]))
    with open(STAMP, "w", encoding="utf-8") as fh:
        json.dump(stamp, fh, indent=1, sort_keys=True)
    unreal.log("import_meshes: %d imported, %d unchanged, %d problems" % (imported, skipped, len(bad)))
    for b in bad:
        unreal.log_error(b)


main()
