# Live weapon test coverage

Verified 2026-10-26 against the real C++ `Game::update` in both the headless host and the native hidden raylib game. **All nine base weapons and all nine evolutions have individually discoverable live combat tests.**

## Executed checks

| Coverage | Distinct tests | What is observed |
|---|---:|---|
| Every base weapon at every rank 1?8 | 72 | Actual weapon actors, durable enemy HP loss, native hit events, a real kill, and XP/drop reward |
| Every evolution at rank 8 | 9 | Its distinct actors, actual damage, real kill, and ordinary reward or direct devour growth |
| Out-of-range controls | 18 | Every variant leaves an unreachable stationary target unharmed |
| Content completeness | 1 | Every catalog weapon/evolution maps to an explicit combat case and all required rank functions exist |
| Real boss ? chest ? evolution ? combat | 9 | Base weapon earns the chest; collection applies the correct evolution once, clears base actors, and the evolved attack damages fresh prey |
| Evolution prerequisite gates | 18 | Every recipe requires both maximum weapon rank and its paired passive |
| Secondary mechanics and interactions | 5 | Real piercing, child bites, recursive splinters, laser coverage, and cross-weapon consumption |

**132 new tests: 396 headless passes across three repeats, plus 132 native passes. Zero failures or skips in these runs.** Structured evidence is stored in `artifacts/weapon-coverage/headless.json` and `artifacts/weapon-coverage/native.json`. Existing JavaScript-oracle, native C++, movement, and combat-hunt coverage remains separate.

## Every weapon and evolution

All positive cases follow the real actor ? HP damage ? kill ? reward path. A durable target proves damage is delivered; a separate fresh one-HP target verifies kill resolution and reward accounting. Reckoning also damages a surviving drag/blast bystander, because its primary target is executed atomically. Each fixture starts with only the tested weapon and, for evolutions, its recipe passive.

| Variant | Form | Ranks with live damage/kill/reward tests | Observed stages |
|---|---|---|---|
| Venom Shot | Base | 1, 2, 3, 4, 5, 6, 7, 8 | projectile |
| Viper Fangs | Base | 1, 2, 3, 4, 5, 6, 7, 8 | projectile |
| Plague Mortar | Base | 1, 2, 3, 4, 5, 6, 7, 8 | projectile, zone |
| Snake Nest | Base | 1, 2, 3, 4, 5, 6, 7, 8 | egg, summon |
| Venom Nova | Base | 1, 2, 3, 4, 5, 6, 7, 8 | zone |
| Sidewinder Beam | Base | 1, 2, 3, 4, 5, 6, 7, 8 | beam |
| Ricochet Fang | Base | 1, 2, 3, 4, 5, 6, 7, 8 | projectile |
| Cobra Pit | Base | 1, 2, 3, 4, 5, 6, 7, 8 | projectile, pit, cobra |
| Tongue Lash | Base | 1, 2, 3, 4, 5, 6, 7, 8 | zone |
| Ouroboros | Evolution | 8 | orbit |
| Serpent's Gatling | Evolution | 8 | projectile |
| Singularity Mortar | Evolution | 8 | projectile, zone |
| Hydra Brood | Evolution | 8 | egg, summon |
| Miasma | Evolution | 8 | zone |
| Consumption Beam | Evolution | 8 | beam |
| Shatter Fang | Evolution | 8 | projectile |
| Ancient Brood Pit | Evolution | 8 | projectile, leviathan, ancient beam |
| Serpent's Reckoning | Evolution | 8 | grapple |

The positive tests equip the real weapon and advance exact simulation ticks. They create enemy/pickup fixtures through the public test bridge; they do not inject projectile/zone/summon actors, manually reduce target HP, or manufacture hit/kill events. Every case checks actual native state and bounded invariants. The catalog gate fails if a shipped weapon or evolution has no explicit case.

## Secondary effects exercised by real casts

- Gatling: two naturally fired fangs each pierce three targets with exact 100% / 70% / 50% damage falloff.
- Hydra: naturally expired parents produce children that actually bite, then expire and stop dealing damage.
- Shatter: a real critical cast reaches generations 0?3 and the 60-projectile cap; children still deal damage after the parent disappears; the family eventually expires.
- Ancient Brood Pit: a naturally raised leviathan damages three collinear enemies with one laser while an off-axis target remains untouched.
- Consumption: a real Venom Shot finishes the beam target; absorption and XP vacuum occur exactly once and do not heal the player.

Expected scalar values remain grounded in the checked-in executed JavaScript oracle and explicit authored native tuning, rather than a second combat simulation in Python. These fixtures test correctness; balance comparisons are recorded in [BALANCE_SIMULATIONS.md](BALANCE_SIMULATIONS.md) and [EVOLUTION_UPGRADES.md](EVOLUTION_UPGRADES.md).

## Sources and commands

- [Per-rank functionality, rewards, range controls, and catalog gate](../game_tests/test_weapon_functionality.py).
- [All nine boss-earned evolution paths and prerequisites](../game_tests/test_evolution_functionality.py).
- [Authentic secondary effects and interactions](../game_tests/test_weapon_interactions.py).
- [Independent scalar oracle and existing per-weapon mechanical contracts](../game_tests/test_weapon_parity.py).

Run the complete added coverage:

```powershell
python -m game_tests.run --single test_weapon_functionality,test_evolution_functionality,test_weapon_interactions --repeat 3
python -m game_tests.run --single test_weapon_functionality,test_evolution_functionality,test_weapon_interactions --native
```

Run one weapon/rank directly:

```powershell
python -m game_tests.run --single test_weapon_functionality::test_cobra_pit_rank_1_hits_kills_and_rewards
```

All three modules belong to the default `logic` tier, so normal Python logic tests and CTest run them automatically when Python is configured. With `SNAKE_RUN_NATIVE_TESTS=ON`, CTest also registers `snake_python_native_weapons` against the real graphical executable. Native tests require a working desktop OpenGL context. Validation covers the listed deterministic fixtures.
