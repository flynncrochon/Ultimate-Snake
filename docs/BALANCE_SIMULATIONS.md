# Simulated weapon DPS and kills

Measured 2026-10-26 using the real C++17 combat simulation at 60 Hz. 184 fixed loadouts × 3 scenarios × 5 seeds = **2,760 fights, 61.3 simulated combat hours**. These are executed fights, including projectile travel, misses, retargeting, area damage, summons, splitting, slows, knockback, and executions.

In the late moving fixture, the highest mean single was **Hydra Brood** (38,500 effective DPS) and the highest mean pair was **Hydra Brood + Ancient Brood Pit** (39,188 effective DPS). Use the scenario-specific tables below: target supply and movement can change the ranking.

## Reading the numbers

Every result cell is **effective DPS ± sample standard deviation / kills ± sample standard deviation**, over 5 matched seeds. Kills are the mean **total for that column's fight duration**, not kills per second. Fractional kill counts come from averaging integer results. To get kills/minute, multiply kills by 60/fight seconds. The ± values describe seed variation, not confidence intervals.

Effective DPS = (all HP supplied − HP still alive) / exact simulated seconds. Each target's contribution stops at zero HP: overkill is excluded, while partial damage to surviving enemies is included. Native splitter children add their actual max HP to the supply ledger after they spawn. Reckoning's devour counts the victim's remaining HP; it is an execute, so this metric must not be read as its raw hit damage. Damage is the whole loadout's observed output; a pair is simulated together, never calculated by adding single-weapon DPS.

## Scenarios and fixed assumptions

| Fixture | Fight length | Starting run clock / player level | Player movement |
|---|---:|---|---|
| Early director, moving square route | 120 s | 0 s / level 5 | Square route; turn every 150 ticks (2.5 s) |
| Late director, moving square route | 60 s | 840 s / level 40 | Square route; turn every 150 ticks (2.5 s) |
| Late director, stationary east-facing defense | 60 s | 840 s / level 40 | Stationary; normal facing |

- All builds use **Cobra** and its native ×1.15 direct-damage bonus. Indirect poison and nest bites keep their normal exclusions. Character comparisons are outside this matrix.
- Base singles test ranks **1, 4, and 8**, with no passives. Pairs use rank-8 weapons. Evolutions replace the base family and include their required recipe passive at **rank 1**. Evolved pairs own the union of those recipe passives, which affect the whole build. Thus pair gains include shared passive effects as well as weapon interaction; these tables do not isolate additive synergy or equal upgrade cost.
- A separate unarmed native world records the actual director's timed red, swift, and splitter spawns. That schedule is reused for every loadout of the same scenario and seed. Enemy HP, speed, radius, and bodies come from that native world; weapon RNG cannot change the supplied wave schedule. Real splitter children still spawn inside each tested fight.
- Fights begin with no enemies or lingering weapon actors, and attacks ready to fire. No artificial prewarming. Enemy pursuit, target recycling, actor caps, pickup behavior, and contact displacement stay active. No adaptive steering.
- The player uses a large HP pool with **normal contact damage enabled**, preserving hit cadence and knockback. XP is collected, but a huge next-level threshold fixes level and equipment. This measures offensive output, not survival probability or a human's optimal play.
- **Boss spawns and chests are excluded** to prevent random evolution/rank changes. Results therefore describe swarm combat, not isolated boss DPS, boss time-to-kill, or a complete 20-minute progression run. Four example full loadouts are included; the remaining three/four-weapon space, every passive investment, other characters, and optimized routes need separate simulations.
- Finite target supply limits measured DPS. A strong build can run out of reachable enemies; a weak one builds a denser crowd. High area DPS and kills reflect that actual crowd interaction. Neither these measurements nor simulation speed establish rendering FPS.

## Balance signals from these fights

- **Evolution payoff:** Singularity Mortar changes effective DPS by +4% relative to rank-8 Plague Mortar in late moving combat. The separate [evolution upgrade checks](EVOLUTION_UPGRADES.md) compare identical passive investment and include durable boss targets.
- **Damage versus finishes:** Serpent's Gatling improves late moving effective DPS by 308% over rank-8 Viper Fangs; mean kills change from 63.4 to 337.2. Partial damage and kills tell different balance stories.
- **Setup and seed sensitivity:** Ancient Brood Pit ranges from 17,638 to 36,010 late moving DPS. Its first 15s averages 2,703 DPS, versus 33,817 later. Setup deserves separate attention from mature output.
- **Crowd specialization:** Rank-8 Plague Mortar averages 34,554 late moving DPS versus 722 for Sidewinder Beam. AoE repeatedly benefits from accumulated crowds; use the separate matched boss checks to assess single-target evolution payoff.

## Base weapons at ranks 1, 4, and 8

