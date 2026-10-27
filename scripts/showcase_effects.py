"""Capture matched native weapon art before/after a renderer overhaul.

Run ``python scripts/showcase_effects.py --record before`` before rebuilding,
then ``--record after`` with the changed renderer. Both use the actual 800x500
raylib framebuffer. Changed pixels are expected; changed simulation is an error.
Only the standard library is required.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests.snake_test import Game
from game_tests._png import read


@dataclass(frozen=True)
class Scene:
    name: str
    weapon: str
    evolved: bool
    ticks: int
    title: str


# Individual families keep each silhouette readable. Extra lifecycle frames
# show the real lob, hatch, summon and turret actors, not injected stand-ins.
SCENES = (
    Scene("venom_shot", "venom_shot", False, 5, "Venom Shot"),
    Scene("ouroboros", "venom_shot", True, 36, "Ouroboros"),
    Scene("viper_fangs", "viper_fangs", False, 12, "Viper Fangs"),
    Scene("serpent_gatling", "viper_fangs", True, 7, "Serpent's Gatling"),
    Scene("mortar_flight", "plague_mortar", False, 20, "Plague Mortar / flight"),
    Scene("plague_mortar", "plague_mortar", False, 75, "Plague Mortar / poison pool"),
    Scene("singularity_mortar", "plague_mortar", True, 75, "Singularity Mortar"),
    Scene("nest_egg", "snake_nest", False, 25, "Snake Nest / egg"),
    Scene("snake_nest", "snake_nest", False, 105, "Snake Nest / hatchlings"),
    Scene("hydra_egg", "snake_nest", True, 25, "Hydra Brood / egg"),
    Scene("hydra_brood", "snake_nest", True, 105, "Hydra Brood / summons"),
    Scene("venom_nova", "venom_nova", False, 8, "Venom Nova"),
    Scene("miasma", "venom_nova", True, 45, "Miasma"),
    Scene("sidewinder_beam", "sidewinder_beam", False, 22, "Sidewinder Beam"),
    Scene("consumption_beam", "sidewinder_beam", True, 22, "Consumption Beam"),
    Scene("ricochet_fang", "ricochet_fang", False, 10, "Ricochet Fang"),
    Scene("shatter_fang", "ricochet_fang", True, 24, "Shatter Fang"),
    Scene("pit_flight", "cobra_pit", False, 20, "Cobra Pit / seed"),
    Scene("cobra_pit", "cobra_pit", False, 140, "Cobra Pit / turrets"),
    Scene("ancient_brood_pit", "cobra_pit", True, 170, "Ancient Brood Pit"),
    Scene("tongue_lash", "tongue_lash", False, 5, "Tongue Lash"),
    Scene("serpents_reckoning", "tongue_lash", True, 10, "Serpent's Reckoning"),
)


def prepare(game: Game, scene: Scene):
    game.reset(seed=0xC0FFEE)
    game.configure(invulnerable=False)
    # A large XP threshold keeps normal progression from interrupting fixtures.
    game.player(next_xp=1000000, invulnerable=0)
    game.equip(scene.weapon, 8, scene.evolved)
    game.equip("toxic_expanse", 4)
    game.equip("dead_eye", 8)
    # Sparse targets leave room to see effects. The front row gives fangs real
    # impacts and chaining; the surrounding targets allow multi-target beams.
    offsets = ((48, 0), (72, -24), (80, 24), (104, 0),
               (32, -48), (16, 48), (-48, -32), (-48, 48))
    if scene.weapon in ("venom_nova", "tongue_lash"):
        offsets = ((16, 0), (24, -16), (24, 16), (48, -32),
                   (64, 32), (-48, -40), (-64, 40))
    game.spawn_enemies(
        {"x": 800 + dx, "y": 800 + dy, "hp": 100000000,
         "speed": 0, "length": 2, "direction": [-1, 0],
         "kind": "splitter" if i % 3 == 2 else "red"}
        for i, (dx, dy) in enumerate(offsets))
    game.step(scene.ticks, validate=True)


def simulation_state(game: Game):
    state = game.snapshot()
    # CPU measurements naturally vary; every gameplay field must match exactly.
    state.pop("timing", None)
    return state


def differences(before: Path, after: Path):
    old, new = read(before), read(after)
    if (old.width, old.height) != (800, 500) or (new.width, new.height) != (800, 500):
        raise AssertionError("Showcases must capture the native 800x500 framebuffer")
    changed = [i // 4 for i in range(0, len(old.rgba), 4)
               if old.rgba[i:i + 4] != new.rgba[i:i + 4]]
    bounds = None
    if changed:
        xs, ys = [p % 800 for p in changed], [p // 800 for p in changed]
        bounds = [min(xs), min(ys), max(xs) + 1, max(ys) + 1]
    return {"different_pixels": len(changed), "changed_bounds": bounds}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", choices=("before", "after"), required=True)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/effects-overhaul")
    parser.add_argument("--scene", action="append", choices=[s.name for s in SCENES],
                        help="Capture only selected scenes (repeatable)")
    args = parser.parse_args()
    output = args.output.resolve()
    folder = output / args.record
    folder.mkdir(parents=True, exist_ok=True)
    results = []
    with Game(args.exe, native=True) as game:
        for scene in SCENES:
            if args.scene and scene.name not in args.scene:
                continue
            prepare(game, scene)
            state = simulation_state(game)
            image_path = folder / f"{scene.name}.png"
            game.capture(image_path, world_only=True)
            state_path = folder / f"{scene.name}.json"
            state_path.write_text(json.dumps(state, indent=2), encoding="utf-8")
            result = {"scene": scene.name, "title": scene.title,
                      "weapon": scene.weapon, "evolved": scene.evolved,
                      "ticks": scene.ticks, "counts": state["counts"],
                      "image": str(image_path)}
            if args.record == "after":
                previous = output / "before" / f"{scene.name}.json"
                old = json.loads(previous.read_text(encoding="utf-8"))
                if old != state:
                    fields = sorted(k for k in old.keys() | state.keys()
                                    if old.get(k) != state.get(k))
                    raise AssertionError(f"{scene.name}: simulation changed in {fields}")
                result["simulation_identical"] = True
                result.update(differences(output / "before" / image_path.name, image_path))
            results.append(result)
            print(json.dumps(result), flush=True)
    (folder / "result.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
