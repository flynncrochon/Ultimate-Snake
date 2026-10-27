# Native port design

## Intended experience

Recreate the JavaScript game's presentation closely: a pure black full-window arena, connected flat white player snake, saturated red pursuers, blue splitters, purple bosses, green venom, and plain white fruit. Preserve the original four-card character selection, floating HUD arrangement, narrow item cards, and simple pause/result overlays. Use Warforge's exact Pixel Operator 8 font and rasterized versions of the original upgrade artwork.

The target is a complete 20-minute run with a start, escalating enemy pressure, repeated build choices, chest-driven evolution payoffs, and a results overlay. Visual composition follows the original. The C++ backend preserves the nine base weapons' source formulas and the nine evolutions' distinct actor lifecycles. Evolutions apply intentional native balance tuning so they retain the offensive passive investment used by their base weapons. [`WEAPON_PARITY.md`](WEAPON_PARITY.md) records the original reference, current native overrides, and grid-safe fixes. The director and character framework remain native systems. [`REFERENCE_VISUALS.md`](REFERENCE_VISUALS.md) records the original screen layouts and source coordinates.

Automated fixed-build combat experiments support native evolution tuning; [`BALANCE_SIMULATIONS.md`](BALANCE_SIMULATIONS.md) records measured scenarios and their limits. A complete human playthrough and progression/survival balance pass remain outstanding. Automated 20-minute simulation checks verify run stability and scheduling with invulnerability, which does not establish survival difficulty, fair progression, or equivalent strength between builds.

## Run specification

| System | Rule |
|---|---|
| Simulation | Fixed 60 Hz, seeded random generator, simulation clocks freeze in selection and pause states |
| Arena | 200×200 simulation tiles, eight simulation units per tile |
| Presentation scale | 20 source pixels per world cell; 25 cells across the smaller 500-pixel gameplay dimension |
| Start | One-second countdown after character selection |
| Movement | Cardinal steering, no immediate reversal; base step interval 0.12 seconds |
| Enemy movement | One cardinal grid cell per speed-dependent tick; bodies follow prior cells and rendering interpolates between waypoints |
| Starting snake | 7 connected segments; one character weapon |
| Health | 100 HP; contact consumes a shield charge when available, otherwise damages HP and grants brief invulnerability |
| Growth | Each collected XP fruit adds its raw XP value to length, capped at 120 segments; XP bonuses and level-ups add no extra length |
| Level offers | Three unique eligible definitions; first offer is a weapon when one is available |
| Equipment | Four weapon slots plus four passive slots |
| Offer weighting | Definition weight ×4 for an upgrade already owned |
| Rank caps | Eight for normal definitions; two for Coiled Volley |
| Reroll / banish | Three of each per run; banished definitions leave the offer pool |
| Exhausted pool | Choose healing and gold when no eligible upgrades remain |
| Bosses | First at 1:00, then every 2:00; bosses drop chests |
| Evolution | Max-rank weapon plus its paired passive at any rank; collect a chest |
| Victory | Reach 20:00 |
| Results | Space or Enter returns to character selection |
| Pause | Escape/P toggles pause; Space or Enter also resumes |

Enemy spawning speeds up over elapsed run time. Red serpents form the basic pursuing swarm, blue splitters create fast children when defeated, swift enemies introduce faster pressure, and bosses supply chest milestones. Enemy and projectile capacities are bounded to keep overload behavior predictable.

Enemy head and body positions are logical grid waypoints, not continuously steered coordinates. Each tick chooses a cardinal step toward the player while avoiding the actual neck cell and arena bounds. Slows lengthen the step interval. Spawn, recycling, and forced movement snap to the same grid; knockback translates the body and resets interpolation history so it cannot leave diagonal drift or a stretched trail. Presentation interpolates prior/current waypoints without changing their logical coordinates.

