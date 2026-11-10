# Data Schemas (v1.0, binding for every agent)

All gameplay content is data. C++ defines the row structs below (module `ThistlewickVale`, prefix `FTV` / `UTV`). Content Designer authors CSV/JSON in `/Data/Source/` which import into `DT_` tables under `/Game/MMO/Data/`. Every art/audio reference is a **manifest ID** (FName) resolved through `DT_AssetManifest` to a `TSoftObjectPtr`, so swapping a model is one row edit.

> Engine-version rule: field types below are plain UE types that have been stable for many versions. Anything version-specific (GAS setup, Leader Pose / mesh merging, level instancing, Iris replication, Enhanced Input APIs) must be checked against the installed 5.8 headers or docs before use, and the engineer notes which header they verified in the PR.

## 0. Conventions
- IDs are `FName`, UPPER_SNAKE (e.g. `ABL_WAR_SHIELD_SLAM`). Row name = ID.
- Gameplay tags root: `TV.` (e.g. `TV.Ability.Warrior.ShieldSlam`, `TV.State.Mounted`, `TV.Cooldown.Ability.X`, `TV.Debuff.Magic`). Tag list lives in `Config/Tags/TVGameplayTags.ini`, owned by the Combat Engineer.
- Times in seconds, distances in cm, percentages as 0–1 floats.

## 1. Asset manifest — `DT_AssetManifest` (`FTVAssetManifestRow`)
| Field | Type | Notes |
|---|---|---|
| ManifestId | FName (row name) | e.g. `CHR_WARRIOR_BODY` |
| AssetType | ETVAssetType | SkeletalMesh, StaticMesh, Material, Texture, AnimSequence, AnimMontage, AnimBlueprint, Niagara, Sound, Music, Widget, Icon |
| Asset | TSoftObjectPtr<UObject> | Placeholder first, final later |
| Status | ETVAssetStatus | Planned, Placeholder, Generated, Cleaned, Imported, Final, FailedKeptPlaceholder |
| TriCount | int32 | Measured, validated against budget |
| Budget | int32 | From ART_CONTRACT §7 |
`UTVAssetRegistrySubsystem::Resolve(FName) -> TSoftObjectPtr` with async load helper. `Docs/ASSET_MANIFEST.csv` is the human source; `Tools/sync_manifest.py` writes the DT CSV.

## 2. Classes — `DT_Classes` (`FTVClassRow`)
| Field | Type |
|---|---|
| ClassId | FName (`CLS_WARRIOR`, `CLS_MAGE`, `CLS_CLERIC`, `CLS_RANGER`) |
| DisplayName, Description | FText |
| Role | ETVRole (Tank, Healer, DPS) |
| ResourceType | ETVResource (Rage, Mana, Focus) |
| BodyMeshId, AnimBlueprintId, PortraitIconId | FName (manifest) |
| ClassTintPrimary, ClassTintSecondary | FLinearColor |
| ArmorFamily | ETVArmorFamily (Plate, Leather, Robe) |
| WeaponType | ETVWeaponType (Sword+Shield, Staff, Mace+Tome, Bow+Daggers) |
| StartingAbilities | TArray<FTVAbilityUnlock> {AbilityId, UnlockLevel, DefaultSlot} |
| BaseStatsCurveId | FName → `DT_ClassBaseStats` row |

`DT_ClassBaseStats` (`FTVClassLevelStatsRow`): ClassId, Level (1–10), Stamina, Strength, Agility, Intellect, Spirit, BaseHealth, BaseResourceMax, BaseResourceRegen.
`DT_XPCurve`: Level, XPToNext.