| Loadout | Early moving (120s) | Late moving (60s) | Late hold (60s) |
|---|---|---|---|
| Venom Shot (rank 1) | 307 ± 11 / 106.6 ± 6.2 | 964 ± 30 / 0.2 ± 0.4 | 1,195 ± 20 / 39.0 ± 2.2 |
| Venom Shot (rank 4) | 452 ± 16 / 186.4 ± 8.4 | 2,539 ± 92 / 20.2 ± 5.4 | 3,088 ± 30 / 101.4 ± 2.6 |
| Venom Shot (rank 8) | 674 ± 25 / 300.6 ± 10.3 | 6,805 ± 186 / 97.2 ± 4.9 | 8,730 ± 34 / 290.2 ± 6.8 |
| Viper Fangs (rank 1) | 62 ± 1 / 20.6 ± 1.8 | 69 ± 0 / 0.0 ± 0.0 | 65 ± 1 / 1.8 ± 0.8 |
| Viper Fangs (rank 4) | 417 ± 4 / 177.4 ± 3.8 | 776 ± 10 / 3.4 ± 1.9 | 724 ± 4 / 23.8 ± 0.4 |
| Viper Fangs (rank 8) | 703 ± 28 / 325.2 ± 9.9 | 3,800 ± 15 / 63.4 ± 9.2 | 3,488 ± 35 / 114.0 ± 2.8 |
| Plague Mortar (rank 1) | 494 ± 25 / 220.4 ± 11.9 | 3,976 ± 822 / 3.2 ± 1.8 | 33,592 ± 208 / 1038.6 ± 7.2 |
| Plague Mortar (rank 4) | 624 ± 27 / 293.0 ± 10.0 | 26,914 ± 847 / 628.4 ± 38.8 | 35,465 ± 215 / 1142.0 ± 15.0 |
| Plague Mortar (rank 8) | 689 ± 29 / 320.4 ± 9.6 | 34,554 ± 831 / 1096.0 ± 45.8 | 36,329 ± 309 / 1179.2 ± 17.9 |
| Snake Nest (rank 1) | 542 ± 9 / 255.4 ± 3.0 | 2,183 ± 69 / 6.0 ± 0.7 | 15,880 ± 1,979 / 148.4 ± 42.0 |
| Snake Nest (rank 4) | 683 ± 28 / 317.2 ± 9.0 | 5,937 ± 199 / 82.6 ± 9.3 | 27,125 ± 1,807 / 738.6 ± 90.0 |
| Snake Nest (rank 8) | 733 ± 32 / 337.6 ± 12.1 | 23,968 ± 799 / 668.6 ± 20.3 | 35,453 ± 244 / 1147.0 ± 11.9 |
| Venom Nova (rank 1) | 22 ± 3 / 0.0 ± 0.0 | 150 ± 11 / 0.0 ± 0.0 | 14,190 ± 30 / 116.0 ± 10.0 |
| Venom Nova (rank 4) | 118 ± 16 / 22.2 ± 2.2 | 692 ± 46 / 0.0 ± 0.0 | 27,595 ± 231 / 705.8 ± 6.7 |
| Venom Nova (rank 8) | 481 ± 10 / 186.6 ± 5.6 | 6,475 ± 174 / 22.4 ± 3.0 | 33,802 ± 186 / 1036.8 ± 8.7 |
| Sidewinder Beam (rank 1) | 38 ± 1 / 7.2 ± 1.1 | 40 ± 1 / 0.0 ± 0.0 | 40 ± 0 / 0.6 ± 0.9 |
| Sidewinder Beam (rank 4) | 170 ± 3 / 82.0 ± 2.8 | 214 ± 2 / 0.0 ± 0.0 | 206 ± 2 / 5.8 ± 0.8 |
| Sidewinder Beam (rank 8) | 469 ± 7 / 217.2 ± 4.1 | 722 ± 5 / 8.6 ± 2.1 | 705 ± 5 / 22.4 ± 1.1 |
| Ricochet Fang (rank 1) | 143 ± 5 / 54.6 ± 4.9 | 228 ± 4 / 0.0 ± 0.0 | 218 ± 6 / 5.2 ± 0.8 |
| Ricochet Fang (rank 4) | 313 ± 10 / 143.4 ± 6.9 | 1,191 ± 9 / 0.4 ± 0.5 | 1,048 ± 41 / 32.8 ± 2.0 |
| Ricochet Fang (rank 8) | 612 ± 23 / 290.0 ± 8.0 | 4,524 ± 110 / 55.4 ± 6.9 | 4,006 ± 60 / 129.4 ± 6.4 |
| Cobra Pit (rank 1) | 149 ± 4 / 47.8 ± 4.4 | 192 ± 2 / 0.0 ± 0.0 | 192 ± 2 / 0.4 ± 0.5 |
| Cobra Pit (rank 4) | 467 ± 21 / 199.4 ± 6.8 | 1,050 ± 10 / 2.6 ± 1.1 | 1,048 ± 4 / 5.8 ± 0.8 |
| Cobra Pit (rank 8) | 658 ± 23 / 303.2 ± 8.6 | 4,104 ± 59 / 49.6 ± 5.4 | 4,146 ± 28 / 44.4 ± 3.7 |
| Tongue Lash (rank 1) | 35 ± 4 / 10.2 ± 1.8 | 91 ± 19 / 0.0 ± 0.0 | 573 ± 41 / 0.0 ± 0.0 |
| Tongue Lash (rank 4) | 74 ± 17 / 29.8 ± 5.9 | 606 ± 85 / 0.0 ± 0.0 | 4,565 ± 305 / 64.4 ± 7.0 |
| Tongue Lash (rank 8) | 234 ± 35 / 99.0 ± 11.7 | 3,690 ± 145 / 8.0 ± 3.2 | 12,003 ± 307 / 363.0 ± 4.1 |

## Evolutions with their recipe passive

| Loadout | Early moving (120s) | Late moving (60s) | Late hold (60s) | Recipe passive(s), rank 1 |
|---|---|---|---|---|
| Ouroboros | 724 ± 35 / 334.2 ± 12.5 | 36,528 ± 484 / 1203.4 ± 30.2 | 37,438 ± 407 / 1219.6 ± 22.0 | serpents scales |
| Serpent's Gatling | 725 ± 33 / 333.6 ± 11.2 | 15,502 ± 126 / 337.2 ± 18.9 | 14,928 ± 93 / 493.4 ± 9.7 | rapid fire |
| Singularity Mortar | 704 ± 30 / 326.6 ± 10.5 | 36,023 ± 555 / 1188.0 ± 27.0 | 36,852 ± 343 / 1195.0 ± 17.6 | graviton |
| Hydra Brood | 761 ± 19 / 348.4 ± 7.0 | 38,500 ± 576 / 1270.2 ± 30.5 | 38,854 ± 657 / 1269.8 ± 30.4 | coiled volley |
| Miasma | 716 ± 30 / 330.2 ± 10.1 | 35,216 ± 419 / 1109.6 ± 25.7 | 36,580 ± 330 / 1176.0 ± 16.8 | toxic expanse |
| Consumption Beam | 631 ± 20 / 269.4 ± 3.6 | 2,053 ± 10 / 61.8 ± 1.8 | 2,026 ± 12 / 61.8 ± 1.8 | ravenous maw |
| Shatter Fang | 700 ± 23 / 324.8 ± 7.9 | 17,835 ± 2,482 / 476.4 ± 101.9 | 16,463 ± 1,178 / 542.8 ± 43.6 | dead eye |
| Ancient Brood Pit | 748 ± 39 / 343.2 ± 14.4 | 26,038 ± 7,454 / 753.8 ± 340.3 | 37,708 ± 405 / 1233.8 ± 23.0 | chronofield |
| Serpent's Reckoning | 664 ± 36 / 301.6 ± 12.9 | 34,844 ± 487 / 1126.8 ± 19.9 | 35,974 ± 331 / 1162.4 ± 16.0 | serpents reach |

### Evolution change relative to rank-8 base

Observed change in effective DPS with the same fixture and seeds. The evolved build also gains its rank-1 recipe passive; the base has no passive. Negative values flag a weaker evolution in this specific swarm fixture, not a universal downgrade or an isolated measurement of the evolution mechanic.

| Evolution | Early moving | Late moving | Late hold |
|---|---:|---:|---:|
| Ouroboros | +8% | +437% | +329% |
| Serpent's Gatling | +3% | +308% | +328% |
| Singularity Mortar | +2% | +4% | +1% |
| Hydra Brood | +4% | +61% | +10% |
| Miasma | +49% | +444% | +8% |
| Consumption Beam | +35% | +184% | +187% |
| Shatter Fang | +14% | +294% | +311% |
| Ancient Brood Pit | +14% | +534% | +809% |
| Serpent's Reckoning | +183% | +844% | +200% |

