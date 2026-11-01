"""Capture actual grey interceptions and preserved enclosure escape routes."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests.snake_test import Game, EAST, SOUTH, WEST, NORTH


def drive(game, direction, cells):
    for _ in range(cells):
        before = game.player()['position']
        for _ in range(9):
            state = game.step(1, direction)
            if state['phase'] != 'playing':
                raise RuntimeError('Showcase route stopped playing')
            if state['player']['position'] != before:
                break
        else:
            raise RuntimeError('Showcase route was obstructed')


def wall_path(start, legs):
    path = [list(start)]
    for direction, cells in legs:
        for _ in range(cells):
            p = path[-1]
            path.append([p[0]+direction[0]*8,p[1]+direction[1]*8])
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/grey-prediction')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    samples = []
    with Game(args.exe, native=True) as game:
        def capture(name):
            state = game.snapshot()
            game.capture(args.output/(name+'.png'), world_only=False, render_alpha=1)
            samples.append({'scene': name, 'state': state})

        game.reset(seed=710)
        game.configure(grey_snakes=True, freeze_player=False)
        game.player(position=[320,800], length=7,
                    segments=[[320-i*8,800] for i in range(7)], move_clock=0)
        game.step(602, EAST)
        capture('straight-approach')
        game.step(48, EAST)
        capture('straight-crossing')
        for _ in range(120):
            state = game.step(1, EAST)
            if state['player']['collision_clock'] >= 0:
                capture('straight-blocked')
                break
        else:
            raise RuntimeError('Straight lane was not intercepted')
        drive(game, NORTH, 2)
        capture('straight-escape')

        game.reset(seed=710)
        game.configure(grey_snakes=True, freeze_player=False, grey_next_spawn=1200)
        game.player(position=[800,800], length=7,
                    segments=[[800-i*8,800] for i in range(7)], move_clock=0)
        for _ in range(2):
            for direction, cells in ((EAST,12),(SOUTH,10),(WEST,12),(NORTH,10)):
                drive(game, direction, cells)
        game.configure(grey_next_spawn=0)
        game.step(1)
        capture('rectangle-approach')
        game.step(48, EAST)
        capture('rectangle-crossing')
        for _ in range(180):
            p = game.player()['position']
            state = game.step(1, EAST if p[1] == 800 and p[0] < 896 else SOUTH)
            if state['player']['collision_clock'] >= 0:
                capture('rectangle-blocked')
                break
        else:
            raise RuntimeError('Learned rectangle was not intercepted')

        game.reset(seed=710)
        game.configure(invulnerable=False)
        game.player(position=[792,800], direction=NORTH, length=7,
                    segments=[[792,800+i*8] for i in range(7)], move_clock=0)
        path = wall_path([800,720], ((EAST,10),(SOUTH,20),(WEST,20),(NORTH,20),(EAST,8)))
        game.spawn_grey_snake(path, wall=True, wall_direction=EAST, target=[808,720], age=9.93)
        game.step(4)
        capture('loop-doorway-preserved')
        game.step(1)
        game.configure(freeze_player=False)
        drive(game, NORTH,20)
        capture('loop-escape')

        game.reset(seed=710)
        game.configure(invulnerable=False)
        game.player(position=[8,800], direction=SOUTH, length=7,
                    segments=[[8,800-i*8] for i in range(7)], move_clock=0)
        path = wall_path([8,720], ((EAST,9),(SOUTH,20),(WEST,8)))
        game.spawn_grey_snake(path, wall=True, wall_direction=WEST, target=[8,880], age=9.93)
        game.step(4)
        capture('arena-doorway-preserved')
        game.step(1)
        game.configure(freeze_player=False)
        drive(game, SOUTH,20)
        capture('arena-escape')
    (args.output/'showcase.json').write_text(json.dumps(samples, indent=2)+'\n', encoding='utf-8')
    print(args.output.resolve())


if __name__ == '__main__':
    main()
