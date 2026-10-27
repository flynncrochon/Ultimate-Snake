"""Exact native framebuffer checks for clean cardinal snake turns.

The oracle walks the historical centerline pixel by pixel, trims head and tail
travel, and stamps fixed-width squares. It does not call the C++ geometry code.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
import shutil
import struct

from . import snake_test as st
from . import _png

TIER = "visual"
WIDTH, HEIGHT, WORLD, SCALE, STROKE = 800, 500, 4000, 2.5, 14
ALPHAS = (0, .1, .25, .5, .75, .9, 1)
ARTIFACTS = st.ROOT / "artifacts" / "gametest" / "turns"


def _native():
    if not st.current().native or not st.current().capabilities.get("native_capture"):
        st.skip("Native turn pixels require --native and the raylib test host")
    ARTIFACTS.mkdir(parents=True, exist_ok=True)


def _f32(value):
    return struct.unpack("f", struct.pack("f", value))[0]


def _round(value):
    return math.floor(value + .5) if value >= 0 else math.ceil(value - .5)


def _project(point):
    return _round(point[0] * SCALE), _round(point[1] * SCALE)


def _distance(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])


def _sign(value):
    return (value > 0) - (value < 0)


def _frame(previous, direction):
    previous = [tuple(point) for point in previous]
    head = (previous[0][0] + direction[0] * st.TILE,
            previous[0][1] + direction[1] * st.TILE)
    return {"previous": previous, "current": [head, *previous[:-1]], "direction": direction}


def _rotate(frame, count):
    def point(value):
        x, y = value[0] - 800, value[1] - 800
        for _ in range(count):
            x, y = -y, x
        return x + 800, y + 800
    dx, dy = frame["direction"]
    for _ in range(count):
        dx, dy = -dy, dx
    return {"previous": [point(p) for p in frame["previous"]],
            "current": [point(p) for p in frame["current"]], "direction": (dx, dy)}


def _oracle(frame, alpha):
    previous, current = list(map(_project, frame["previous"])), list(map(_project, frame["current"]))
    step = max(_distance(a, b) for a, b in zip(previous, current))
    progress = math.floor(step * max(0, min(1, alpha)) + .5)
    hx = previous[0][0] + _sign(current[0][0] - previous[0][0]) * min(progress, _distance(previous[0], current[0]))
    hy = previous[0][1] + _sign(current[0][1] - previous[0][1]) * min(progress, _distance(previous[0], current[0]))
    camera = (max(0, min(WORLD - WIDTH, hx - WIDTH // 2)),
              max(0, min(WORLD - HEIGHT, hy - HEIGHT // 2)))
    route = [current[0]]
    for point in previous:
        if point != route[-1]:
            route.append(point)
    total = sum(_distance(a, b) for a, b in zip(route, route[1:]))
    head_move, tail_move = _distance(previous[0], current[0]), _distance(previous[-1], current[-1])
    start, finish = (head_move - progress if head_move else 0), total - (progress if tail_move else 0)
    occupied = set()

    def stamp(point):
        x, y = point[0] - camera[0] - STROKE // 2, point[1] - camera[1] - STROKE // 2
        left, right = max(0, x), min(WIDTH, x + STROKE)
        top, bottom = max(0, y), min(HEIGHT, y + STROKE)
        for row in range(top, bottom):
            occupied.update(range(row * WIDTH + left, row * WIDTH + right))

    if len(route) == 1:
        stamp(route[0])
    traveled = 0
    for a, b in zip(route, route[1:]):
        assert a[0] == b[0] or a[1] == b[1], ("invalid diagonal fixture", frame)
        length = _distance(a, b)
        for offset in range(max(0, start - traveled), min(length, finish - traveled) + 1):
            stamp((a[0] + _sign(b[0] - a[0]) * offset, a[1] + _sign(b[1] - a[1]) * offset))
        traveled += length
    head = (hx - camera[0], hy - camera[1])
    return occupied, head, camera


def _border(camera):
    excluded = set()
    if camera[0] == 0:
        excluded.update(row * WIDTH + column for row in range(HEIGHT) for column in range(2))
    if camera[0] + WIDTH == WORLD:
        excluded.update(row * WIDTH + column for row in range(HEIGHT) for column in range(WIDTH - 2, WIDTH))
    if camera[1] == 0:
        excluded.update(range(WIDTH * 2))
    if camera[1] + HEIGHT == WORLD:
        excluded.update(range((HEIGHT - 2) * WIDTH, HEIGHT * WIDTH))
    return excluded


def _foreground(image):
    rgba = image.rgba
    return {offset // 4 for offset in range(0, len(rgba), 4)
            if rgba[offset + 3] and (rgba[offset] or rgba[offset + 1] or rgba[offset + 2])}


def _check(frame, alpha, label, *, character=0, interval=.12):
    st.reset(character=character)
    st.configure(invulnerable=False)
    player = st.player(segments=frame["current"], previous_segments=frame["previous"],
                       direction=list(frame["direction"]), move_clock=alpha * interval,
                       move_interval=interval, invulnerable=0)
    _assert_frame(frame, player, alpha, label, character)


def _assert_frame(frame, player, alpha, label, character=0):
    # Use the submitted native float clock, including its exact division/rounding
    # at half-pixel fractions. The oracle itself remains independent of geometry.
    displayed_alpha = _f32(player["move_clock"] / player["move_interval"])
    expected, head, camera = _oracle(frame, displayed_alpha)
    path = ARTIFACTS / "last.png"
    st.capture(path, world_only=True)
    image = _png.read(path)
    assert (image.width, image.height) == (WIDTH, HEIGHT), (image.width, image.height)
    actual = _foreground(image)
    ignored = _border(camera)
    # Eyes/slit pupils are intentional holes. Any colored pixel beyond the head
    # still has to belong to the body, catching the former detached venom mark.
    for y in range(max(0, head[1] - STROKE // 2), min(HEIGHT, head[1] - STROKE // 2 + STROKE)):
        ignored.update(range(y * WIDTH + max(0, head[0] - STROKE // 2),
                             y * WIDTH + min(WIDTH, head[0] - STROKE // 2 + STROKE)))
    extra, missing = actual - expected - ignored, expected - actual - ignored
    if extra or missing:
        safe_label = "".join(char if char.isalnum() or char in "_-" else "_" for char in label)
        failure = ARTIFACTS / (safe_label + ".png")
        shutil.copyfile(path, failure)
        details = {"case": label, "alpha": alpha, "native_alpha": displayed_alpha,
                   "camera": camera, "head": head, "character": character,
                   "extra_count": len(extra), "missing_count": len(missing),
                   "extra": [(pixel % WIDTH, pixel // WIDTH) for pixel in sorted(extra)[:24]],
                   "missing": [(pixel % WIDTH, pixel // WIDTH) for pixel in sorted(missing)[:24]],
                   "frame": frame, "capture": str(failure), "state": st.snapshot()}
        failure.with_suffix(".json").write_text(json.dumps(details, indent=2), encoding="utf-8")
        raise AssertionError(details)


def test_cardinal_l_s_and_tight_turns_have_exact_native_pixels():
    _native()
    frames = {
        "straight": _frame([(800, 800), (792, 800), (784, 800), (776, 800)], (1, 0)),
        "left": _frame([(800, 800), (792, 800), (784, 800), (776, 800)], (0, -1)),
        "s": _frame([(800, 800), (792, 800), (792, 808), (784, 808), (784, 816)], (0, -1)),
        "tight": _frame([(800, 800), (792, 800), (792, 808), (800, 808), (808, 808), (808, 816)], (0, -1)),
    }
    for name, frame in frames.items():
        for rotation in range(4):
            for alpha in ALPHAS:
                _check(_rotate(frame, rotation), alpha, f"{name}-r{rotation}-a{alpha}")


def test_long_spiral_growth_and_wall_clamps_have_exact_native_pixels():
    _native()
    points = [(800, 800)]
    directions = ((-1, 0), (0, 1), (1, 0), (0, -1))
    leg = 0
    while len(points) < 120:
        dx, dy = directions[leg % 4]
        for _ in range(1 + leg // 2):
            if len(points) == 120:
                break
            points.append((points[-1][0] + dx * 8, points[-1][1] + dy * 8))
        leg += 1
    frames = {
        "spiral120": _frame(points, (0, -1)),
        "growth": _frame([(800, 800), (792, 800), (784, 800), (784, 800)], (0, -1)),
        "wall": {"previous": [(8, 800), (16, 800), (24, 800), (32, 800)],
                 "current": [(8, 800), (8, 800), (16, 800), (24, 800)], "direction": (-1, 0)},
    }
    for name, frame in frames.items():
        for rotation in range(4):
            for alpha in ALPHAS:
                _check(_rotate(frame, rotation), alpha, f"{name}-r{rotation}-a{alpha}")


def test_fast_movement_interpolation_uses_the_same_clean_pixel_grid():
    _native()
    frame = _frame([(800, 800), (792, 800), (792, 808), (784, 808)], (0, -1))
    for rotation in range(4):
        for alpha in ALPHAS:
            _check(_rotate(frame, rotation), alpha, f"fast-r{rotation}-a{alpha}", interval=.06)


def test_faces_do_not_leave_detached_pixels_during_turns():
    _native()
    frame = _frame([(800, 800), (792, 800), (784, 800)], (0, -1))
    for character in (0, 1, 3):
        for rotation in range(4):
            for alpha in (0, .25, .5, .75, 1):
                _check(_rotate(frame, rotation), alpha,
                       f"face-c{character}-r{rotation}-a{alpha}", character=character)



def test_actual_game_turn_history_has_no_spurious_pixels():
    _native()
    st.reset(character=0)
    st.configure(invulnerable=False, freeze_player=False)
    st.player(position=[1200, 800], length=120, direction=list(st.EAST),
              invulnerable=0, move_clock=0)
    for turn, direction in enumerate((st.NORTH, st.WEST, st.SOUTH, st.EAST) * 3):
        for phase in range(3):
            st.step(2, direction)
            state = st.snapshot()["player"]
            frame = {"previous": state["previous_segments"],
                     "current": state["segments"], "direction": state["direction"]}
            alpha = _f32(state["move_clock"] / state["move_interval"])
            _assert_frame(frame, state, alpha, f"live-turn{turn}-phase{phase}")