## 3. Abilities — `DT_Abilities` (`FTVAbilityRow`)
| Field | Type | Notes |
|---|---|---|
| AbilityId | FName | `ABL_WAR_CLEAVE_STRIKE` … |
| ClassId | FName | |
| SlotKind | ETVAbilitySlot | Builder, Spender, Utility, Defensive, Ultimate |
| AbilityClass | TSoftClassPtr<UTVGameplayAbility> | Generic executors are preferred: `GA_TV_DirectDamage`, `GA_TV_Heal`, `GA_TV_ApplyEffect`, `GA_TV_GroundAoE`, `GA_TV_Channel`, `GA_TV_Taunt`, `GA_TV_Leap` |
| AbilityTag | FGameplayTag | |
| CastTime, ChannelTime | float | 0 = instant |
| Cooldown | float | |
| TriggersGCD | bool | |
| ResourceCost | float | negative = generates |
| ComboResource | FName | `Kindling`, `Devotion` or None |
| ComboDelta | int32 | +1 builders, -All spenders (−99) |
| RangeCm, RadiusCm | float | |
| RequiresTarget, RequiresLOS, Interruptible, UsableWhileMoving | bool | |
| TargetType | ETVTargetType | Enemy, Friendly, Self, Ground, PartyAoE |
| BaseMagnitude | float | damage/heal/absorb at level 1 |
| CoefficientStat | ETVStat | scaling stat |
| Coefficient | float | magnitude += stat × coefficient |
| ThreatMultiplier | float | |
| Effects | TArray<FTVEffectSpec> {EffectClass soft class, Duration, Magnitude, SetByCallerTag} |
| MontageId, VfxCastId, VfxImpactId, SfxId, IconId | FName (manifest) |
| Tooltip | FText (supports `{Magnitude}`, `{Cooldown}` tokens) |

Attribute sets (C++): `UTVAttributeSet_Vitals` (Health, MaxHealth, Resource, MaxResource, ResourceRegen), `UTVAttributeSet_Stats` (Stamina, Strength, Agility, Intellect, Spirit, Crit, Haste, Armor, MoveSpeedMultiplier), meta attributes `IncomingDamage`, `IncomingHeal`.

## 4. Items and randomized gear
### Item definitions — `DT_ItemDefs` (`FTVItemDefRow`)
| Field | Type |
|---|---|
| ItemDefId | FName (`ITM_T2_PLATE_CHEST`, `ITM_T1_WEAPON_WARRIOR`) |
| DisplayName | FText |
| SetTier | ETVSetTier (None, T1_Wanderers, T2_ValeGuard, T3_HollowCrown) |
| Slot | ETVEquipSlot (Head, Chest, Legs, Boots, MainHand, OffHand, None) |
| ArmorFamily / AllowedClasses | ETVArmorFamily / TArray<FName> |
| ItemLevelMin, ItemLevelMax | int32 |
| MeshId, MaterialInstanceId, IconId | FName (manifest) |
| StatPoolId | FName → `DT_StatPools` |
| BaseArmor / BaseWeaponDamage | float |
| SellValue | int32 |

### Item instance (replicated) — `FTVItemInstance`
```
FName   ItemDefId;
ETVSetTier SetTier;
ETVEquipSlot Slot;
ETVRarity Rarity;     // Common=1 stat, Uncommon=2, Rare=3, Epic=4 (dungeon only)
int32   ItemLevel;
int32   Seed;         // server-generated
TArray<FTVRolledStat> RolledStats;  // {ETVStat Stat; int32 Value;}
FGuid   InstanceGuid;
```
### Roll algorithm (server only, deterministic)
1. `FRandomStream Stream(Seed)`.
2. Number of stats = rarity count (1–4).
3. Draw stats **without replacement** from the slot's weighted pool `DT_StatPools[StatPoolId]` (`FTVStatPoolRow`: Slot, ArmorFamily, TArray<{Stat, Weight, MinPerIlvl, MaxPerIlvl}>). Primary stats weight-favored for the armor family; first stat is always a primary.
4. Value = round(Stream.FRandRange(MinPerIlvl, MaxPerIlvl) × ItemLevel × RarityMultiplier) with RarityMultiplier Common 1.0, Uncommon 1.1, Rare 1.25, Epic 1.45. Minimum 1.
5. `UTVLootLibrary::RollItem(Def, Rarity, ItemLevel, Seed)` is a pure function; clients and the save loader call `ValidateItem(Instance)` which re-derives and compares. Mismatch = item rejected and logged.
Stats: Stamina, Strength, Agility, Intellect, Spirit (primary); Crit, Haste, Armor (secondary).

### Set bonuses — `DA_SetBonus_<Tier>` (`UTVSetBonusDataAsset`)
SetTier, TArray<{PiecesRequired (2 or 4), DisplayText, GameplayEffect soft class, Magnitude}>.

### Loot — `DT_LootTables` (`FTVLootTableRow`)
LootTableId (`LT_MOB_T1`, `LT_MOB_T2`, `LT_ELITE_GRISTLEHORN`, `LT_DUNGEON_TRASH`, `LT_BOSS_ROTFANG`, `LT_BOSS_MORROW`, `LT_BOSS_MALDREK`), GoldMin/Max, DropChance, Entries[{ItemDefId, Weight}], RarityWeights[{Rarity, Weight}], ItemLevelMin/Max, GuaranteedDrops (int). Personal loot: rolled per eligible party member.

