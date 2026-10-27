"""Seeded stateful bug hunts against the real native game, never a Python sim."""
import math
import random

from . import snake_test as st

TIER = "hunt"
WEAPONS = ("venom_shot", "viper_fangs", "plague_mortar", "snake_nest", "venom_nova",
           "sidewinder_beam", "ricochet_fang", "cobra_pit", "tongue_lash")
PASSIVES = ("graviton", "rapid_fire", "dead_eye", "ravenous_maw", "chronofield",
            "coiled_volley", "toxic_expanse", "serpents_reach", "serpents_scales")
DIRECTIONS = (st.EAST, st.SOUTH, st.WEST, st.NORTH)


def _context(state, seed, action):
    return {"seed": seed, "action": action, "tick": state["tick"], "phase": state["phase"],
            "player": state["player"], "counts": state["counts"], "owned": state["owned"],
            "enemies": state.get("enemies", [])[:20], "grapples": state.get("grapples", [])}


def _finite(value):
    if isinstance(value, float):
        return math.isfinite(value)
    if isinstance(value, list):
        return all(_finite(item) for item in value)
    if isinstance(value, dict):
        return all(_finite(item) for item in value.values())
    return True


def _grid(point):
    return (all(8 <= coordinate <= 1592 for coordinate in point)
            and all(abs(coordinate / st.TILE - round(coordinate / st.TILE)) < 1e-5 for coordinate in point))


def _check(seed, action, previous_events=None):
    state = st.snapshot()
    detail = _context(state, seed, action)
    assert _finite(state), detail
    assert state["phase"] == "playing", detail
    assert 0 <= state["player"]["hp"] <= state["player"]["max_hp"], detail
    assert _grid(state["player"]["position"]), detail
    assert all(_grid(point) for point in state["player"]["segments"]), detail
    counts = state["counts"]
    limits = {"enemies": 1800, "projectiles": 1600, "zones": 128, "nest_eggs": 25,
              "mini_snakes": 60, "beams": 32, "cobra_pits": 25, "cobras": 30,
              "grapples": 16, "pickups": 1403, "particles": 1600, "damage_numbers": 150,
              "orbit_fangs": 44}
    assert all(0 <= counts[name] <= cap for name, cap in limits.items()), detail
    ids = [enemy["id"] for enemy in state["enemies"]]
    assert len(ids) == len(set(ids)), detail
    captured = [grapple["target"] for grapple in state["grapples"] if grapple["phase"] == 1]
    assert len(captured) == len(set(captured)), detail
    for enemy in state["enemies"]:
        assert _grid(enemy["position"]), detail
        assert all(_grid(point) for point in enemy["trail"][:enemy["length"]]), detail
        if enemy["grabbed"]:
            assert enemy["id"] in captured, detail
    for mini in state["mini_snakes"]:
        assert _grid(mini["position"]), detail
        assert all(_grid(point) for point in mini["segments"]), detail
        if mini["segment_count"] == 5:
            assert all(_grid(point) for point in mini["extra_segments"]), detail
    for projectile in state["projectiles"]:
        assert projectile["hit_count"] <= 32 and projectile["life"] > 0, detail
    if previous_events:
        assert all(state["events"][key] >= value for key, value in previous_events.items()), detail
    st.invariants()
    return state


def _spawn(rng, count=36):
    center = st.player()["position"]
    targets = []
    for index in range(count):
        angle = rng.random() * math.tau
        radius = rng.randrange(3, 14) * st.TILE
        kind = "boss" if index % 17 == 0 else "splitter" if index % 7 == 0 else "red"
        targets.append({"x": center[0] + math.cos(angle) * radius,
                        "y": center[1] + math.sin(angle) * radius,
                        "hp": 5000 if kind == "boss" else rng.choice((45, 150, 600, 1800)),
                        "speed": rng.choice((8, 16, 24, 40)), "kind": kind})
    return st.spawn_enemies(targets)


def test_every_base_and_evolved_weapon_survives_seeded_live_combat():
    seeds = []
    for variant in range(18):
        seed = 0xA11000 + variant * 97
        seeds.append(seed)
        rng = random.Random(seed)
        st.reset(seed=seed, character=variant % 4)
        st.configure(freeze_player=False)
        weapon = WEAPONS[variant % 9]
        st.equip(weapon, rank=8 if variant >= 9 else rng.randint(1, 8), evolved=variant >= 9)
        st.equip("coiled_volley", rank=2)
        st.equip("chronofield", rank=3)
        _spawn(rng)
        previous = None
        for chunk in range(24):
            st.step(25, direction=rng.choice(DIRECTIONS))
            state = _check(seed, (weapon, variant >= 9, chunk), previous)
            previous = state["events"]
            if chunk % 6 == 5 and state["counts"]["enemies"] < 24:
                _spawn(rng, 18)
        assert st.snapshot(full=False)["elapsed"] > 9, (seed, st.snapshot(full=False))
    print("combat_hunt weapon seeds:", seeds)


