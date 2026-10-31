# Ultimate Snake

## CMake build

Requires CMake 3.20+, Ninja and a C++17 compiler. Use a developer shell on Windows.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Run `build/bin/snake_survivors` (`.exe` on Windows).

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

Requires Python 3.10+, with no pip dependencies.

```sh
python -m game_tests.run --list
python -m game_tests.run --tier logic
```

```python
from game_tests.snake_test import Game

with Game() as game:
    game.reset(seed=123)
    game.step(60)
    print(game.snapshot()["counts"])
```

[API reference](docs/GAME_TEST_API.md).
