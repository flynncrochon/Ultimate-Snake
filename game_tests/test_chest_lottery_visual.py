"""Native lottery samples must land on the awarded raster without changing play."""
from . import snake_test as st
from . import _png

TIER = "visual"
OUTPUT = st.ROOT / "artifacts" / "gametest" / "chest-lottery"
REVEAL_SECONDS = 0.22 + 6.0
LANDED_SECONDS = 6.7
WINNER_BACKGROUND = (37, 31, 17, 255)

# Independent asset layout from data/icons/README.md, not the renderer's lookup.
UPGRADES = (
    ("venom_shot", 0), ("viper_fangs", 1), ("plague_mortar", 2),
    ("snake_nest", 3), ("venom_nova", 4), ("sidewinder_beam", 5),
    ("ricochet_fang", 6), ("cobra_pit", 7), ("tongue_lash", 8),
    ("graviton", 9), ("rapid_fire", 10), ("dead_eye", 11),
    ("ravenous_maw", 12), ("chronofield", 13), ("coiled_volley", 14),
    ("toxic_expanse", 15), ("serpents_reach", 16), ("serpents_scales", 17),
)
EVOLUTIONS = (
    ("venom_shot", "serpents_scales", 18),
    ("viper_fangs", "rapid_fire", 19),
    ("plague_mortar", "graviton", 20),
    ("snake_nest", "coiled_volley", 21),
    ("venom_nova", "toxic_expanse", 22),
    ("sidewinder_beam", "ravenous_maw", 23),
    ("ricochet_fang", "dead_eye", 24),
    ("cobra_pit", "chronofield", 25),
    ("tongue_lash", "serpents_reach", 26),
)