Rendering retains one fixed update of history and samples movement phase at the display's refresh rate, including the correct path and face direction across cell boundaries. The camera shares the sampled head's integer source-pixel coordinates. Speed changes retain cell progress, and pause preserves the fractional displayed pose. See [`MOVEMENT.md`](MOVEMENT.md). Boss pickups use swept collection and durable reward records; the desktop reveal pauses combat until acknowledged, as described in [`LOOT.md`](LOOT.md). Quiet sound synthesis and the bounded event director are documented in [`AUDIO.md`](AUDIO.md).

An attempted move into the retained body or an arena wall holds the entire snake in place and starts a one-second grace period, matching the JavaScript reference. Combat continues, and a safe turn cancels the pending collision. Remaining blocked ends the run on the first movement attempt after grace expires; the final 300 ms fade in a red warning. All grace timing uses simulation time and freezes during pause and selection. Invulnerability holds a self-bite without starting grace; it does not protect against walls. Shields alone do not prevent collision death. Entering a tail cell is safe only if the tail vacates it on that step. Bosses can damage the head through normal contact but cannot destroy or shorten the tail.

## Character identities

| Character | Starts with | Innate bonus |
|---|---|---|
| Cobra | Venom Shot | ×1.15 damage |
| Taipan | Viper Fangs | ×0.90 cooldown |
| Mamba | Tongue Lash | ×1.15 area |
| Basilisk | Plague Mortar | ×1.20 duration |

## Content model

`src/core.hpp` defines data structures and the public simulation API. `src/core.cpp` loads definitions, evaluates bonuses, selects eligible offers, resolves combat, and advances a run. `src/weapon_spec.cpp` resolves source-specific rank, evolution, and bonus formulas; `src/weapon_system.cpp` owns projectiles, pools, beams, cobras, the leviathan, grapples, orbiting fangs, and shields. `src/enemy_system.cpp` handles enemy grid ticks and placement; `src/nest_system.cpp` owns eggs and summoned snakes. `src/snake_geometry.cpp` builds continuous cardinal snake silhouettes. These simulation and geometry files contain no raylib dependency.

`src/main.cpp` owns the window and fixed-step accumulator. `src/presentation.cpp` handles input, screens, HUD, sound, and local profile; `src/world_renderer.cpp` draws the world and its camera/interpolation. `src/pixel_art.cpp` supplies pixel artwork and loads Warforge's Pixel Operator 8 font plus the original game's converted PNG icon atlas. Display code reads the same catalog used by simulation.

### Upgrade CSV

`data/upgrades.csv` uses these columns:

| Column | Meaning |
|---|---|
| `id` | Unique stable identifier; referenced by recipes |
| `type` | `weapon` or `passive`; determines equipment slot category |
| `name`, `description` | Player-facing text |
| `max_rank` | Positive rank cap, at most 20 |
| `weight` | Positive base weight in level offers |
| `pattern` | `shot`, `homing`, `mortar`, `nest`, `nova`, `beam`, `ricochet`, `pit`, or `lash` |
| `damage`, `cooldown`, `speed` | Pattern base damage, seconds between attacks, simulation units per second |
| `range`, `area` | Targeting range and effect size, in simulation units |
| `duration` | Projectile/effect lifetime in seconds |
| `amount`, `pierce` | Base projectile count and additional pierced targets |
| `modifiers` | Semicolon-separated `stat:value` entries applied for every owned rank |

Passive rows still need valid placeholder values for weapon columns. Modifier percentages are fractions: a custom `damage:0.15` modifier means +15% damage per rank. Cooldown reductions compound: `cooldown:-0.15` gives a `0.85^rank` multiplier. Multiple entries compose in one row, such as `damage:0.15;growth:0.20`. Original passives with specialized source behavior, Ravenous Maw and Serpent's Scales, retain the contracts described below.

Supported modifier names:

