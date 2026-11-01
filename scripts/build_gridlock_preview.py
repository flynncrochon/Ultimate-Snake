"""Capture the real turn-charged Gridlock passive and optionally encode a GIF."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from game_tests.snake_test import Game


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--ffmpeg',type=Path)
    parser.add_argument('--evolved',action='store_true',help='Earn Singularity Mortar through a chest before capturing')
    args = parser.parse_args()
    output = (args.output or ROOT/'artifacts/graviton-gridlock'/('evolved' if args.evolved else '')).resolve()
    gif = output/('gridlock-singularity.gif' if args.evolved else 'gridlock.gif')
    frames = output/'frames'
    frames.mkdir(parents=True,exist_ok=True)
    report = []
    directions = ((0,1),(1,0),(0,-1),(-1,0))
    with Game(native=True) as game:
        game.reset(seed=0x610C)
        game.configure(freeze_player=False,invulnerable=False)
        game.player(hp=1000,max_hp=1000,next_xp=1000000)
        game.equip('gridlock',rank=8)
        if args.evolved:
            game.equip('plague_mortar',rank=8,cooldown=1000)
            game.spawn_pickup(kind='chest')
            evolved=game.step(1)
            assert evolved['chest_rewards'][0]['title']=='Singularity Mortar', evolved
        # A bounded ring of actual pursuers approaches while the player turns.
        for index in range(20):
            side,offset = divmod(index,5)
            lane = (offset-2)*16
            x,y = ((864,800+lane),(800+lane,864),(752,800+lane),(800+lane,744))[side]
            game.spawn_enemy(x,y,hp=100000,speed=16,kind='splitter' if index%5==0 else 'swift' if index%7==0 else 'red',length=3)
        for frame in range(126):
            tick = frame*2
            state = game.step(2,direction=directions[(tick//18)%4],full=True,validate=True)
            game.capture(frames/f'frame-{frame:03d}.png',world_only=True,render_alpha=.5)
            report.append({'frame':frame,'tick':state['tick'],'turns':state['graviton']['turns'],
                           'cast':state['graviton']['serial'],'pinned':sum(enemy['gravity_hold']>0 for enemy in state['enemies']),
                           'center':state['graviton']['center'],'player':state['player']['position'],
                           'empowered':state['graviton']['empowered']})
    assert max(frame['pinned'] for frame in report)>=5, 'Demo failed to pin a group of real enemies'
    assert report[-1]['cast']>=1 and report[-1]['pinned']==0, 'Demo did not show release'
    assert not args.evolved or any(frame['empowered'] for frame in report), 'Evolution did not inherit the trap'
    (output/'capture-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    if args.ffmpeg:
        subprocess.run([str(args.ffmpeg),'-y','-hide_banner','-loglevel','error',
            '-framerate','30','-i',str(frames/'frame-%03d.png'),'-frames:v','126',
            '-filter_complex','[0:v]crop=560:380:120:60,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=none',
            '-loop','0',str(gif)],check=True)
    print(json.dumps({'frames':len(report),'fps':30,'maximum_pinned':max(frame['pinned'] for frame in report),
                      'gif':str(gif) if args.ffmpeg else None}))


if __name__=='__main__':
    main()
