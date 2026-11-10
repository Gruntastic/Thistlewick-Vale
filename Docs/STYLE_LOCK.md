# Style Lock

Owner: Art Director. Binding companion to `Docs/ART_CONTRACT.md` (the contract wins on conflict).

## ForgeGUI style identity
| Field | Value |
|---|---|
| name | `ThistlewickVale_Style_v1` |
| style_id | `0529e1f3-427e-46e6-adcd-dd51ed2d8023` |
| style_version | `1` |
| created | 2026-11-10 (free `style_create`, no credits) |

Every ForgeGUI image or 3D generation passes `style_id` + `style_version` (`1`). Never create another style; revisions go through the Art Director (`style_revise`, then this file is bumped).

### Exact brief (as stored)
```
stylized low-poly fantasy, flat shading, chunky readable silhouettes, warm saturated palette, soft ambient glow.

Original design language for "The Ember Crown of Thistlewick Vale". Faceted flat-shaded planes only, no photo textures, no painterly noise, no normal-map detail, no realistic skin or fabric texture; color is flat fills from a shared 16-swatch palette.

PALETTE (use only these plus lighting): Hearth Red #C8553D, Ember Gold #FFB347, Wheat #F2CC8F, Clay #B5835A, Timber #7F5539, Dark Bark #4A3428, Sky Teal #5FA8A0, Dusk Blue #3D5A80, Moss Green #6A994E, Deep Pine #386641, Meadow Lime #A7C957, Moss Glow #9CFF7A, Stone Grey #8D99AE, Slate #4A5568, Bone #EDE6D6, Hollow Violet #7A5CFF.

SILHOUETTE RULES: must read at 30 m. Oversized heads, hands, feet and weapons (about 1.15x realistic), thick chunky shapes, no thin protrusions, no filigree, simple bold shape language, strong readable outline, minimal small detail.

GLOW MOTIF: glowmoss, lanterns, crown shards and Cleric/Mage magic emit Ember Gold #FFB347 or Moss Glow #9CFF7A as soft ambient glow. Undead / Hollow content (Hollowed knights, wisps, Maldrek, Sister Morrow) glows Hollow Violet #7A5CFF. Glow is the only non-flat element.

CLASS ACCENTS: Warrior Hearth Red, Mage Dusk Blue, Cleric Wheat and Bone, Ranger Deep Pine. Gear tiers: T1 Wanderer's worn leather and cloth in Clay and Timber; T2 Vale Guard steel in Stone Grey with Moss Green tabard; T3 Hollow Crown in Slate with Ember Gold emissive trim.

WORLD MOOD: warm sunlit meadow, cozy village of timber cottages, moss-covered ruins, fading golden light versus creeping violet undead corruption. Clean neutral background for asset sheets, single subject, A-pose for characters, even soft lighting, no text, no logos.
```

## Usage for other agents
1. Pass `style_id` and `style_version: 1` on every `generation_image` / `generation_model_3d` call.
2. Start your prompt with the locked phrase, then the subject, then the per-asset notes from `Docs/CONCEPT_PROMPTS.md`. Do not restate the palette at length; the style carries it.
3. Characters/mobs/gear for 3D: single subject, A-pose, neutral background, flat colors, no base-color texture detail beyond palette fills (we re-color through the atlas in Blender).
4. Budget: nothing paid is submitted without lead approval; max 2 regenerations per asset (ART_CONTRACT §12).
5. Reject outputs with photo texture, thin spikes/filigree, realistic proportions, non-palette hues, or text.

## Shared color workflow (conventions the material scripts rely on)
- Atlas: `SourceArt/Core/T_Palette_Atlas.png`, `SourceArt/Core/palette.json` (cell index, hex, UV center; row-major from top-left, v measured downward from the top as in an image; flip v when writing UVs in Blender where v=0 is bottom: `v_blender = 1 - v`). Regenerate with `python3 SourceArt/Core/make_palette_atlas.py`.
- Glow-only cells: 1 (Ember Gold), 11 (Moss Glow), 15 (Hollow Violet); `EmissiveMaskFromAtlas = 1` makes only these emit.
- Tinted gear/class regions are UV-mapped to cell 14 (Bone). The material instances use `tint = target / Bone`, so the result lands on the target palette color. Primary regions: vertex alpha 0 (vertex color mode) or UV1.x <= 0.5; secondary regions: vertex alpha 1 or UV1.x > 0.5.
- T3 trim goes on cell 1 (Ember Gold) so it glows; everything else on cell 14.
- Class MI tints (secondary region): Warrior Hearth Red, Mage Dusk Blue, Cleric Wheat, Ranger Deep Pine. Gear tints: T1 Clay/Timber, T2 Stone Grey/Moss Green, T3 Slate + Ember Gold emissive 3.0.

## Tools
- `Tools/ue/create_master_material.py` then `Tools/ue/create_material_instances.py`, headless: `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<path>`. Idempotent.
- Flat shading is derived from DDX/DDY of world position (material outputs a world-space normal); `FlatShaded` off falls back to the vertex normal.
- `DissolveAmount` is a true dissolve on `M_Master_LowPoly_Masked`; on the opaque master it only darkens, so dying mobs swap to a Masked MI.
- Engine API names in those scripts are unverified against 5.8 headers (see `# VERIFY-5.8:` comments); first in-editor run is the check.
