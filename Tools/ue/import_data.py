"""Import gameplay data into the Unreal project (Systems Engineer owns this file).

What it does
  1. Every Data/Source/DT_<Key>.json  -> /Game/MMO/Data/DT_<Key> (UDataTable with the matching row struct).
  2. Every Data/Source/DA_<Name>.json -> /Game/MMO/Data/DA_<Name> (data asset, class picked by name prefix).
  3. Registers the tables in Config/DefaultGame.ini under [/Script/ThistlewickVale.TVProjectSettings]
     (AssetManifest + the Tables map, keys from Docs/DATA_SCHEMAS.md section 12).

Run headless (Windows, from the project folder):
  "C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
      "%CD%\\ThistlewickVale.uproject" -run=pythonscript -script="%CD%\\Tools\\ue\\import_data.py"
or from the editor: Tools > Execute Python Script. Needs the ThistlewickVale module compiled (row structs and
UTVDataImportLibrary) and the Python Editor Script Plugin enabled.

Optional arguments (append inside -script="... --only ItemDefs,StatPools"):
  --only K1,K2   import just these table keys / data asset names
  --dry-run      print what would happen, change nothing
"""

import json
import os
import re
import sys

import unreal

MODULE = "/Script/ThistlewickVale"
CONTENT_DIR = "/Game/MMO/Data"
SETTINGS_SECTION = "[/Script/ThistlewickVale.TVProjectSettings]"

# DATA_SCHEMAS section 12 keys -> row struct (C++ name without the F prefix).
TABLE_STRUCTS = {
    "AssetManifest": "TVAssetManifestRow",  # registered as AssetManifest, not in the Tables map
    "Classes": "TVClassRow",
    "ClassBaseStats": "TVClassLevelStatsRow",
    "XPCurve": "TVXPCurveRow",
    "Abilities": "TVAbilityRow",
    "Mobs": "TVMobRow",
    "ItemDefs": "TVItemDefRow",
    "StatPools": "TVStatPoolRow",
    "LootTables": "TVLootTableRow",
    "Quests": "TVQuestRow",
    "Npcs": "TVNpcRow",
    "Dialogue": "TVDialogueRow",
}

# DA_<Prefix>_* -> data asset class (C++ name without the U prefix).
DATA_ASSET_CLASSES = {
    "SetBonus": "TVSetBonusDataAsset",
    "Mount": "TVMountDataAsset",
    "Boss": "TVBossDataAsset",  # Combat Engineer's class
}


def log(msg):
    unreal.log("[import_data] " + msg)


def warn(msg):
    unreal.log_warning("[import_data] " + msg)


def error(msg):
    unreal.log_error("[import_data] " + msg)


def parse_args():
    args = {"only": None, "dry_run": False}
    argv = list(sys.argv[1:])
    i = 0
    while i < len(argv):
        if argv[i] == "--only" and i + 1 < len(argv):
            args["only"] = set(x.strip() for x in argv[i + 1].split(",") if x.strip())
            i += 2
        elif argv[i] == "--dry-run":
            args["dry_run"] = True
            i += 1
        else:
            i += 1
    return args


def project_dir():
    return os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def find_struct(name):
    """UScriptStruct for a module struct, or None."""
    path = "{}.{}".format(MODULE, name)
    try:
        struct = unreal.load_object(None, path)
        if struct:
            return struct
    except Exception:  # noqa: BLE001 - unreal raises plain Exception
        pass
    wrapper = getattr(unreal, name, None)
    if wrapper is not None and hasattr(wrapper, "static_struct"):
        return wrapper.static_struct()
    return None


def find_class(name):
    path = "{}.{}".format(MODULE, name)
    try:
        cls = unreal.load_class(None, path)
        if cls:
            return cls
    except Exception:  # noqa: BLE001
        pass
    wrapper = getattr(unreal, name, None)
    if wrapper is not None and hasattr(wrapper, "static_class"):
        return wrapper.static_class()
    return None