## Startup versus later output

Late moving fight only: the first 15s includes travel and setup; the remaining 45s retains every actor and accumulated enemy. Later output includes density changes, so this is not a target-independent sustained-DPS estimate.

| Rank-8 weapon / evolution | First-window DPS | Later-window DPS | Total DPS seed range | Total kill seed range |
|---|---:|---:|---:|---:|
| Venom Shot (rank 8) | 5,475 | 7,248 | 6,582–7,091 | 90–103 |
| Ouroboros | 24,000 | 40,705 | 35,987–37,108 | 1173–1245 |
| Viper Fangs (rank 8) | 3,514 | 3,895 | 3,776–3,816 | 50–74 |
| Serpent's Gatling | 12,674 | 16,445 | 15,319–15,652 | 319–368 |
| Plague Mortar (rank 8) | 17,075 | 40,381 | 33,592–35,398 | 1036–1151 |
| Singularity Mortar | 21,279 | 40,937 | 35,503–36,767 | 1160–1227 |
| Snake Nest (rank 8) | 11,300 | 28,190 | 23,412–25,358 | 641–696 |
| Hydra Brood | 31,677 | 40,775 | 37,639–39,092 | 1229–1314 |
| Venom Nova (rank 8) | 3,436 | 7,488 | 6,267–6,744 | 19–26 |
| Miasma | 19,697 | 40,389 | 34,759–35,844 | 1083–1142 |
| Sidewinder Beam (rank 8) | 679 | 737 | 714–729 | 7–12 |
| Consumption Beam | 1,912 | 2,100 | 2,041–2,066 | 59–63 |
| Ricochet Fang (rank 8) | 3,824 | 4,757 | 4,359–4,622 | 44–61 |
| Shatter Fang | 13,057 | 19,428 | 15,839–21,633 | 401–642 |
| Cobra Pit (rank 8) | 3,043 | 4,457 | 4,047–4,172 | 41–55 |
| Ancient Brood Pit | 2,703 | 33,817 | 17,638–36,010 | 300–1191 |
| Tongue Lash (rank 8) | 2,458 | 4,101 | 3,435–3,781 | 3–11 |
| Serpent's Reckoning | 18,694 | 40,227 | 34,173–35,394 | 1097–1145 |

## Every legal two-weapon combo

All 144 pairs of different families are below: 36 base/base, 36 evolution/evolution, and 72 mixed. A base weapon and its own evolution cannot coexist. Ordering is by measured late moving DPS within each group; the native equipment/update order follows catalog order and is held constant across seeds.

### Base + base (36)

