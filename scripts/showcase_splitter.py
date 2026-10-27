"""Capture the real splitter-to-offspring animation and compare seeded state."""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests.snake_test import Game
from game_tests._png import read


def fixture(game, scene):
    game.reset(seed=731)
    if scene == "edge":
        game.player(position=[64, 32])
        parent = game.spawn_enemy(8, 32, kind="splitter", speed=0, length=5)
    else:
        parent = game.spawn_enemy(896 if scene == "straight" else 960, 800,
                                  kind="splitter", speed=40 if scene == "straight" else 80,
                                  length=5 if scene == "straight" else 8)
        game.step(60)
        if scene == "turn":
            game.player(position=[800, 880])
            game.step(45)
    game.set_enemy(parent, alive=False)
    game.step(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", choices=("before", "after"), required=True)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/splitter-death")
    args = parser.parse_args()
    folder = args.output / args.record
    folder.mkdir(parents=True, exist_ok=True)
    results = []
    with Game(args.exe, native=True) as game:
        for scene in ("straight", "turn", "edge"):
            fixture(game, scene)
            states = []
            for frame in range(31):
                state = game.snapshot()
                state.pop("timing", None)
                states.append(state)
                game.capture(folder / f"{scene}-{frame:02d}.png")
                if frame != 30:
                    game.step(1)
            (folder / f"{scene}.json").write_text(json.dumps(states), encoding="utf-8")
            result = {"scene": scene, "frames": len(states)}
            if args.record == "after":
                baseline = args.output / "before"
                assert states == json.loads((baseline / f"{scene}.json").read_text()), scene
                expected, actual = read(baseline / f"{scene}-30.png"), read(folder / f"{scene}-30.png")
                differences = sum(expected.rgba[i:i+4] != actual.rgba[i:i+4]
                                  for i in range(0, len(actual.rgba), 4))
                result.update(simulation_matches=True, final_different_pixels=differences)
                assert differences == 0, result
            results.append(result)
            print(json.dumps(result), flush=True)
    (folder / "result.json").write_text(json.dumps(results, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