| Modifier | Interpretation |
|---|---|
| `damage` | Adds to damage multiplier, base 1 |
| `cooldown` | Multiplies by `(1 + value)^rank`; product clamped to 0.025 through 3 before character bonus |
| `area`, `duration`, `range`, `speed` | Add to their base-1 multipliers, each bounded by the evaluator |
| `amount` | Adds to projectile count, rounded and bounded |
| `pickup` | Adds simulation units to the base 20-unit attraction radius |
| `crit` | Adds probability from a zero base; total capped at 100%; critical hits double eligible damage |
| `growth` | Adds to the XP gain multiplier, base 1 |
| `armor` | Optional custom flat reduction to contact damage, bounded; the shipped Serpent's Scales uses shield charges instead |

Most bonus contributions sum by stat before character multipliers; cooldown reductions multiply. Ravenous Maw's direct-damage bonus rises linearly from +10% at rank 1 to +50% at rank 8, while XP gain is `1 + 0.25 * rank`. Its XP bonus does not multiply snake growth: length follows raw collected XP, including fruit vacuumed by evolved weapons. Dead Eye adds 12.5% critical chance per rank. Chronofield, Toxic Expanse, and Serpent's Reach add 25% duration, 15% area, and 12% range per rank respectively. Serpent's Scales replenishes shield charges with counts `[1,1,1,2,2,3,3,3]` and a cooldown decreasing linearly from 10 to 4 seconds. A charge absorbs an entire enemy hit; this passive grants no armor or protection from self-contact.

Generic stat bonuses require a CSV row and optionally an icon, without per-frame wiring. Weapons have individual rank formulas, targeting rules, bonus exclusions, rounding stages, and evolved lifecycles rather than a common percentage-per-rank rule. [`WEAPON_PARITY.md`](WEAPON_PARITY.md) is the detailed weapon contract. New behavior belongs in the native weapon system; resolved values are exposed as `WeaponSpec` for UI and tests. Poison and nest bites retain the source exclusions from direct-damage bonuses and critical hits.

### Snake Nest lifecycle

Snake Nest is a summon system rather than a homing projectile with a snake-shaped sprite:

1. A cast chooses a nearby enemy cluster and snapshots its landing cell. The egg follows an arcing 0.75-second flight to that fixed point; it does not chase a moving enemy or deal contact damage.
2. The grounded egg waits 0.5 seconds before hatching exactly once.
3. Each egg releases `3 + floor((rank - 1) / 2)` green snakes. Every snake has three real grid segments and advances one cardinal cell per 0.08-second tick. Its body follows prior cells, and it avoids its actual neck cell and arena bounds. A newly hatched, stacked body can face its target immediately because there is no unfolded neck cell to reverse into.
4. A snake can bite overlapping enemies once per grid tick while passing through them. A hit does not consume the snake. Brood count and bite damage resolve at hatch; bite damage then remains fixed. Duration upgrades change the allowed lifetime without resetting age, using the original rounded tick counts. Pause freezes these timers; a new run clears all eggs and summons.

The base nest casts one egg, with Coiled Volley adding eggs independently of snakes per egg. Hydra Brood replaces it with one egg containing six five-segment parents and concentrates Volley investment into their bite damage. Each expiring parent creates up to two three-segment children. Parent damage uses the authored evolution multiplier and Volley factor; child damage is 75% of the parent's resolved hatch damage. Movement intervals remain 0.08 and 0.07 seconds and base lifetimes remain 5.6 and 3.5 seconds respectively. Both generations briefly wander before hunting. The source-specific live-upgrade rules and intentional native tuning are detailed in [`WEAPON_PARITY.md`](WEAPON_PARITY.md); an evolution clears the replaced family's actors.

There are at most 25 eggs, 30 base summoned snakes, or 60 with Hydra Brood, with separate limits of 30 hydra parents and 30 children. A full pool suppresses additional spawning without evicting an existing snake. A nest is consumed after its one hatch attempt even when the pool is full, preventing repeated hatching.

### Evolution CSV

