# Ultimate Snake agent guide

Work from the repository root. This is a C++17/raylib recreation of Flynn
Crochon's JavaScript Snake Survivors. Preserve its look and weapon behavior.
The runtime source is authoritative; detailed contracts live in `docs/`.

## Non-negotiable requirements

- **Ultra performant, no performance hitches.** Treat frame-time spikes,
  including first-use effects, loading, resize, settings changes, and dense
  combat, as bugs to investigate. This is an engineering requirement, not a
  claim that a passing benchmark proves every machine hitch-free.
- Keep normal update/render paths free of synchronous file I/O, shader
  compilation, GPU readback, blocking waits, large allocations, and unbounded
  work. Prepare assets/shaders before play; reuse bounded storage and batches.
  Explicit screenshot/testing tools may perform readback outside gameplay.
- Preserve fixed **60 Hz simulation**. Render FPS, VSync, focus/background caps,
  and interpolation must not change combat speed, bonuses, or RNG behavior.
- Keep the **800×500 source framebuffer**, square pixels, integer presentation
  scale, point filtering, and Warforge's unchanged Pixel Operator 8 font.
  Art must use native raster pixels; avoid fractional scaling or antialiasing.
- Preserve cardinal grid movement, smooth historical-path interpolation, and
  one shared quantized camera. Speed changes and pause must not cause snaps.
- Keep full-quality visuals and source weapon contracts intact during
  optimization. Optional effects settings must not change simulation outcomes.
- Preserve unrelated user changes and runtime preferences. Do not commit build
  outputs, captures, local saves, or `snake-settings.json`.

## Build and run

Requirements: CMake 3.20+, C++17 compiler, Ninja, and desktop OpenGL 3.3.
Windows needs Visual Studio's **Desktop development with C++** workload.
`scripts/build.ps1` imports the installed MSVC/Ninja environment automatically.
The desktop build uses pinned raylib 5.5 with GLFW. The first build may download
raylib; the headless build needs no graphics libraries or network.

```powershell
./snake.ps1 build
./scripts/run.ps1
./snake.ps1 build -Configuration Debug
./snake.ps1 build -CoreOnly -Configuration Debug
```

Desktop output: `build/bin/snake_survivors.exe`, with `data` beside it.
`-CoreOnly` uses `build-tests`; its test host is
`build-tests/bin/snake_game_test.exe`. Desktop Debug/Release share `build`, so
rebuild Release before comparing performance. `-SkipTests` builds without CTest.
For offline raylib, pass `-RaylibSource C:/path/to/raylib` to the build script.
Rebuild after CSV/asset changes to synchronize `build/bin/data`.

Portable builds from a configured compiler shell:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For headless-only CMake, add `-DSNAKE_BUILD_GAME=OFF` and use `build-tests`.
See [README](README.md) for Linux dependencies and installed-raylib options.

## Python GameTest API

Python 3.10+ and its standard library are sufficient; no pip packages are needed.
`snake.ps1` finds Python, including an installed Blender runtime. Select another
with `-Python PATH` or `SNAKE_PYTHON`. The API drives the actual C++ game through
JSON lines; it is not a second Python simulation.

```powershell
./snake.ps1 gametest --list
./snake.ps1 gametest --single test_boss_loot
./snake.ps1 gametest --single test_weapon_parity --repeat 3
./snake.ps1 gametest --single api_surface::test_seeded_control_is_reproducible
./snake.ps1 gametest --tier visual --native
./snake.ps1 gametest --tier bench
```

Equivalent portable entry point: `python -m game_tests.run` with the same flags.
Use `--exe build-tests/bin/snake_game_test.exe` to explicitly test the headless
Debug binary when a desktop Release binary also exists. `--data PATH` overrides
the catalog. Test reports go beneath `artifacts/runs`, or to `--json PATH`.

- `logic` (default): deterministic state, weapons, progression, and API checks.
- `visual --native`: actual hidden raylib framebuffer captures; needs OpenGL.
- `bench`: warmed-up native simulation timing and percentile budgets.
- `hunt`: opt-in seeded mixed-combat/stateful bug scenarios.