Tooltip colors: Common #FFFFFF, Uncommon #1EFF00, Rare #0070DD, Epic #A335EE (stat lines colored by item rarity; set bonus lines grey until active, green when active).

## 5. Mobs — `DT_Mobs` (`FTVMobRow`)
MobId, DisplayName, Tier (Normal, Elite, WorldElite, MiniBoss, Boss), LevelMin/Max, MeshId, AnimBlueprintId, Scale, BaseHealth, HealthPerLevel, BaseDamage, AttackSpeed, AttackRangeCm, AggroRadiusCm, LeashRadiusCm, MoveSpeed, Abilities TArray<FName> (AbilityIds usable by AI, same `DT_Abilities` table with ClassId `MOB`), BehaviorTreeId, LootTableId, XPReward, FactionTag, PartyScaling bool.

Boss mechanics: `DA_Boss_<Name>` (`UTVBossDataAsset`): Phases[{HealthThreshold, AbilityRotation[{AbilityId, Interval, InitialDelay}], OnEnterAbilities}], EnrageTime, AddSpawns[{MobId, Count, HealthTrigger}], ArenaRadius.

Spawns: `BP_TVSpawnPoint` actors in the level hold MobId, Count, RespawnTime, PatrolPath; no hard-coded mob classes.

## 6. Quests — `DT_Quests` (`FTVQuestRow`)
QuestId (`Q01_FADING_GLOW` …), Title, Description, CompletionText (FText), GiverNpcId, TurnInNpcId, RequiredLevel, PrereqQuestIds, Objectives[{Type (Kill, Collect, Interact, Escort/Defend, Event), TargetId, Count, AreaTag, TrackerText}], RewardXP, RewardGold, RewardItems[{ItemDefId, Rarity or "Roll"}], RewardMount (FName), SetsWorldFlag (FName).
NPCs: `DT_Npcs` (NpcId, DisplayName, MeshId, Greeting FText, QuestIds, IsVendor, VendorItems).
Dialogue: `DT_Dialogue` (DialogueId, NpcId, Lines TArray<FText>, Options[{Text, ActionType (AcceptQuest, CompleteQuest, OpenVendor, Close), Param}]).

## 7. Mount — `DA_Mount_Dawnstag` (`UTVMountDataAsset`)
MountId, DisplayName, MeshId, AnimBlueprintId, SummonCastTime 1.5, SpeedMultiplier 1.6, DismountOnDamage true, BlockAbilities true, SeatSocket `mount_seat`, RiderPoseAnimId, SummonVfxId, SummonSfxId, IconId, RequiredQuestId `Q06_DAWNSTAG_TRUST`.

## 8. Save format (server, JSON) — `Saved/Characters/<Account>_<Character>.json`
```json
{
  "schemaVersion": 1,
  "account": "PlayerOne", "name": "Ash", "classId": "CLS_WARRIOR",
  "level": 7, "xp": 1240, "gold": 85,
  "location": {"map": "L_Thistlewick", "x": 0, "y": 0, "z": 0, "yaw": 0},
  "inventory": [ {"itemDefId": "ITM_T1_PLATE_CHEST", "setTier": "T1_Wanderers", "slot": "Chest",
                  "rarity": "Uncommon", "itemLevel": 6, "seed": 918273, "rolledStats": [{"stat": "Strength", "value": 7}, {"stat": "Stamina", "value": 5}],
                  "instanceGuid": "..."} ],
  "equipment": {"Chest": "<instanceGuid>"},
  "quests": {"Q01_FADING_GLOW": {"state": "Completed"}, "Q02_MOSSBACK_MISCHIEF": {"state": "Active", "progress": [5]}},
  "mounts": ["MNT_DAWNSTAG"],
  "actionBar": ["ABL_WAR_CLEAVE_STRIKE", "ABL_WAR_SHIELD_SLAM", null, null, null, null, null, null, null, null],
  "worldFlags": []
}
```
Loaded items are re-validated with `ValidateItem`. Login is "name + class" on the entry map (no auth).

