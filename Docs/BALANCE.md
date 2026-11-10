# Balance model (vertical slice, levels 1-10)

All numbers live in `Data/Source/*.json`. This page explains how they were derived so they can be retuned. Conventions below marked (P) are proposals the engineers must confirm; the schemas do not define them.

## 1. Formulas (P)
- Player stats: `stat(L) = a + b*(L-1)` per class (see `DT_ClassBaseStats`). Primary stat goes 12/14 -> ~102/104 (L1 -> L10), so damage grows about 10x over ten levels, matching mob health growth.
- MaxHealth = BaseHealth + 10 x Stamina (base + gear). MaxMana = BaseResourceMax (already includes 100 + 5 x base Intellect) + 5 x gear Intellect. Rage/Focus max 100. Rage regen -2 = decay per second out of combat.
- Ability magnitude (class abilities) = BaseMagnitude + stat x Coefficient. Combo spenders multiply by (1 + stacks consumed).
- Mana regen per second = BaseResourceRegen + 0.04 x Spirit; halved while casting or within 5 s of a cast.
- Mob health = BaseHealth + HealthPerLevel x (Level - LevelMin). Mob hit damage = BaseDamage x (1 + 0.12 x (Level - LevelMin)). Mob XP = XPReward x (1 + 0.15 x (Level - LevelMin)).
- Mob abilities (ClassId MOB): damage `BaseMagnitude` = multiple of the caster's current hit damage; heal `BaseMagnitude` = fraction of target max health; effect magnitudes of the form 0.25 are multiples of hit damage per tick when the effect is a DoT.
- Armor mitigation = Armor / (Armor + 25 x attackerLevel + 100) (level 10: A / (A + 350)). Crit chance = 5% + Crit/3 (%); Haste% = Haste/3 (GCD floor 1.0 s); crit multiplier 1.5x.
- Party scaling (GDD section 7, applies to every mob with PartyScaling true): health x (0.3 + 0.175 N), damage x (0.5 + 0.1 N), capped at 1.0 at N = 5.
- Vendor buy price = 4 x SellValue. A set counts every equipped piece of the tier, weapon included (max 5).
- Loot: `GuaranteedDrops` items always drop, then one extra roll at `DropChance`; each roll picks an entry by weight, a rarity by RarityWeights, an item level in range. Entries are listed for all classes, so the roller should filter by the looter's AllowedClasses.

## 2. Time to kill, solo, at level (target 10-15 s)
Player damage per second (rotation, no gear) at L1/L4/L7/L10: Warrior 8/22/37/51, Mage 10/26/41/56, Cleric 9/23/38/52, Ranger 10/25/39/54. Mob health is set to about 12 s of that DPS.

| Mob (level) | Health | Solo TTK | Mob DPS vs player |
|---|---|---|---|
| Scrapper (1 / 4) | 70 / 310 | 8 / 12-14 s | 3.5 / 4.8 |
| Boar (2 / 5) | 160 / 415 | 13 s | 4.5 / 6.7 |
| Shaman (4 / 7) | 320 / 575 | 13-14 s | about 5 + Bog Bolt |
| Wisp (5 / 8) | 210 / 360 | 7-9 s (fragile by design) | 5 |
| Footman (7 / 10) | 560 / 815 | 12-15 s | 10 / 14 |
| Hask (L6 elite) | 1500 | about 40 s | about 8 |
| Gristlehorn (solo scaling 0.475 x health, 0.6 x damage) | 2850 | about 45-55 s | about 26 |

A level-10 player in T1 gear (about +12 main stat) is roughly +12% DPS; T2 +30%; T3 +50-55% (Mage 56 -> 62 / 70 / 81 DPS; Warrior 51 -> 57 / 65 / 76).

## 3. Role throughput at level 10 (T2 gear unless noted)
- Tank (Warrior): about 65 DPS, 1480 health naked (about 1750 in T2), armor about 105 + gear (about 25-30% mitigation). Iron Wall -40% for 6 s every 45 s; Bulwark +30% health for 12 s every 120 s.
- Healer (Cleric): Mending Light 346 per 2.0 s (173 HPS, 30 mana); Sanctuary Pulse 57 x (1 + 3) = 228 per member per GCD; Smite about 52 DPS when idle. Mana pool about 510 (naked) with 14/s regen (7/s while casting): sustains about 70 s of continuous Mending, indefinite at about 50% uptime.
- DPS (Mage/Ranger): about 70 each in T2 (Mage 3 Ember Bolts + Crown Flare at 4 Kindling-multiplier; Ranger 2 Quick Shot + Twin Fang). Starfall 102 x 6 ticks = 610 per target. Group effective DPS (tank + healer idle + 3 DPS, 70% uptime) is about 210.

## 4. Bosses (5 players, scaling 1.0) and enrage math
| Boss | Health | Intended duration | Notes |
|---|---|---|---|
| Rotfang | 18,000 | about 85 s | hit 70 (x1.4 Sundering Blows = 98, 5 stacks = -50% armor); 3 adds at 70% and 40%, 450 health each, 24 damage |
| Sister Morrow | 24,000 | about 115 s | Grave Lament 4x hit (220) to everyone unless interrupted (3 s cast, every 20 s); Withering Mark 0.6x hit per 2 s for 12 s |
| Maldrek | 54,000 | about 260 s (4.3 min) | enrage 420 s: 1.6x margin over expected 210 DPS |
| Gristlehorn (3 players, 0.825 x) | 4950 | about 25 s | Moss Slam 150, Boulder Toss 90 |

