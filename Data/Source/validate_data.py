#!/usr/bin/env python3
"""Validates Data/Source/*.json against Docs/DATA_SCHEMAS.md conventions (stdlib only).

Checks: valid JSON, unique row names, enum values (parsed from Source/ThistlewickVale/Core/TVTypes.h),
manifest IDs (Docs/ASSET_MANIFEST.csv), and every cross-reference between tables.
Exit code 0 = pass, 1 = errors. Run: python3 Data/Source/validate_data.py
"""
import csv, json, re, sys
from pathlib import Path

SRC = Path(__file__).resolve().parent
ROOT = SRC.parents[1]
errors, warnings = [], []
def err(f, row, msg): errors.append(f"{f}: [{row}] {msg}")
def warn(f, row, msg): warnings.append(f"{f}: [{row}] {msg}")

# ---------- enums from TVTypes.h ----------
def parse_enums():
    txt = (ROOT / "Source/ThistlewickVale/Core/TVTypes.h").read_text()
    enums = {}
    for m in re.finditer(r"enum class (ETV\w+)\s*:\s*\w+\s*\{(.*?)\};", txt, re.S):
        body = re.sub(r"//.*", "", m.group(2))
        names = []
        for part in body.split(","):
            part = part.strip()
            if part:
                names.append(part.split("=")[0].strip())
        enums[m.group(1)] = set(names)
    return enums
ENUMS = parse_enums()
for need in ("ETVRole","ETVResource","ETVStat","ETVArmorFamily","ETVWeaponType","ETVEquipSlot","ETVSetTier","ETVRarity",
             "ETVAbilitySlot","ETVTargetType","ETVMobTier","ETVQuestObjectiveType"):
    if need not in ENUMS: sys.exit(f"cannot parse {need} from TVTypes.h")

# ---------- manifest ----------
MANIFEST = {}
with open(ROOT / "Docs/ASSET_MANIFEST.csv", newline="") as fh:
    for r in csv.DictReader(fh): MANIFEST[r["ID"]] = r["Type"]

