"""Actual native loot pixels stay on the grid while pickup growth follows the tail."""
from . import snake_test as st
from . import _png

TIER = "visual"
OUTPUT = st.ROOT / "artifacts" / "gametest" / "pickup-grid"


def _setup():
    if not st.current().native:
        st.skip("Pickup raster checks require --native")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    st.reset(seed=1208)
    st.configure(invulnerable=False)
    st.player(next_xp=1000000)


def test_distant_ground_fruit_keeps_identical_native_pixels_between_ticks():
    _setup()
    for x,y in ((816,800), (808,808), (800,784)):
        st.spawn_pickup(x,y)
    initial = st.snapshot()
    before = OUTPUT / "stationary-before.png"
    after = OUTPUT / "stationary-after.png"
    st.capture(before,world_only=True)
    st.step(60)
    st.capture(after,world_only=True,render_alpha=0.5)
    image = _png.read(after)
    assert _png.read(before).rgba == image.rgba, "Distant ground loot drifted in the framebuffer"
    assert st.snapshot()["pickups"] == initial["pickups"]
    for x,y in ((440,250), (420,270), (400,210)):
        assert image.pixel(x,y) == (255,255,255,255), (x,y,image.pixel(x,y))


def test_one_tile_collection_grows_the_tail_as_segments_unfold():
    _setup()
    st.spawn_pickup(816,800)
    st.spawn_pickup(808,808)
    st.player(position=[808,800])
    collected = st.step(1,full=True,validate=True)
    assert not collected["pickups"] and collected["player"]["length"] == 9, collected
    assert len(collected["player"]["segments"]) == 7, collected
    st.configure(freeze_player=False)
    st.player(move_clock=0)
    grown = st.step(15,direction=st.EAST,full=True,validate=True)
    assert len(grown["player"]["segments"]) == 9, grown
    st.capture(OUTPUT / "tail-grown.png",world_only=True,render_alpha=0.5)


test_distant_ground_fruit_keeps_identical_native_pixels_between_ticks.timeout_ticks = 90
test_one_tile_collection_grows_the_tail_as_segments_unfold.timeout_ticks = 30
