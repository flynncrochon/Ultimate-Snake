"""Earn every evolution through a boss chest, then exercise its real combat.

These tests never equip an evolved fixture or inject a weapon actor. The native
base weapon earns the chest, the normal collection path evolves it, and the
replacement must create its own actors and remove actual enemy HP.
"""
from . import snake_test as st

TIER = "logic"

# Explicit cases make a changed recipe/name an intentional test update.
RECIPES = (
    ("venom_shot", "serpents_scales", "ouroboros", "Ouroboros", "shot"),
    ("viper_fangs", "rapid_fire", "serpents_gatling", "Serpent's Gatling", "homing"),
    ("plague_mortar", "graviton", "singularity_mortar", "Singularity Mortar", "mortar"),
    ("snake_nest", "coiled_volley", "hydra_brood", "Hydra Brood", "nest"),
    ("venom_nova", "toxic_expanse", "miasma", "Miasma", "nova"),
    ("sidewinder_beam", "ravenous_maw", "consumption_beam", "Consumption Beam", "beam"),
    ("ricochet_fang", "dead_eye", "shatter_fang", "Shatter Fang", "ricochet"),
    ("cobra_pit", "chronofield", "ancient_brood_pit", "Ancient Brood Pit", "pit"),
    ("tongue_lash", "serpents_reach", "serpents_reckoning", "Serpent's Reckoning", "lash"),
)


def _detail(recipe, stage, state):
    return {"weapon": recipe[0], "evolution": recipe[2], "stage": stage,
            "tick": state["tick"], "phase": state["phase"], "events": state["events"],
            "counts": state["counts"], "owned": state["owned"]}


def _earn_chest(recipe, *, rank=8, passive=True):
    weapon, prerequisite, _, name, _ = recipe
    st.reset(seed=0xE701 + next(i for i, row in enumerate(RECIPES) if row == recipe))
    st.player(position=[800, 800], direction=st.EAST, next_xp=1000000)
    st.equip(weapon, rank=rank)
    if passive:
        st.equip(prerequisite, rank=1)
    catalog_recipe = next(row for row in st.catalog()["evolutions"] if row["weapon"] == weapon)
    assert (catalog_recipe["passive"], catalog_recipe["name"]) == (prerequisite, name)
    boss = st.spawn_enemy(816, 800, hp=1, speed=0, kind="boss")
    state = st.snapshot()
    for _ in range(360):
        state = st.step(1, full=True)
        if state["bosses_killed"]:
            break
    else:
        raise AssertionError(_detail(recipe, "base weapon failed to kill boss", state))
    assert state["kills"] == state["bosses_killed"] == 1, _detail(recipe, "boss death", state)
    assert not any(enemy["id"] == boss for enemy in state["enemies"])
    assert state["events"]["hit"] > 0, _detail(recipe, "base weapon made no real hit", state)
    assert state["chests"] == state["events"]["evolve"] == 0
    boxes = [pickup for pickup in state["pickups"] if pickup["kind"] == "chest"]
    assert len(boxes) == 1 and boxes[0]["value"] == 1, _detail(recipe, "boss chest missing", state)
    st.player(position=boxes[0]["position"])
    return st.step(1, full=True)


def _replacement_cleared_base(recipe, state):
    pattern = recipe[4]
    assert not any(shot["pattern"] == pattern and not shot["evolved"] and not shot["cobra_spit"]
                   for shot in state["projectiles"]), _detail(recipe, "base projectile survived", state)
    assert not any(zone["pattern"] == pattern and not zone["evolved"]
                   for zone in state["zones"]), _detail(recipe, "base zone survived", state)
    if pattern == "nest":
        assert not state["nest_eggs"] and not state["mini_snakes"]
    elif pattern == "beam":
        assert not any(not beam["evolved"] for beam in state["beams"])
    elif pattern == "pit":
        assert not state["cobra_pits"] and not state["cobras"]
        assert not any(shot["cobra_spit"] for shot in state["projectiles"])


def _actor_stages(recipe, state):
    weapon = recipe[0]
    stages = set()
    if weapon == "venom_shot":
        if state["ouroboros_active"] and state["counts"]["orbit_fangs"] == 44:
            stages.add("orbit")
    elif weapon == "viper_fangs":
        if any(shot["evolved"] and shot["pattern"] == "homing" and shot["gatling"] and shot["pierce"] == 2
               for shot in state["projectiles"]):
            stages.add("piercing_fang")
    elif weapon == "plague_mortar":
        if any(shot["evolved"] and shot["pattern"] == "mortar" for shot in state["projectiles"]):
            stages.add("bomb")
        if any(zone["evolved"] and zone["pattern"] == "mortar" for zone in state["zones"]):
            stages.add("well")
    elif weapon == "snake_nest":
        if any(egg["evolved"] for egg in state["nest_eggs"]):
            stages.add("egg")
        if any(mini["evolved"] and mini["segment_count"] == 5 for mini in state["mini_snakes"]):
            stages.add("parent")
    elif weapon == "venom_nova":
        if any(zone["evolved"] and zone["pattern"] == "nova" and zone["follow_player"] for zone in state["zones"]):
            stages.add("aura")
    elif weapon == "sidewinder_beam":
        if any(beam["evolved"] and not beam["ancient"] for beam in state["beams"]):
            stages.add("consumption")
    elif weapon == "ricochet_fang":
        if any(shot["evolved"] and shot["pattern"] == "ricochet" and shot["generation"] == 0
               for shot in state["projectiles"]):
            stages.add("shatter_fang")
    elif weapon == "cobra_pit":
        if any(shot["evolved"] and shot["pattern"] == "pit" for shot in state["projectiles"]):
            stages.add("seed")
        if state["leviathan"]["active"]:
            stages.add("leviathan")
        if any(beam["ancient"] for beam in state["beams"]):
            stages.add("laser")
    elif weapon == "tongue_lash":
        if any(grapple["phase"] == 0 for grapple in state["grapples"]):
            stages.add("extension")
        if any(grapple["phase"] == 1 for grapple in state["grapples"]):
            stages.add("latch")
    return stages


