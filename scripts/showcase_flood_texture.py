"""Capture matched native water-texture animations before and after a rebuild."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests.snake_test import Game


def fixture(game):
    game.reset(seed=1942)
    game.configure(invulnerable=False)
    body = [[800, y] for y in range(800, 825, 8)]
    body += [[x, 824] for x in range(792, 775, -8)]
    body += [[776, y] for y in range(816, 775, -8)]
    body += [[x, 776] for x in range(784, 825, 8)]
    body += [[824, y] for y in range(784, 825, 8)]
    body += [[816, 824], [808, 824]]
    game.player(segments=body, direction=[1, 0])
    handles = [game.spawn_enemy(920, y, hp=1e8, speed=40, length=5)
               for y in (744, 800, 856)]
    game.step(30)
    for handle in handles:
        game.set_enemy(handle, speed=0)
    game.equip('viper_fangs', 2, cooldown=1.0)
    game.spawn_flood(872, 824, count=768, warning=0)
    game.step(15)


def record(folder, frames, executable):
    folder.mkdir(parents=True, exist_ok=True)
    report = []
    with Game(executable, native=True) as game:
        fixture(game)
        for frame in range(frames):
            state = game.snapshot()
            state.pop('timing', None)
            # Presentation impulses are intentionally new in the breaking-wave
            # renderer. Compare the complete physical/combat state separately.
            state.pop('water_impacts', None)
            state['counts'].pop('water_impacts', None)
            digest = hashlib.sha256(json.dumps(state, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
            assert state['phase'] == 'playing', (frame, state['notice'])
            game.capture(folder / f'frame-{frame:03d}.png', render_alpha=0.5)
            report.append({'frame': frame, 'tick': state['tick'], 'state_sha256': digest})
            if frame + 1 < frames:
                game.step(2)
    (folder / 'state-digests.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'captured': len(report), 'folder': str(folder), 'fps': 30}), flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', choices=('before', 'after'))
    parser.add_argument('--frames', type=int, default=120)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--allow-flow-changes', action='store_true', help='Compare matched ticks when wave physics intentionally changes')
    parser.add_argument('--require-pixel-parity', action='store_true', help='Require the restored renderer to match every reference RGBA pixel')
    parser.add_argument('--output', type=Path, default=ROOT / 'artifacts/flood-texture')
    args = parser.parse_args()
    if args.record:
        report = record(args.output / args.record, args.frames, args.exe)
        if args.record == 'after':
            before = json.loads((args.output / 'before/state-digests.json').read_text())
            assert [(r['frame'],r['tick']) for r in before] == [(r['frame'],r['tick']) for r in report], 'Comparison timing differs'
            if args.allow_flow_changes:
                print(f'Matched native capture timing; physical state identical: {before == report}')
            else:
                assert before == report, 'Texture changes altered seeded simulation or capture timing'
                print('Verified: every before/after frame has identical native simulation state.')
            if args.require_pixel_parity:
                from scripts.build_flood_texture_gif import read
                for row in report:
                    name = f"frame-{row['frame']:03d}.png"
                    old, restored = read(args.output/'before'/name), read(args.output/'after'/name)
                    assert (old.width, old.height, old.rgba) == (restored.width, restored.height, restored.rgba), f'Original pixels differ: {name}'
                print(f'Verified: all {len(report)} restored native frames match the original RGBA pixels exactly.')


if __name__ == '__main__':
    main()
