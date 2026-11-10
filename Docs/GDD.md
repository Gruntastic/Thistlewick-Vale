# The Ember Crown of Thistlewick Vale — Game Design Document (vertical slice)

Engine: Unreal Engine 5.8 (target 5.8.4). Genre: stylized low-poly, tab-target fantasy MMORPG. All names, art, audio and content are original.
Status: DRAFT v0.1. Sections marked *(ref)* will be revisited once the reference video summary arrives.

## 1. Pillars
1. **Readable combat**: tab-target, 1.5 s GCD, clear cast bars, every boss mechanic answers to a role.
2. **Small and finished**: one zone, one dungeon, four classes, three gear sets, one mount, levels 1–10.
3. **Swappable everything**: content is data; art is referenced by manifest ID; no code changes to upgrade assets.
4. **Warm, glowing world**: the fading glowmoss is both story and lighting motif; things get brighter as you succeed.

## 2. Story
For centuries the Dawnstag spirits kept the Vale's **Ember Crown** lit, and the crown's light fed the **glowmoss** that sustains the village of **Lanternrest**. The ambitious knight **Maldrek** shattered the crown to rule the dead and became the **Hollow King**, sealed in **Maldrek's Barrow**. The seal is cracking: the glowmoss is fading, **hollowed knights** walk the hills, and moss-covered goblins, the **Mossbacks**, raid the meadow for the last of the light.

