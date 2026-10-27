"""Compare native moving swarms, camera motion, and actor reuse pixel for pixel."""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests.snake_test import Game
from game_tests._png import read


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", choices=("before", "after"), required=True)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/performance/swarm/motion-parity")
    args = parser.parse_args()
    folder = args.output / args.record
    folder.mkdir(parents=True, exist_ok=True)
    results = []
    with Game(args.exe, native=True) as game:
        game.reset(seed=0xBEEF)
        game.configure(freeze_player=False)
        handles = game.spawn_enemies({"x": 800 + ((i % 36) - 18) * 8,
                                      "y": 800 + (((i // 36) % 22) - 11) * 8,
                                      "hp": 1e12, "speed": 24 + (i % 5) * 16,
                                      "kind": "splitter" if i % 13 == 0 else "red",
                                      "length": 8}
                                     for i in range(1800))
        tick = 0
        for checkpoint in (1, 7, 29, 60, 91, 140):
            if checkpoint == 29:
                for handle in handles[:8]:
                    game.set_enemy(handle, speed=32)
            if checkpoint == 60:
                for handle in handles[:8]:
                    game.set_enemy(handle, speed=96)
            if checkpoint == 91:
                # Remove slots, compact the swarm, and reuse storage for new IDs.
                for handle in handles[1:5]:
                    game.set_enemy(handle, alive=False)
                game.step(1, direction=(0, 1))
                tick += 1
                game.spawn_enemies({"x": 864 + i * 8, "y": 800, "speed": 80,
                                    "hp": 1e12, "length": 8} for i in range(4))
            game.step(checkpoint - tick, direction=(1, 0) if checkpoint < 60 else (0, 1))
            tick = checkpoint
            for alpha in (0, .25, .5, .75, 1):
                name = f"tick-{checkpoint:03}-phase-{int(alpha * 100):03}.png"
                game.capture(folder / name, render_alpha=alpha, world_only=True)
                result = {"frame": name, "enemies": game.snapshot(full=False)["counts"]["enemies"]}
                if args.record == "after":
                    expected, actual = read(args.output / "before" / name), read(folder / name)
                    assert (actual.width, actual.height) == (expected.width, expected.height)
                    result["different_pixels"] = sum(actual.rgba[i:i+4] != expected.rgba[i:i+4]
                                                     for i in range(0, len(actual.rgba), 4))
                results.append(result)
            game.invariants()
    (folder / "result.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    failures = [result for result in results if result.get("different_pixels", 0)]
    print(json.dumps({"frames": len(results), "different_frames": failures, "output": str(folder)}))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