def test_mixed_builds_survive_pause_swap_clear_and_dirty_reset():
    seeds = (0xB12003, 0xB12017, 0xB1202B, 0xB12041, 0xB12059, 0xB1206D)
    for seed in seeds:
        rng = random.Random(seed)
        st.reset(seed=seed)
        st.configure(freeze_player=False)
        equipped = rng.sample(WEAPONS, 4)
        for weapon in equipped:
            st.equip(weapon, rank=rng.randint(1, 8), evolved=rng.random() < .5)
        for passive in rng.sample(PASSIVES, 4):
            st.equip(passive, rank=rng.randint(1, 2 if passive == "coiled_volley" else 8))
        _spawn(rng, 64)
        previous = None
        for chunk in range(30):
            if chunk % 7 == 2:
                before = st.snapshot()
                st.pause()
                paused = st.step(3, full=True)
                assert paused["phase"] == "paused", _context(paused, seed, "pause")
                assert paused["elapsed"] == before["elapsed"], (seed, before, paused)
                for key in ("enemies", "projectiles", "nest_eggs", "mini_snakes", "beams", "grapples"):
                    assert paused[key] == before[key], (seed, key, before, paused)
                st.pause()
            if chunk == 10:
                old = equipped.pop(0)
                st.unequip(old)
                replacement = rng.choice([weapon for weapon in WEAPONS if weapon not in equipped and weapon != old])
                st.equip(replacement, rank=8, evolved=True)
                equipped.append(replacement)
            if chunk == 18:
                st.clear(owned=False)
                cleared = st.snapshot()
                assert all(count == 0 for count in cleared["counts"].values()), _context(cleared, seed, "clear")
                _spawn(rng, 48)
            st.step(20, direction=rng.choice(DIRECTIONS))
            state = _check(seed, ("mixed", chunk), previous)
            previous = state["events"]
            assert len(state["owned"]) <= 8, _context(state, seed, chunk)
        st.reset(seed=seed + 1)
        clean = st.snapshot()
        assert not clean["owned"] and clean["tick"] == 0, _context(clean, seed, "reset")
        assert all(count == 0 for count in clean["counts"].values()), _context(clean, seed, "reset")
        assert all(count == 0 for count in clean["events"].values()), _context(clean, seed, "reset events")
    print("combat_hunt mixed seeds:", seeds)


def test_grabbed_targets_finish_or_release_during_mixed_damage():
    seeds = (0xC13007, 0xC13019, 0xC1302F, 0xC13043)
    for seed in seeds:
        rng = random.Random(seed)
        st.reset(seed=seed)
        st.configure(freeze_player=False)
        st.equip("tongue_lash", rank=8, evolved=True)
        st.equip("sidewinder_beam", rank=8, evolved=True)
        st.equip("ricochet_fang", rank=8, evolved=True)
        st.equip("viper_fangs", rank=8, evolved=True)
        for passive in ("rapid_fire", "dead_eye", "coiled_volley", "serpents_reach"):
            st.equip(passive, rank=2 if passive == "coiled_volley" else 8)
        _spawn(rng, 60)
        previous = None
        for chunk in range(40):
            st.step(15, direction=rng.choice(DIRECTIONS))
            state = _check(seed, ("grapple", chunk), previous)
            previous = state["events"]
            if chunk % 8 == 3:
                grabbed = next((enemy for enemy in state["enemies"] if enemy["grabbed"]), None)
                if grabbed:
                    st.set_enemy(grabbed["id"], alive=False)
                    st.step(2)
                    state = _check(seed, ("target removed", grabbed["id"]), previous)
                    assert not any(grapple["target"] == grabbed["id"] and grapple["phase"] == 1
                                   for grapple in state["grapples"]), _context(state, seed, "released removed target")
                    previous = state["events"]
            if chunk % 10 == 9 and state["counts"]["enemies"] < 40:
                _spawn(rng, 30)
    print("combat_hunt grapple seeds:", seeds)


test_every_base_and_evolved_weapon_survives_seeded_live_combat.timeout_ticks = 18000
test_mixed_builds_survive_pause_swap_clear_and_dirty_reset.timeout_ticks = 6000
test_grabbed_targets_finish_or_release_during_mixed_damage.timeout_ticks = 4000
