"""Real actor -> damage -> kill -> reward coverage for every shipped weapon.

These fixtures drive the shipping C++ Game::update through the test bridge.
They do not inject projectiles, manufacture hit events, or lower a victim's HP.
Scalar parity and detailed mechanics remain independently checked in
test_weapon_parity; this module ensures every playable rank actually fights.
"""
from dataclasses import dataclass

from . import snake_test as st

TIER = "logic"


@dataclass(frozen=True)
class WeaponCase:
    slug: str
    family: str
    name: str
    evolved: bool
    pattern: str
    stages: tuple[str, ...]
    target_offset: tuple[int, int]


# Explicit coverage inventory: adding content without adding a real case fails
# the catalog gate below rather than silently expanding a generic smoke test.
VARIANTS = (
    WeaponCase("venom_shot", "venom_shot", "Venom Shot", False, "shot", ("projectile",), (48, 0)),
    WeaponCase("viper_fangs", "viper_fangs", "Viper Fangs", False, "homing", ("projectile",), (48, 8)),
    WeaponCase("plague_mortar", "plague_mortar", "Plague Mortar", False, "mortar", ("projectile", "zone"), (48, 0)),
    WeaponCase("snake_nest", "snake_nest", "Snake Nest", False, "nest", ("egg", "summon"), (48, 0)),
    WeaponCase("venom_nova", "venom_nova", "Venom Nova", False, "nova", ("zone",), (16, 0)),
    WeaponCase("sidewinder_beam", "sidewinder_beam", "Sidewinder Beam", False, "beam", ("beam",), (40, 0)),
    WeaponCase("ricochet_fang", "ricochet_fang", "Ricochet Fang", False, "ricochet", ("projectile",), (48, 0)),
    WeaponCase("cobra_pit", "cobra_pit", "Cobra Pit", False, "pit", ("projectile", "pit", "cobra"), (48, 0)),
    WeaponCase("tongue_lash", "tongue_lash", "Tongue Lash", False, "lash", ("zone",), (16, 0)),
    WeaponCase("ouroboros", "venom_shot", "Ouroboros", True, "shot", ("orbit",), (48, 0)),
    WeaponCase("serpent_gatling", "viper_fangs", "Serpent's Gatling", True, "homing", ("projectile",), (48, 8)),
    WeaponCase("singularity_mortar", "plague_mortar", "Singularity Mortar", True, "mortar", ("projectile", "zone"), (48, 0)),
    WeaponCase("hydra_brood", "snake_nest", "Hydra Brood", True, "nest", ("egg", "summon"), (48, 0)),
    WeaponCase("miasma", "venom_nova", "Miasma", True, "nova", ("zone",), (32, 0)),
    WeaponCase("consumption_beam", "sidewinder_beam", "Consumption Beam", True, "beam", ("beam",), (40, 0)),
    WeaponCase("shatter_fang", "ricochet_fang", "Shatter Fang", True, "ricochet", ("projectile",), (48, 0)),
    WeaponCase("ancient_brood_pit", "cobra_pit", "Ancient Brood Pit", True, "pit", ("projectile", "leviathan", "ancient_beam"), (48, 0)),
    WeaponCase("serpents_reckoning", "tongue_lash", "Serpent's Reckoning", True, "lash", ("grapple",), (64, 0)),
)


def _context(case, rank, state, **extra):
    return {"weapon": case.name, "id": case.family, "rank": rank,
            "evolved": case.evolved, "tick": state["tick"],
            "phase": state["phase"], "events": state["events"],
            "counts": state["counts"], "enemies": state["enemies"], **extra}


def _prepare(case, rank):
    seed = 0xC00B00 + VARIANTS.index(case) * 97 + rank
    st.reset(seed=seed)
    st.player(next_xp=1000000)
    st.equip(case.family, rank, case.evolved)
    if case.evolved:
        recipe = next(row for row in st.catalog()["evolutions"] if row["weapon"] == case.family)
        st.equip(recipe["passive"], 1)
    return st.snapshot()


def _enemy(state, handle):
    return next((enemy for enemy in state["enemies"] if enemy["id"] == handle), None)


def _seen_stages(case, state):
    stages = set()
    if any(p["pattern"] == case.pattern and p["evolved"] == case.evolved and not p["cobra_spit"]
           for p in state["projectiles"]):
        stages.add("projectile")
    if any(z["pattern"] == case.pattern and z["evolved"] == case.evolved for z in state["zones"]):
        stages.add("zone")
    if any(egg["evolved"] == case.evolved for egg in state["nest_eggs"]):
        stages.add("egg")
    if any(snake["evolved"] == case.evolved and snake["segment_count"] == (5 if case.evolved else 3)
           for snake in state["mini_snakes"]):
        stages.add("summon")
    if any(beam["evolved"] == case.evolved and not beam["ancient"] for beam in state["beams"]):
        stages.add("beam")
    if any(beam["ancient"] for beam in state["beams"]):
        stages.add("ancient_beam")
    if state["cobra_pits"]:
        stages.add("pit")
    if state["cobras"]:
        stages.add("cobra")
    if state["leviathan"]["active"]:
        stages.add("leviathan")
    if state["ouroboros_active"] and len(state["orbit_fangs"]) == 44:
        stages.add("orbit")
    if state["grapples"]:
        stages.add("grapple")
    return stages


