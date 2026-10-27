"""Native grey silhouettes, stationary walls, head turns, and minimap pixels."""
import math
from . import snake_test as st
from . import _png

TIER = 'visual'
OUTPUT = st.ROOT / 'artifacts/gametest/grey-snake'
GREY_COLORS = {bytes([v,v,v]) for v in (85,102,238,17)}


def _native():
    if not st.current().native:
        st.skip('Grey snake pixels require --native')
    OUTPUT.mkdir(parents=True, exist_ok=True)


def _round(x):
    return math.floor(x+.5)


def _mask(path, camera=(1600,1750)):
    """Independent oracle: stamp a 14px square at each centerline pixel."""
    points = [(_round(p[0]*2.5)-camera[0], _round(p[1]*2.5)-camera[1]) for p in path]
    occupied = set()

    def stamp(p):
        x,y = p
        for row in range(max(0,y-7), min(500,y+7)):
            occupied.update(range(row*800+max(0,x-7), row*800+min(800,x+7)))

    stamp(points[0])
    for a,b in zip(points,points[1:]):
        assert a[0] == b[0] or a[1] == b[1], (a,b)
        length = abs(a[0]-b[0])+abs(a[1]-b[1])
        dx = (b[0]>a[0])-(b[0]<a[0]); dy = (b[1]>a[1])-(b[1]<a[1])
        for i in range(length+1):
            stamp((a[0]+dx*i,a[1]+dy*i))
    return occupied


def _check(path, name, **capture):
    target = OUTPUT / (name+'.png')
    st.capture(target, world_only=True, **capture)
    image = _png.read(target)
    expected = _mask(path)
    # The white player also has dark pupils. Only count that shared dark ink
    # inside the grey silhouette; all grey body/pale-eye pixels are checked.
    actual = {i//4 for i in range(0,len(image.rgba),4)
              if image.rgba[i:i+3] in GREY_COLORS
              and (image.rgba[i:i+3] != bytes([17,17,17]) or i//4 in expected)}
    assert actual == expected, (name,len(actual),len(expected), sorted(actual ^ expected)[:20])
    return image


def test_grey_native_long_trails_and_all_head_directions():
    _native()
    # 160 historical cells exceed the normal 128-point player geometry buffer.
    long_path = [[704+(col if row%2==0 else 15-col)*8,704+row*8]
                 for row in range(10) for col in range(16)]
    st.spawn_grey_snake(long_path, alive=False)
    image = _check(long_path, 'long-permanent-wall')
    assert image.pixel(160,10) == (85,85,85,255)
    for rotation in range(4):
        def rotate(p):
            x,y = p[0]-800,p[1]-800
            for _ in range(rotation):
                x,y = -y,x
            return [x+800,y+800]
        path = list(map(rotate, [[824,760],[832,760],[840,760],[840,768]]))
        direction = rotate([800,801]); direction = [direction[0]-800,direction[1]-800]
        for phase in (0,.25,.5,.75,1):
            st.reset()
            st.spawn_grey_snake(path, direction=direction, move_clock=.06*phase)
            head = [path[-2][i]+(path[-1][i]-path[-2][i])*phase for i in range(2)]
            image = _check([*path[:-1],head], f'active-{rotation}-{phase}')
            assert bytes([238,238,238]) in image.rgba, 'Pale eyes missing'
        st.reset()
        st.spawn_grey_snake(path, alive=False)
        _check(path, f'permanent-{rotation}')


def test_grey_native_turn_interpolates_the_head_without_moving_old_walls():
    _native()
    path = [[840,760],[848,760]]
    st.spawn_grey_snake(path, direction=st.EAST, target=[856,760],
                        wall_direction=st.SOUTH, move_clock=.05)
    st.step(1)
    before = st.snapshot()
    assert before['grey_trails'][0]['path'][-1] == [848,768]
    for frame in range(25):
        alpha = frame/24
        # One native fixed update crosses the 60ms movement boundary.
        progress = (.05+st.FIXED_DT*alpha)/.06
        if progress < 1:
            expected = [[840,760],[840+8*progress,760]]
        else:
            expected = [[840,760],[848,760],[848,760+8*(progress-1)]]
        _check(expected, f'turn-{frame:02}', render_alpha=alpha)
    assert st.snapshot() == before, 'Captures changed grey timing or RNG'


def test_grey_native_minimap_and_collision_warning():
    _native()
    st.spawn_grey_snake([[864,760],[872,760]], direction=st.EAST, move_clock=.06)
    capture = OUTPUT / 'grey-minimap.png'
    st.capture(capture, world_only=False)
    image = _png.read(capture)
    x = int(686+872/1600*108); y = int(386+760/1600*108)
    r,g,b,_ = image.pixel(x,y)
    assert r >= 200 and r == g == b, image.pixel(x,y)

    st.reset()
    st.configure(freeze_player=False, invulnerable=False)
    st.spawn_grey_snake([[808,800]], alive=False)
    st.player(direction=st.EAST, move_clock=.119)
    st.step(56)
    capture = OUTPUT / 'grey-collision-warning.png'
    st.capture(capture, world_only=False)
    red,green,blue,_ = _png.read(capture).pixel(0,499)
    assert red > 40 and (green,blue) == (0,0)


def test_grey_native_retirement_finishes_the_head_edge_without_a_snap():
    _native()
    st.spawn_grey_snake([[840,760],[848,760]], direction=st.EAST, target=[856,760],
                        age=10-st.FIXED_DT, move_clock=.023333333)
    st.step(1)
    trail = st.snapshot()['grey_trails'][0]
    assert not trail['alive'] and trail['settling']
    for frame in range(9):
        alpha = frame/8
        progress = (.023333333+st.FIXED_DT*alpha)/.06
        _check([[840,760],[840+8*progress,760]], f'retirement-{frame}', render_alpha=alpha)
    st.step(1)
    _check([[840,760],[840+8*(.023333333+st.FIXED_DT*2)/.06,760]], 'retirement-ending')
    st.step(1)
    _check([[840,760],[848,760]], 'retirement-settled')
    assert not st.snapshot()['grey_trails'][0]['settling']
