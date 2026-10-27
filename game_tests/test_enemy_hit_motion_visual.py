"""Track real framebuffer pixels through surviving enemy hit reactions."""
import math

from . import _png
from . import snake_test as st

TIER = "visual"
OUTPUT = st.ROOT / "artifacts" / "gametest" / "enemy-hit-motion"


def _head(path):
    image = _png.read(path)
    red = [x for x in range(430, 580)
           if image.pixel(x, 254)[:3] in ((204, 0, 0), (255, 85, 85))]
    assert red, "The hit victim disappeared"
    return min(red) + 5


def test_native_lash_knockback_moves_through_intermediate_pixels():
    if not st.current().native:
        st.skip("Enemy hit motion requires the native framebuffer")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for hz in (60, 120, 144, 240):
        st.reset(seed=83)
        enemy = st.spawn_enemy(824, 800, hp=100000, speed=0, length=0)
        st.step(1)  # Initialize movement history before the hit.
        st.equip("tongue_lash")
        previous, ticks = 460, 0
        for frame in range(1, hz // 4 + 1):
            time = frame / hz
            while (ticks + 1) * st.FIXED_DT <= time + 1e-10:
                st.step(1)
                ticks += 1
            alpha = max(0, min(1, (time - ticks * st.FIXED_DT) / st.FIXED_DT))
            path = OUTPUT / f"lash-{hz}-{frame:03}.png"
            st.capture(path, world_only=True, render_alpha=alpha)
            actual = _head(path)
            expected = 460 + min(60, max(0, time - st.FIXED_DT) * 480)
            assert abs(actual - expected) <= 1, (hz, frame, actual, expected)
            assert 0 <= actual - previous <= math.ceil(480 / hz) + 1, (hz, frame, previous, actual)
            previous = actual
        victim = st.enemy(enemy)
        assert victim["position"] == [848, 800] and victim["hp"] == 99827, victim
        assert previous == 520, (hz, previous)
        # Raw fixture captures still show the authoritative logical position.
        raw = OUTPUT / f"lash-{hz}-raw.png"
        st.capture(raw, world_only=True)
        assert _head(raw) == 520
        st.invariants()


def test_native_death_during_knockback_stays_at_the_visible_victim():
    if not st.current().native:
        st.skip("Enemy death motion requires the native framebuffer")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    st.reset(seed=101)
    enemy = st.spawn_enemy(824, 800, hp=100000, speed=0, length=0)
    st.step(1)
    st.equip("tongue_lash")
    st.step(1)
    before = OUTPUT / "death-before.png"
    st.capture(before, world_only=True, render_alpha=1)
    position = _head(before)
    st.set_enemy(enemy, hp=1)
    st.equip("venom_nova", 8)
    st.equip("toxic_expanse", 8)
    st.step(1)
    assert st.enemy(enemy) is None
    for alpha in (0, .25, .5, .75, 1):
        path = OUTPUT / f"death-alpha-{alpha}.png"
        st.capture(path, world_only=True, render_alpha=alpha)
        image = _png.read(path)
        pixels = [x for x in range(430, 580)
                  if image.pixel(x, 254)[0] > 240 and image.pixel(x, 254)[1] > 180
                  and image.pixel(x, 254)[2] > 140]
        assert pixels, "The death flash disappeared"
        actual = min(pixels) + 5
        assert abs(actual - (position + alpha * 8)) <= 1, (alpha, position, actual)


test_native_lash_knockback_moves_through_intermediate_pixels.timeout_ticks = 80
test_native_death_during_knockback_stays_at_the_visible_victim.timeout_ticks = 5
