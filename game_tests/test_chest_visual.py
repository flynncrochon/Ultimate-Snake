"""The real chest renderer must reproduce native asset pixels exactly."""
from . import snake_test as st
from . import _png

TIER = "visual"
OUTPUT = st.ROOT / "artifacts" / "gametest" / "chest-pixels"


def _setup():
    if not st.current().native:
        st.skip("Chest raster checks require --native")
    st.reset(seed=0xC8E57)
    st.configure(invulnerable=False)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    atlas = _png.read(st.ROOT / "data/sprites/chests.png")
    assert (atlas.width, atlas.height) == (48,32)
    assert all(alpha in (0,255) for alpha in atlas.rgba[3::4])
    return atlas


def _compare(atlas, image, source_x, size, x, y, background):
    for row in range(size):
        for column in range(size):
            source = atlas.pixel(source_x+column,row)
            expected = source if source[3] else (*background,255)
            actual = image.pixel(x+column,y+row)
            assert actual == expected, {"pixel":[column,row],"actual":actual,"expected":expected}
    # No halo, oversized rectangle, or detached sparkle outside the sprite.
    for row in range(-1,size+1):
        for column in range(-1,size+1):
            if row in (-1,size) or column in (-1,size):
                assert image.pixel(x+column,y+row) == (*background,255), (column,row)


def test_ground_chest_matches_every_native_16px_asset_pixel():
    atlas = _setup()
    st.spawn_pickup(864,800,kind="chest")
    path = OUTPUT / "ground.png"
    st.capture(path,world_only=True)
    image = _png.read(path)
    _compare(atlas,image,0,16,552,242,(0,0,0))
    assert st.snapshot()["chests"] == 0, "Capturing a ground chest opened it"


def test_reward_chest_matches_every_native_32px_asset_pixel():
    atlas = _setup()
    st.spawn_pickup(kind="chest")
    st.step(1)
    path = OUTPUT / "reward.png"
    metadata = st.capture(path,world_only=False)
    assert metadata["reward_open"], metadata
    image = _png.read(path)
    _compare(atlas,image,16,32,384,184,(12,12,12))
    assert st.snapshot()["chests"] == 1
