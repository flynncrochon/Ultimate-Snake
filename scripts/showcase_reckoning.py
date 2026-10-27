"""Capture the real Reckoning extension, pull and bite before/after an art edit."""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests.snake_test import Game
from game_tests._png import read

SCENES = {
    "east": ((880, 800),),
    "diagonal": ((856, 760),),
    "volley": ((872, 800), (752, 768), (800, 864)),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", choices=("before", "after"), required=True)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/tongue-evolution")
    args = parser.parse_args()
    folder = args.output / args.record
    folder.mkdir(parents=True, exist_ok=True)
    states, stages = {}, {}
    with Game(args.exe, native=True) as game:
        for scene, targets in SCENES.items():
            game.reset(seed=0x70169)
            game.configure(invulnerable=False)
            game.player(position=[800, 800], direction=[1, 0], next_xp=1000000)
            game.equip("tongue_lash", 8, True, cooldown=100)
            if len(targets) > 1:
                game.equip("coiled_volley", 2)
            # Enable one cast, then postpone the next one beyond this clip.
            game.equip("tongue_lash", 8, True, cooldown=0)
            for x, y in targets:
                game.spawn_enemy(x, y, hp=1000000, speed=0, length=4, direction=[-1, 0])
            states[scene], stages[scene] = [], {}
            for tick in range(1, 125):
                state = game.step(1, full=True)
                state.pop("timing", None)
                states[scene].append(state)
                if tick == 1:
                    game.equip("tongue_lash", 8, True, cooldown=100)
                tongues = state["grapples"]
                stage = "extend" if any(t["phase"] == 0 for t in tongues) else "pull" if tongues else "bite" if state["zones"] else None
                if stage and stage not in stages[scene]:
                    stages[scene][stage] = tick
                if tick % 2 == 0:
                    game.capture(folder / f"{scene}-{tick:03d}.png", world_only=True, render_alpha=1)
            game.invariants()
    (folder / "states.json").write_text(json.dumps(states), encoding="utf-8")
    (folder / "stages.json").write_text(json.dumps(stages, indent=2), encoding="utf-8")
    print(json.dumps(stages))
    if args.record == "after":
        before = json.loads((args.output / "before/states.json").read_text(encoding="utf-8"))
        assert before == states, "Art edit changed simulation or RNG"
        different = {}
        for scene in SCENES:
            count = 0
            for tick in range(2, 125, 2):
                old = read(args.output / "before" / f"{scene}-{tick:03d}.png")
                new = read(folder / f"{scene}-{tick:03d}.png")
                count += sum(old.rgba[i:i+4] != new.rgba[i:i+4] for i in range(0, len(old.rgba), 4))
            different[scene] = count
        print(json.dumps({"simulation_checkpoints_equal": sum(map(len, states.values())), "different_pixels": different}))


if __name__ == "__main__":
    main()