| Loadout | Early moving (120s) | Late moving (60s) | Late hold (60s) |
|---|---|---|---|
| Venom Shot + Plague Mortar | 726 ± 27 / 333.4 ± 9.6 | 35,275 ± 703 / 1124.4 ± 29.9 | 36,506 ± 228 / 1184.0 ± 15.0 |
| Plague Mortar + Venom Nova | 701 ± 35 / 322.4 ± 12.6 | 35,120 ± 654 / 1109.6 ± 30.0 | 36,428 ± 254 / 1184.2 ± 13.0 |
| Plague Mortar + Snake Nest | 750 ± 28 / 344.0 ± 10.1 | 35,042 ± 726 / 1124.2 ± 30.6 | 36,568 ± 305 / 1185.8 ± 16.0 |
| Plague Mortar + Cobra Pit | 721 ± 25 / 330.6 ± 9.8 | 34,993 ± 641 / 1113.2 ± 33.2 | 36,416 ± 266 / 1183.0 ± 15.3 |
| Viper Fangs + Plague Mortar | 718 ± 33 / 331.2 ± 11.9 | 34,956 ± 686 / 1112.6 ± 35.0 | 36,395 ± 268 / 1181.4 ± 16.2 |
| Plague Mortar + Ricochet Fang | 699 ± 32 / 324.8 ± 12.0 | 34,938 ± 583 / 1112.4 ± 23.6 | 36,422 ± 293 / 1181.8 ± 15.7 |
| Plague Mortar + Tongue Lash | 697 ± 33 / 323.4 ± 11.5 | 34,877 ± 625 / 1102.6 ± 29.6 | 36,368 ± 273 / 1177.6 ± 15.5 |
| Plague Mortar + Sidewinder Beam | 705 ± 31 / 326.8 ± 11.7 | 34,698 ± 672 / 1100.6 ± 29.8 | 36,333 ± 270 / 1178.6 ± 15.5 |
| Venom Shot + Snake Nest | 757 ± 20 / 345.8 ± 7.5 | 28,475 ± 654 / 839.8 ± 21.6 | 35,445 ± 238 / 1148.8 ± 10.2 |
| Snake Nest + Venom Nova | 738 ± 24 / 338.4 ± 9.3 | 27,287 ± 651 / 757.2 ± 38.9 | 35,653 ± 258 / 1152.4 ± 14.2 |
| Snake Nest + Cobra Pit | 749 ± 26 / 343.4 ± 9.4 | 26,615 ± 580 / 759.4 ± 30.2 | 35,467 ± 206 / 1145.0 ± 17.6 |
| Snake Nest + Ricochet Fang | 744 ± 17 / 341.8 ± 6.4 | 26,555 ± 728 / 764.8 ± 27.0 | 35,431 ± 307 / 1148.2 ± 18.8 |
| Viper Fangs + Snake Nest | 736 ± 14 / 338.6 ± 4.8 | 26,140 ± 690 / 747.2 ± 28.5 | 35,358 ± 219 / 1146.0 ± 12.4 |
| Snake Nest + Tongue Lash | 738 ± 21 / 339.8 ± 8.5 | 26,024 ± 523 / 729.4 ± 18.4 | 35,651 ± 239 / 1153.2 ± 15.8 |
| Snake Nest + Sidewinder Beam | 738 ± 26 / 339.0 ± 10.5 | 23,597 ± 593 / 651.4 ± 14.9 | 35,410 ± 218 / 1146.2 ± 17.3 |
| Venom Shot + Venom Nova | 705 ± 24 / 323.0 ± 9.0 | 13,223 ± 308 / 194.6 ± 9.4 | 34,045 ± 205 / 1066.8 ± 10.8 |
| Venom Shot + Ricochet Fang | 723 ± 28 / 332.4 ± 10.1 | 10,986 ± 76 / 197.2 ± 13.3 | 13,002 ± 57 / 429.8 ± 7.8 |
| Venom Shot + Cobra Pit | 726 ± 32 / 333.0 ± 11.9 | 10,794 ± 120 / 169.6 ± 5.9 | 12,935 ± 42 / 364.2 ± 12.3 |
| Venom Nova + Cobra Pit | 691 ± 27 / 311.8 ± 9.7 | 10,712 ± 195 / 119.8 ± 11.2 | 34,177 ± 190 / 1056.4 ± 8.8 |
| Venom Nova + Ricochet Fang | 667 ± 22 / 305.8 ± 6.7 | 10,578 ± 234 / 131.6 ± 7.8 | 33,824 ± 189 / 1047.6 ± 11.7 |
| Venom Shot + Tongue Lash | 690 ± 30 / 311.4 ± 12.5 | 10,398 ± 109 / 168.2 ± 7.4 | 20,142 ± 168 / 647.8 ± 10.0 |
| Venom Shot + Viper Fangs | 725 ± 30 / 333.6 ± 10.7 | 10,361 ± 114 / 190.8 ± 6.0 | 12,064 ± 103 / 399.4 ± 9.1 |
| Venom Nova + Tongue Lash | 542 ± 34 / 232.2 ± 14.0 | 10,359 ± 121 / 85.0 ± 7.6 | 34,465 ± 327 / 1071.6 ± 11.4 |
| Viper Fangs + Venom Nova | 703 ± 28 / 325.8 ± 10.4 | 9,929 ± 193 / 114.6 ± 6.8 | 33,851 ± 198 / 1050.6 ± 8.0 |
| Ricochet Fang + Cobra Pit | 704 ± 36 / 324.8 ± 12.1 | 8,505 ± 117 / 126.2 ± 11.0 | 8,005 ± 52 / 181.4 ± 9.1 |
| Ricochet Fang + Tongue Lash | 670 ± 24 / 313.4 ± 8.2 | 8,099 ± 169 / 86.8 ± 4.0 | 15,505 ± 170 / 481.6 ± 12.0 |
| Viper Fangs + Ricochet Fang | 709 ± 32 / 328.0 ± 11.6 | 8,051 ± 85 / 148.2 ± 8.7 | 7,535 ± 40 / 248.6 ± 9.4 |
| Viper Fangs + Cobra Pit | 712 ± 26 / 328.8 ± 11.4 | 7,873 ± 34 / 125.8 ± 4.4 | 7,551 ± 30 / 165.2 ± 4.8 |
| Cobra Pit + Tongue Lash | 677 ± 24 / 311.4 ± 6.2 | 7,740 ± 115 / 75.4 ± 4.7 | 15,472 ± 360 / 420.6 ± 8.4 |
| Venom Shot + Sidewinder Beam | 727 ± 32 / 333.8 ± 11.0 | 7,631 ± 234 / 125.4 ± 10.7 | 9,360 ± 76 / 310.4 ± 7.0 |
| Viper Fangs + Tongue Lash | 705 ± 29 / 326.4 ± 10.3 | 7,464 ± 221 / 92.6 ± 4.7 | 15,480 ± 353 / 484.4 ± 11.2 |
| Venom Nova + Sidewinder Beam | 648 ± 20 / 296.6 ± 7.9 | 7,172 ± 137 / 38.4 ± 7.8 | 33,840 ± 192 / 1040.0 ± 8.8 |
| Sidewinder Beam + Ricochet Fang | 682 ± 25 / 317.8 ± 9.0 | 5,208 ± 83 / 75.8 ± 5.0 | 4,713 ± 58 / 151.6 ± 4.7 |
| Sidewinder Beam + Cobra Pit | 694 ± 32 / 320.4 ± 12.1 | 4,821 ± 50 / 64.6 ± 5.3 | 4,825 ± 26 / 64.0 ± 5.7 |
| Sidewinder Beam + Tongue Lash | 608 ± 15 / 283.8 ± 6.2 | 4,634 ± 182 / 30.6 ± 4.0 | 12,222 ± 226 / 372.2 ± 5.4 |
| Viper Fangs + Sidewinder Beam | 707 ± 26 / 327.0 ± 9.1 | 4,490 ± 17 / 80.2 ± 3.8 | 4,156 ± 26 / 133.4 ± 3.0 |

### Evolution + evolution (36)