def _native():
    if not st.current().native:
        st.skip("Lottery raster checks require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)


def _fixture():
    st.reset(seed=0x1077E2)
    st.configure(invulnerable=False)


def _open():
    st.spawn_pickup(kind="chest")
    state = st.step(1, full=True)
    assert state["chests"] == 1 and len(state["chest_rewards"]) == 1, state
    return state


def _capture(name, seconds=None):
    path = OUTPUT / (name + ".png")
    options = {} if seconds is None else {"reward_time": seconds}
    metadata = st.capture(path, world_only=False, **options)
    assert metadata["reward_open"], metadata
    expected_time = 0 if seconds is None else seconds
    assert abs(metadata["reward_time"] - expected_time) < 0.00001, metadata
    assert metadata["reward_revealed"] == (expected_time >= REVEAL_SECONDS), metadata
    return _png.read(path)


def _compare_sprite(image, atlas, source_x, source_y, size, x, y):
    for row in range(size):
        for column in range(size):
            source = atlas.pixel(source_x + column, source_y + row)
            assert source[3] in (0, 255), source
            expected = source if source[3] else WINNER_BACKGROUND
            actual = image.pixel(x + column, y + row)
            assert actual == expected, {
                "pixel": [column, row], "actual": actual, "expected": expected,
                "source": [source_x + column, source_y + row],
            }


def _region(image, x, y, width, height):
    return b"".join(
        image.rgba[((y + row) * image.width + x) * 4:
                   ((y + row) * image.width + x + width) * 4]
        for row in range(height)
    )


def _assert_scissor(image):
    # Fixed side borders may never be painted by the scrolling edge cells.
    for x in (224, 225, 574, 575):
        for y in range(234, 294):
            assert image.pixel(x, y) == (57, 48, 30, 255), (x, y)
    # The upper progress line and pointer are gone, including the green state.
    for y in range(220,231):
        for x in range(340,461):
            assert image.pixel(x,y) == (12,12,12,255), (x,y)
    # Only the bottom selection arrow remains.
    for y in (230, 297):
        for x in range(226, 574):
            if abs(x - 400) > 6:
                assert image.pixel(x, y) == (12, 12, 12, 255), (x, y)


def test_every_upgrade_lands_on_its_actual_native_32px_icon():
    _native()
    atlas = _png.read(st.ROOT / "data/icons/original-icons.png")
    assert (atlas.width, atlas.height) == (864, 90)
    for upgrade, icon in UPGRADES:
        _fixture()
        st.equip(upgrade, rank=1, cooldown=10000)
        before = _open()
        reward = before["chest_rewards"][0]
        assert reward["kind"] == "upgrade" and reward["id"] == upgrade and reward["rank"] == 2, reward
        image = _capture("upgrade-" + upgrade, LANDED_SECONDS)
        _compare_sprite(image, atlas, icon * 32, 58, 32, 384, 248)
        assert st.snapshot() == before, "Rendering a lottery upgrade changed the real world"


def test_every_evolution_lands_on_its_recorded_evolved_icon():
    _native()
    atlas = _png.read(st.ROOT / "data/icons/original-icons.png")
    for weapon, passive, icon in EVOLUTIONS:
        _fixture()
        st.equip(weapon, rank=8, cooldown=10000)
        st.equip(passive, rank=1)
        before = _open()
        reward = before["chest_rewards"][0]
        assert reward["kind"] == "evolution" and reward["id"] == weapon, reward
        assert next(item for item in before["owned"] if item["id"] == weapon)["evolved"], before
        image = _capture("evolution-" + weapon, LANDED_SECONDS)
        _compare_sprite(image, atlas, icon * 32, 58, 32, 384, 248)
        assert st.snapshot() == before, "Rendering a lottery evolution changed the real world"


def test_treasure_lands_on_the_native_16px_chest_without_repeating_its_reward():
    _native()
    _fixture()
    st.player(hp=10)
    before = _open()
    reward = before["chest_rewards"][0]
    assert reward["kind"] == "treasure" and reward["gold"] == 25 and reward["healing"] == 25, reward
    atlas = _png.read(st.ROOT / "data/sprites/chests.png")
    image = _capture("treasure-landed", LANDED_SECONDS)
    _compare_sprite(image, atlas, 0, 0, 16, 392, 256)
    assert st.snapshot() == before, "Rendering treasure repeated its gold or healing"


def test_sampled_reel_moves_deterministically_stays_clipped_and_never_steps_combat():
    _native()
    _fixture()
    st.equip("snake_nest", rank=1, cooldown=10000)
    before = _open()
    opening = _capture("opening")
    first = _capture("spinning-early", 0.45)
    second = _capture("spinning-late", 1.37)
    repeated = _capture("spinning-early-repeat", 0.45)
    landed = _capture("landed", LANDED_SECONDS)
    opening_again = _capture("opening-after-preview")
    assert first.rgba == repeated.rgba, "The same reel time produced different native pixels"
    assert opening.rgba == opening_again.rgba, "A preview changed the live animation clock"
    assert _region(first, 226, 234, 348, 60) != _region(second, 226, 234, 348, 60), "The lottery reel did not scroll"
    for image in (opening, first, second, repeated, landed, opening_again):
        _assert_scissor(image)
    assert st.snapshot() == before, "Animation samples changed combat, reward history, or equipment"


def test_bad_capture_times_leave_the_real_world_and_next_valid_capture_intact():
    _native()
    _fixture()
    st.equip("venom_shot", rank=1, cooldown=10000)
    before = _open()
    path = OUTPUT / "invalid-time.png"
    path.unlink(missing_ok=True)
    for seconds in (-1, 31, "bad", None, [], True, float("inf"), float("nan")):
        try:
            st.capture(path, world_only=False, reward_time=seconds)
        except (st.GameTestError, ValueError):
            pass
        else:
            raise AssertionError({"accepted_invalid_reward_time": seconds})
        assert not path.exists(), "An invalid animation time wrote a capture"
        assert st.snapshot() == before, "Invalid animation timing mutated the native fixture"
    _capture("valid-after-errors", LANDED_SECONDS)
    assert st.snapshot() == before, "The native capture target was corrupted after rejected timing"


def test_square_centers_use_the_same_native_pixel_grid_as_the_click_tracker():
    _native()
    _fixture()
    st.equip("venom_shot", rank=1, cooldown=10000)
    before = _open()
    # Sample card geometry directly. At the pixel before its center reaches
    # x=400, raylib's one-pixel outline starts at x=375; on the marker
    # it becomes x=374. The icon and filled card share that same translation.
    # The inverse curve chooses fixture poses, not expected sprite pixels.
    for cell in (1,8,16,24):
        for offset,expected in ((-1,(5,5,5,255)),(-0.1,(48,48,48,255))):
            progress = (cell*56+offset)/(24*56)
            seconds = 0.22+6*(1-(1-progress)**(1/3))
            image = _capture(f"center-{cell}-{abs(offset)}",seconds)
            assert image.pixel(374,239) == expected, {
                "cell":cell,"offset":offset,"actual":image.pixel(374,239),"expected":expected,
            }
    assert st.snapshot() == before,"Inspecting center crossings changed play"


for _name, _function in list(globals().items()):
    if _name.startswith("test_") and callable(_function):
        _function.timeout_ticks = 100
