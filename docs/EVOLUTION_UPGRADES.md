# Evolution upgrade validation

Measured 2026-10-26 after native evolution tuning. **All nine evolutions pass all 135 matched upgrade comparisons**, covering 1,350 fights with five seeds; every fight was executed twice with identical results. Another 25 unarmed controls deal zero damage and score zero kills.

## What changed

- Strengthened evolved damage/cadence/coverage through the authored evolution CSV. The original base weapon formulas remain the reference.
- Retained Volley, Duration, and Rapid Fire investment when a repeated attack becomes a persistent aura/beam or a single concentrated well/egg.
- Added eight close-orbit Ouroboros fangs, extended Consumption to 12 cells, gave Singularity a faster landing hit, and let mature Ancient seeds retain overflow power.
- Routed actual orbit hits, well detonations, Hydra children, laser growth, and grapple damage through the configured evolution values.

See [the native evolution contracts](WEAPON_PARITY.md#native-evolution-upgrade-tuning) for formulas and intentional changes from the original JavaScript.

## Matched comparison

Each pair uses a rank-8 base weapon versus its rank-8 evolution with **identical character, passives, ranks, enemy wave schedule, route, duration, and seed**. All builds use Cobra. Profiles are recipe passive at rank 1, recipe passive at its maximum rank, and recipe plus offensive support filling four passive slots. The offensive profile adds max-rank Rapid Fire, Coiled Volley, Chronofield, Dead Eye, and Ravenous Maw in that order until slots are full, retaining the recipe passive. Exact equipment is recorded in the raw JSON.

Five fixtures per profile: early moving swarm (120 s, level 5, run clock 0); late moving and stationary swarms (60 s, level 40, run clock 840); stationary actual boss actors at 24 and 64 simulation units (90 s, 100,000,000 HP). The oversized boss HP measures sustained damage; it is not a claim about normal boss health. Normal contact mechanics remain enabled with a large player HP pool. Progression is locked and steering is scripted.

**Pass rule:** mean damage to the identical incoming enemies must retain at least 98% of the base result; swarm kills may vary by max(1 kill, 2% of base kills). Mean damage or kills must improve by more than 5%, unless the base already removes at least 85% of the available targets/HP. Boss kills allow no losses; if both versions kill the durable boss in every seed, evolution must reduce mean kill time by at least 5%. These are aggregate regression thresholds with explicit seed tolerance, rather than guarantees about every individual frame or random encounter.

**Splitter accounting:** Consumption and Reckoning can devour splitters before they create two children. Their upgrade comparison uses damage/kills against the identical incoming enemies and records prevented splits separately. Raw damage and kills, including actual children, remain unchanged in the report; no hypothetical child damage is credited. For fights without additional split prevention, the raw damage and kill floors must also pass.

## Results for every evolution

Each ratio below is the **lowest evolved/base incoming-enemy DPS ratio across all five fixtures** for that support profile. A ratio near 1 can mean both versions clear almost all available enemies; boss kill-time improvements are checked separately.

| Evolution | Recipe rank 1: worst ratio | Recipe max: worst ratio | Offensive build: worst ratio | Checks passed |
|---|---:|---:|---:|---:|
| Ouroboros | 1.027× | 1.027× | 0.994× | 15 / 15 |
| Serpent's Gatling | 1.030× | 1.018× | 1.014× | 15 / 15 |
| Singularity Mortar | 1.016× | 1.016× | 0.986× | 15 / 15 |
| Hydra Brood | 1.020× | 1.021× | 1.026× | 15 / 15 |
| Miasma | 1.067× | 1.050× | 1.013× | 15 / 15 |
| Consumption Beam | 1.355× | 1.236× | 1.008× | 15 / 15 |
| Shatter Fang | 1.139× | 1.150× | 1.021× | 15 / 15 |
| Ancient Brood Pit | 1.102× | 1.019× | 1.000× | 15 / 15 |
| Serpent's Reckoning | 2.072× | 1.096× | 1.037× | 15 / 15 |

## Late moving fight with rank-1 recipe passive

DPS below counts all HP actually removed, and kills include actual splitter children. A devouring evolution can finish the same incoming wave with fewer raw kills because it prevents child spawns.

| Evolution | Base DPS → evolved DPS | Base kills → evolved kills | Incoming-enemy kills: base → evolved | Splitters prevented from splitting |
|---|---:|---:|---:|---:|
| Ouroboros | 6,827 → 36,528 | 89.8 → 1203.4 | 78.6 → 912.0 | 0.0 |
| Serpent's Gatling | 4,474 → 15,502 | 72.4 → 337.2 | 61.6 → 246.2 | 0.0 |
| Singularity Mortar | 34,554 → 36,023 | 1096.0 → 1188.0 | 842.2 → 904.2 | 0.0 |
| Hydra Brood | 32,455 → 38,500 | 1050.2 → 1270.2 | 810.4 → 963.0 | 0.0 |
| Miasma | 9,816 → 35,216 | 84.8 → 1109.6 | 83.4 → 835.6 | 0.0 |
| Consumption Beam | 789 → 2,053 | 10.8 → 61.8 | 10.8 → 61.8 | 9.4 |
| Shatter Fang | 4,864 → 17,835 | 68.0 → 476.4 | 55.6 → 358.4 | 0.0 |
| Ancient Brood Pit | 5,228 → 26,038 | 73.2 → 753.8 | 65.2 → 594.0 | 0.0 |
| Serpent's Reckoning | 4,573 → 34,844 | 13.4 → 1126.8 | 13.4 → 869.8 | 6.0 |

## Authored evolution multipliers

These multiply the original evolved specification, followed by the documented passive-retention rules. Smaller cooldown multipliers mean faster casts.

| Evolution | Damage | Cooldown | Area | Range |
|---|---:|---:|---:|---:|
| Ouroboros | 4 | 1 | 1 | 1 |
| Serpent's Gatling | 4 | 1 | 1 | 1 |
| Singularity Mortar | 1.5 | 0.25 | 1.5 | 1 |
| Hydra Brood | 2 | 0.5 | 1 | 1 |
| Miasma | 1 | 1 | 1 | 1 |
| Consumption Beam | 1.75 | 1 | 1 | 1.2 |
| Shatter Fang | 1.25 | 0.4 | 1 | 1 |
| Ancient Brood Pit | 3 | 0.5 | 1 | 1 |
| Serpent's Reckoning | 3 | 0.5 | 2 | 1 |

## Local performance checks

Release, Windows x64, same seeded positions, 1,800 moving enemies, rank-8 evolved four-weapon fixtures, and identical passive ranks. The first update and following 119 cold updates are measured separately; warm samples contain 900 updates after 360 warmup updates. Every before/after sample ends with 1,800 living targets. After tuning, fixture HP was raised from 100 million to one trillion to prevent the stronger lasers from emptying the benchmark; ordinary HP damage has no target-selection effect while all these targets remain alive. The benchmark now asserts that target population remains dense. Timings below are native simulation work, excluding rendering and protocol I/O.

| Dense fixture | First update: before → after (ms) | Cold p99: before → after (ms) | Warm mean: before → after (ms) | Warm p99: before → after (ms) |
|---|---:|---:|---:|---:|
| hydra | 0.347 → 0.360 | 0.143 → 0.258 | 0.102 → 0.114 | 0.282 → 0.294 |
| tracking | 0.277 → 0.289 | 0.130 → 0.150 | 0.036 → 0.085 | 0.146 → 0.204 |

Rendering is measured separately with the unchanged full-quality 800×500 render stress fixture, 600 sampled frames after 60 warmup frames, uncapped pacing. This fixture fixes actor populations rather than simulating combat.

| Render fixture | CPU mean: before → after (ms) | CPU p99: before → after (ms) | Frame mean: before → after (ms) | Frame p99: before → after (ms) |
|---|---:|---:|---:|---:|
| representative | 0.108 → 0.105 | 0.186 → 0.154 | 0.239 → 0.161 | 0.854 → 0.704 |
| dense | 0.655 → 0.623 | 1.059 → 0.801 | 0.681 → 0.642 | 1.135 → 0.841 |

The stronger attacks increase simulation work in these dense fixtures, with bounded actors and sub-millisecond sampled p99 update times on this machine. Native captures were opened and inspected for the changed effects and HUD. These samples do not establish full-game FPS or hitch-free behavior on every machine.


## Reproduce and regression gate

```powershell
cmake --build build --config Release --target snake_balance_sim
./build/bin/snake_balance_sim.exe --evolution-check --profile all --seeds 5 --seconds 120 --verify-repeat --assert-upgrades --output artifacts/balance/evolution-upgrades.json
python scripts/report_evolution_upgrades.py
```

For CTest, configure `-DSNAKE_RUN_BALANCE_TESTS=ON`, build, then run `ctest --test-dir build -R snake_evolution_upgrades --output-on-failure`. The option builds the analysis executable and enables a failing upgrade regression gate. `--weapon ID` and `--profile recipe|max|offense` select focused tuning checks.

Raw measurements: `artifacts/balance/evolution-upgrades.json`. The original broad weapon/combo matrix is refreshed in [BALANCE_SIMULATIONS.md](BALANCE_SIMULATIONS.md).

Source fingerprint: `bd9f6851921f1ccb7e5cddf1651fd26e3e4a92ee91b9e51d9e653a654a0a86e0`.

These checks establish upgrade behavior in the measured fixtures. Other characters, arbitrary passive combinations, complete progression runs, survival difficulty, and every possible enemy arrangement remain outside this finite validation set.