| Loadout | Early moving (120s) | Late moving (60s) | Late hold (60s) |
|---|---|---|---|
| Hydra Brood + Ancient Brood Pit | 763 ± 16 / 349.4 ± 6.1 | 39,188 ± 873 / 1293.2 ± 34.3 | 39,513 ± 470 / 1291.2 ± 25.4 |
| Ouroboros + Hydra Brood | 763 ± 20 / 349.0 ± 7.5 | 39,008 ± 680 / 1287.6 ± 33.1 | 39,143 ± 220 / 1278.0 ± 16.3 |
| Serpent's Gatling + Hydra Brood | 766 ± 21 / 349.6 ± 8.4 | 38,913 ± 570 / 1283.4 ± 29.2 | 39,360 ± 310 / 1286.0 ± 18.8 |
| Hydra Brood + Shatter Fang | 761 ± 18 / 348.4 ± 6.1 | 38,887 ± 595 / 1283.2 ± 29.8 | 39,225 ± 344 / 1279.2 ± 21.4 |
| Singularity Mortar + Hydra Brood | 772 ± 25 / 352.0 ± 9.6 | 38,883 ± 552 / 1281.4 ± 28.6 | 39,251 ± 317 / 1283.0 ± 21.3 |
| Hydra Brood + Miasma | 766 ± 29 / 350.2 ± 11.2 | 38,873 ± 650 / 1282.0 ± 32.8 | 38,936 ± 297 / 1270.6 ± 20.5 |
| Hydra Brood + Consumption Beam | 751 ± 20 / 338.0 ± 7.2 | 38,196 ± 345 / 1253.0 ± 23.9 | 38,642 ± 431 / 1258.6 ± 20.2 |
| Ouroboros + Ancient Brood Pit | 758 ± 34 / 346.8 ± 12.1 | 37,912 ± 703 / 1251.0 ± 32.8 | 38,246 ± 453 / 1249.6 ± 24.5 |
| Miasma + Ancient Brood Pit | 753 ± 31 / 344.6 ± 11.5 | 37,643 ± 1,175 / 1232.6 ± 47.4 | 38,082 ± 371 / 1243.2 ± 21.6 |
| Singularity Mortar + Ancient Brood Pit | 749 ± 22 / 344.2 ± 7.4 | 37,514 ± 721 / 1233.2 ± 37.6 | 38,368 ± 226 / 1251.6 ± 14.4 |
| Hydra Brood + Serpent's Reckoning | 728 ± 24 / 326.8 ± 8.6 | 37,079 ± 357 / 1213.8 ± 16.4 | 37,917 ± 549 / 1231.2 ± 30.0 |
| Ancient Brood Pit + Serpent's Reckoning | 729 ± 37 / 327.6 ± 14.9 | 37,034 ± 1,057 / 1210.4 ± 46.7 | 37,516 ± 337 / 1220.0 ± 15.2 |
| Ouroboros + Miasma | 730 ± 35 / 336.0 ± 12.7 | 36,823 ± 529 / 1213.2 ± 31.4 | 37,725 ± 395 / 1226.8 ± 22.6 |
| Ouroboros + Shatter Fang | 722 ± 34 / 333.4 ± 12.4 | 36,729 ± 523 / 1210.0 ± 28.3 | 37,878 ± 395 / 1234.4 ± 24.0 |
| Ouroboros + Singularity Mortar | 730 ± 36 / 336.4 ± 13.4 | 36,704 ± 505 / 1210.2 ± 32.1 | 37,708 ± 421 / 1228.4 ± 23.4 |
| Ouroboros + Serpent's Gatling | 726 ± 34 / 334.4 ± 12.0 | 36,620 ± 471 / 1207.4 ± 30.3 | 37,586 ± 441 / 1225.0 ± 23.7 |
| Singularity Mortar + Shatter Fang | 711 ± 34 / 329.6 ± 11.4 | 36,437 ± 651 / 1199.4 ± 34.6 | 37,389 ± 385 / 1218.4 ± 21.0 |
| Ouroboros + Consumption Beam | 715 ± 42 / 325.4 ± 17.3 | 36,411 ± 531 / 1198.8 ± 31.6 | 37,375 ± 367 / 1217.4 ± 20.1 |
| Ouroboros + Serpent's Reckoning | 694 ± 28 / 314.0 ± 7.5 | 36,354 ± 518 / 1192.4 ± 30.5 | 37,006 ± 385 / 1199.4 ± 21.1 |
| Singularity Mortar + Miasma | 723 ± 32 / 334.0 ± 11.0 | 36,345 ± 401 / 1196.6 ± 24.3 | 37,342 ± 301 / 1214.2 ± 18.2 |
| Serpent's Gatling + Singularity Mortar | 736 ± 26 / 338.2 ± 9.1 | 36,141 ± 512 / 1187.6 ± 22.6 | 37,240 ± 198 / 1214.8 ± 14.8 |
| Miasma + Shatter Fang | 727 ± 29 / 334.2 ± 9.9 | 35,878 ± 395 / 1173.0 ± 25.0 | 36,889 ± 387 / 1197.0 ± 20.3 |
| Singularity Mortar + Serpent's Reckoning | 694 ± 34 / 313.2 ± 12.3 | 35,834 ± 572 / 1170.6 ± 26.0 | 36,698 ± 447 / 1187.8 ± 17.6 |
| Serpent's Gatling + Miasma | 721 ± 33 / 332.6 ± 11.9 | 35,815 ± 409 / 1162.4 ± 20.9 | 36,761 ± 331 / 1192.6 ± 17.6 |
| Shatter Fang + Serpent's Reckoning | 677 ± 24 / 303.6 ± 7.2 | 35,721 ± 737 / 1164.0 ± 34.5 | 36,217 ± 260 / 1172.4 ± 14.6 |
| Miasma + Serpent's Reckoning | 685 ± 28 / 306.6 ± 9.9 | 35,694 ± 436 / 1156.4 ± 22.2 | 36,578 ± 345 / 1183.4 ± 19.2 |
| Serpent's Gatling + Serpent's Reckoning | 690 ± 23 / 308.4 ± 6.6 | 35,402 ± 485 / 1151.0 ± 25.3 | 36,261 ± 230 / 1172.4 ± 12.5 |
| Singularity Mortar + Consumption Beam | 679 ± 16 / 300.4 ± 4.6 | 35,335 ± 626 / 1158.0 ± 23.4 | 36,576 ± 156 / 1186.4 ± 11.5 |
| Shatter Fang + Ancient Brood Pit | 738 ± 51 / 339.8 ± 19.2 | 35,328 ± 3,497 / 1133.0 ± 159.6 | 38,036 ± 229 / 1242.6 ± 17.4 |
| Miasma + Consumption Beam | 646 ± 29 / 277.8 ± 8.4 | 35,002 ± 357 / 1119.4 ± 20.3 | 36,675 ± 331 / 1184.8 ± 17.5 |
| Consumption Beam + Serpent's Reckoning | 645 ± 20 / 276.0 ± 3.0 | 34,753 ± 540 / 1120.6 ± 27.5 | 36,104 ± 159 / 1165.0 ± 11.6 |
| Serpent's Gatling + Shatter Fang | 725 ± 35 / 334.0 ± 12.0 | 33,055 ± 654 / 1053.6 ± 36.6 | 34,225 ± 1,314 / 1117.8 ± 39.2 |
| Serpent's Gatling + Ancient Brood Pit | 753 ± 28 / 345.2 ± 11.1 | 32,050 ± 5,681 / 955.6 ± 271.9 | 37,940 ± 327 / 1239.0 ± 17.9 |
| Consumption Beam + Ancient Brood Pit | 687 ± 46 / 299.2 ± 24.3 | 31,256 ± 5,154 / 925.4 ± 249.4 | 37,618 ± 323 / 1227.0 ± 19.7 |
| Consumption Beam + Shatter Fang | 658 ± 28 / 286.4 ± 8.7 | 23,756 ± 927 / 698.8 ± 42.0 | 19,155 ± 891 / 627.2 ± 30.6 |
| Serpent's Gatling + Consumption Beam | 654 ± 29 / 282.4 ± 7.7 | 18,557 ± 88 / 468.6 ± 9.8 | 18,102 ± 64 / 592.8 ± 5.4 |

### Base + evolution (72)

