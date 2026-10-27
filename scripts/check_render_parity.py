"""Record/compare full-quality native effects before and after render changes."""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests.snake_test import Game
from game_tests._png import read

SCENES = {
    "area_and_summons": ([('plague_mortar', True), ('venom_nova', True),
                           ('snake_nest', True), ('cobra_pit', True)], 300),
    "beams_and_fangs": ([('sidewinder_beam', True), ('venom_shot', True),
                          ('ricochet_fang', True), ('viper_fangs', True)], 73),
    "base_turrets_and_lobs": ([('plague_mortar', False), ('snake_nest', False),
                               ('cobra_pit', False), ('venom_nova', False)], 153),
    "base_projectiles_and_lash": ([('venom_shot', False), ('viper_fangs', False),
                                  ('ricochet_fang', False), ('tongue_lash', False)], 17),
    "tracking_and_grapples": ([('sidewinder_beam', False), ('tongue_lash', True),
                               ('cobra_pit', True), ('venom_nova', True)], 151),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', choices=('before', 'after'), required=True)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/performance/render-parity')
    args = parser.parse_args()
    output = args.output
    folder = output / args.record
    folder.mkdir(parents=True, exist_ok=True)
    results = []
    with Game(args.exe, native=True) as game:
        for name, (weapons, ticks) in SCENES.items():
            game.reset(seed=0xC0FFEE)
            for weapon, evolved in weapons:
                game.equip(weapon, 8, evolved)
            for passive in ('coiled_volley','toxic_expanse','chronofield','dead_eye'):
                game.equip(passive, 2 if passive == 'coiled_volley' else 8)
            game.spawn_enemies({'x':800+((i%15)-7)*8,'y':800+((i//15)-5)*8,
                                'hp':100000000,'speed':0,'kind':'boss' if i%31==0 else 'red'}
                               for i in range(150))
            game.step(ticks)
            path = folder / (name+'.png')
            game.capture(path, world_only=True)
            result = {'scene':name,'counts':game.snapshot(full=False)['counts']}
            if args.record == 'after':
                expected, actual = read(output/'before'/(name+'.png')), read(path)
                assert (actual.width,actual.height)==(expected.width,expected.height)
                differences = [i//4 for i in range(0,len(actual.rgba),4)
                               if actual.rgba[i:i+4] != expected.rgba[i:i+4]]
                result['different_pixels'] = len(differences)
                result['first_differences'] = [[p%actual.width,p//actual.width] for p in differences[:24]]
            results.append(result)
            print(json.dumps(result))
    (folder/'result.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    return 1 if any(result.get('different_pixels',0) for result in results) else 0


if __name__ == '__main__':
    raise SystemExit(main())
