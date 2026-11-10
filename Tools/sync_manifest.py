#!/usr/bin/env python3
"""Docs/ASSET_MANIFEST.csv -> Data/Source/DT_AssetManifest.json (UE DataTable JSON, FTVAssetManifestRow).

    python3 Tools/sync_manifest.py                 validate + write the JSON
    python3 Tools/sync_manifest.py --apply-measurements
        also update the Status/TriCount columns of the CSV from SourceArt/*/_tricounts.json
        (written by Tools/blender/ue5_export.py, so only measured numbers get in). A row whose
        kit piece (SourceArt/Env|Props) exists becomes `cleaned`, otherwise a placeholder export
        makes it `placeholder`. Rows already past `cleaned` are never downgraded.
    python3 Tools/sync_manifest.py --check         validate only

Exit code is non-zero on any violation (naming prefix, budget, unknown enum, duplicate ID).
Stdlib only.
"""
import csv
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
CSV_PATH = os.path.join(ROOT, "Docs", "ASSET_MANIFEST.csv")
OUT_PATH = os.path.join(ROOT, "Data", "Source", "DT_AssetManifest.json")
MEASURE_DIRS = {"placeholder": "Placeholders", "kit": ("Env", "Props")}

# enum names exactly as in Source/ThistlewickVale/Core/TVTypes.h
ASSET_TYPES = {"SkeletalMesh", "StaticMesh", "Skeleton", "Material", "MaterialInstance", "Texture", "Icon",
               "AnimSequence", "AnimMontage", "AnimBlueprint", "Niagara", "Sound", "Music", "Widget", "Map"}
STATUS_MAP = {"planned": "Planned", "placeholder": "Placeholder", "generated": "Generated", "cleaned": "Cleaned",
              "imported": "Imported", "final": "Final", "failed-kept-placeholder": "FailedKeptPlaceholder"}
STATUS_ORDER = ["planned", "placeholder", "generated", "cleaned", "imported", "final"]

# ART_CONTRACT section 9 prefixes by manifest type (asset name prefix)
NAME_PREFIX = {"SkeletalMesh": ("SK_",), "StaticMesh": ("SM_",), "Skeleton": ("SKEL_",), "Material": ("M_",),
               "MaterialInstance": ("MI_",), "Texture": ("T_",), "Icon": ("T_",), "AnimSequence": ("A_",),
               "AnimMontage": ("AM_",), "AnimBlueprint": ("ABP_",), "Niagara": ("NS_",), "Sound": ("SFX_",),
               "Music": ("MUS_",), "Widget": ("WBP_",), "Map": ("L_",)}
ID_PREFIXES = ("CHR_", "MOB_", "MNT_", "GEAR_", "WPN_", "ENV_", "PRP_", "VFX_", "AUD_", "MUS_", "UI_", "MAT_",
               "TEX_", "ANIM_", "MAP_")


def budget_cap(row):
    """Hard limit from ART_CONTRACT section 7 for the row's category, or None if not a mesh budget."""
    i, name = row["ID"], row["Path"].rsplit("/", 1)[-1]
    if row["Type"] not in ("SkeletalMesh", "StaticMesh"):
        return None
    if i.startswith("CHR_"):
        return 5000
    if i.startswith("MOB_"):
        return 8000
    if i.startswith("MNT_"):
        return 4000
    if i.startswith("GEAR_"):
        return 1200
    if i.startswith("WPN_"):
        return 800
    if i.startswith("PRP_"):
        return 500
    if i.startswith("ENV_"):
        if "DGN" in i or "Dgn" in name or "RUIN" in i:
            return 600
        if "TREE" in i:
            return 800
        if "ROCK" in i:
            return 400
        if any(k in i for k in ("BUSH", "GLOWMOSS", "FENCE", "TORCH")):
            return 500
        return 2000
    return None


def soft_path(path):
    name = path.rsplit("/", 1)[-1]
    return "%s.%s" % (path, name)


def load_rows():
    with open(CSV_PATH, newline="", encoding="utf-8") as fh:
        rdr = csv.DictReader(fh)
        return rdr.fieldnames, list(rdr)