| Loadout | Early moving (120s) | Late moving (60s) | Late hold (60s) |
|---|---|---|---|
| Hydra Brood + Ricochet Fang | 751 ± 25 / 345.4 ± 8.4 | 38,790 ± 533 / 1279.0 ± 25.5 | 38,986 ± 216 / 1271.8 ± 13.9 |
| Venom Shot + Hydra Brood | 766 ± 14 / 350.0 ± 5.8 | 38,787 ± 464 / 1276.6 ± 28.5 | 39,036 ± 441 / 1272.6 ± 23.2 |
| Hydra Brood + Venom Nova | 755 ± 28 / 346.0 ± 10.9 | 38,748 ± 495 / 1276.8 ± 25.7 | 38,964 ± 352 / 1271.2 ± 23.2 |
| Hydra Brood + Cobra Pit | 767 ± 23 / 350.4 ± 9.0 | 38,706 ± 487 / 1275.6 ± 20.4 | 39,139 ± 306 / 1276.6 ± 15.4 |
| Plague Mortar + Hydra Brood | 760 ± 14 / 348.6 ± 5.0 | 38,665 ± 572 / 1273.2 ± 26.5 | 39,270 ± 356 / 1280.2 ± 19.9 |
| Viper Fangs + Hydra Brood | 767 ± 14 / 351.0 ± 5.2 | 38,618 ± 621 / 1272.6 ± 30.3 | 39,008 ± 398 / 1273.6 ± 22.5 |
| Hydra Brood + Sidewinder Beam | 746 ± 30 / 342.6 ± 11.5 | 38,523 ± 436 / 1269.0 ± 23.4 | 38,778 ± 286 / 1266.4 ± 15.6 |
| Hydra Brood + Tongue Lash | 766 ± 16 / 350.2 ± 6.6 | 38,514 ± 671 / 1269.6 ± 28.2 | 38,881 ± 198 / 1268.8 ± 17.3 |
| Ouroboros + Snake Nest | 745 ± 23 / 342.6 ± 8.0 | 36,794 ± 421 / 1210.6 ± 28.8 | 37,650 ± 458 / 1226.2 ± 25.8 |
| Ouroboros + Plague Mortar | 730 ± 36 / 336.6 ± 13.2 | 36,725 ± 465 / 1209.0 ± 29.5 | 37,575 ± 385 / 1223.0 ± 22.9 |
| Ouroboros + Cobra Pit | 737 ± 37 / 337.8 ± 13.4 | 36,706 ± 525 / 1208.0 ± 32.1 | 37,514 ± 405 / 1221.0 ± 23.6 |
| Plague Mortar + Miasma | 723 ± 38 / 333.6 ± 12.5 | 36,619 ± 583 / 1181.6 ± 25.9 | 36,958 ± 385 / 1199.0 ± 18.4 |
| Ouroboros + Ricochet Fang | 727 ± 35 / 334.8 ± 12.9 | 36,590 ± 475 / 1207.2 ± 30.3 | 37,559 ± 374 / 1223.0 ± 21.9 |
| Ouroboros + Viper Fangs | 724 ± 35 / 334.2 ± 12.5 | 36,584 ± 486 / 1205.6 ± 31.2 | 37,435 ± 400 / 1220.0 ± 22.8 |
| Ouroboros + Sidewinder Beam | 723 ± 33 / 333.6 ± 11.8 | 36,576 ± 455 / 1205.2 ± 30.1 | 37,466 ± 389 / 1220.0 ± 21.9 |
| Ouroboros + Tongue Lash | 724 ± 35 / 334.2 ± 12.5 | 36,557 ± 519 / 1205.6 ± 30.4 | 37,435 ± 384 / 1219.4 ± 21.9 |
| Ouroboros + Venom Nova | 724 ± 35 / 334.2 ± 12.5 | 36,542 ± 474 / 1203.8 ± 30.7 | 37,439 ± 405 / 1219.8 ± 21.7 |
| Singularity Mortar + Snake Nest | 734 ± 22 / 338.6 ± 8.4 | 36,329 ± 648 / 1191.8 ± 24.2 | 37,053 ± 348 / 1205.2 ± 16.8 |
| Venom Shot + Singularity Mortar | 731 ± 34 / 336.4 ± 12.7 | 36,125 ± 606 / 1186.6 ± 25.2 | 36,952 ± 234 / 1204.4 ± 16.3 |
| Singularity Mortar + Ricochet Fang | 717 ± 26 / 330.8 ± 9.0 | 36,074 ± 638 / 1191.6 ± 25.1 | 36,820 ± 181 / 1198.2 ± 11.4 |
| Singularity Mortar + Cobra Pit | 712 ± 29 / 330.0 ± 10.7 | 36,036 ± 629 / 1179.8 ± 22.4 | 36,804 ± 222 / 1198.4 ± 17.4 |
| Singularity Mortar + Tongue Lash | 712 ± 31 / 329.6 ± 11.1 | 36,028 ± 700 / 1183.2 ± 27.2 | 36,754 ± 241 / 1196.0 ± 19.6 |
| Singularity Mortar + Venom Nova | 714 ± 37 / 329.4 ± 12.5 | 35,978 ± 564 / 1182.8 ± 24.4 | 36,831 ± 432 / 1197.2 ± 22.5 |
| Serpent's Gatling + Plague Mortar | 731 ± 32 / 336.0 ± 11.4 | 35,953 ± 655 / 1154.4 ± 33.2 | 36,527 ± 281 / 1187.0 ± 15.1 |
| Viper Fangs + Singularity Mortar | 716 ± 33 / 331.2 ± 11.5 | 35,951 ± 610 / 1181.2 ± 30.2 | 36,768 ± 304 / 1199.0 ± 19.5 |
| Plague Mortar + Shatter Fang | 705 ± 28 / 327.4 ± 10.2 | 35,939 ± 612 / 1159.2 ± 26.2 | 36,645 ± 517 / 1191.0 ± 22.8 |
| Singularity Mortar + Sidewinder Beam | 718 ± 40 / 331.8 ± 14.3 | 35,928 ± 486 / 1184.8 ± 25.1 | 36,784 ± 244 / 1195.6 ± 12.3 |
| Snake Nest + Miasma | 750 ± 28 / 344.2 ± 10.4 | 35,926 ± 521 / 1159.2 ± 21.9 | 36,792 ± 332 / 1191.2 ± 16.5 |
| Venom Shot + Miasma | 730 ± 29 / 335.4 ± 10.2 | 35,609 ± 414 / 1133.8 ± 21.4 | 36,628 ± 323 / 1178.6 ± 13.8 |
| Plague Mortar + Serpent's Reckoning | 685 ± 36 / 309.2 ± 13.4 | 35,593 ± 630 / 1155.8 ± 27.2 | 36,402 ± 352 / 1177.0 ± 16.5 |
| Miasma + Cobra Pit | 727 ± 36 / 334.2 ± 12.7 | 35,545 ± 433 / 1127.2 ± 23.4 | 36,609 ± 320 / 1178.0 ± 15.1 |
| Miasma + Tongue Lash | 715 ± 33 / 330.0 ± 11.5 | 35,446 ± 439 / 1127.0 ± 23.6 | 36,640 ± 290 / 1181.0 ± 13.9 |
| Snake Nest + Serpent's Reckoning | 692 ± 35 / 312.6 ± 10.6 | 35,424 ± 519 / 1154.4 ± 26.7 | 36,164 ± 328 / 1166.6 ± 11.9 |
| Plague Mortar + Ancient Brood Pit | 749 ± 38 / 343.2 ± 14.8 | 35,360 ± 617 / 1120.4 ± 27.1 | 38,026 ± 331 / 1240.8 ± 21.7 |
| Miasma + Ricochet Fang | 717 ± 31 / 331.2 ± 11.2 | 35,317 ± 432 / 1119.6 ± 22.0 | 36,596 ± 330 / 1177.2 ± 15.4 |
| Viper Fangs + Miasma | 719 ± 31 / 331.6 ± 10.8 | 35,292 ± 441 / 1116.2 ± 24.4 | 36,596 ± 307 / 1176.8 ± 15.2 |
| Miasma + Sidewinder Beam | 720 ± 32 / 331.4 ± 11.5 | 35,257 ± 398 / 1114.8 ± 20.2 | 36,580 ± 316 / 1175.6 ± 14.0 |
| Cobra Pit + Serpent's Reckoning | 684 ± 23 / 308.0 ± 8.4 | 34,998 ± 391 / 1127.0 ± 20.5 | 36,202 ± 224 / 1170.0 ± 18.8 |
| Venom Shot + Serpent's Reckoning | 692 ± 28 / 309.0 ± 9.3 | 34,899 ± 598 / 1129.0 ± 24.8 | 36,195 ± 202 / 1169.0 ± 9.8 |
| Viper Fangs + Serpent's Reckoning | 687 ± 23 / 310.6 ± 8.1 | 34,898 ± 373 / 1125.4 ± 20.7 | 36,088 ± 111 / 1162.6 ± 9.7 |
| Venom Nova + Serpent's Reckoning | 666 ± 30 / 300.8 ± 10.5 | 34,897 ± 581 / 1126.0 ± 28.8 | 36,114 ± 295 / 1161.6 ± 13.4 |
| Ricochet Fang + Serpent's Reckoning | 674 ± 26 / 304.4 ± 10.0 | 34,785 ± 507 / 1123.0 ± 22.5 | 35,969 ± 208 / 1161.8 ± 12.6 |
| Plague Mortar + Consumption Beam | 650 ± 23 / 281.0 ± 5.8 | 34,414 ± 565 / 1089.8 ± 22.6 | 36,334 ± 326 / 1177.8 ± 19.5 |
| Sidewinder Beam + Serpent's Reckoning | 673 ± 30 / 302.2 ± 9.6 | 34,376 ± 852 / 1113.0 ± 27.7 | 35,854 ± 147 / 1156.4 ± 11.1 |
| Serpent's Gatling + Snake Nest | 745 ± 27 / 342.2 ± 9.3 | 33,250 ± 590 / 1057.0 ± 28.5 | 35,671 ± 237 / 1158.6 ± 11.7 |
| Snake Nest + Ancient Brood Pit | 749 ± 23 / 343.8 ± 7.8 | 32,881 ± 2,879 / 1034.6 ± 126.2 | 38,009 ± 219 / 1241.4 ± 14.2 |
| Snake Nest + Shatter Fang | 749 ± 22 / 343.0 ± 7.2 | 32,358 ± 757 / 1021.4 ± 36.6 | 35,978 ± 167 / 1169.4 ± 9.4 |
| Venom Nova + Ancient Brood Pit | 750 ± 24 / 344.0 ± 9.4 | 31,409 ± 4,348 / 929.4 ± 212.2 | 37,797 ± 384 / 1235.6 ± 20.5 |
| Ancient Brood Pit + Tongue Lash | 749 ± 36 / 343.8 ± 12.6 | 30,318 ± 5,949 / 902.2 ± 310.1 | 37,674 ± 315 / 1232.4 ± 19.9 |
| Ricochet Fang + Ancient Brood Pit | 751 ± 46 / 343.4 ± 17.6 | 30,127 ± 6,916 / 917.4 ± 300.4 | 37,738 ± 212 / 1234.2 ± 16.8 |
| Venom Shot + Ancient Brood Pit | 757 ± 32 / 346.2 ± 12.3 | 29,393 ± 3,541 / 827.6 ± 186.5 | 37,841 ± 83 / 1237.0 ± 11.4 |
| Snake Nest + Consumption Beam | 712 ± 21 / 316.4 ± 7.6 | 24,690 ± 455 / 697.4 ± 26.7 | 34,839 ± 316 / 1118.6 ± 22.8 |
| Viper Fangs + Ancient Brood Pit | 740 ± 53 / 340.0 ± 19.5 | 24,641 ± 1,214 / 629.4 ± 101.2 | 37,702 ± 338 / 1231.6 ± 21.8 |
| Venom Nova + Shatter Fang | 702 ± 32 / 325.2 ± 10.6 | 24,160 ± 1,470 / 655.6 ± 51.3 | 34,358 ± 331 / 1083.2 ± 19.3 |
| Venom Shot + Shatter Fang | 725 ± 34 / 333.6 ± 12.6 | 22,964 ± 1,530 / 631.8 ± 73.4 | 26,891 ± 1,962 / 884.4 ± 65.0 |
| Viper Fangs + Shatter Fang | 710 ± 24 / 328.6 ± 8.7 | 22,763 ± 3,830 / 659.0 ± 152.6 | 20,436 ± 2,290 / 673.4 ± 67.2 |
| Venom Shot + Serpent's Gatling | 733 ± 31 / 336.8 ± 10.9 | 22,264 ± 161 / 571.2 ± 18.3 | 25,240 ± 223 / 833.2 ± 10.4 |
| Shatter Fang + Cobra Pit | 715 ± 22 / 330.4 ± 9.4 | 21,705 ± 1,991 / 605.2 ± 85.0 | 20,740 ± 863 / 641.2 ± 28.8 |
| Serpent's Gatling + Venom Nova | 728 ± 34 / 334.8 ± 11.8 | 21,275 ± 221 / 507.6 ± 15.8 | 34,732 ± 197 / 1103.2 ± 8.8 |
| Shatter Fang + Tongue Lash | 700 ± 29 / 324.0 ± 9.8 | 20,753 ± 3,341 / 554.6 ± 134.8 | 27,276 ± 1,090 / 886.4 ± 32.2 |
| Serpent's Gatling + Cobra Pit | 739 ± 22 / 339.2 ± 8.8 | 20,104 ± 64 / 471.2 ± 13.7 | 19,514 ± 53 / 593.4 ± 9.6 |
| Sidewinder Beam + Ancient Brood Pit | 748 ± 28 / 342.2 ± 9.7 | 19,950 ± 4,975 / 411.6 ± 158.8 | 37,705 ± 264 / 1233.2 ± 17.6 |
| Serpent's Gatling + Tongue Lash | 725 ± 33 / 333.2 ± 11.6 | 19,637 ± 197 / 477.4 ± 22.8 | 27,949 ± 228 / 907.0 ± 15.4 |
| Serpent's Gatling + Ricochet Fang | 725 ± 34 / 334.0 ± 12.1 | 19,577 ± 220 / 498.4 ± 11.7 | 19,579 ± 139 / 647.0 ± 6.2 |
| Sidewinder Beam + Shatter Fang | 705 ± 29 / 326.4 ± 10.2 | 18,933 ± 2,038 / 512.8 ± 79.5 | 17,468 ± 2,537 / 574.8 ± 81.2 |
| Serpent's Gatling + Sidewinder Beam | 727 ± 35 / 334.2 ± 11.9 | 16,202 ± 160 / 369.0 ± 14.4 | 15,692 ± 42 / 520.0 ± 9.1 |
| Venom Shot + Consumption Beam | 671 ± 31 / 294.2 ± 12.1 | 9,268 ± 110 / 187.6 ± 8.4 | 10,688 ± 91 / 349.6 ± 8.0 |
| Venom Nova + Consumption Beam | 631 ± 20 / 269.4 ± 3.6 | 8,791 ± 146 / 99.6 ± 2.3 | 33,570 ± 222 / 1036.6 ± 14.3 |
| Consumption Beam + Ricochet Fang | 642 ± 24 / 276.4 ± 6.7 | 6,890 ± 62 / 135.6 ± 4.3 | 6,217 ± 95 / 198.2 ± 7.0 |
| Consumption Beam + Cobra Pit | 655 ± 28 / 284.0 ± 7.2 | 6,431 ± 54 / 125.2 ± 8.0 | 6,519 ± 53 / 114.8 ± 4.6 |
| Consumption Beam + Tongue Lash | 632 ± 21 / 270.0 ± 3.8 | 6,064 ± 171 / 89.2 ± 2.9 | 13,708 ± 205 / 415.4 ± 5.9 |
| Viper Fangs + Consumption Beam | 634 ± 20 / 271.0 ± 4.1 | 6,001 ± 40 / 134.6 ± 5.4 | 5,500 ± 47 / 174.8 ± 4.1 |