`data/evolutions.csv` columns are `weapon,passive,name,description,damage,cooldown,area,range`. The final four columns multiply evolved source damage, cast cooldown, effect area, and targeting range. `range` is optional and defaults to 1 for older content; every supplied multiplier must be positive. The shipped recipes author native evolution upgrade tuning rather than all using identity multipliers. Damage multipliers reach related effects such as Singularity detonations, Hydra children, Ancient laser growth, and Reckoning's drag and bite. Evolution mechanics and support-bonus retention come from the weapon system rather than these multipliers alone; [`WEAPON_PARITY.md`](WEAPON_PARITY.md#native-evolution-upgrade-tuning) specifies the resulting contracts. A weapon can have one recipe. The loader checks prerequisite existence, equipment categories, and duplicate weapon recipes.

| Weapon | Paired passive | Evolved name |
|---|---|---|
| Venom Shot | Serpent's Scales | Ouroboros |
| Viper Fangs | Rapid Fire | Serpent's Gatling |
| Plague Mortar | Graviton | Singularity Mortar |
| Snake Nest | Coiled Volley | Hydra Brood |
| Venom Nova | Toxic Expanse | Miasma |
| Sidewinder Beam | Ravenous Maw | Consumption Beam |
| Ricochet Fang | Dead Eye | Shatter Fang |
| Cobra Pit | Chronofield | Ancient Brood Pit |
| Tongue Lash | Serpent's Reach | Serpent's Reckoning |

An evolution replaces its base attack. Distinct state represents 44 orbiting fangs, piercing gatling fangs, gravity wells, splitting hydras, a breathing aura, consuming tracking beams, recursive critical splinters, a growing laser leviathan, and cardinally dragged grapple victims. Consumption and devours grant growth/XP rather than healing. Their targeting, timing, damage, bonus interactions, and bounds are specified in [`WEAPON_PARITY.md`](WEAPON_PARITY.md).

Malformed CSV, duplicate IDs, unknown patterns/stats, invalid numeric values, and invalid evolution prerequisites fail at startup with a content-file error. Standard quoted CSV fields and escaped double quotes are supported; embedded multiline cells are not.

## Presentation and pixel contract

- The sole logical framebuffer is 800×500.
- Gameplay uses the full pure black canvas. There is no reserved sidebar or decorative terrain.
- Kills, five hearts, and two rows of equipment slots float at the upper left. The timer is upper center, length is upper right, XP is bottom center, and the minimap is bottom right, following the JavaScript composition.
- Character selection shows all four original cards simultaneously. Level selection retains the original `CHOOSE AN ITEM` heading and card hierarchy. Pause and results use the original simple text overlays.
- A displayed world cell is 20 source pixels. Player blocks are 14×14, ordinary enemy blocks 11×11, and bosses 15×15, matching the reference's 70%, 55%, and 75% cell proportions.
- Backend `Tile = 8` remains an independent simulation unit. Rendering maps simulation coordinates into source pixels rather than changing physics to fit the artwork.
- The font is the unchanged `PixelOperator8.ttf` used by Warforge, baked at its native eight-source-pixel size with monochrome rasterization and point filtering. Case and original glyph advances are preserved.
- Upgrade icons are raster conversions of the original SVG definitions, stored as `data/icons/original-icons.png`. Native variants are 8, 10, 16, 24, and 32 pixels; runtime rendering uses point filtering.
- World-to-screen coordinates, sprite edges, effects, text, and UI are quantized to the same source-pixel grid.
- Window presentation uses the largest fitting integer multiplier and centered letterboxing, with no smooth texture filtering or fractional final scale.

Projectiles can have fractional simulation positions. Player, enemy, and summoned-snake head/body waypoints stay on the logical grid. Snake rendering follows the historical cardinal centerline with a single quantized interpolation progress, fixed-width opaque spans, and a shared integer camera. This keeps elbows filled and trims moving heads and tails consistently, avoiding diagonal corner pixels and segment gaps. Every visible source pixel has the same physical dimensions. HUD and world share the framebuffer; there is no separately scaled high-resolution text overlay.

## Performance design