The player arrives in Lanternrest as a newcomer. Elder **Wren Lamplight** asks them to help. They gather the three **crown shards** (one per sub-area), earn the trust of the last **Dawnstag** (mount), and descend into the Barrow to defeat the Hollow King and relight the Ember Crown. On completion the glowmoss in Lanternrest visibly brightens (a world-state flag swaps the hub's emissive material parameter).

### Key NPCs
| NPC | Role | Location |
|---|---|---|
| Elder Wren Lamplight | Main quest giver | Lanternrest square |
| Bram Tallow | Quartermaster, vendor, gear repair | Lanternrest |
| Ysolde Thorn | Ranger scout, forest quests | Hollow Forest edge camp |
| Brother Cael | Cleric, hills and Barrow quests | Ruined Hills watchtower |
| The Dawnstag (spirit) | Mount quest | Stag Shrine, meadow cliffs |

## 3. World: Thistlewick Vale (~1 km²)
| Area | Levels | Content |
|---|---|---|
| **Lanternrest** (hub) | – | Spawn point, 5 NPCs, vendor, bind stone, mailbox-free; safe zone |
| **Glimmer Meadow** | 1–4 | Mossback Scrappers, Thornback Boars, glowmoss nodes, Stag Shrine |
| **Hollow Forest** | 4–7 | Mossback Shamans, Barrow Wisps, lost lanterns, shard #2 |
| **Ruined Hills** | 7–10 | Hollowed Footmen, world elite **Gristlehorn**, Barrow entrance |
| **Maldrek's Barrow** (instance) | 10 | 5-player dungeon (see §7) |

Roaming: each sub-area has 2–3 patrol paths plus static camps; respawn 60 s (quest mobs 30 s).

**World elite: Gristlehorn, the Moss Titan** (level 10 elite, Ruined Hills). Mossback warchief riding a moss-covered boulder beast. Mechanics: *Boulder Toss* (2 s cast, interruptible, ranged knockback), *Moss Slam* (frontal cone, telegraphed). Intended for 2–3 players; soloable by a level-10 player in Tier 2 gear with defensives. Drops a guaranteed Tier 2 piece.

## 4. Classes
All classes: 5 abilities, levels 1–10, one new ability at levels 1, 2, 4, 6, 10. GCD 1.5 s (haste floor 1.0 s). Max resources: Rage 100, Mana scales with Intellect, Focus 100.

| Ability slot | Warrior (tank, Rage) | Mage (ranged DPS, Mana) | Cleric (healer, Mana) | Ranger (DPS, Focus) |
|---|---|---|---|---|
| Builder (L1) | **Cleave Strike**: melee hit, +15 Rage, 1.5× threat | **Ember Bolt**: 1.8 s cast fire bolt, grants 1 *Kindling* (max 3) | **Radiant Smite**: 1.5 s cast holy damage, grants 1 *Devotion* (max 3) | **Quick Shot**: instant ranged shot, +10 Focus next 3 s (net builder) |
| Spender (L2) | **Shield Slam**: 30 Rage, heavy hit, 3× threat | **Crown Flare**: instant, consumes Kindling, damage ×(1+stacks) | **Sanctuary Pulse**: consumes Devotion, party heal radius 15 m, scaled by stacks | **Twin Fang**: 40 Focus, two melee strikes (works ranged-to-melee hybrid) |
| Utility (L4) | **Taunting Roar**: taunt target + nearby within 8 m, off-GCD, 10 s CD | **Frost Snare**: root 4 s (breaks on damage after 1 s), 15 s CD | **Purify**: remove 1 magic/curse debuff + small HoT, 8 s CD | **Snare Trap**: ground trap, roots first enemy 5 s, 20 s CD |
| Defensive / Heal (L6) | **Iron Wall**: −40% damage taken 6 s, off-GCD, 45 s CD | **Glimmer Ward**: absorb shield 25% max HP, 30 s CD | **Mending Light**: 2.0 s cast big single-target heal | **Fade Step**: backwards leap + 50% dodge 3 s, 25 s CD |
| Ultimate (L10) | **Bulwark of the Vale**: +30% max HP, threat ×2, 12 s, 120 s CD | **Starfall**: 3 s channel AoE 8 m, 90 s CD | **Dawn's Vigil**: party −50% damage taken 6 s, 120 s CD | **Stag Volley**: 6 s, Quick Shot is free and fires twice, 90 s CD |

Resources: Rage starts at 0, +rage on dealing/taking damage, decays 2/s out of combat. Mana regen from Spirit (5 s rule: reduced while casting). Focus regenerates 10/s.
Balance numbers live in `DT_Abilities` and `DT_ClassBaseStats`; the Content Designer owns tuning.

## 5. Combat rules
- **Targeting**: Tab cycles hostile targets within 40 m in a 90° cone in front of the camera, nearest first; click-select; F1 self, F2–F5 party members. *(ref)*
- **Range & LOS**: melee 5 m, ranged 30 m, heals 40 m; ranged/heals require line of sight (server trace).
- **Casting**: moving cancels cast-time abilities; interruptible mob casts show an orange bar, uninterruptible grey.
- **Threat**: damage 1.0 threat/point, healing 0.5 threat split across engaged mobs, ability multipliers in data; taunt = top threat + 10%; target switch at 110% (melee range) / 130% (ranged). *(ref)*
- **Death**: release to bind stone in Lanternrest, run back; no durability loss in the slice. In the dungeon, release to the dungeon entrance.
- **Floating combat text**, cooldown sweeps, GCD sweep, cast bars, target-of-target: see UI §10.

## 6. Mobs
| Mob | Tier | Level | Behaviour |
|---|---|---|---|
| Mossback Scrapper | Normal | 1–4 | Melee goblin, flees at 20% HP to pull a friend |
| Thornback Boar | Normal | 2–5 | Beast, *Charge* on aggro from 15 m |
| Mossback Shaman | Normal | 4–7 | Caster, 2 s *Bog Bolt* (interruptible), heals allies at 50% |
| Barrow Wisp | Normal | 5–8 | Floating, fast, low HP, drains mana |
| Hollowed Footman | Normal/Elite | 7–10 | Undead knight, shield block (frontal −50%), *Rend* bleed |
| Gristlehorn | World elite | 10 | See §3 |
Mob AI: Behavior Tree + perception (sight 20 m, aggro radius by level difference), leash at 40 m from spawn with full reset.

## 7. Dungeon: Maldrek's Barrow (5 players, level 10)
Layout: entrance hall → ossuary corridor → flooded crypt → Choir Hall → throne vault. Target clear time 20–25 min.

| Encounter | Content | Role check |
|---|---|---|
| Trash 1 | 3 Hollowed Footmen + 1 Barrow Wisp | Tank holds 3, DPS kill wisp first |
| **Mini-boss 1: Rotfang, Mossback Warchief** | *Sundering Blows* stacks −10% armor on tank (max 5, falls off after 10 s); at 70/40% calls 3 Scrapper adds | **Tank** uses Iron Wall at high stacks; **DPS** kill adds |
| Trash 2 | 2 Shamans + 2 Footmen | Interrupt Bog Bolt |
| **Mini-boss 2: Sister Morrow, Barrow Keeper** | *Grave Lament* 3 s cast (must be interrupted or deals party AoE); *Withering Mark* on random player (heavy DoT, dispellable) | **DPS** interrupt; **Healer** Purify |
| Trash 3 | Elite Footman pair | Crowd control one (Frost Snare / Snare Trap) |
| **Final boss: Maldrek, the Hollow King** | P1 (100–40%): *Hollow Cleave* frontal cone (tank faces away), *Grave Brand* on tank (absorbs 30% of healing, healer must over-heal), *Shard Pillars*: every 45 s three crown-shard pillars spawn, must be destroyed in 15 s or the boss heals 10%. P2 (<40%): *Crownfire* fire pools under random players, Cleave becomes faster. Enrage at 7 min | **Tank** facing, **Healer** Grave Brand, **DPS** pillars, **everyone** moves out of fire |

Solo / small group scaling: mob health × (0.3 + 0.175 × partySize), mob damage × (0.5 + 0.1 × partySize), capped at 1.0 for 5. Shard Pillars count = min(3, partySize).

Instancing: each party gets its own runtime-loaded level instance of `L_Barrow` on the same dedicated server, offset far below the world. (Engineers to verify the 5.8 API; see DATA_SCHEMAS §9.)

## 8. Quests (8)
| # | Quest | Giver | Objective | Reward |
|---|---|---|---|---|
| 1 | **A Fading Glow** | Wren | Gather 6 glowmoss clumps in Glimmer Meadow | XP, T1 boots |
| 2 | **Mossback Mischief** | Bram | Defeat 8 Mossback Scrappers | XP, gold |
| 3 | **The First Shard** | Wren | Recover shard from the Scrapper camp chest (guarded by 4 Scrappers) | XP, T1 piece (roll) |
| 4 | **Lanterns in the Hollow** | Ysolde | Relight 4 lost lanterns in Hollow Forest (interact, each guarded) | XP, T1 weapon |
| 5 | **The Second Shard** | Ysolde | Defeat Shaman Elder Hask (named, Hollow Forest) | XP, T2 piece |
| 6 | **Trust of the Dawnstag** | Dawnstag spirit | Bring the two shards and 3 glowmoss to the Stag Shrine, defend the shrine from 3 waves of wisps | **Dawnstag mount**, XP |
| 7 | **The Hollow Patrol** | Cael | Defeat 6 Hollowed Footmen in Ruined Hills | XP, T2 piece |
| 8 | **The Titan of the Hills** *(elite)* | Cael | Defeat Gristlehorn; take the third shard | XP, T2 piece |
| 9 *(dungeon)* | **Relight the Ember Crown** | Wren | Defeat Maldrek in the Barrow and use the reforged crown | T3 piece, world-state glow |

Leveling curve: ~1 level per quest plus kills; level 10 reached around quest 8. XP table in `DT_XPCurve`.

## 9. Gear and loot
3 sets × 4 armor slots + class weapon. Stats randomized per drop (full spec: DATA_SCHEMAS §4).

| Set | Tier | Item level | Sources | 2-piece | 4-piece |
|---|---|---|---|---|---|
| Wanderer's | T1 | 5–8 | Zone mobs, quests 1–4 | +5% movement speed out of combat | +3% all primary stats |
| Vale Guard | T2 | 9–12 | Quests 5–8, Gristlehorn, dungeon trash | +5% max HP | Defensive ability CD −15% |
| Hollow Crown | T3 | 14–16 | Dungeon bosses only (Epic possible) | +5% crit | Ultimate deals/heals +20% and glows (VFX) |

Rarity: Common (1 stat), Uncommon (2), Rare (3), Epic (4, dungeon only). Drop weights per loot table. Personal loot (each player rolls their own drop, server-side).

## 10. UI (*(ref)* for exact layout)
HUD: player frame (top-left), target + target-of-target frames, party frames (left), 10-slot action bar (bottom-center, keys 1–0), cast bar, XP bar, minimap (top-right), quest tracker (right), chat box (bottom-left, party + say only), floating combat text, nameplates.
Windows: Bags (B, 20 slots), Character (C, paper doll + stats + set bonuses), Quest log (L), Ability book (P), Escape menu. Tooltips color stat lines by item rarity (white / green / blue / purple).

## 11. Mount: the Dawnstag
Earned from quest 6 (around level 6). Summon: 1.5 s cast (interrupted by movement or damage), +60% run speed. Dismount on taking damage, on using any ability, or on entering the dungeon. Cannot attack or cast while mounted. Rider attaches to the `mount_seat` socket; the rider plays a seated pose.

## 12. Multiplayer
Server-authoritative UE replication, dedicated server + ≥2 clients. Party of up to 5 (invite/accept/leave/kick). No database: character data saved to `Saved/Characters/<AccountName>_<CharName>.json` on the server (level, XP, class, position, inventory item instances, equipment, quest state, mount owned).

## 13. Out of scope (stretch list)
Talents, professions, trade/auction, mail, guilds, PvP, more zones, flying, raid frames, loot rolling, voice chat, real account login, persistence DB, server meshing.