## Four-weapon example loadouts

These four presets are additional executed fights, not an exhaustive search for the best full build. Base weapons are rank 8 without passives; evolved presets own their four recipe passives at rank 1.

| Loadout | Early moving (120s) | Late moving (60s) | Late hold (60s) | Recipe passive(s), rank 1 |
|---|---|---|---|---|
| Singularity Mortar + Hydra Brood + Consumption Beam + Ancient Brood Pit | 759 ± 15 / 345.2 ± 4.4 | 39,058 ± 533 / 1282.6 ± 32.1 | 39,467 ± 372 / 1289.6 ± 22.2 | chronofield, coiled volley, graviton, ravenous maw |
| Ouroboros + Serpent's Gatling + Miasma + Serpent's Reckoning | 708 ± 36 / 321.2 ± 13.8 | 36,501 ± 589 / 1198.0 ± 27.9 | 37,419 ± 392 / 1213.0 ± 23.3 | rapid fire, serpents reach, serpents scales, toxic expanse |
| Plague Mortar + Snake Nest + Venom Nova + Cobra Pit | 749 ± 19 / 343.4 ± 7.0 | 35,983 ± 667 / 1157.2 ± 31.4 | 36,624 ± 250 / 1189.2 ± 16.7 | — |
| Venom Shot + Viper Fangs + Ricochet Fang + Tongue Lash | 725 ± 30 / 333.4 ± 10.5 | 17,179 ± 222 / 380.2 ± 9.3 | 27,813 ± 365 / 900.4 ± 3.1 | — |

