"""Measure actual native pixels over time, rather than only static turn masks."""
import json
import math
from pathlib import Path

from . import snake_test as st
from . import _png

TIER = "visual"
OUTPUT = st.ROOT / "artifacts" / "gametest" / "smooth-motion"


def _setup(x=8, y=64):
    if not st.current().native:
        st.skip("Temporal framebuffer checks require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    st.reset(seed=71)
    st.configure(freeze_player=False, invulnerable=False)
    path = [[x, y] for _ in range(7)]
    st.player(position=[x,y], segments=path, previous_segments=path,
              direction=[1,0], move_clock=.12, move_interval=.12, invulnerable=0)


def _frames(hz, duration=.25):
    ticks = 0
    for frame in range(1, round(hz * duration) + 1):
        time = frame / hz
        while (ticks + 1) * st.FIXED_DT <= time + 1e-10:
            st.step(1)
            ticks += 1
        fraction = max(0, min(1, (time - ticks * st.FIXED_DT) / st.FIXED_DT))
        capture = st.capture(OUTPUT / "last.png", world_only=True, render_alpha=fraction)
        yield frame, time, _png.read(capture["path"])


def _fail(context):
    (OUTPUT / "failure.json").write_text(json.dumps(context, indent=2), encoding="utf-8")
    raise AssertionError(context)


def test_native_head_moves_at_display_cadence_without_cell_boundary_stalls():
    speed = st.TILE / .12 * 2.5
    for hz in (60, 120, 144, 240):
        _setup()
        previous = None
        for frame, time, image in _frames(hz):
            white = [x for x in range(1, 400) if image.pixel(x, 160)[:3] == (255,255,255)]
            assert white, (hz, frame)
            actual = max(white) - 6  # Right edge of the fixed 14px head block.
            expected = math.floor(20 + max(0, time - st.FIXED_DT) * speed + .5)
            if abs(actual - expected) > 1:
                _fail({"hz":hz,"frame":frame,"actual":actual,"expected":expected,"state":st.snapshot()})
            if previous is not None and not 0 <= actual - previous <= math.ceil(speed / hz):
                _fail({"hz":hz,"frame":frame,"delta":actual-previous,"previous":previous,"actual":actual})
            previous = actual


def test_native_camera_and_stationary_enemy_share_the_same_smooth_pixel_motion():
    _setup(800,800)
    st.spawn_enemy(864,800,hp=100000,speed=0,length=0)
    previous = None
    speed = st.TILE / .12 * 2.5
    for frame,time,image in _frames(240):
        red = [x for x in range(400,650) if image.pixel(x,250)[:3] == (204,0,0)]
        assert red, frame
        actual = min(red) + 5
        expected = 560 - math.floor(max(0,time-st.FIXED_DT)*speed+.5)
        if abs(actual-expected)>1 or (previous is not None and not -1<=actual-previous<=0):
            _fail({"frame":frame,"actual":actual,"expected":expected,"previous":previous,"state":st.snapshot()})
        previous=actual


def test_native_moving_enemy_stays_smooth_inside_an_1800_snake_swarm():
    if not st.current().native:
        st.skip("Temporal framebuffer checks require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for hz in (60, 120, 144, 240):
        st.reset(seed=79)
        st.spawn_enemies({"x": 800 + ((i % 32) - 16) * 8,
                          "y": 800 + (((i // 32) % 20) - 10) * 8,
                          "hp": 100000000, "speed": 24 + (i % 4) * 16,
                          "kind": "splitter", "length": 8}
                         for i in range(1799))
        target = st.spawn_enemy(936, 800, hp=100000000, speed=96, length=0)
        st.step(20)
        enemy = st.enemy(target)
        phase = enemy["move_clock"] / enemy["move_interval"]
        origin = enemy["previous"][0] + (enemy["position"][0] - enemy["previous"][0]) * phase
        previous = None
        for frame, time, image in _frames(hz):
            red = [x for x in range(450, 799) if image.pixel(x, 250)[:3] == (204, 0, 0)]
            assert red, (hz, frame, "Tracked enemy disappeared inside the swarm")
            actual = min(red) + 5
            expected = math.floor((origin - 96 * (time - st.FIXED_DT)) * 2.5 + .5) - 1600
            if abs(actual - expected) > 1 or (previous is not None and not -math.ceil(240 / hz) <= actual - previous <= 0):
                _fail({"hz": hz, "frame": frame, "actual": actual, "expected": expected,
                       "previous": previous, "enemy": st.enemy(target)})
            previous = actual
        assert st.snapshot(full=False)["counts"]["enemies"] == 1800


def test_native_moving_enemies_never_backstep_against_the_tracking_camera():
    if not st.current().native:
        st.skip("Temporal framebuffer checks require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for direction in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        axis = 0 if direction[0] else 1
        sign = direction[axis]
        for hz in (60, 120, 144, 240):
            _setup(800, 800)
            st.player(direction=direction)
            st.spawn_enemy(800 - direction[0] * 32, 800 - direction[1] * 32,
                           hp=1000000, speed=24, kind="splitter", length=0)
            st.step(60, direction=direction)
            previous = None
            bound = math.ceil((st.TILE / .12 - 24) * 2.5 / hz)
            for frame, time, image in _frames(hz):
                blue = [p for p in range(image.width if axis == 0 else image.height)
                        if image.pixel(p if axis == 0 else 400, 250 if axis == 0 else p)[:3] == (34, 102, 204)]
                assert blue, (direction, hz, frame, "Tracked enemy disappeared")
                actual = (min(blue) + 5) * sign
                if previous is not None and not -bound <= actual - previous <= 0:
                    _fail({"direction": direction, "hz": hz, "frame": frame,
                           "previous": previous, "actual": actual, "delta": actual - previous})
                previous = actual


test_native_head_moves_at_display_cadence_without_cell_boundary_stalls.timeout_ticks = 120
test_native_camera_and_stationary_enemy_share_the_same_smooth_pixel_motion.timeout_ticks = 30
test_native_moving_enemy_stays_smooth_inside_an_1800_snake_swarm.timeout_ticks = 180
test_native_moving_enemies_never_backstep_against_the_tracking_camera.timeout_ticks = 1400