Maldrek damage: hit 80 (tank, 25-30% mitigated: about 55 DPS incl. cleave 256 every 8 s). Grave Brand removes 30% of healing for 12 s, so the healer needs about 85 effective HPS on the tank (one Mending Light per 4 s covers it). Shard pillars: 1300 health x 3 = 3900, must die in 15 s = 260 DPS on the pillars (the whole group's effective DPS: it is tight by design); a miss heals the boss 10% (5400, about 26 s of group DPS). Phase 2 (below 40%): Crownfire 80 + 64 per second in pools, faster Cleave (every 5.5 s).
Enrage (+200% damage) is a hard check: required group DPS = Health x scale / 420 s.

Solo / small groups against Maldrek (health scale 0.475 / 0.65 / 0.825 / 1.0 for N = 1 / 2 / 3 / 4 and 5; the cap is reached at 4 players):
| Party | Boss health | DPS needed for enrage | Damage scale |
|---|---|---|---|
| 1 | 25,650 | 61 | 0.6 |
| 2 | 35,100 | 84 | 0.7 |
| 3 | 44,550 | 106 | 0.8 |
| 5 | 54,000 | 129 | 1.0 |
Solo is therefore possible only with strong uptime and T2 Rare gear, as the GDD intends ("hard but not forbidden").

Trash (5 players): Trash 1 4,800 health, Trash 2 5,000, Trash 3 6,400 (2 Captains, 3200 each), about 20-30 s each. Estimated dungeon time: 3 trash blocks plus connectors about 10 min, bosses about 8 min, walking and recovery 4-6 min = 20-25 min.

## 5. Item stats
Roll value = round(Uniform(MinPerIlvl, MaxPerIlvl) x ItemLevel x RarityMultiplier) (min 1), with the slot budget Head 0.8, Chest 1.0, Legs 0.9, Boots 0.6, Weapon 1.3. Primary range 0.50-0.75 per item level times budget; Crit/Haste x0.5; Armor x1.6.

Chest primary stat (one roll), by tier and rarity:
| Tier (item level) | Common | Uncommon | Rare | Epic |
|---|---|---|---|---|
| T1 (5-8) | 2-6 | 3-7 | 3-8 | 4-9 |
| T2 (9-12) | 4-9 | 5-10 | 6-11 | 7-13 |
| T3 (14-16) | 7-12 | 8-13 | 9-15 | 10-17 |
Other slots scale by their budget. BaseArmor = tier base (12 / 24 / 38) x family (Plate 1.0, Leather 0.6, Robe 0.3) x slot (Head 1.0, Chest 1.4, Legs 1.2, Boots 0.8); Plate T2 full set = 105. BaseWeaponDamage 12 / 24 / 40 (tooltip only in v1).

Expected five-piece totals (simulated, typical rarity mix T1 Common/Uncommon, T2 Uncommon/Rare, T3 Rare/Epic): main stat about +12 / +29 / +51, Stamina +6-11 / +17-27 / +35-50 (Plate highest). Level-10 main stat 102 naked becomes about 115 / 131 / 154. Set bonuses: T1 4-piece +3% primaries is about +4; T2 +5% health about +90; T3 +5% crit.

## 6. Leveling pace
XPToNext: 120, 200, 300, 420, 560, 720, 900, 1100, 1320 (total 5640 to level 10). Quest XP 150 / 220 / 330 / 450 / 580 / 700 / 850 / 1000 (total 4280), about 1400-1500 from kills (about 45 kills). Simulated cumulative XP after quests 1-8: 182, 466, 904, 1474, 2194, 3074, 4092, 5862: levels 2, 3, 4, 5, 6, 7, 8, 10. Level 10 arrives with quest 8 as intended. Quest 9 gives no XP (cap).

## 7. Tuning knobs
Mob difficulty: BaseHealth/HealthPerLevel (target = 12 s x player DPS at that level). Player power: Coefficient per ability and the (a, b) stat growth in `DT_ClassBaseStats`. Dungeon: boss BaseHealth, enrage time, pillar BaseHealth and interval. Gear: pool Min/MaxPerIlvl and rarity weights in loot tables.

## 8. Effect classes referenced (Combat Engineer to provide)
All under `/Script/ThistlewickVale.`: GE_TV_Taunt, DamageTakenMod, MaxHealthMod, ThreatMod, Root, Absorb, ChannelTick, Dispel, HoT, PartyDamageTakenMod, FocusGain, MultiHit, TrapZone, DodgeMod, StagVolleyBuff, SocialAggro, Stun, ManaDrain, FrontalDamageTakenMod, DotBleed, DotMagic, Knockback, ArmorShred, StackLimit, SummonAdds, HealingReceivedMod, GroundZoneDot, Enrage; set bonuses GE_TV_Set_MoveSpeedOutOfCombat, PrimaryStatPct, MaxHealthPct, DefensiveCooldownReduction, CritChance, UltimatePower. SummonAdds uses SetByCallerTag `TV.Spawn.<MOB_ID>` and Magnitude = count (capped by party size for pillars).
