# Art Contract (v1.0, binding for every agent)

Any asset that breaks this contract is rejected by QA validation. Changes to this file are made only by the lead; agents raise a request instead.

## 1. Style
**Style prompt (locked):** "stylized low-poly fantasy, flat shading, chunky readable silhouettes, warm saturated palette, soft ambient glow".
- ForgeGUI style identity: created by the Art Director in Phase 1 as `ThistlewickVale_Style_v1`. Its `style_id` + `style_version` are recorded in `Docs/STYLE_LOCK.md`; **every** ForgeGUI image or 3D generation passes them.
- Silhouettes read at 30 m: oversized heads/hands/weapons on characters (about 1.15× realistic), thick shapes, no thin protrusions under 4 cm.
- No photo textures, no normal maps on characters/props. Color comes from the master material and the palette atlas. Faceted flat normals.
- Glow motif: glowmoss, lanterns, crown shards and Cleric/Mage effects carry the emissive "ember gold" (#FFB347) or "moss glow" (#9CFF7A). Undead/Hollow content uses "hollow violet" (#7A5CFF) emissive.

## 2. Palette (shared, 16 swatches)
Stored as `T_Palette_Atlas` (256×256, 16 4×4 cells of 64 px, sRGB, no mips beyond 64 px, nearest filtering). UVs of every mesh point to cell centers (vertex color may substitute, see §6).

| Cell | Name | Hex | | Cell | Name | Hex |
|---|---|---|---|---|---|---|
| 0 | Hearth Red | #C8553D | | 8 | Moss Green | #6A994E |
| 1 | Ember Gold | #FFB347 | | 9 | Deep Pine | #386641 |
| 2 | Wheat | #F2CC8F | | 10 | Meadow Lime | #A7C957 |
| 3 | Clay | #B5835A | | 11 | Moss Glow | #9CFF7A |
| 4 | Timber | #7F5539 | | 12 | Stone Grey | #8D99AE |
| 5 | Dark Bark | #4A3428 | | 13 | Slate | #4A5568 |
| 6 | Sky Teal | #5FA8A0 | | 14 | Bone | #EDE6D6 |
| 7 | Dusk Blue | #3D5A80 | | 15 | Hollow Violet | #7A5CFF |

Class accent colors (material-instance tints): Warrior Hearth Red, Mage Dusk Blue, Cleric Wheat/Bone, Ranger Deep Pine.
Tier tints: T1 Wanderer's = Clay + Timber (worn leather/cloth), T2 Vale Guard = Stone Grey + Moss Green (steel and green tabard), T3 Hollow Crown = Slate + Ember Gold emissive trim.

## 3. Master material
`/Game/MMO/Core/Materials/M_Master_LowPoly` (opaque, default lit), one instance per use. Parameters (names are code-facing, do not rename):

| Parameter | Type | Default | Purpose |
|---|---|---|---|
| `PaletteAtlas` | Texture | T_Palette_Atlas | Base color source |
| `UseVertexColor` | Static switch | false | Use vertex color instead of atlas |
| `BaseTint` | Vector | (1,1,1) | Multiplies primary regions |
| `SecondaryTint` | Vector | (1,1,1) | Multiplies regions masked by vertex alpha / UV1.x > 0.5 |
| `EmissiveColor` | Vector | (0,0,0) | Glow color |
| `EmissiveStrength` | Scalar | 0 | 0–20 |
| `EmissiveMaskFromAtlas` | Scalar | 0 | 1 = only atlas cells 1, 11, 15 glow |
| `FlatShaded` | Static switch | true | Derive faceted normals from DDX/DDY of world position |
| `Roughness` | Scalar | 0.8 | |
| `HitFlash` | Scalar | 0 | Runtime white flash on hit (0–1) |
| `DissolveAmount` | Scalar | 0 | Death/despawn dissolve |

Masked variant `M_Master_LowPoly_Masked` (foliage cards) and `M_Master_Unlit_FX` (VFX) share the same parameter names where applicable.

## 4. Scale, axes, pivots
- 1 UU = 1 cm. Humanoid height 180 cm (Mossbacks 120 cm, Hollowed Footmen 200 cm, Maldrek 320 cm, Dawnstag 190 cm at the shoulder).
- Forward = +X, up = +Z, right = +Y (UE convention). Blender exports go through one shared script, `Tools/blender/ue5_export.py` (Phase 1 deliverable from the Environment Artist), which sets units, axes and scale; nobody exports by hand. The import result is checked in-editor: a 180 cm humanoid must measure 180 cm facing +X.
- Pivots: characters/mobs/mount/props at the feet center on the ground plane; weapons at the grip; modular kit pieces at the bottom-left-back corner on a **100 cm grid** (walls 400×50×400 cm, floors 400×400 cm).

## 5. Skeletons and animation
- Humanoids (classes, Mossbacks, Footmen, Maldrek): skeleton **compatible with the UE5 Manny/Quinn mannequin** bone names and hierarchy (`root`, `pelvis`, `spine_01..05`, `neck_01`, `head`, `clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r`, fingers optional, `thigh_l/r`, `calf_l/r`, `foot_l/r`, `ball_l/r`, IK bones). Generated rigs from ForgeGUI are **retargeted** with an IK Rig + IK Retargeter to `SK_Mannequin`-compatible `SK_MMO_Humanoid`; the shared animation set lives on `SK_MMO_Humanoid`.
- Non-humanoids (Thornback Boar, Barrow Wisp, Dawnstag, Gristlehorn's beast): own skeleton `SK_MMO_Quadruped` (boar, stag, beast share it; scale via retargeter) and `SK_MMO_Wisp` (3 bones).
- **Required sockets** (exact names): `hand_r_weapon`, `hand_l_offhand`, `head_slot`, `back_slot` on every humanoid; `mount_seat` on the Dawnstag. Also `fx_chest`, `fx_overhead` on all characters/mobs (VFX attach, nameplate anchor).
- Required clips per humanoid set: `Idle, Walk, Run, Jump_Start, Jump_Loop, Jump_Land, Attack_Melee_01, Attack_Melee_02, Cast_Start, Cast_Loop, Cast_Release, Hit_React, Death`, plus `Mounted_Idle` for players. Quadrupeds: `Idle, Walk, Trot, Gallop, Attack, Hit_React, Death`. Mount: `Idle, Trot, Gallop` minimum.
- Animation Blueprints: `ABP_MMO_Humanoid` (locomotion blend space by speed, upper-body slot `UpperBody` for abilities, full-body slot `DefaultSlot`), `ABP_MMO_Quadruped`. Abilities play **montages** referenced from data, never hard-coded.

## 6. UVs and color
Option A (default): single UV channel mapped to palette atlas cell centers. Option B: vertex colors with `UseVertexColor` on. Vertex alpha (or UV1.x > 0.5) marks secondary-tint regions so gear can be tinted per tier. One material slot per mesh (two max for emissive parts).

## 7. Triangle budgets (hard limits)
| Category | Budget |
|---|---|
| Player character (body, nude/base) | 3,000–5,000 |
| Mob | 1,000–3,000 |
| Boss / world elite | ≤ 8,000 |
| Mount | ≤ 4,000 |
| Armor piece (per slot) | ≤ 1,200 |
| Weapon | ≤ 800 |
| Prop | ≤ 500 |
| Building | ≤ 2,000 |
| Tree / large rock | ≤ 800 / ≤ 400 |
| Dungeon kit piece | ≤ 600 |
LODs: characters/mobs LOD1 at 50%, props auto LOD via Nanite **off** (low-poly; use standard LODs). Textures: atlas only; UI textures ≤ 512 px (icons 128 px).

## 8. Modular gear
Armor pieces are skeletal meshes skinned to `SK_MMO_Humanoid` and attached with Leader Pose (or the 5.8 equivalent the engineer verifies). Slots: `Head`, `Chest`, `Legs`, `Boots`. The base body hides covered regions via per-slot body section toggles. Weapons are static meshes attached to `hand_r_weapon` / `hand_l_offhand` / `back_slot` (sheathed). One mesh per slot per **silhouette family** (plate / leather / robe); tiers differ by material instance, not mesh, unless budget allows a T3 variant.

## 9. Naming
Prefixes: `SK_` skeletal mesh, `SKEL_` skeleton, `SM_` static mesh, `M_`/`MI_` materials, `T_` textures, `ABP_` anim blueprint, `AM_` montage, `A_` anim sequence, `BS_` blend space, `DA_` data asset, `DT_` data table, `BP_` blueprint, `NS_` Niagara, `SFX_` sound wave, `SC_` MetaSound/cue, `MUS_` music, `WBP_` widget, `L_` level, `IKR_`/`RTG_` IK rig / retargeter.
Pattern: `<Prefix>_<Category>_<Name>[_<Variant>]`, PascalCase, no spaces. Examples: `SK_Class_Warrior`, `SK_Gear_Plate_Chest`, `MI_Gear_T2_ValeGuard_Plate`, `SM_Env_Cottage_A`, `SFX_Ability_Warrior_ShieldSlam`, `MUS_Zone_Meadow`.

## 10. Folder layout
```
/Game/MMO/
  Core/        (master materials, palette, GAS base classes' BP children, game mode)
  Characters/  (classes, humanoid skeleton, ABPs, shared anims)
  Mobs/        Gear/   Mounts/   Environment/   Props/
  VFX/         Audio/{Music,SFX}   UI/{Widgets,Icons,Frames}
  Data/        (all DA_ and DT_)
  Maps/        (L_Thistlewick, L_Barrow, L_Entry)
```
Source files outside Content: `/SourceArt/<Category>/<ManifestID>/` (GLB/FBX/blend), `/SourceAudio/`.

## 11. Audio
WAV 48 kHz 16-bit for import (convert from ForgeGUI MP3). SFX mono, ≤ 3 s, peak −3 dBFS. Music stereo, loopable, −14 LUFS target. Every ability has exactly one `SFX_Ability_*` (variations optional).

## 12. Asset manifest
Every asset has a manifest ID (`CHR_`, `MOB_`, `MNT_`, `GEAR_`, `WPN_`, `ENV_`, `PRP_`, `VFX_`, `AUD_`, `MUS_`, `UI_`, `MAT_`) in `Docs/ASSET_MANIFEST.csv`. Data tables reference assets through `DT_AssetManifest` (ID → soft object path). Status flow: `planned → placeholder → generated → cleaned → imported → final` (or `failed-kept-placeholder`). Max 2 regeneration retries per asset.
