"""Exercise the real pull-to-bite animation on the native pixel grid."""
import math

from . import _png
from . import snake_test as st

TIER = "visual"
OUTPUT = st.ROOT / "artifacts/gametest/reckoning"
JAW_COLORS = {(67, 35, 57), (156, 132, 160), (199, 189, 209),
              (242, 239, 237), (255, 242, 239)}


def _native():
    if not st.current().native:
        st.skip("Reckoning pixels require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)


def _prepare(dx=64, dy=0):
    st.reset(seed=0x70169)
    st.configure(invulnerable=False)
    st.player(position=[800, 800], direction=st.EAST, length=1, next_xp=1000000)
    st.equip("tongue_lash", 8, True)
    st.spawn_enemy(800+dx, 800+dy, hp=1000000, speed=0, length=4)
    st.step(1)
    st.equip("tongue_lash", 8, True, cooldown=100)


def _bite(dx=64, dy=0):
    _prepare(dx, dy)
    last_target = None
    for _ in range(160):
        state = st.snapshot()
        if state["zones"]:
            assert state["kills"] == 1 and not state["grapples"]
            return last_target
        if state["enemies"]:
            last_target = state["enemies"][0]["position"]
        st.step(1)
    raise AssertionError("Reckoning never reached its actual devour")


def _capture(label, **options):
    path = OUTPUT / (label + ".png")
    st.capture(path, world_only=True, **options)
    frame = _png.read(path)
    assert (frame.width, frame.height) == (800, 500)
    return frame


def _jaws(frame):
    return {(x-400, y-250) for y in range(180, 321) for x in range(330, 471)
            if frame.pixel(x, y)[:3] in JAW_COLORS}


def test_jaws_face_the_devoured_target_and_snap_in_all_eight_directions():
    _native()
    for dx, dy in ((64, 0), (64, 64), (0, 64), (-64, 64),
                   (-64, 0), (-64, -64), (0, -64), (64, -64)):
        target = _bite(dx, dy)
        vx, vy = target[0]-800, target[1]-800
        length = math.hypot(vx, vy)
        assert length > 0
        vx, vy = vx/length, vy/length
        opened = _jaws(_capture(f"{dx}-{dy}-open"))
        assert len(opened) > 600, (dx, dy, "Missing white jaw banks or teeth")
        forward = sum(x*vx+y*vy for x, y in opened)/len(opened)
        assert forward > 5, (dx, dy, "Jaws face away from captured prey", forward)
        st.step(6)
        closed = _jaws(_capture(f"{dx}-{dy}-clamped"))
        assert len(closed) > 600
        width = lambda points: max(-x*vy+y*vx for x, y in points)-min(-x*vy+y*vx for x, y in points)
        assert width(opened) > width(closed)+35, (dx, dy, "Jaws did not snap shut")
        st.step(32)
        assert not st.snapshot()["zones"]
        assert not _jaws(_capture(f"{dx}-{dy}-finished")), "Jaw pixels survived effect expiry"


test_jaws_face_the_devoured_target_and_snap_in_all_eight_directions.timeout_ticks = 1600


def test_bite_keeps_its_heading_when_the_player_turns_and_freezes_on_pause():
    _native()
    _bite()
    before = _jaws(_capture("before-turn"))
    st.player(direction=st.NORTH)
    assert _jaws(_capture("after-turn")) == before, "Turning rotated an existing bite"
    st.pause()
    paused = _capture("paused")
    elapsed = st.snapshot()["elapsed"]
    st.step(30)
    assert st.snapshot()["elapsed"] == elapsed
    assert _capture("paused-repeat").rgba == paused.rgba, "Paused jaws kept animating"


def test_pull_and_snap_sampling_preserves_future_combat_and_interpolation():
    _native()
    for stage, ticks in (("extension", 4), ("pull", 22), ("bite", None)):
        if ticks is None:
            _bite()
            st.step(4)
        else:
            _prepare()
            st.step(ticks)
        original = st.snapshot()
        original.pop("timing", None)
        for alpha in (0, .5, 1):
            frame = _capture(f"{stage}-{alpha}", render_alpha=alpha)
            assert _capture(f"{stage}-{alpha}-repeat", render_alpha=alpha).rgba == frame.rgba
        actual = st.snapshot()
        actual.pop("timing", None)
        assert actual == original, "Rendering changed combat state"
        if stage == "bite":
            previous = _jaws(_capture("before-update", render_alpha=1))
            st.step(1)
            assert _jaws(_capture("after-update", render_alpha=0)) == previous, "Jaw pose jumped at an update boundary"
        st.step(40)
        sampled = st.snapshot()
        sampled.pop("timing", None)
        if ticks is None:
            _bite()
            st.step(5)
        else:
            _prepare()
            st.step(ticks)
        st.step(40)
        unsampled = st.snapshot()
        unsampled.pop("timing", None)
        assert unsampled == sampled, "Rendering changed future combat/RNG"


test_pull_and_snap_sampling_preserves_future_combat_and_interpolation.timeout_ticks = 700