def _real_combat(case, rank):
    _prepare(case, rank)
    x, y = st.player()["position"]
    dx, dy = case.target_offset
    # Reckoning executes its primary prey rather than gradually reducing its
    # HP. Its real drag/blast must also damage an ordinary surviving bystander.
    if case.slug == "serpents_reckoning":
        st.spawn_enemy(x + dx, y + dy, hp=100000, speed=0)
        durable = st.spawn_enemy(x + 32, y + 8, hp=100000, speed=0)
    else:
        durable = st.spawn_enemy(x + dx, y + dy, hp=100000, speed=0)
    observed = set()
    for _ in range(360):
        state = st.step(1, full=True)
        observed.update(_seen_stages(case, state))
        victim = _enemy(state, durable)
        detail = _context(case, rank, state, observed=sorted(observed))
        assert victim is not None, detail
        assert state["phase"] == "playing", detail
        if victim["hp"] < 100000 and set(case.stages) <= observed:
            assert state["events"]["hit"] > 0, detail
            break
    else:
        raise AssertionError(_context(case, rank, state, missing=sorted(set(case.stages) - observed),
                                      error="No real actor/damage chain within 360 ticks"))
    st.invariants()

    # A separate fresh low-HP victim proves real kill resolution and the actual
    # native reward path without assigning HP or manufacturing damage events.
    initial = _prepare(case, rank)
    x, y = initial["player"]["position"]
    prey = st.spawn_enemy(x + dx, y + dy, hp=1, speed=0)
    for _ in range(360):
        state = st.step(1, full=True)
        if _enemy(state, prey) is None:
            break
    else:
        raise AssertionError(_context(case, rank, state, error="Weapon never killed its in-range victim"))
    detail = _context(case, rank, state)
    assert state["kills"] == state["events"]["kill"] == 1, detail
    assert state["events"]["hit"] > 0 and state["counts"]["enemies"] == 0, detail
    assert state["events"]["level"] == 0, detail
    xp_on_ground = sum(pickup["value"] for pickup in state["pickups"] if pickup["kind"] == "experience")
    if case.slug in ("consumption_beam", "serpents_reckoning"):
        assert xp_on_ground == 0, detail
        growth = 3 if case.slug == "consumption_beam" else 1
        assert state["player"]["length"] == initial["player"]["length"] + growth, detail
        assert state["player"]["xp"] > initial["player"]["xp"], detail
        assert state["gems"] == growth, detail
    else:
        assert xp_on_ground + state["player"]["xp"] == 1, detail
    st.invariants()


def _out_of_range(case):
    rank = 8
    _prepare(case, rank)
    x, y = st.player()["position"]
    # Below the 420-unit recycle threshold, outside every tested attack's reach.
    handle = st.spawn_enemy(x + 304, y, hp=100000, speed=0)
    state = st.step(180, full=True)
    victim = _enemy(state, handle)
    detail = _context(case, rank, state)
    assert victim is not None and victim["hp"] == 100000, detail
    assert state["events"]["hit"] == state["events"]["kill"] == state["kills"] == 0, detail
    assert victim["position"] == [x + 304, y], detail
    st.invariants()


def test_catalog_has_a_real_combat_case_for_every_weapon_and_evolution():
    catalog = st.catalog()
    base_definitions = [row for row in catalog["upgrades"] if row["kind"] == "weapon"]
    base_ids = {row["id"] for row in base_definitions}
    evolved_ids = {row["weapon"] for row in catalog["evolutions"]}
    assert len(VARIANTS) == len({case.slug for case in VARIANTS}) == 18
    assert {case.family for case in VARIANTS if not case.evolved} == base_ids
    assert {case.family for case in VARIANTS if case.evolved} == evolved_ids
    assert all(row["max_rank"] == 8 for row in base_definitions), "Update live rank coverage for the changed catalog"
    assert len({(case.family, case.evolved) for case in VARIANTS}) == len(VARIANTS)
    for case in VARIANTS:
        ranks = (8,) if case.evolved else range(1, 9)
        for rank in ranks:
            name = f"test_{case.slug}_rank_{rank}_hits_kills_and_rewards"
            assert name in globals() and callable(globals()[name]), name
        name = f"test_{case.slug}_does_not_damage_out_of_range_target"
        assert name in globals() and callable(globals()[name]), name


def _combat_test(case, rank):
    def test():
        _real_combat(case, rank)
    test.__name__ = f"test_{case.slug}_rank_{rank}_hits_kills_and_rewards"
    test.__doc__ = f"{case.name}, rank {rank}: real actors, HP loss, kill, and native reward."
    test.timeout_ticks = 720
    return test


def _range_test(case):
    def test():
        _out_of_range(case)
    test.__name__ = f"test_{case.slug}_does_not_damage_out_of_range_target"
    test.__doc__ = f"{case.name} cannot hit a stationary target outside its reach."
    test.timeout_ticks = 180
    return test


for _case in VARIANTS:
    for _rank in ((8,) if _case.evolved else range(1, 9)):
        _test = _combat_test(_case, _rank)
        globals()[_test.__name__] = _test
    _test = _range_test(_case)
    globals()[_test.__name__] = _test
