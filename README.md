# Snake Survivors — native C++ / raylib

A C++17 / raylib recreation of [Flynn Crochon's Snake Survivors](https://github.com/flynncrochon/Snake-Survivors). The presentation follows the JavaScript game's pure black arena, flat white and red snakes, floating HUD, four character cards, item picker, and simple pause/result overlays. It uses the exact **Pixel Operator 8** font from Warforge and raster conversions of the original upgrade icons. The backend is a new implementation with separate simulation and presentation, and upgrade content lives in editable CSV files.

## Build and play

Requires CMake 3.20+, a C++17 compiler, and a desktop OpenGL driver. The first game build downloads the pinned **raylib 5.5** source archive; an installed raylib package or local checkout can also be used.

On Windows, install Visual Studio's **Desktop development with C++** workload and CMake. The build script finds Visual Studio and its bundled Ninja automatically:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run.ps1
```

The game is written to `build/bin/snake_survivors.exe`, with a `data` folder beside it. Copy both to distribute a build.

Repository automation and contributor instructions, including builds, Python tests, native screenshots, and performance requirements, are in [`AGENTS.md`](AGENTS.md).

On Linux or macOS:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/bin/snake_survivors
```

Linux additionally needs the development packages required by raylib's GLFW/OpenGL/audio backend. On Ubuntu:

```sh
sudo apt-get install libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
```

For an offline build with a raylib checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -RaylibSource C:/path/to/raylib
```

Or pass `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=/path/to/raylib` to CMake. For a debug build, add `-Configuration Debug` to the Windows script.

## Build for the web

With the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
activated and CMake/Ninja installed, run:

```powershell
./snake.ps1 web
```

Host the contents of `build-web/site` on a static web server. To try it locally,
run `python -m http.server 8080 --directory build-web/site` and visit
`http://localhost:8080`. The browser build retains the 60 Hz simulation and
800×500 integer-scaled pixel rendering, with browser-local saves.
See [`docs/WEB.md`](docs/WEB.md) for portable commands, hosting, and browser limits.

## Settings

Open **Settings** from character selection, pause, or **F10**. Display options follow Warforge's model: Windowed/Borderless/Fullscreen, monitor, remembered window size, fullscreen refresh rate, FPS limits or uncapped rendering, VSync, display-refresh limiting, and integer pixel scale. Background FPS and automatic pause while unfocused are configurable too.

Graphics includes effect quality, particles, damage numbers, weapon effects, FPS counter, and minimap. Audio has master and category volumes; controls can be rebound. Apply saves preferences to `snake-settings.json` beside the executable; Cancel discards drafts. Display changes revert after 15 seconds unless kept. See [`docs/SETTINGS.md`](docs/SETTINGS.md).

The optimized renderer measured **2.68 → 0.64 ms per dense frame**, about **4.2× faster** locally at full quality, with identical reference pixels. See [`docs/VALIDATION.md`](docs/VALIDATION.md) for workloads and timing limits.

## The run

Choose Cobra, Taipan, Mamba, or Basilisk from the four original character cards. A one-second countdown starts the run. Steer the snake and survive 20 minutes while weapons fire automatically. Collect fruit and XP, choose from three upgrades, and shape a build with four weapon slots and four passive slots. You have three rerolls and three banishes per run. Health pickups restore damage.

The nine weapon families, nine passives, and nine evolutions from the JavaScript game are retained. Weapons reach rank 8; Coiled Volley has two ranks. A boss chest evolves a maxed weapon when you own its matching passive. Before an evolution is ready, chests improve an owned upgrade or award gold and healing. Evolved weapons retain their equipment slot.

This first native version has not had a complete human playthrough or full balance pass. Automated full-run checks exercise stability and scheduling; they do not establish fair pacing or build balance.

Enemies advance between grid cells on their own speed-dependent ticks. Each body segment follows the previous segment's cell. Rendering samples between simulation updates at the display's refresh rate, including cell boundaries, turns, and camera motion. Speed changes preserve movement progress; pause and loot reveals preserve the displayed pose. Slows change the tick interval, and knockback and arena bounds keep the logical positions on the grid. See [`docs/MOVEMENT.md`](docs/MOVEMENT.md).

Every 90 seconds, a grey snake appears ahead of you and builds walls across your path for ten seconds. Its trail stays as a permanent barrier for the rest of the run. Turning away during the one-second collision warning saves you; shields and contact immunity do not protect against these walls. Grey snakes and their barriers appear on the minimap. See [`docs/GREY_SNAKE.md`](docs/GREY_SNAKE.md) for the original JavaScript behavior and native checks.

Snake Nest lobs an egg toward a fixed enemy-cluster location. After a 0.75-second flight and 0.5-second hatch delay, it releases three-segment green snakes that hunt enemies one grid cell every 0.08 seconds. Higher ranks increase the brood to `3 + floor((rank - 1) / 2)` snakes per egg; count and bite damage resolve when the egg hatches. Coiled Volley adds eggs. Hydra Brood instead casts six five-segment parents that split into three-segment children when their lifetime expires. Duration upgrades extend living summons using the original tick-based lifespan rules without resetting age. Bites retain the original exclusions from direct-damage bonuses and critical hits. Summons persist through contact; an evolution replaces its base family's actors.

The native backend preserves the original weapon signatures, including 44 individually orbiting Ouroboros fangs, homing and ricocheting projectiles, tracking beams, independent cobra turrets, the growing Ancient Brood leviathan, and grappling devours. Rank formulas and live behavior are checked against the original JavaScript; the exact contracts and intentional grid-safe fixes are recorded in [`docs/WEAPON_PARITY.md`](docs/WEAPON_PARITY.md). The enemy director, character framework, and fixed-step simulation remain native systems. The original battle royale, solo mode, and mobile controls are outside this port's scope.

## Controls

| Action | Keys |
|---|---|
| Steer / select character | WASD or arrow keys |
| Pick upgrade | 1–3, click a card, or Left/Right then Enter |
| Reroll / banish selected card | R / B |
| Pause / resume | Escape or P; Space or Enter also resumes |
| Evolution codex | Tab or F2 |
| Toggle sound | M |
| Performance counters | F3 |
| Settings | F10, or Settings in the menu/pause screen |
| Start selected character | Enter or Space |
| Return to character selection after death or victory | Enter or Space |
| Return to menu while paused | Q |

These are the default controls; Settings supports rebinding. The simulation stays at 60 Hz regardless of the chosen render FPS or VSync setting.

## Pixel rules

Everything renders into one **800×500** framebuffer on a shared source-pixel grid. A displayed world cell is 20 source pixels, putting 25 cells across the smaller gameplay dimension as in the JavaScript game. Flat player, ordinary enemy, and boss blocks are 14, 11, and 15 source pixels wide, preserving the original 70% / 55% / 75% proportions. The simulation's tile remains eight units; simulation units and displayed pixels are independent.

The arena fills the canvas with a pure black background. Kills, hearts, equipment slots, time, and length float above it; XP is bottom center and the minimap is bottom right. Pixel Operator 8 is baked at its native eight-pixel size, and original upgrade artwork is loaded from a PNG atlas. The framebuffer is enlarged only by integer multiples with nearest-neighbor filtering and letterboxing, keeping all visible pixels the same size.

## Add a bonus

Edit [`data/upgrades.csv`](data/upgrades.csv). A passive row can combine any supported modifiers, such as:

```csv
quick_metabolism,passive,Quick Metabolism,Move faster and gain more XP.,5,1,shot,0,1,0,1,1,1,1,0,speed:0.05;growth:0.10
```

The same definition drives offer selection, ranks, stat evaluation, and UI descriptions. No per-bonus update wiring is needed. Add an evolution by putting its weapon and passive IDs in [`data/evolutions.csv`](data/evolutions.csv). New weapon content can reuse an existing pattern; a fundamentally new combat behavior requires one new pattern implementation. The original nine weapon families resolve their source-specific rank formulas and effect lifecycles in the native weapon specification and system code. Restart the game to reload content. Rebuild to copy source `data` changes into `build/bin/data`, or launch with `--data ./data` from the project root.

See [`docs/DESIGN.md`](docs/DESIGN.md) for the progression specification, CSV schema, stat semantics, and architecture.

## Verification tools

The current checks pass in Release, Debug, and AddressSanitizer builds, including all 18 weapon variants, boss loot, audio, and static/temporal movement pixels. Measured dense-scene simulation and rendering results are recorded in [`docs/VALIDATION.md`](docs/VALIDATION.md).

The Warforge-style Python GameTest API drives the real C++ simulation with seeded resets, exact fixed ticks, equipment and enemy fixtures, full actor snapshots, invariant checks, and structured failure reports. Python does not duplicate the game simulation. Python 3.10+ and its standard library are sufficient; the Windows wrapper finds an installed Python or an existing Blender Python runtime. Set `SNAKE_PYTHON` or pass `-Python PATH` to choose a runtime.

```powershell
./snake.ps1 gametest --list
./snake.ps1 gametest --tier logic
./snake.ps1 gametest --single test_weapon_parity --repeat 3
./snake.ps1 gametest --single api_surface::test_seeded_control_is_reproducible
./snake.ps1 gametest --tier hunt
./snake.ps1 gametest --tier visual --native
./snake.ps1 gametest --tier bench
./snake.ps1 bench 240
```

The wrapper builds missing executables; after source changes, rebuild with `./snake.ps1 build`. The equivalent portable runner is `python -m game_tests.run`, with the same selection flags. Each run writes a `result.json` beneath `artifacts/runs`; use `--json PATH` for a fixed output path.

| Tier | Coverage | Host |
|---|---|---|
| `logic` (default) | API isolation and errors; all nine base weapons and nine evolutions, rank formulas, bonuses, targeting, damage, and actor lifecycles | Headless `snake_game_test` |
| `visual` | Actual framebuffer pixels across cardinal turns, interpolation, camera positions, and snake shapes, compared with an independent Python pixel oracle | Native raylib game with `--native` |
| `hunt` | Seeded random movement, all weapon variants, mixed builds, normal/splitter/boss enemies, pause, clear, reset, and grapple ownership | Headless `snake_game_test` |
| `bench` | Warmed-up dense combat update timings, percentile budgets, and actor invariants | Headless `snake_game_test` |

`./snake.ps1 bench 2400` measures native full-quality rendering, 1,800 live moving enemies with and without evolved weapons, and a separate 10,000-snake rendering stress case. Moving fixtures retain 60 Hz simulation and sample presentation at a virtual 240 Hz; the benchmark runs uncapped. Reports include mean/p99/worst frame times, first use, peak actor counts, simulation timings, and deterministic enemy checksums. The 10,000-snake case does not run simulation; the gameplay cap remains 1,800.

`./snake.ps1 bench 2400 --diagnose` additionally samples Windows thread CPU time and records up to 64 frames exceeding 16.67 ms, including warmup, to help investigate stalls. Headless update timings exclude rendering, JSON serialization, and Python. Native frame timings include submission and presentation, plus simulation on moving fixtures' fixed updates. These are local workload measurements, not full-game FPS promises for other hardware.

The independent weapon oracle in [`game_tests/fixtures/javascript_weapon_oracle.json`](game_tests/fixtures/javascript_weapon_oracle.json) was produced by executing the original JavaScript classes, getters, and constants at commit `b6cb72e66eebc1291d57efe63ebbe93ec602e863`. [`game_tests/reference_probe.mjs`](game_tests/reference_probe.mjs) records the browser probe; expected values are not generated from the C++ implementation. Live behavior tests additionally observe real hits, targeting, expiry, absorption, splitting, and devouring. See [`docs/GAME_TEST_API.md`](docs/GAME_TEST_API.md) for writing tests and the JSON-lines protocol.

[`docs/BALANCE_SIMULATIONS.md`](docs/BALANCE_SIMULATIONS.md) records executed fixed-build DPS and kill comparisons for every weapon, evolution, and legal two-weapon pair, with seed variation, scenario assumptions, and reproduction commands.

[`docs/EVOLUTION_UPGRADES.md`](docs/EVOLUTION_UPGRADES.md) checks every evolution against its max-rank base with identical passives, including swarm and durable-boss fixtures. Configure `SNAKE_RUN_BALANCE_TESTS=ON` to enable the CTest upgrade regression gate.

[`docs/WEAPON_TEST_COVERAGE.md`](docs/WEAPON_TEST_COVERAGE.md) maps all 18 weapon variants to real actor, damage, kill, and reward tests. Every base rank is covered, every evolution is earned through a real boss chest, and a catalog gate rejects untested weapon additions. These checks run in the default logic tier; `SNAKE_RUN_NATIVE_TESTS=ON` also registers them against the native raylib game.

The C++ regression suites cover progression, enemy grid movement, summon lifecycles, combat edges, source weapon contracts, and snake geometry. They and the headless API do not require raylib, a GPU, or network access:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -CoreOnly
```

```sh
cmake -S . -B build-tests -DSNAKE_BUILD_GAME=OFF
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

The native executable also supports reproducible rendering and a smoke run:

```powershell
./build/bin/snake_survivors.exe --smoke-test
./build/bin/snake_survivors.exe --scene gameplay --capture artifacts/gameplay.png
./build/bin/snake_survivors.exe --scene upgrades --capture artifacts/upgrades.png
./build/bin/snake_survivors.exe --smoke-test --scene nest-brood --frames 600
```

Available scenes are `menu`, `gameplay`, `upgrades`, `evolutions`, `pause`, `results`, `nest-flight`, `nest-hatch`, and `nest-brood`. The nest scenes exercise the real simulation's egg and summon lifecycle in a controlled encounter. Hidden-window captures still need a working desktop OpenGL context.

`snake_core_stress` runs two full 20-minute simulations with scripted movement and invulnerability: normal progression and a sustained Snake Nest/Hydra build. It then checks a dense fixture with 1,800 enemies and up to 60 summons, including grid alignment and entity caps. The target is built with the test targets and can be run from the build directory. Add `-DSNAKE_RUN_STRESS_TESTS=ON` during CMake configuration to include it in CTest. Its timings measure the simulation only. Automated checks target explicit contracts and invariants; they do not prove the absence of every bug or establish balance.

Boss boxes remain on free grid cells until the head collects them. Opening one pauses combat and shows the actual upgrade, evolution, or treasure; Enter, Space, or a click continues. Merged rewards reveal in order and are granted once. See [`docs/LOOT.md`](docs/LOOT.md).

Personal bests, completed run counts, and sound preference are saved locally. Sounds use quiet plucks, low thuds, and distinct reward chords. Combat cadence, voice limits, and reward ducking prevent sound spam; M mutes immediately. See [`docs/AUDIO.md`](docs/AUDIO.md).

## Attribution

Game concept, naming, visual design, and original upgrade artwork: Flynn Crochon, [Snake-Survivors](https://github.com/flynncrochon/Snake-Survivors/tree/b6cb72e66eebc1291d57efe63ebbe93ec602e863). Reference source inspected at commit `b6cb72e66eebc1291d57efe63ebbe93ec602e863`. The C++ implementation is new; the PNG upgrade atlas is converted from the original SVG definitions. Pixel Operator 8 by Jayvee Enaguas (HarvettFox96), reused from Warforge, is included under CC0 1.0 with its notices in [`data/fonts`](data/fonts). [raylib](https://github.com/raysan5/raylib/tree/5.5) uses its zlib/libpng license; the test protocol uses [nlohmann/json 3.11.3](https://github.com/nlohmann/json/tree/v3.11.3) under MIT. See [`docs/THIRD_PARTY.md`](docs/THIRD_PARTY.md).