Tests are ordinary `test_*` functions in `game_tests`. The runner binds
`game_tests.snake_test` as `st`, resets each test, and enforces its tick budget.
Use `st.step(n)` and `wait_until`; avoid wall-clock sleeps. Fixtures initially
have no equipment/enemies, disabled spawning, a stationary immune player, and
automatic level choices. Explicitly configure the real behavior being tested.
Long tests declare `test_function.timeout_ticks`; choose a bounded budget.

Standalone control and screenshot example:

```python
from game_tests.snake_test import Game

with Game(native=True) as game:
    game.reset(seed=123)
    game.configure(invulnerable=False)
    game.spawn_pickup(864, 800, kind="chest")
    game.capture("artifacts/gametest/chest.png", world_only=False)
    game.equip("snake_nest")
    game.spawn_enemy(864, 816, hp=100000)
    game.step(100)
    game.capture("artifacts/gametest/nest.png", render_alpha=0.5)
    print(game.snapshot()["counts"])
```

`native=True` uses `snake_survivors --test-api`; the headless host cannot capture.
`world_only=False` includes HUD/overlays. `render_alpha=0..1` samples presentation
between the last two fixed updates; omit it for the exact raw fixture pose.
`reward_time=0..30` samples the chest lottery without advancing gameplay: use
`3.0` for a spinning reel or `6.7` for the awarded item.
Open and inspect saved PNGs; do not infer visual correctness from exit codes.
`game_tests/_png.py` supports dependency-free pixel assertions.

Menu/settings captures are also available:

```powershell
./build/bin/snake_survivors.exe --scene settings --capture artifacts/settings.png
```

See [GameTest API](docs/GAME_TEST_API.md) for controls, actors, invariants, and
protocol details, and [weapon parity](docs/WEAPON_PARITY.md) for the independent
JavaScript oracle. Do not generate expected weapon values from native formulas.

## Verification and performance

Choose checks for the changed paths. Run meaningful focused tests after a
coherent change; repeat broader suites only when changes or failures warrant it.
Visual changes need actual native capture review. Rendering changes additionally
need pixel comparisons and before/after timings at the same full-quality workload.

```powershell
./snake.ps1 unit
./snake.ps1 unit -R snake_boss_loot_tests
./snake.ps1 bench 1200
./scripts/check-asan.ps1 -Python C:/path/to/python.exe
```

Native UI/raster/display tests are opt-in. After an initial desktop build:

```powershell
cmake -S . -B build -DSNAKE_RUN_NATIVE_TESTS=ON
./snake.ps1 build -SkipTests
ctest --test-dir build --output-on-failure -L native
```

Measure Release mean, p99, and worst frame times, including cold first use and
dense combat. Warm steady-state measurements alone do not establish hitch-free
play. Native render benchmarks include presentation; headless timings exclude
rendering, Python, and protocol I/O and must not be presented as full-game FPS.
Keep regression coverage for batching limits, texture/alpha order, actor caps,
and startup/shutdown resource lifetimes. See [validation](docs/VALIDATION.md).

`snake_core_stress` runs two complete 30-minute simulations and a dense fixture.
Use it when lifecycle, progression, capacities, or scheduling changes warrant it.
`scripts/check_render_parity.py` records/compares deterministic full-quality
effect scenes; capture `--record before` before rebuilding the old renderer.

## Delivery

Keep tests in `tests`/`game_tests`, assets in `data`, reusable helpers in
`scripts`, and generated logs/captures in ignored `artifacts`/build directories.
Native textures/shaders belong to RAII owners and must be unloaded before the
OpenGL context closes. Update CMake when adding runtime sources or test targets.
Summarize actual changes, checks, and measured limits without promising zero bugs.

```powershell
cmake --install build --config Release --prefix build/package --component game
```

Ship the executable with its `data` directory. Preserve existing settings/saves.