## 9. Networking contracts (Network & Dungeon Engineer owns)
- Dedicated server target `ThistlewickValeServer`; clients connect by IP (`open <ip>:7777`).
- Ownership of state: AttributeSets and ASC on **PlayerState** (survives pawn death); inventory/equipment/quests in replicated components on PlayerState; mobs own their ASC.
- All ability activation is `LocalPredicted` for player abilities, `ServerOnly` for AI. Loot rolls, quest progress, item validation and saves are server-only.
- Party: `UTVPartySubsystem` on server, `ATVPartyInfo` replicated actor per party.
- Dungeon instancing: per-party runtime level instance of `L_Barrow` at a Z offset (verify the 5.8 dynamic level-instance API before implementing); `BP_TVDungeonPortal` teleports the party; instance unloads 60 s after the last member leaves.

## 10. Input — Enhanced Input
`IMC_TV_Default` (move, look, jump, tab-target, target self/party F1–F5, action slots 1–0, interact, toggle bags/char/quest/abilities, auto-run, mount key), `IMC_TV_Mounted` (movement only + dismount). Input actions `IA_TV_<Name>`; owned by the UI Engineer for UI keys, Combat Engineer for combat keys.

## 11. Ownership of files (merge-conflict avoidance)
| Path | Owner |
|---|---|
| `Source/ThistlewickVale/Combat/**`, `Config/Tags/**`, `Data/Source/abilities*.csv`(schema) | Combat Engineer |
| `Source/ThistlewickVale/Systems/**` (Inventory, Loot, Quests, Mount, Save) | Systems Engineer |
| `Source/ThistlewickVale/Net/**`, `Source/ThistlewickValeServer*`, `Config/DefaultEngine.ini` net sections | Network & Dungeon Engineer |
| `Source/ThistlewickVale/UI/**`, `Content/MMO/UI/**` | UI Engineer |
| `Source/ThistlewickVale/Core/**` (types in this doc, manifest subsystem) | Lead (Phase 0/1), then frozen |
| `Data/Source/**` values | Content Designer |
| `SourceArt/**`, `Tools/blender/**` | Art agents per category |
| `SourceAudio/**` | Audio |
| `Tests/**`, `Tools/validate_*.py` | QA |

## 12. Table registry and data file format (added in Phase 1)
- Row structs: shared enums, `FTVItemInstance`, `FTVRolledStat` and `FTVAssetManifestRow` are in `Source/ThistlewickVale/Core/TVTypes.h` (lead). Every other row struct is written by the engineer who owns that domain, **with field names exactly as in this document**: Combat owns `FTVClassRow`, `FTVClassLevelStatsRow`, `FTVXPCurveRow`, `FTVAbilityRow`, `FTVMobRow`, `UTVBossDataAsset`; Systems owns `FTVItemDefRow`, `FTVStatPoolRow`, `FTVLootTableRow`, `UTVSetBonusDataAsset`, `FTVQuestRow`, `FTVNpcRow`, `FTVDialogueRow`, `UTVMountDataAsset`.
- Code finds tables through `UTVProjectSettings::Get()->LoadTable(Key)`. Keys: `Classes`, `ClassBaseStats`, `XPCurve`, `Abilities`, `Mobs`, `ItemDefs`, `StatPools`, `LootTables`, `Quests`, `Npcs`, `Dialogue`. Asset path convention: `/Game/MMO/Data/DT_<Key>`.
- Source data is **JSON in UE DataTable import format**: `Data/Source/DT_<Key>.json`, an array of objects whose `"Name"` is the row name (the ID) and whose other keys are the field names in this document. Enums are written as their name strings (`"Tank"`, `"T2_ValeGuard"`). Soft class/object fields are path strings. Data assets (bosses, set bonuses, mount) are authored as `Data/Source/DA_<Name>.json` with the same field names and created by `Tools/ue/import_data.py`.
- `Tools/ue/import_data.py` (Systems Engineer owns) imports every `Data/Source/DT_*.json` into `/Game/MMO/Data/` with the matching row struct and registers it in `Config/DefaultGame.ini`.

## 13. Engine facts checked on Matthew's PC (2026-11-10)
- Installed engine: **5.8.4** launcher build at `C:\Program Files\Epic Games\UE_5.8` (it was 5.8.3 when the Phase 1 code was checked against its headers, and updated the same day). Targets use `BuildSettingsVersion.V7` and `EngineIncludeOrderVersion.Unreal5_8` (copied from the 5.8 third-person template).
- A launcher build cannot compile a `Server` target. The dedicated server runs from the editor binary: `UnrealEditor.exe ThistlewickVale.uproject L_Thistlewick -server -log`, clients with `-game`. A real `ThistlewickValeServer.Target.cs` is included but only builds with a source engine.
- Engine headers can be read from the PC for API checks; mark anything not checked with `// VERIFY-5.8:`.
