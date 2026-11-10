# Agent Task Board

Status key: ☐ not started · ◐ in progress · ☑ done · ⛔ blocked. Phase 1 starts only after Matthew's go-ahead.

## Phase 0 — Lead (done except where marked)
- ☑ Toolchain check (see "Tool reality" below)
- ⛔ `Docs/reference_notes.md` — reference video unreachable from this session; waiting on Matthew's summary
- ☑ `Docs/GDD.md` (draft v0.1; *(ref)* sections revisited after the summary)
- ☑ `Docs/ART_CONTRACT.md` v1.0
- ☑ `Docs/DATA_SCHEMAS.md` v1.0
- ☑ `Docs/ASSET_MANIFEST.csv` (223 planned entries)
- ☑ Git repository: Gruntastic/Thistlewick-Vale (binaries via Git LFS)
- ☐ Lead Phase 1 kickoff: UE project skeleton (`ThistlewickVale.uproject`, module, Core types from DATA_SCHEMAS, manifest subsystem), so agents share one compiled base

## Tool reality (checked 2026-11-10)
| Tool | Reachable? | Consequence |
|---|---|---|
| ForgeGUI (image, GUI, 3D, remesh, rig, animate, music, SFX, styles) | Yes, from this session | Art/audio agents can generate. Uses Matthew's ForgeGUI credits; ~25 3D models × (generate + remesh + rig + up to 14 animations) is the big spend |
| Blender MCP | Yes, on Matthew's PC (nitronugget) | Needs Blender running there; outputs land on the PC, so we need one PC folder granted for SourceArt |
| Unreal Editor | Not drivable (no Unreal MCP). UE 5.7 and 5.8 are installed on the PC | Engineers write C++, data CSV/JSON and editor Python scripts here; compiling and in-editor import run on the PC (Remote Control session, or Matthew runs one command) |
| Browser / YouTube | No browser tool; YouTube blocked to plain fetch | Reference notes need Matthew's summary |
| Roblox Studio | Present, ignored per brief | – |
| GitHub | Repo Gruntastic/Thistlewick-Vale attached | Worktrees per code agent, PRs into main, LFS for binaries |

## Phase 1 — Contracts to placeholders (parallel)
| # | Agent | Model | Tasks | Depends on | Deliverables |
|---|---|---|---|---|---|
| 1 | Art Director | sonnet | Create ForgeGUI style `ThistlewickVale_Style_v1`, record id in `Docs/STYLE_LOCK.md`; generate palette atlas PNG from ART_CONTRACT §2; write `Tools/ue/create_master_material.py` (M_Master_LowPoly + MIs); 4 class + 3 tier + mount + 5 mob + boss + hub concept images | Contract | STYLE_LOCK.md, T_Palette_Atlas.png, concept sheets in `SourceArt/Concepts/`, material script |
| 11 | Content Designer | sonnet | All `Data/Source/*.csv` per DATA_SCHEMAS: classes, base stats, XP curve, 20 abilities, 5 mobs + elite + 3 bosses, item defs (3 tiers × 3 families × 4 slots + 4 weapons), stat pools, loot tables, 9 quests, NPCs, dialogue | Schemas | CSVs + `Docs/BALANCE.md` |
| 7 | Combat Engineer | opus | GAS module: ASC on PlayerState, attribute sets, generic ability executors, GCD, cast/channel/interrupt, combo resources, threat table, tab targeting, mob AI (BT, leash), boss data-asset driver | Lead skeleton | `Source/.../Combat/**`, tags ini, unit tests |
| 8 | Systems Engineer | opus | Inventory + equipment components, deterministic roll library + validator, set bonuses, loot drops, quest component, mount summon/dismount, JSON save/load | Lead skeleton | `Source/.../Systems/**`, tests for roll determinism |
| 9 | Network & Dungeon Engineer | opus | Server target, entry map login (name + class), party subsystem, dungeon instancing + portal, party-size scaling hook, 2-client test script | Lead skeleton | `Source/.../Net/**`, `Tools/run_server.bat`, `Tools/run_2_clients.bat` |
| 4 | Environment Artist | sonnet | `Tools/blender/ue5_export.py`, greybox placeholders for every manifest ID (correct size, pivot, tri budget), then procedural kit | Contract | `SourceArt/Env/**`, placeholder FBXs |

## Phase 2 — Production (parallel)
| # | Agent | Model | Tasks |
|---|---|---|---|
| 2 | Character & Mob Artist | sonnet + haiku workers | Bulk-submit 3D for 4 classes, 9 mobs/bosses, 4 NPCs, mount → remesh to budget → rig → Blender cleanup (pivots, sockets) |
| 3 | Animator | sonnet | Animate humanoid and quadruped sets, IK Rig/Retargeter script to SK_MMO_Humanoid, ABP_MMO_Humanoid / Quadruped / Dawnstag, ability montages |
| 5 | Gear & Weapon Artist | haiku batch + sonnet review | 12 armor meshes (3 families × 4 slots), 4 weapons, 9 tier material instances, 15 item icons |
| 6 | Audio | haiku | 4 music tracks, 20 ability SFX, 15 general SFX, MP3→WAV, import script |
| 10 | UI Engineer | sonnet | All WBP widgets, tooltips with rarity colors, action bar with cooldown/GCD sweep, cast bars, quest tracker, minimap, nameplates, FCT |
| 4 | Environment Artist | sonnet | Zone layout (hub + 3 sub-areas + entrance), dungeon modules and L_Barrow, spawn points |

## Phase 3 — Integration (Lead + QA)
Swap placeholders by manifest ID, build dungeon + boss encounters, world-state glow flag.

## Phase 4 — QA (opus, final pass by Lead)
`Tools/validate_assets.py` (naming, budget, sockets, manifest status), automation tests, 2-client + dedicated server smoke test, Definition-of-Done checklist. Max two fix rounds.
