# Reference Notes

**Status: BLOCKED on the reference video.**

Reference: https://www.youtube.com/watch?v=VvxC5027GCg (playlist PLIhWkZM76kX0m8IoyzdfrHoyB5Umc45Pg)

## Access attempts (2026-11-10)
- Plain fetch of the watch page: refused by YouTube (client error, then rate limited).
- oEmbed metadata endpoint: blocked by the session's network proxy (403).
- Web search for the video id: no matching results.
- Browser tools: none available in this session (the linked PC exposes Blender and computer-use only).

Per the brief, nothing below is inferred from the video. Matthew is asked for a short summary; this file is rewritten from that summary before Phase 1.

## Genre baseline used for the draft GDD (NOT from the video)
These are the conventions of the tab-target fantasy MMO genre in general, used only so the GDD draft is not empty. Every line is to be confirmed or replaced by the video summary.

| Topic | Baseline assumption |
|---|---|
| Targeting | Tab cycles hostile targets in front of the camera, nearest first; click to target; Esc clears; abilities need a valid target in range and (for ranged) line of sight |
| Global cooldown | 1.5 s on most abilities; haste lowers it to a 1.0 s floor; off-GCD defensives |
| Cast bars | Player cast bar centered above the action bar; target cast bar under the target frame; moving cancels non-instant casts; interruptible casts shown with a distinct color |
| Cooldown display | Clockwise sweep plus remaining-seconds text on action-bar buttons; GCD sweep on all GCD buttons |
| Floating combat text | Damage dealt above target (crits larger), damage taken / heals near the player, resource gains small |
| Aggro | Threat table per mob; damage = 1 threat, healing = 0.5 split across engaged mobs, tank abilities multiply threat; taunt sets you to top threat; mob switches target at 110% (melee) / 130% (ranged) of current |
| Unit frames | Player frame top-left, target frame next to it, target-of-target small, party frames on the left |
| Action bars | One 10-slot main bar bottom-center (keys 1–0), small bag/menu bar bottom-right |
| Nameplates | Over-head name + health bar for hostiles; color by reaction; cast bar on nameplate |
| Minimap | Top-right round minimap with quest markers and zone name |
| Quest tracker | Right side under the minimap, objectives with counters |
| Bags / character sheet | Toggleable windows (B, C), paper-doll with 4 armor slots + weapon |
| Camera / movement | Third-person orbit camera, mouse-look with right button, WASD + strafe, jump, auto-run |
| Mount | Cast to summon, faster movement, dismount on damage or ability use |
| Dungeon pacing | Trash pack, pull, rest, mini-boss; ~20–30 minutes; bosses telegraph mechanics with ground markers |

## 5 things to copy (draft, to confirm)
1. Strict tab-target + GCD rhythm so every class has the same readable cadence.
2. Visible cast bars on targets, so interrupts and heals have something to react to.
3. Threat table with clear tank tools (taunt, high-threat spender).
4. Quest tracker plus minimap markers that make solo questing self-explaining.
5. Boss mechanics that each target one role (tank swap/facing, healer burst, DPS add or object check).

## 3 things to deliberately simplify (draft, to confirm)
1. No talent trees or specs: each class has exactly 5 abilities.
2. No professions, auction house, mail or economy beyond vendor gold.
3. Party finder is a single "join party" invite flow; no raid frames, no loot rolling (personal loot).