REQUIRED_STAGES = {
    "venom_shot": {"orbit"}, "viper_fangs": {"piercing_fang"},
    "plague_mortar": {"bomb", "well"}, "snake_nest": {"egg", "parent"},
    "venom_nova": {"aura"}, "sidewinder_beam": {"consumption"},
    "ricochet_fang": {"shatter_fang"}, "cobra_pit": {"seed", "leviathan", "laser"},
    "tongue_lash": {"extension", "latch"},
}


def _chest_to_combat(recipe):
    weapon, passive, _, name, _ = recipe
    opened = _earn_chest(recipe)
    context = _detail(recipe, "collect chest", opened)
    assert opened["chests"] == opened["events"]["chest"] == opened["events"]["evolve"] == 1, context
    assert len(opened["chest_rewards"]) == 1, context
    reward = opened["chest_rewards"][0]
    assert (reward["kind"], reward["id"], reward["rank"], reward["title"]) == ("evolution", weapon, 8, name), context
    assert reward["description"], context
    owned = {item["id"]: item for item in opened["owned"]}
    assert set(owned) == {weapon, passive}, context
    assert owned[weapon]["rank"] == 8 and owned[weapon]["evolved"], context
    assert owned[passive]["rank"] == 1, context
    assert opened["player"]["level"] == 1, context
    _replacement_cleared_base(recipe, opened)

    st.player(position=[800, 800], direction=st.EAST)
    initial_hp = 1000000
    victim = st.spawn_enemy(848, 800, hp=initial_hp, speed=0, kind="red")
    seen = set()
    state = opened
    hit_before = opened["events"]["hit"]
    for _ in range(360):
        state = st.step(1, full=True)
        seen.update(_actor_stages(recipe, state))
        enemy = next((enemy for enemy in state["enemies"] if enemy["id"] == victim), None)
        if enemy is None or enemy["hp"] < initial_hp:
            break
    else:
        raise AssertionError(_detail(recipe, "evolved weapon caused no HP damage", state))
    assert REQUIRED_STAGES[weapon] <= seen, (context, {"seen_stages": sorted(seen)})
    assert state["events"]["hit"] > hit_before, _detail(recipe, "missing actual evolved hit", state)
    assert state["chests"] == state["events"]["evolve"] == 1, _detail(recipe, "reward applied twice", state)
    assert state["chest_rewards"] == opened["chest_rewards"], _detail(recipe, "reward ledger changed", state)
    assert state["player"]["level"] == 1, _detail(recipe, "unexpected random progression", state)
    if enemy is None:
        # Reckoning removes all remaining HP atomically through its real devour.
        assert weapon == "tongue_lash" and state["kills"] == 2, _detail(recipe, "unexpected target removal", state)
        assert state["player"]["length"] >= opened["player"]["length"] + 1
    else:
        assert 0 < enemy["hp"] < initial_hp, _detail(recipe, "damage did not reach live target", state)
    st.invariants()


def _recipe_gate(recipe, missing):
    opened = _earn_chest(recipe, rank=7 if missing == "max_rank" else 8,
                         passive=missing != "passive")
    context = _detail(recipe, "recipe gate: " + missing, opened)
    own = next(item for item in opened["owned"] if item["id"] == recipe[0])
    assert opened["chests"] == opened["events"]["chest"] == 1, context
    assert not own["evolved"] and opened["events"]["evolve"] == 0, context
    assert len(opened["chest_rewards"]) == 1 and opened["chest_rewards"][0]["kind"] != "evolution", context
    st.invariants()


def _make_case(recipe, gate=None):
    def test():
        if gate:
            _recipe_gate(recipe, gate)
        else:
            _chest_to_combat(recipe)
    suffix = "requires_" + gate if gate else "earned_from_boss_chest_then_damages_enemy"
    test.__name__ = "test_" + recipe[2] + "_" + suffix
    test.__qualname__ = test.__name__
    test.__doc__ = "Exercise " + recipe[3] + " through the real boss/chest/combat path."
    test.timeout_ticks = 900
    return test


for _recipe in RECIPES:
    for _gate in (None, "max_rank", "passive"):
        _case = _make_case(_recipe, _gate)
        globals()[_case.__name__] = _case

del _recipe, _gate, _case
