"""Real casts exercise secondary weapon effects, without injected combat actors."""
import json
import math
from pathlib import Path

from . import snake_test as st

TIER = "logic"
ORACLE = json.loads((Path(__file__).parent / "fixtures/javascript_weapon_oracle.json").read_text())
HP = 1000000


def _reset(seed=0x5EED):
    st.reset(seed=seed)
    st.player(next_xp=1000000)


def _spawn(dx, dy=0, hp=HP):
    return st.spawn_enemy(800 + dx, 800 + dy, hp=hp, speed=0)


def _enemy(state, handle):
    return next(enemy for enemy in state["enemies"] if enemy["id"] == handle)


def _tuning(weapon):
    return next(row for row in st.catalog()["evolutions"] if row["weapon"] == weapon)


def _round(value):
    return math.floor(value + .5)


def test_naturally_fired_gatling_pierces_three_targets_with_damage_falloff():
    _reset()
    st.equip("viper_fangs", rank=8, evolved=True)
    victims = [_spawn(dx) for dx in (24, 40, 56)]
    state = st.step(1, full=True)
    assert len(state["projectiles"]) == 2
    assert all(shot["gatling"] for shot in state["projectiles"])
    # Stop later casts; both naturally fired fangs continue their real flight.
    st.unequip("viper_fangs")
    reached_second_impact = False
    for _ in range(120):
        state = st.step(1, full=True)
        reached_second_impact |= any(shot["impacts"] >= 2 for shot in state["projectiles"])
    source = ORACLE["constants"]["serpent_gatling"]["GATLING_DAMAGE"]
    damage = source * _tuning("viper_fangs")["damage"] * 1.15
    # The original three-impact contract is 100%, 70%, 50%; two fangs
    # independently traverse this row, rather than three unrelated casts.
    expected = [2 * _round(damage * factor) for factor in (1, .7, .5)]
    observed = [HP - _enemy(state, handle)["hp"] for handle in victims]
    assert reached_second_impact, state
    assert observed == expected, (observed, expected, state)
    assert not state["projectiles"]
    assert state["events"]["shot"] == 1, state


def test_expired_hydra_parents_leave_children_that_really_bite_then_expire():
    _reset()
    st.equip("snake_nest", rank=8, evolved=True)
    victim = _spawn(64)
    st.step(1)
    st.unequip("snake_nest")
    st.wait_until(
        lambda: bool(st.snapshot()["mini_snakes"])
        and all(snake["generation"] == 1 for snake in st.snapshot()["mini_snakes"]),
        ticks=450, message="natural Hydra parent expiry and child birth")
    state = st.snapshot()
    assert len(state["mini_snakes"]) == 12, state
    assert all(snake["segment_count"] == 3 for snake in state["mini_snakes"])
    child_damage = ORACLE["constants"]["hydra_brood"]["BROOD_DAMAGE"] * _tuning("snake_nest")["damage"]
    assert all(snake["damage"] == child_damage for snake in state["mini_snakes"])
    before = _enemy(state, victim)["hp"]
    st.wait_until(lambda: st.enemy(victim)["hp"] < before, ticks=150,
                  message="only the naturally hatched Hydra children can deal this bite")
    state = st.snapshot()
    assert all(snake["generation"] == 1 for snake in state["mini_snakes"]), state
    dealt = before - _enemy(state, victim)["hp"]
    assert dealt >= child_damage and math.isclose(dealt / child_damage, round(dealt / child_damage)), state
    st.wait_until(lambda: not st.snapshot()["mini_snakes"], ticks=240,
                  message="natural child lifespan expires")
    before = st.enemy(victim)["hp"]
    st.step(60)
    assert st.enemy(victim)["hp"] == before, "Expired children must stop biting"
    assert st.snapshot(full=False)["counts"]["nest_eggs"] == 0


test_expired_hydra_parents_leave_children_that_really_bite_then_expire.timeout_ticks = 900


