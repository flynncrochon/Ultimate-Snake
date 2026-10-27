"""Death pixels are deterministic, bounded in time, and separate from combat."""
from . import snake_test as st
from . import _png

TIER = "visual"
OUTPUT = st.ROOT / "artifacts/gametest/death-integration"


def _native():
    if not st.current().native:
        st.skip("Death captures require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)


def _state():
    state = st.snapshot()
    state.pop("timing", None)
    return state


def test_lethal_weapon_hit_keeps_a_repeatable_death_pose():
    _native()
    st.reset(seed=123)
    st.spawn_enemy(896, 800, hp=1, speed=40, kind="red", length=8)
    st.step(60)
    st.equip("venom_nova", 8)
    st.equip("toxic_expanse", 8)
    st.step(1)
    assert st.snapshot(full=False)["kills"] == 1
    assert st.snapshot(full=False)["counts"]["snake_deaths"] == 1
    st.step(8)
    before = _state()
    for alpha in (0, .5, 1):
        path = OUTPUT / f"weapon-death-{alpha}.png"
        repeat = OUTPUT / f"weapon-death-{alpha}-repeat.png"
        st.capture(path, render_alpha=alpha)
        st.capture(repeat, render_alpha=alpha)
        assert _png.read(path).rgba == _png.read(repeat).rgba
        assert _state() == before
    st.step(60)
    assert st.snapshot(full=False)["counts"]["snake_deaths"] == 0


def test_all_player_deaths_finish_before_results_without_advancing_gameplay():
    _native()
    for character in range(4):
        st.reset(seed=123, character=character)
        st.configure(invulnerable=False)
        st.player(hp=0, segments=[[800, 800], [792, 800], [784, 800],
                                  [784, 808], [784, 816], [776, 816], [768, 816]])
        st.step(1)
        before = _state()
        assert before["phase"] == "dead"
        occupied = []
        for age in (0, .30, .72):
            path = OUTPUT / f"player-{character}-{age}.png"
            st.capture(path, death_time=age)
            image = _png.read(path)
            assert (image.width, image.height) == (800, 500)
            occupied.append(sum(image.rgba[i:i+3] != b"\0\0\0"
                                for i in range(0, len(image.rgba), 4)))
            assert _state() == before
        assert occupied[0] > occupied[1] > occupied[2] == 0
        path = OUTPUT / f"player-{character}-results.png"
        st.capture(path, death_time=.8, world_only=False)
        assert _state() == before


def test_bad_death_preview_time_leaves_capture_host_usable():
    _native()
    for age in (-1, 3, "bad"):
        try:
            st.capture(OUTPUT / "invalid.png", death_time=age)
        except st.GameTestError:
            pass
        else:
            raise AssertionError(f"Accepted invalid death_time {age!r}")
    st.capture(OUTPUT / "recovered.png")


def test_drawing_deaths_does_not_change_future_loot_or_combat():
    _native()
    outcomes = []
    for render in (False, True):
        st.reset(seed=987)
        st.spawn_enemy(832, 800, hp=1, speed=0)
        st.equip("venom_nova", 8)
        st.equip("toxic_expanse", 8)
        st.step(1)
        assert st.snapshot(full=False)["counts"]["snake_deaths"] == 1
        if render:
            for alpha in (0, .25, .5, .75, 1):
                st.capture(OUTPUT / f"rng-{alpha}.png", render_alpha=alpha)
        st.spawn_enemies({"x": 800+(i % 8-4)*8, "y": 800+(i//8-4)*8,
                          "hp": 1, "speed": 0} for i in range(64))
        st.step(120)
        outcomes.append(_state())
    assert outcomes[0] == outcomes[1]


def test_splitter_opens_into_two_active_red_offspring_on_the_pixel_grid():
    _native()
    st.reset(seed=731)
    parent = st.spawn_enemy(896, 800, kind="splitter", speed=40, length=5)
    st.step(60)
    st.set_enemy(parent, alive=False)
    state = st.step(1, full=True)
    assert len(state["enemies"]) == 2
    assert all(enemy["kind"] == "swift" and enemy["alive"] for enemy in state["enemies"])
    for frame, ticks in ((4, 4), (9, 5), (19, 10)):
        st.step(ticks)
        before = _state()
        path = OUTPUT / f"splitter-{frame}.png"
        repeat = OUTPUT / f"splitter-{frame}-repeat.png"
        st.capture(path)
        st.capture(repeat)
        image = _png.read(path)
        assert image.rgba == _png.read(repeat).rgba
        assert _state() == before
        blue = sum(image.pixel(x, y)[2] > image.pixel(x, y)[0] + 20
                   and image.pixel(x, y)[2] > image.pixel(x, y)[1] + 20
                   for y in range(215, 285) for x in range(470, 675))
        if frame == 4:
            assert blue > 100, "Offspring did not inherit the blue body"
        if frame == 19:
            assert blue == 0, "A generic blue corpse remained after the split"
    assert all(enemy["kind"] == "swift" for enemy in st.snapshot()["enemies"])


def test_splitter_handoff_has_no_color_or_pose_pop_between_fixed_updates():
    _native()
    st.reset(seed=731)
    parent = st.spawn_enemy(896, 800, kind="splitter", speed=40, length=5)
    st.step(60)
    st.set_enemy(parent, alive=False)
    st.step(1)
    st.step(17)
    previous = OUTPUT / "splitter-handoff-before.png"
    following = OUTPUT / "splitter-handoff-after.png"
    st.capture(previous, render_alpha=1)
    st.step(1)
    st.capture(following, render_alpha=0)
    assert _png.read(previous).rgba == _png.read(following).rgba