## Enemy pressure and measurement checks

These diagnostics help distinguish damage ceilings from limited target availability. Remaining enemies are alive at the end. Supply includes actual splitter children, so it can differ between builds.

| Fixture | Best rank-8 single by DPS | DPS | Kills | Enemies remaining | HP supply removed |
|---|---|---:|---:|---:|---:|
| Early director, moving square route | Hydra Brood | 761 | 348.4 | 7.6 | 97.2% |
| Late director, moving square route | Hydra Brood | 38,500 | 1270.2 | 48.6 | 96.1% |
| Late director, stationary east-facing defense | Hydra Brood | 38,854 | 1269.8 | 35.0 | 97.2% |

Recorded external waves, before each fight's additional splitter children:

| Fixture | Supplied enemies (seed range) | External target HP range | Enemy speed range (units/s) |
|---|---:|---:|---:|
| Early director, moving square route | 288–288 | 150–650 | 17.5–41.8 |
| Late director, moving square route | 1008–1008 | 1,000–2,400 | 29.9–53.8 |
| Late director, stationary east-facing defense | 1008–1008 | 1,000–2,400 | 29.9–53.8 |

Seeds: `195141632, 195142609, 195143586, 195144563, 195145540`.

Native validation results:

```json
{
  "all_runs_passed": true,
  "build_count": 184,
  "control_count": 15,
  "repeat_verified": true,
  "run_count": 2760,
  "zero_damage_controls_passed": true,
  "independent_shards": 2
}
```

The executable rejects non-finite state, invalid grid paths, lost HP accounting, changed equipment, unexpected phases, and exceeded pool bounds. Unarmed controls must deal exactly zero damage and score zero kills. Every armed fight is repeated to check deterministic damage, kills, and actor totals; repeats are additional validation executions and are excluded from the tabulated fight/hour totals.

## Reproduce

From the repository root, configure a Release build using the normal compiler environment, then:

```powershell
cmake --build build --config Release --target snake_balance_sim
./build/bin/snake_balance_sim.exe --data data --output artifacts/balance/results.json --seeds 5 --seconds 120 --verify-repeat
python scripts/report_balance.py --input artifacts/balance/results.json --output docs/BALANCE_SIMULATIONS.md
```

`snake_balance_sim` is an optional target and is excluded from normal builds. A headless Release build (`SNAKE_BUILD_GAME=OFF`) works too; use its build directory. On Windows, `snake.ps1` can locate Blender's bundled Python; the report script needs only Python's standard library.

Raw per-seed measurements: `artifacts/balance/results.json` (ignored local artifact). The Markdown preserves the aggregate results; rerunning regenerates the raw evidence. Rebuild after simulation or content changes before using this report to make balance decisions.

Source fingerprint (SHA-256 of sorted `src/*.cpp`, `src/*.hpp`, `data/*.csv`, and the harness with paths): `bd9f6851921f1ccb7e5cddf1651fd26e3e4a92ee91b9e51d9e653a654a0a86e0`.

Measurement implementation: [`tests/balance_sim.cpp`](../tests/balance_sim.cpp). Aggregation and matrix checks: [`scripts/report_balance.py`](../scripts/report_balance.py). Combat rules: [`WEAPON_PARITY.md`](WEAPON_PARITY.md), [`DESIGN.md`](DESIGN.md).