def test_natural_shatter_splinters_hit_after_parent_expires_and_stop_at_generation_three():
    _reset()
    st.equip("ricochet_fang", rank=8, evolved=True)
    st.equip("dead_eye", rank=8)
    victims = st.spawn_enemies([
        {"x": 824 + (i % 6) * 8, "y": 776 + (i // 6) * 8, "hp": HP, "speed": 0}
        for i in range(36)])
    state = st.step(1, full=True)
    assert len(state["projectiles"]) == 1 and state["projectiles"][0]["generation"] == 0
    st.unequip("ricochet_fang")
    source = ORACLE["constants"]["shatter_fang"]
    generations = {0}
    peak = 1
    child_impact = child_only_damage = False
    previous_child_only_hp = None
    for _ in range(420):
        state = st.step(1, full=True)
        shots = state["projectiles"]
        generations.update(shot["generation"] for shot in shots)
        assert all(0 <= shot["generation"] <= source["MAX_GENERATION"] for shot in shots), state
        peak = max(peak, len(shots))
        assert len(shots) <= source["MAX_ACTIVE_FANGS"], state
        child_impact |= any(shot["generation"] > 0 and shot["impacts"] > 0 for shot in shots)
        if shots and all(shot["generation"] > 0 for shot in shots):
            total = sum(_enemy(state, handle)["hp"] for handle in victims)
            if previous_child_only_hp is not None and total < previous_child_only_hp:
                child_only_damage = True
            previous_child_only_hp = total
        elif not shots:
            break
    assert generations == {0, 1, 2, 3}, (generations, state)
    assert peak == source["MAX_ACTIVE_FANGS"], "Fixture must exercise the real family cap"
    assert child_impact and child_only_damage, "Spawned splinters must really hit without the parent"
    assert not state["projectiles"], "The one cast must eventually expire"
    assert state["events"]["shot"] == 1, state


def test_naturally_raised_ancient_laser_pierces_a_row_but_misses_an_off_axis_enemy():
    _reset()
    st.equip("cobra_pit", rank=8, evolved=True)
    victims = [_spawn(dx, dy) for dx, dy in ((32, 0), (48, 0), (64, 0), (48, 24))]
    state = st.step(1, full=True)
    assert len(state["projectiles"]) == 1 and state["projectiles"][0]["pattern"] == "pit"
    st.equip("cobra_pit", rank=8, evolved=True, cooldown=100)
    st.wait_until(lambda: any(enemy["hp"] < HP for enemy in st.snapshot()["enemies"]),
                  ticks=240, message="the real seed raises its leviathan and fires a laser")
    state = st.snapshot()
    assert state["leviathan"]["active"] and state["leviathan"]["growth"] == 0
    assert len(state["beams"]) == 1 and state["beams"][0]["ancient"]
    raw = ORACLE["weapons"]["ancient_brood_pit"][0]["getters"]["get_laser_damage"]
    expected = _round(raw * _tuning("cobra_pit")["damage"] * 1.15)
    observed = [HP - _enemy(state, handle)["hp"] for handle in victims]
    assert observed == [expected, expected, expected, 0], (observed, expected, state)


def test_consumption_absorbs_a_real_gun_kill_exactly_once_without_healing():
    _reset()
    st.player(hp=37)
    st.equip("sidewinder_beam", rank=8, evolved=True)
    st.equip("venom_shot", rank=8)
    victim = _spawn(32, hp=600)
    initial_length = st.player()["length"]
    state = st.step(1, full=True)
    assert any(beam["target"] == victim and beam["evolved"] for beam in state["beams"])
    assert any(shot["pattern"] == "shot" for shot in state["projectiles"])
    # 600 HP survives the beam's first two ticks. The real center fang
    # arrives before then and supplies the finishing hit.
    st.wait_until(lambda: st.snapshot(full=False)["kills"] == 1, ticks=9,
                  message="actual Venom Shot projectile finishes Consumption's target")
    state = st.snapshot()
    assert any(beam["pending_absorb"] for beam in state["beams"]), state
    assert any(pickup["kind"] == "experience" for pickup in state["pickups"]), state
    assert st.enemy(victim) is None
    st.step(2)
    state = st.snapshot()
    source_growth = ORACLE["constants"]["consumption_beam"]["GROWTH_PER_KILL"]
    expected_growth = source_growth + 1  # One real red-enemy XP fruit vacuumed.
    assert state["player"]["length"] == initial_length + expected_growth, state
    assert state["player"]["xp"] == state["gems"] == expected_growth, state
    assert state["player"]["hp"] == 37 and not state["pickups"], state
    st.step(30)
    after = st.snapshot(full=False)
    assert after["kills"] == 1 and after["player"]["length"] == state["player"]["length"], after
    assert after["player"]["xp"] == after["gems"] == expected_growth, after