World shapes use an ordered packed quad stream with startup-allocated CPU/GPU buffers. Raster coverage, alpha overlap, texture interleaving, and integer camera placement remain unchanged. Cached circle spans and coalesced line runs feed that stream; independent before/after captures and native equivalence tests check full-quality results. Settings offer additional decorative effect budgets. See [`VALIDATION.md`](VALIDATION.md) for measured comparisons and [`SETTINGS.md`](SETTINGS.md) for display/frame pacing.

The core reserves entity storage, reuses compact vectors, caps population, and uses a 32-simulation-unit spatial grid for projectile, zone, targeting, and player-contact queries. Dead entities compact after combat queries so grid indices remain stable during resolution. Distant enemies are recycled near the active camera region. This prevents the unbounded accumulation of unseen pursuers.

Capacities currently include 1,800 enemies, 1,600 projectiles, 128 zones, 25 nest eggs, 30 base summoned snakes (60 with Hydra), 30 cobras, and 1,400 ordinary pickups. Specialized weapon families impose additional limits, including 40 Venom Shot bullets and 60 Shatter family projectiles. Pickups at capacity merge further XP into existing XP entries. Boss chest rewards retain quantities when storage is full and award every preserved reward when collected. These are implementation budgets, not measured performance promises on every machine.

## Validation

The C++ regression targets cover core progression, enemy grid movement, Snake Nest, combat edges, snake geometry, and source weapon contracts. They include data validation, slot/rank eligibility, pause clocks, neck avoidance, stat composition, chest evolution, swept projectile contacts, generation limits, seeded state, and bonus/rounding exclusions. They run with `SNAKE_BUILD_GAME=OFF` and need no graphics device.

The Python GameTest API uses a JSON-lines subprocess bridge to the same `Game::update`, exposing fixture controls, seeded resets, exact ticks, equipment specifications, actors, event counters, and invariant checks. It has four tiers:

| Tier | Purpose |
|---|---|
| `logic` | API contracts and all 18 weapon variants, with per-rank source-oracle comparisons and live behavior assertions |
| `visual` | Native raylib captures compared pixel-for-pixel with an independent Python cardinal-centerline oracle |
| `hunt` | Reproducible seeded sequences of movement, mixed builds, damage, pause, clear, reset, and target removal |
| `bench` | Warmed-up dense native updates, percentile timings, and finite/grid/cap invariants |

The weapon oracle is generated by executing the original JavaScript classes, getters, and constants at the pinned reference commit. Its probe and fixture are checked in under `game_tests`; expected values do not come from the native formulas. Behavior assertions inspect actual spawned actors, hits, damage, expiry, absorption, splitting, and grapples. See [`GAME_TEST_API.md`](GAME_TEST_API.md) for the API and [`../README.md`](../README.md#verification-tools) for commands. Every Python run writes structured results and failure tracebacks to `artifacts/runs` or the requested JSON path; seeded hunts include their seed and action context.

CTest registers the Python logic tier when Python 3.10+ is found; set `SNAKE_PYTHON_EXECUTABLE` during CMake configuration to choose an interpreter. `scripts/build.ps1 -CoreOnly -Configuration Debug` builds headless Debug targets in `build-tests`. `scripts/check-asan.ps1` configures a separate MSVC AddressSanitizer build using the installed compiler/runtime. The optional `snake_core_stress` target exercises two complete scripted 20-minute runs and a dense 1,800-enemy fixture. These checks target explicit contracts and invariants and do not prove the absence of every bug or establish game balance.

The native executable also exposes fixed scene captures, a simulation/render smoke test, and nest flight/hatch/brood scenes. Visual tests use the real hidden raylib game through `--test-api`, requiring a working desktop OpenGL context; the headless host initializes no graphics or audio. The separate `snake_render_benchmark` reports representative and dense render timings. Headless update timings exclude rendering, protocol I/O, and Python assertions; do not present them as full-game FPS. CI builds desktop targets and runs C++ regressions, Python logic, and seeded hunts on Windows, Linux, and macOS. GPU capture validation remains a local step.