def load_measurements():
    out = {"placeholder": {}, "kit": {}}
    for kind, dirs in MEASURE_DIRS.items():
        for d in ([dirs] if isinstance(dirs, str) else dirs):
            p = os.path.join(ROOT, "SourceArt", d, "_tricounts.json")
            if os.path.exists(p):
                with open(p, encoding="utf-8") as fh:
                    for k, v in json.load(fh).items():
                        # only trust entries whose FBX is really on disk
                        if os.path.exists(os.path.join(os.path.dirname(p), k + ".fbx")):
                            out[kind][k] = v["tris"]
    return out


def validate(rows):
    errs, seen = [], set()
    for r in rows:
        i = r["ID"]
        if i in seen:
            errs.append("duplicate ID %s" % i)
        seen.add(i)
        if not i.startswith(ID_PREFIXES):
            errs.append("%s: ID prefix not in contract list" % i)
        if r["Type"] not in ASSET_TYPES:
            errs.append("%s: unknown Type %s" % (i, r["Type"]))
            continue
        if r["Status"] not in STATUS_MAP:
            errs.append("%s: unknown Status %s" % (i, r["Status"]))
        name = r["Path"].rsplit("/", 1)[-1]
        if not r["Path"].startswith("/Game/"):
            errs.append("%s: path must start with /Game/" % i)
        if not name.startswith(NAME_PREFIX[r["Type"]]):
            errs.append("%s: asset name %s lacks prefix %s" % (i, name, "/".join(NAME_PREFIX[r["Type"]])))
        if " " in r["Path"]:
            errs.append("%s: spaces in path" % i)
        cap = budget_cap(r)
        b = int(r["TriBudget"]) if r["TriBudget"].strip() else None
        if cap is not None and (b is None or b > cap):
            errs.append("%s: TriBudget %s exceeds category limit %s" % (i, b, cap))
        if r["TriCount"].strip():
            if b is None:
                errs.append("%s: TriCount without TriBudget" % i)
            elif int(r["TriCount"]) > b:
                errs.append("%s: TriCount %s over budget %s" % (i, r["TriCount"], b))
    return errs


def apply_measurements(fields, rows, meas):
    changed = 0
    for r in rows:
        i = r["ID"]
        if r["Type"] not in ("SkeletalMesh", "StaticMesh"):
            continue
        kit, ph = meas["kit"].get(i), meas["placeholder"].get(i)
        cur = r["Status"]
        if cur in STATUS_ORDER and STATUS_ORDER.index(cur) > STATUS_ORDER.index("cleaned"):
            continue
        if kit is not None:
            new = ("cleaned", kit)
        elif ph is not None:
            new = ("placeholder", ph) if cur in ("planned", "placeholder") else None
        else:
            new = None
        if new and (r["Status"], r["TriCount"]) != (new[0], str(new[1])):
            r["Status"], r["TriCount"] = new[0], str(new[1])
            changed += 1
    with open(CSV_PATH, "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=fields, lineterminator="\r\n")
        w.writeheader()
        w.writerows(rows)
    return changed


def write_json(rows):
    out = []
    for r in rows:
        out.append({
            "Name": r["ID"],
            "AssetType": r["Type"],
            "Asset": soft_path(r["Path"]),
            "Status": STATUS_MAP[r["Status"]],
            "TriCount": int(r["TriCount"]) if r["TriCount"].strip() else 0,
            "Budget": int(r["TriBudget"]) if r["TriBudget"].strip() else 0,
        })
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(out, fh, indent=2)
        fh.write("\n")
    return len(out)


def main(argv):
    fields, rows = load_rows()
    if "--apply-measurements" in argv:
        n = apply_measurements(fields, rows, load_measurements())
        print("updated %d CSV rows from measurements" % n)
    errs = validate(rows)
    for e in errs:
        print("VIOLATION:", e)
    if errs:
        return 1
    if "--check" not in argv:
        print("wrote %d rows to %s" % (write_json(rows), os.path.relpath(OUT_PATH, ROOT)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
