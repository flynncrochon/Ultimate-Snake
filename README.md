# Ultimate Snake

## CMake build

Requires CMake 3.20+, Ninja and a C++17 compiler. Use a developer shell on Windows.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Run `build/bin/snake_survivors` (`.exe` on Windows).

Runs last 30 simulation minutes, with one-minute enemy waves and researched
XP progression. See [progression research and balance measurements](docs/PROGRESSION.md).

Choose **UPGRADES [U]** on character selection to spend banked run gold on
[permanent Shedskin upgrades](docs/PERMANENT_UPGRADES.md). They apply to all characters,
support full refunds, and preserve the existing gameplay art and movement.

The [flood enemy](docs/FLOOD.md) flows around bodies and enemies, recoils from
weapons, and kills on head contact. It appears automatically every 60 gameplay
seconds. Waves last 90 seconds and merge when they meet. Preview it immediately with
`build/bin/snake_survivors --scene flood`.

## Web compile

[Activate Emscripten 4.0.23](docs/WEB.md#toolchain) first.

```sh
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web --parallel
cmake --install build-web --prefix build-web/site --component game
python -m http.server 18765 --directory build-web/site
```

Open http://localhost:18765.

## Python API

```sh
python -m game_tests.run
```

[API docs](docs/GAME_TEST_API.md).
