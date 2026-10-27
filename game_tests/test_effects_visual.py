"""Native effect sampling is repeatable and cannot change future combat.

These checks deliberately avoid golden images: the art may evolve, while the
renderer must retain the native framebuffer, draw live actors, and leave the
fixed simulation and its RNG untouched at every interpolation fraction.
"""
from dataclasses import dataclass

from . import snake_test as st
from . import _png

TIER = "visual"
OUTPUT = st.ROOT / "artifacts/gametest/effect-pixels"


@dataclass(frozen=True)
class Case:
    name: str
    weapon: str
    evolved: bool
    ticks: int
    actor: str


CASES = (
    Case("venom_shot", "venom_shot", False, 5, "projectiles"),
    Case("ouroboros", "venom_shot", True, 36, "orbit_fangs"),
    Case("viper_fangs", "viper_fangs", False, 12, "projectiles"),
    Case("gatling", "viper_fangs", True, 7, "projectiles"),
    Case("mortar_flight", "plague_mortar", False, 20, "projectiles"),
    Case("mortar_pool", "plague_mortar", False, 75, "zones"),
    Case("singularity", "plague_mortar", True, 75, "zones"),
    Case("nest_egg", "snake_nest", False, 25, "nest_eggs"),
    Case("nest_snakes", "snake_nest", False, 105, "mini_snakes"),
    Case("hydra_egg", "snake_nest", True, 25, "nest_eggs"),
    Case("hydra_snakes", "snake_nest", True, 105, "mini_snakes"),
    Case("nova", "venom_nova", False, 8, "zones"),
    Case("miasma", "venom_nova", True, 45, "zones"),
    Case("sidewinder", "sidewinder_beam", False, 22, "beams"),
    Case("consumption", "sidewinder_beam", True, 22, "beams"),
    Case("ricochet", "ricochet_fang", False, 10, "projectiles"),
    Case("shatter", "ricochet_fang", True, 24, "projectiles"),
    Case("pit_seed", "cobra_pit", False, 20, "projectiles"),
    Case("cobra_turrets", "cobra_pit", False, 140, "cobras"),
    Case("ancient", "cobra_pit", True, 170, "beams"),
    Case("lash", "tongue_lash", False, 5, "zones"),
    Case("reckoning", "tongue_lash", True, 10, "grapples"),
)


def _state():
    result = st.snapshot()
    result.pop("timing", None)
    return result


def _prepare(case):
    st.reset(seed=0xC0FFEE)
    st.configure(invulnerable=False)
    st.player(next_xp=1000000)
    st.equip(case.weapon, 8, case.evolved)
    st.equip("toxic_expanse", 4)
    st.equip("dead_eye", 8)
    offsets = ((48, 0), (72, -24), (80, 24), (104, 0),
               (32, -48), (16, 48), (-48, -32), (-48, 48))
    if case.weapon in ("venom_nova", "tongue_lash"):
        offsets = ((16, 0), (24, -16), (24, 16), (48, -32),
                   (64, 32), (-48, -40), (-64, 40))
    st.spawn_enemies({"x": 800 + dx, "y": 800 + dy,
                      "hp": 100000000, "speed": 0, "length": 2,
                      "direction": [-1, 0],
                      "kind": "splitter" if i % 3 == 2 else "red"}
                     for i, (dx, dy) in enumerate(offsets))
    st.step(case.ticks, validate=True)


def _render_is_pure(case):
    if not st.current().native or not st.current().capabilities.get("native_capture"):
        st.skip("Effect pixels require the real raylib --native host")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    _prepare(case)
    original = _state()
    assert original["counts"][case.actor] > 0, (case, original["counts"])
    if case.name == "ouroboros":
        assert original["counts"]["orbit_fangs"] == 44
    if case.name == "ancient":
        assert original["leviathan"]["active"]
        assert any(beam["ancient"] for beam in original["beams"])
    for alpha in (0, .5, 1):
        path = OUTPUT / f"{case.name}-{alpha}.png"
        repeat = OUTPUT / f"{case.name}-{alpha}-repeat.png"
        st.capture(path, world_only=True, render_alpha=alpha)
        st.capture(repeat, world_only=True, render_alpha=alpha)
        image = _png.read(path)
        assert (image.width, image.height) == (800, 500)
        assert image.rgba == _png.read(repeat).rgba, (case, alpha, "unstable pixels")
        # Sparse fixtures leave the outer corners empty. Detect an accidental
        # full-screen fill or a stale/uninitialised capture without constraining
        # translucent effect palettes or the new art's silhouette.
        assert all(image.pixel(x, y) == (0, 0, 0, 255)
                   for x, y in ((0, 0), (799, 0), (0, 499), (799, 499)))
        foreground = sum(image.rgba[i:i + 3] != b"\0\0\0"
                         for i in range(0, len(image.rgba), 4))
        assert foreground >= 1000, (case, alpha, "missing native world")
        assert _state() == original, (case, alpha, "capture changed simulation")

    # A renderer that draws with the gameplay RNG can leave the current
    # snapshot unchanged yet change the next hit/target. Compare future combat
    # with a fresh identical fixture that was never rendered.
    st.step(30)
    sampled_future = _state()
    _prepare(case)
    st.step(30)
    assert _state() == sampled_future, (case, "capture changed future combat/RNG")


def _case_test(case):
    def test():
        _render_is_pure(case)
    test.__name__ = f"test_{case.name}_sampling_preserves_pixels_and_future_simulation"
    test.__doc__ = f"{case.name}: repeatable integer pixels and render-only interpolation."
    test.timeout_ticks = case.ticks * 2 + 60
    return test


for _case in CASES:
    _test = _case_test(_case)
    globals()[_test.__name__] = _test