def asset_path(name):
    return "{}/{}".format(CONTENT_DIR, name)


def object_path(name):
    return "{}/{}.{}".format(CONTENT_DIR, name, name)


def load_or_create(name, check_type, create_class, factory):
    path = asset_path(name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        existing = unreal.EditorAssetLibrary.load_asset(path)
        if existing is not None and isinstance(existing, check_type):
            return existing
        warn("{} exists with a different type; replacing it".format(path))
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(name, CONTENT_DIR, create_class, factory)


def import_table(key, json_path, dry_run):
    struct_name = TABLE_STRUCTS.get(key)
    if struct_name is None:
        warn("DT_{}.json: no row struct registered for key '{}', skipped".format(key, key))
        return False
    struct = find_struct(struct_name)
    if struct is None:
        error("DT_{}: row struct F{} not found (module not compiled, or the owning engineer named it differently)".format(key, struct_name))
        return False

    with open(json_path, "r", encoding="utf-8") as handle:
        rows = json.load(handle)
    if not isinstance(rows, list) or any("Name" not in row for row in rows):
        error("DT_{}.json must be an array of objects that each have a \"Name\"".format(key))
        return False

    name = "DT_" + key
    if dry_run:
        log("would import {} rows into {} as F{}".format(len(rows), asset_path(name), struct_name))
        return True

    factory = unreal.DataTableFactory()
    # Verified in 5.8 Factories/DataTableFactory.h: UPROPERTY(BlueprintReadWrite) const UScriptStruct* Struct.
    factory.set_editor_property("struct", struct)
    table = load_or_create(name, unreal.DataTable, unreal.DataTable, factory)
    if table is None:
        error("could not create " + asset_path(name))
        return False

    # Verified in 5.8 Kismet/DataTableFunctionLibrary.h: FillDataTableFromJSONFile(UDataTable*, const FString&, UScriptStruct*),
    # WITH_EDITOR, ScriptMethod "FillFromJSONFile". Passing the struct forces an automated (dialog-free) import.
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(table, json_path, struct)
    if not ok:
        error("JSON import failed for {} (see LogDataTable above)".format(json_path))
        return False
    imported = len(unreal.DataTableFunctionLibrary.get_data_table_row_names(table))
    if imported != len(rows):
        warn("{}: {} rows in JSON, {} in the table".format(name, len(rows), imported))
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    log("{}: {} rows (F{})".format(name, imported, struct_name))
    return True


def import_data_asset(name, json_path, dry_run):
    match = re.match(r"^DA_([A-Za-z]+)_", name)
    class_name = DATA_ASSET_CLASSES.get(match.group(1)) if match else None
    if class_name is None:
        warn("{}.json: unknown data asset prefix, skipped".format(name))
        return False
    asset_class = find_class(class_name)
    if asset_class is None:
        error("{}: class U{} not found (module not compiled?)".format(name, class_name))
        return False

    with open(json_path, "r", encoding="utf-8") as handle:
        text = handle.read()
    json.loads(text)  # fail early on malformed JSON

    if dry_run:
        log("would create {} as U{}".format(asset_path(name), class_name))
        return True

    factory = unreal.DataAssetFactory()
    # Verified in 5.8 Factories/DataAssetFactory.h: UPROPERTY TSubclassOf<UDataAsset> DataAssetClass.
    factory.set_editor_property("data_asset_class", asset_class)
    asset = load_or_create(name, unreal.DataAsset, asset_class, factory)
    if asset is None:
        error("could not create " + asset_path(name))
        return False

    # Python cannot fill nested struct arrays generically, so the C++ helper runs FJsonObjectConverter on the object.
    result = unreal.TVDataImportLibrary.fill_object_from_json_string(asset, text)
    if isinstance(result, tuple):
        ok, message = result[0], (result[1] if len(result) > 1 else "")
    else:
        ok, message = bool(result), ""
    if not ok:
        error("{}: {}".format(name, message))
        return False
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log("{}: created as U{}".format(name, class_name))
    return True


def write_ini_fallback(manifest_path, tables):
    """Used only when UTVDataImportLibrary.register_tables is unavailable."""
    ini_path = os.path.join(project_dir(), "Config", "DefaultGame.ini")
    with open(ini_path, "r", encoding="utf-8") as handle:
        lines = handle.read().splitlines()

    # VERIFY-5.8: TMap config text format ((Key, Value),...) as written by FMapProperty::ExportText.
    entries = ",".join('("{}", "{}")'.format(k, v) for k, v in sorted(tables.items()))
    new_block = [SETTINGS_SECTION]
    if manifest_path:
        new_block.append("AssetManifest=" + manifest_path)
    new_block.append("Tables=({})".format(entries))

    out, in_section, written = [], False, False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("["):
            in_section = stripped == SETTINGS_SECTION
            if in_section:
                out.extend(new_block)
                written = True
                continue
        if in_section and (stripped.startswith("AssetManifest=") or stripped.startswith("Tables=") or stripped.startswith("+Tables=")):
            continue
        out.append(line)
    if not written:
        out.append("")
        out.extend(new_block)
    with open(ini_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(out) + "\n")
    log("wrote {} (text fallback)".format(ini_path))


def register_tables(imported_keys, dry_run):
    manifest = object_path("DT_AssetManifest") if "AssetManifest" in imported_keys else ""
    tables = {k: object_path("DT_" + k) for k in sorted(imported_keys) if k != "AssetManifest"}
    if dry_run:
        log("would register AssetManifest={} and Tables={}".format(manifest, tables))
        return True
    if not tables and not manifest:
        return True
    helper = getattr(unreal, "TVDataImportLibrary", None)
    if helper is not None and hasattr(helper, "register_tables"):
        if helper.register_tables(manifest, {unreal.Name(k): v for k, v in tables.items()}):
            log("registered {} tables in Config/DefaultGame.ini".format(len(tables)))
            return True
        warn("register_tables failed; falling back to editing the ini text")
    write_ini_fallback(manifest, tables)
    return True


def main():
    args = parse_args()
    source_dir = os.path.join(project_dir(), "Data", "Source")
    if not os.path.isdir(source_dir):
        error("missing " + source_dir)
        return 1

    files = sorted(f for f in os.listdir(source_dir) if f.endswith(".json"))
    # Manifest first so later steps can resolve manifest IDs.
    files.sort(key=lambda f: (f != "DT_AssetManifest.json", f))

    imported_keys, failures = set(), []
    for file_name in files:
        stem = file_name[:-5]
        full = os.path.join(source_dir, file_name)
        if stem.startswith("DT_"):
            key = stem[3:]
            if args["only"] and key not in args["only"]:
                continue
            if import_table(key, full, args["dry_run"]):
                imported_keys.add(key)
            else:
                failures.append(stem)
        elif stem.startswith("DA_"):
            if args["only"] and stem not in args["only"]:
                continue
            if not import_data_asset(stem, full, args["dry_run"]):
                failures.append(stem)

    # Keep keys that were imported earlier but not in this run (e.g. --only) so registration never drops tables.
    for key in TABLE_STRUCTS:
        if key not in imported_keys and unreal.EditorAssetLibrary.does_asset_exist(asset_path("DT_" + key)):
            imported_keys.add(key)
    register_tables(imported_keys, args["dry_run"])

    if failures:
        error("finished with {} failure(s): {}".format(len(failures), ", ".join(failures)))
        return 1
    log("done: {} tables registered".format(len([k for k in imported_keys if k != "AssetManifest"])))
    return 0


if __name__ == "__main__":
    RESULT = main()
    if RESULT:
        # Non-zero exit for CI; the pythonscript commandlet reports script exceptions as errors.
        raise RuntimeError("import_data.py failed, see log")