# ---------- load ----------
TABLES, ASSETS = {}, {}
def load(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as e:
        errors.append(f"{path.name}: invalid JSON: {e}"); return None
for p in sorted(SRC.glob("DT_*.json")):
    if p.name == "DT_AssetManifest.json": continue
    data = load(p)
    if data is None: continue
    if not isinstance(data, list): errors.append(f"{p.name}: top level must be an array of rows"); continue
    names = set()
    for i, row in enumerate(data):
        n = row.get("Name") if isinstance(row, dict) else None
        if not n: errors.append(f"{p.name}: row {i} has no Name"); continue
        if n in names: errors.append(f"{p.name}: duplicate row name {n}")
        names.add(n)
    TABLES[p.stem[3:]] = data
for p in sorted(SRC.glob("DA_*.json")):
    data = load(p)
    if data is None: continue
    if not isinstance(data, dict) or data.get("Name") != p.stem: errors.append(f"{p.name}: must be an object with Name == '{p.stem}'")
    ASSETS[p.stem] = data

REQUIRED_TABLES = ["Classes","ClassBaseStats","XPCurve","Abilities","Mobs","ItemDefs","StatPools","LootTables","Quests","Npcs","Dialogue"]
for t in REQUIRED_TABLES:
    if t not in TABLES: errors.append(f"missing table DT_{t}.json")
for a in ["DA_Boss_Rotfang","DA_Boss_SisterMorrow","DA_Boss_Maldrek","DA_Boss_Gristlehorn","DA_SetBonus_T1","DA_SetBonus_T2","DA_SetBonus_T3","DA_Mount_Dawnstag"]:
    if a not in ASSETS: errors.append(f"missing data asset {a}.json")
if errors:
    for e in errors: print("ERROR", e)
    sys.exit(1)

def ids(table, key=None):
    return {r["Name"] for r in TABLES[table]}

CLASS_IDS = ids("Classes"); ABILITY_IDS = ids("Abilities"); MOB_IDS = ids("Mobs"); ITEM_IDS = ids("ItemDefs")
POOL_IDS = ids("StatPools"); LOOT_IDS = ids("LootTables"); QUEST_IDS = ids("Quests"); NPC_IDS = ids("Npcs")
DIALOGUE_IDS = ids("Dialogue")

def enum(f, row, field, val, enum_name, allow_empty=False):
    if val in ("", None) and allow_empty: return
    if val not in ENUMS[enum_name]: err(f, row, f"{field}={val!r} not in {enum_name} {sorted(ENUMS[enum_name])}")
def manifest(f, row, field, val, types=None):
    if val in ("", None): return
    if val not in MANIFEST: err(f, row, f"{field}={val!r} not in ASSET_MANIFEST.csv"); return
    if types and MANIFEST[val] not in types: err(f, row, f"{field}={val!r} has manifest type {MANIFEST[val]}, expected one of {types}")
def ref(f, row, field, val, pool, allow_empty=False):
    if val in ("", None) and allow_empty: return
    if val not in pool: err(f, row, f"{field}={val!r} does not resolve")
def need(f, row, d, fields):
    for k in fields:
        if k not in d: err(f, row, f"missing field {k}")

# ---------- Classes / stats / xp ----------
for r in TABLES["Classes"]:
    n = r["Name"]; f = "DT_Classes"
    need(f, n, r, ["ClassId","DisplayName","Description","Role","ResourceType","BodyMeshId","AnimBlueprintId","PortraitIconId","ClassTintPrimary","ClassTintSecondary","ArmorFamily","WeaponType","StartingAbilities","BaseStatsCurveId"])
    if r.get("ClassId") != n: err(f, n, "ClassId != Name")
    enum(f, n, "Role", r["Role"], "ETVRole"); enum(f, n, "ResourceType", r["ResourceType"], "ETVResource")
    enum(f, n, "ArmorFamily", r["ArmorFamily"], "ETVArmorFamily"); enum(f, n, "WeaponType", r["WeaponType"], "ETVWeaponType")
    manifest(f, n, "BodyMeshId", r["BodyMeshId"], {"SkeletalMesh"}); manifest(f, n, "AnimBlueprintId", r["AnimBlueprintId"], {"AnimBlueprint"})
    manifest(f, n, "PortraitIconId", r["PortraitIconId"], {"Icon"})
    for u in r["StartingAbilities"]:
        ref(f, n, "StartingAbilities.AbilityId", u["AbilityId"], ABILITY_IDS)
        a = next((x for x in TABLES["Abilities"] if x["Name"] == u["AbilityId"]), None)
        if a and a["ClassId"] != n: err(f, n, f"ability {u['AbilityId']} belongs to {a['ClassId']}")
    if len(r["StartingAbilities"]) != 5: err(f, n, "expected 5 class abilities")
    lv = [u["UnlockLevel"] for u in r["StartingAbilities"]]
    if lv != [1, 2, 4, 6, 10]: err(f, n, f"unlock levels {lv} != [1,2,4,6,10]")
    ref(f, n, "BaseStatsCurveId", r["BaseStatsCurveId"], CLASS_IDS)
seen = set()
for r in TABLES["ClassBaseStats"]:
    f = "DT_ClassBaseStats"; n = r["Name"]
    need(f, n, r, ["ClassId","Level","Stamina","Strength","Agility","Intellect","Spirit","BaseHealth","BaseResourceMax","BaseResourceRegen"])
    ref(f, n, "ClassId", r["ClassId"], CLASS_IDS)
    if not 1 <= r["Level"] <= 10: err(f, n, "Level outside 1-10")
    seen.add((r["ClassId"], r["Level"]))
for c in CLASS_IDS:
    for L in range(1, 11):
        if (c, L) not in seen: err("DT_ClassBaseStats", c, f"missing level {L}")
lv = sorted(r["Level"] for r in TABLES["XPCurve"])
if lv != list(range(1, 11)): err("DT_XPCurve", "-", f"levels {lv} != 1..10")

# ---------- Abilities ----------
AB_ROWS = {r["Name"]: r for r in TABLES["Abilities"]}
for n, r in AB_ROWS.items():
    f = "DT_Abilities"
    need(f, n, r, ["AbilityId","ClassId","SlotKind","AbilityClass","AbilityTag","CastTime","ChannelTime","Cooldown","TriggersGCD","ResourceCost","ComboResource","ComboDelta","RangeCm","RadiusCm",
                   "RequiresTarget","RequiresLOS","Interruptible","UsableWhileMoving","TargetType","BaseMagnitude","CoefficientStat","Coefficient","ThreatMultiplier","Effects","MontageId","VfxCastId","VfxImpactId","SfxId","IconId","Tooltip"])
    if r.get("AbilityId") != n: err(f, n, "AbilityId != Name")
    if r["ClassId"] not in CLASS_IDS | {"MOB", "ALL"}: err(f, n, f"ClassId {r['ClassId']!r} unknown")
    enum(f, n, "SlotKind", r["SlotKind"], "ETVAbilitySlot"); enum(f, n, "TargetType", r["TargetType"], "ETVTargetType")
    enum(f, n, "CoefficientStat", r["CoefficientStat"], "ETVStat")
    if (r["ClassId"] == "MOB") != (r["SlotKind"] == "MobAbility"): err(f, n, "MOB class <-> MobAbility slot mismatch")
    if not re.fullmatch(r"/Script/ThistlewickVale\.GA_TV_\w+", r["AbilityClass"]): err(f, n, f"AbilityClass path {r['AbilityClass']!r}")
    if not r["AbilityTag"].startswith("TV.Ability."): err(f, n, "AbilityTag must start with TV.Ability.")
    if r["ComboResource"] not in ("None", "Kindling", "Devotion"): err(f, n, f"ComboResource {r['ComboResource']!r}")
    for e in r["Effects"]:
        if not re.fullmatch(r"/Script/ThistlewickVale\.GE_TV_\w+", e["EffectClass"]): err(f, n, f"EffectClass path {e['EffectClass']!r}")
        for k in ("Duration", "Magnitude", "SetByCallerTag"):
            if k not in e: err(f, n, f"effect missing {k}")
    for k in ("CastTime","ChannelTime","Cooldown","RangeCm","RadiusCm"):
        if r[k] < 0: err(f, n, f"{k} negative")
    manifest(f, n, "MontageId", r["MontageId"], {"AnimMontage"})
    manifest(f, n, "VfxCastId", r["VfxCastId"], {"Niagara"}); manifest(f, n, "VfxImpactId", r["VfxImpactId"], {"Niagara"})
    manifest(f, n, "SfxId", r["SfxId"], {"Sound"}); manifest(f, n, "IconId", r["IconId"], {"Icon"})
    if r["ClassId"] in CLASS_IDS and (not r["SfxId"] or not r["IconId"]): err(f, n, "class ability needs SfxId and IconId")
    if not r["Tooltip"]: err(f, n, "empty tooltip")
for c in CLASS_IDS:
    if sum(1 for r in AB_ROWS.values() if r["ClassId"] == c) != 5: err("DT_Abilities", c, "expected exactly 5 abilities")
if "ABL_MOUNT_SUMMON" not in AB_ROWS: err("DT_Abilities", "-", "missing ABL_MOUNT_SUMMON")

# ---------- Mobs ----------
for r in TABLES["Mobs"]:
    f = "DT_Mobs"; n = r["Name"]
    need(f, n, r, ["MobId","DisplayName","Tier","LevelMin","LevelMax","MeshId","AnimBlueprintId","Scale","BaseHealth","HealthPerLevel","BaseDamage","AttackSpeed","AttackRangeCm",
                   "AggroRadiusCm","LeashRadiusCm","MoveSpeed","Abilities","BehaviorTreeId","LootTableId","XPReward","FactionTag","PartyScaling"])
    if r.get("MobId") != n: err(f, n, "MobId != Name")
    enum(f, n, "Tier", r["Tier"], "ETVMobTier")
    if r["LevelMin"] > r["LevelMax"]: err(f, n, "LevelMin > LevelMax")
    manifest(f, n, "MeshId", r["MeshId"], {"SkeletalMesh", "StaticMesh"}); manifest(f, n, "AnimBlueprintId", r["AnimBlueprintId"], {"AnimBlueprint"})
    for a in r["Abilities"]:
        ref(f, n, "Abilities", a, ABILITY_IDS)
        if a in AB_ROWS and AB_ROWS[a]["ClassId"] != "MOB": err(f, n, f"ability {a} is not a MOB ability")
    ref(f, n, "LootTableId", r["LootTableId"], LOOT_IDS, allow_empty=True)
    if r["BaseHealth"] <= 0: err(f, n, "BaseHealth <= 0")

# ---------- Items ----------
TIER_ILVL = {"T1_Wanderers": (5, 8), "T2_ValeGuard": (9, 12), "T3_HollowCrown": (14, 16)}
for r in TABLES["ItemDefs"]:
    f = "DT_ItemDefs"; n = r["Name"]
    need(f, n, r, ["ItemDefId","DisplayName","SetTier","Slot","ArmorFamily","AllowedClasses","ItemLevelMin","ItemLevelMax","MeshId","MaterialInstanceId","IconId","StatPoolId","BaseArmor","BaseWeaponDamage","SellValue"])
    if r.get("ItemDefId") != n: err(f, n, "ItemDefId != Name")
    enum(f, n, "SetTier", r["SetTier"], "ETVSetTier"); enum(f, n, "Slot", r["Slot"], "ETVEquipSlot"); enum(f, n, "ArmorFamily", r["ArmorFamily"], "ETVArmorFamily")
    for c in r["AllowedClasses"]: ref(f, n, "AllowedClasses", c, CLASS_IDS)
    lo, hi = TIER_ILVL.get(r["SetTier"], (0, 99))
    if not (lo <= r["ItemLevelMin"] <= r["ItemLevelMax"] <= hi): err(f, n, f"item level range outside tier range {lo}-{hi}")
    manifest(f, n, "MeshId", r["MeshId"], {"SkeletalMesh", "StaticMesh"}); manifest(f, n, "MaterialInstanceId", r["MaterialInstanceId"], {"MaterialInstance"})
    manifest(f, n, "IconId", r["IconId"], {"Icon"})
    ref(f, n, "StatPoolId", r["StatPoolId"], POOL_IDS)
    if r["SellValue"] <= 0: err(f, n, "SellValue <= 0")
for r in TABLES["StatPools"]:
    f = "DT_StatPools"; n = r["Name"]
    need(f, n, r, ["StatPoolId","Slot","ArmorFamily","Stats"])
    enum(f, n, "Slot", r["Slot"], "ETVEquipSlot"); enum(f, n, "ArmorFamily", r["ArmorFamily"], "ETVArmorFamily")
    if len({s["Stat"] for s in r["Stats"]}) != len(r["Stats"]): err(f, n, "duplicate stat in pool")
    if len(r["Stats"]) < 4: err(f, n, "pool needs >= 4 stats for Epic (4 stats without replacement)")
    for s in r["Stats"]:
        enum(f, n, "Stats.Stat", s["Stat"], "ETVStat")
        if s["Weight"] <= 0 or s["MinPerIlvl"] <= 0 or s["MinPerIlvl"] > s["MaxPerIlvl"]: err(f, n, f"bad stat entry {s}")
    if not any(s["Stat"] in ("Stamina","Strength","Agility","Intellect","Spirit") for s in r["Stats"]): err(f, n, "no primary stat in pool")
used = {r["StatPoolId"] for r in TABLES["ItemDefs"]}
for p in POOL_IDS - used: warn("DT_StatPools", p, "pool not used by any item")

# ---------- Loot ----------
for r in TABLES["LootTables"]:
    f = "DT_LootTables"; n = r["Name"]
    need(f, n, r, ["LootTableId","GoldMin","GoldMax","DropChance","Entries","RarityWeights","ItemLevelMin","ItemLevelMax","GuaranteedDrops"])
    if r["GoldMin"] > r["GoldMax"] or r["ItemLevelMin"] > r["ItemLevelMax"]: err(f, n, "min > max")
    if not 0 <= r["DropChance"] <= 1: err(f, n, "DropChance outside 0-1")
    if not r["Entries"] or not r["RarityWeights"]: err(f, n, "empty entries or rarity weights")
    for e in r["Entries"]:
        ref(f, n, "Entries.ItemDefId", e["ItemDefId"], ITEM_IDS)
        if e["Weight"] <= 0: err(f, n, "non-positive weight")
        it = next((x for x in TABLES["ItemDefs"] if x["Name"] == e["ItemDefId"]), None)
        if it and not (it["ItemLevelMin"] <= r["ItemLevelMax"] and r["ItemLevelMin"] <= it["ItemLevelMax"]):
            err(f, n, f"item {e['ItemDefId']} ilvl {it['ItemLevelMin']}-{it['ItemLevelMax']} does not overlap table {r['ItemLevelMin']}-{r['ItemLevelMax']}")
    for w in r["RarityWeights"]: enum(f, n, "RarityWeights.Rarity", w["Rarity"], "ETVRarity");
for lid in ["LT_MOB_T1","LT_MOB_T2","LT_ELITE_GRISTLEHORN","LT_DUNGEON_TRASH","LT_BOSS_ROTFANG","LT_BOSS_MORROW","LT_BOSS_MALDREK"]:
    if lid not in LOOT_IDS: err("DT_LootTables", lid, "required table missing")

# ---------- NPCs / dialogue ----------
Q_BY = {r["Name"]: r for r in TABLES["Quests"]}
for r in TABLES["Npcs"]:
    f = "DT_Npcs"; n = r["Name"]
    need(f, n, r, ["NpcId","DisplayName","MeshId","Greeting","QuestIds","IsVendor","VendorItems"])
    manifest(f, n, "MeshId", r["MeshId"], {"SkeletalMesh"})
    for q in r["QuestIds"]:
        ref(f, n, "QuestIds", q, QUEST_IDS)
        if q in Q_BY and Q_BY[q]["GiverNpcId"] != n: err(f, n, f"quest {q} giver is {Q_BY[q]['GiverNpcId']}")
    for i in r["VendorItems"]: ref(f, n, "VendorItems", i, ITEM_IDS)
    if r["IsVendor"] != bool(r["VendorItems"]): err(f, n, "IsVendor inconsistent with VendorItems")
for q in Q_BY.values():
    if q["Name"] not in next(x for x in TABLES["Npcs"] if x["Name"] == q["GiverNpcId"])["QuestIds"] if q["GiverNpcId"] in NPC_IDS else False:
        err("DT_Npcs", q["GiverNpcId"], f"does not list quest {q['Name']}")
for r in TABLES["Dialogue"]:
    f = "DT_Dialogue"; n = r["Name"]
    need(f, n, r, ["DialogueId","NpcId","Lines","Options"])
    ref(f, n, "NpcId", r["NpcId"], NPC_IDS)
    if not r["Lines"]: err(f, n, "no lines")
    for o in r["Options"]:
        if o["ActionType"] not in ("AcceptQuest", "CompleteQuest", "OpenVendor", "Close"): err(f, n, f"ActionType {o['ActionType']!r}")
        if o["ActionType"] in ("AcceptQuest", "CompleteQuest"): ref(f, n, "Option.Param", o["Param"], QUEST_IDS)
        if o["ActionType"] == "OpenVendor": ref(f, n, "Option.Param", o["Param"], NPC_IDS)

# ---------- Quests ----------
def has_cycle(q, trail=()):
    if q in trail: return True
    return any(has_cycle(p, trail + (q,)) for p in Q_BY.get(q, {}).get("PrereqQuestIds", []) if p in Q_BY)
for r in TABLES["Quests"]:
    f = "DT_Quests"; n = r["Name"]
    need(f, n, r, ["QuestId","Title","Description","CompletionText","GiverNpcId","TurnInNpcId","RequiredLevel","PrereqQuestIds","Objectives","RewardXP","RewardGold","RewardItems","RewardMount","SetsWorldFlag"])
    ref(f, n, "GiverNpcId", r["GiverNpcId"], NPC_IDS); ref(f, n, "TurnInNpcId", r["TurnInNpcId"], NPC_IDS)
    for p in r["PrereqQuestIds"]: ref(f, n, "PrereqQuestIds", p, QUEST_IDS)
    if has_cycle(n): err(f, n, "prerequisite cycle")
    if not r["Objectives"]: err(f, n, "no objectives")
    for o in r["Objectives"]:
        enum(f, n, "Objective.Type", o["Type"], "ETVQuestObjectiveType")
        if o["Type"] == "Kill": ref(f, n, "Objective.TargetId", o["TargetId"], MOB_IDS)
        if o["Count"] < 1 or not o["TrackerText"]: err(f, n, "bad objective")
    for it in r["RewardItems"]:
        ref(f, n, "RewardItems.ItemDefId", it["ItemDefId"], ITEM_IDS)
        if it["Rarity"] != "Roll": enum(f, n, "RewardItems.Rarity", it["Rarity"], "ETVRarity")
    if len(r["Description"]) < 120 or len(r["CompletionText"]) < 80: err(f, n, "description/completion text too short")
    if r["RewardMount"] and r["RewardMount"] != ASSETS["DA_Mount_Dawnstag"]["MountId"]: err(f, n, "RewardMount does not match mount asset")
if len(Q_BY) != 9: err("DT_Quests", "-", f"expected 9 quests, found {len(Q_BY)}")

# ---------- Data assets ----------
for an, a in ASSETS.items():
    f = an
    if an.startswith("DA_Boss_"):
        need(f, an, a, ["BossMobId","Phases","EnrageTime","AddSpawns","ArenaRadius"])
        ref(f, an, "BossMobId", a["BossMobId"], MOB_IDS)
        ref(f, an, "EnrageAbilityId", a.get("EnrageAbilityId", ""), ABILITY_IDS, allow_empty=True)
        if a["EnrageTime"] > 0 and not a.get("EnrageAbilityId"): err(f, an, "EnrageTime set but no EnrageAbilityId")
        th = [p["HealthThreshold"] for p in a["Phases"]]
        if th != sorted(th, reverse=True) or th[0] != 1.0: err(f, an, f"phase thresholds must descend from 1.0: {th}")
        for p in a["Phases"]:
            for x in p["AbilityRotation"]:
                ref(f, an, "AbilityRotation.AbilityId", x["AbilityId"], ABILITY_IDS)
                if x["Interval"] <= 0: err(f, an, "Interval <= 0")
            for x in p["OnEnterAbilities"]: ref(f, an, "OnEnterAbilities", x, ABILITY_IDS)
        for s in a["AddSpawns"]: ref(f, an, "AddSpawns.MobId", s["MobId"], MOB_IDS)
    elif an.startswith("DA_SetBonus_"):
        need(f, an, a, ["SetTier", "Bonuses"])
        enum(f, an, "SetTier", a["SetTier"], "ETVSetTier")
        if sorted(b["PiecesRequired"] for b in a["Bonuses"]) != [2, 4]: err(f, an, "needs a 2-piece and a 4-piece bonus")
        for b in a["Bonuses"]:
            if not re.fullmatch(r"/Script/ThistlewickVale\.GE_TV_\w+", b["GameplayEffect"]): err(f, an, "GameplayEffect path")
    elif an == "DA_Mount_Dawnstag":
        need(f, an, a, ["MountId","DisplayName","MeshId","AnimBlueprintId","SummonCastTime","SpeedMultiplier","DismountOnDamage","BlockAbilities","SeatSocket","RiderPoseAnimId","SummonVfxId","SummonSfxId","IconId","RequiredQuestId"])
        manifest(f, an, "MeshId", a["MeshId"], {"SkeletalMesh"}); manifest(f, an, "AnimBlueprintId", a["AnimBlueprintId"], {"AnimBlueprint"})
        manifest(f, an, "RiderPoseAnimId", a["RiderPoseAnimId"], {"AnimSequence"}); manifest(f, an, "SummonVfxId", a["SummonVfxId"], {"Niagara"})
        manifest(f, an, "SummonSfxId", a["SummonSfxId"], {"Sound"}); manifest(f, an, "IconId", a["IconId"], {"Icon"})
        ref(f, an, "RequiredQuestId", a["RequiredQuestId"], QUEST_IDS)
tiers = {a["SetTier"] for a in ASSETS.values() if "SetTier" in a}
if tiers != {"T1_Wanderers", "T2_ValeGuard", "T3_HollowCrown"}: errors.append(f"set bonus tiers incomplete: {tiers}")

# ---------- manifest gaps (informational) ----------
empty = {}
for tname, field in (("Abilities","MontageId"),("Abilities","IconId"),("Items","MaterialInstanceId")):
    pass
for r in TABLES["Abilities"]:
    if r["ClassId"] != "MOB" and not r["IconId"]: warn("DT_Abilities", r["Name"], "IconId empty (needs a manifest entry)")
    if not r["MontageId"]: empty["MontageId"] = empty.get("MontageId", 0) + 1
if not ASSETS["DA_Mount_Dawnstag"]["IconId"]: warn("DA_Mount_Dawnstag", "-", "IconId empty (needs a manifest entry)")
for r in TABLES["ItemDefs"]:
    if not r["MaterialInstanceId"]: empty["Weapon MaterialInstanceId"] = empty.get("Weapon MaterialInstanceId", 0) + 1
for k, v in empty.items(): warn("summary", k, f"{v} rows left empty (no manifest entry exists yet)")

for w in warnings: print("WARN ", w)
for e in errors: print("ERROR", e)
counts = ", ".join(f"DT_{k}={len(v)}" for k, v in TABLES.items())
print(f"\n{counts}; data assets={len(ASSETS)}; manifest ids={len(MANIFEST)}")
print("FAIL" if errors else "PASS", f"({len(errors)} errors, {len(warnings)} warnings)")
sys.exit(1 if errors else 0)
