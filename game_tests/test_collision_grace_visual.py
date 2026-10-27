"""Check the source collision warning and stationary body in native pixels."""
from . import snake_test as st
from . import _png

TIER = "visual"
OUTPUT = st.ROOT / "artifacts/gametest/collision-grace"


def _state():
    state = st.snapshot()
    state.pop("timing", None)
    return state


def test_body_and_wall_grace_warn_then_clear_on_a_safe_turn():
    if not st.current().native:
        st.skip("Collision warning pixels require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for obstacle in ("body", "wall"):
        st.reset(seed=710)
        st.configure(freeze_player=False, invulnerable=False)
        path = ([[800,800], [800,808], [808,808], [808,800],
                 [816,800], [816,792], [808,792]] if obstacle == "body"
                else [[1592 - i * 8, 800] for i in range(7)])
        st.player(segments=path, direction=[0,-1] if obstacle == "body" else [1,0],
                  move_clock=.119, invulnerable=0)
        st.step(1, st.EAST)
        assert st.player()["collision_clock"] == 0
        blocked = OUTPUT / f"{obstacle}-blocked.png"
        st.capture(blocked, world_only=True)
        original_world = _png.read(blocked)
        st.step(40)
        early = OUTPUT / f"{obstacle}-early.png"
        st.capture(early, world_only=False)
        assert _png.read(early).pixel(0, 499) == (0, 0, 0, 255)

        st.step(15)
        before = _state()
        clock = before["player"]["collision_clock"]
        assert .9 < clock < 1 and before["phase"] == "playing"
        for alpha in (0, .5, 1):
            late = OUTPUT / f"{obstacle}-warning-{alpha}.png"
            st.capture(late, world_only=False, render_alpha=alpha)
            image = _png.read(late)
            assert (image.width, image.height) == (800, 500)
            red, green, blue, _ = image.pixel(0, 499)
            # JavaScript: red alpha ramps from 0 to .3 in the final .3 s.
            assert abs(red - 255 * (clock - .7)) <= 2, (clock, red)
            assert (green, blue) == (0, 0)
            world = OUTPUT / f"{obstacle}-held-{alpha}.png"
            st.capture(world, world_only=True, render_alpha=alpha)
            assert _png.read(world).rgba == original_world.rgba, "Blocked body or camera drifted"
            assert _state() == before, "Display sampling changed collision timing"

        st.pause()
        st.step(90)
        assert st.player()["collision_clock"] == clock
        st.pause()
        st.step(8, st.NORTH)
        after = st.snapshot()
        assert after["phase"] == "playing" and after["player"]["collision_clock"] < 0
        assert after["player"]["position"][1] < 800
        recovered = OUTPUT / f"{obstacle}-recovered.png"
        st.capture(recovered, world_only=False)
        assert _png.read(recovered).pixel(0, 499) == (0, 0, 0, 255)


test_body_and_wall_grace_warn_then_clear_on_a_safe_turn.timeout_ticks = 320
