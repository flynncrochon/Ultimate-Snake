"""Independent JS oracle, explicit native evolution tuning, and live behavior."""
import json
import math
from pathlib import Path

import snake_test as st

TIER = "logic"
ORACLE = json.loads((Path(__file__).parent / "fixtures/javascript_weapon_oracle.json").read_text())
EVOLUTIONS = {
    "ouroboros": "venom_shot", "serpent_gatling": "viper_fangs",
    "singularity_mortar": "plague_mortar", "hydra_brood": "snake_nest",
    "miasma": "venom_nova", "consumption_beam": "sidewinder_beam",
    "shatter_fang": "ricochet_fang", "ancient_brood_pit": "cobra_pit",
    "serpents_reckoning": "tongue_lash",
}


def close(actual, expected, label):
    assert math.isclose(actual, expected, rel_tol=3e-5, abs_tol=3e-4), (label, actual, expected)


def setup(weapon, rank=1, evolved=False):
    st.reset(seed=0x5EED)
    return st.equip(weapon, rank=rank, evolved=evolved)["spec"]


def evolution_tuning():
    """Authoring multipliers from the selected catalog, never resolved specs."""
    recipes = {row["weapon"]: row for row in st.catalog()["evolutions"]}
    for recipe in recipes.values():
        assert all(math.isfinite(recipe[field]) and recipe[field] > 0
                   for field in ("damage", "cooldown", "area", "range")), recipe
    return recipes


def target(dx=48, dy=0, hp=100000, **fields):
    x, y = st.player()["position"]
    return st.spawn_enemy(x + dx, y + dy, hp=hp, **fields)


def health(handle):
    enemy = st.enemy(handle)
    return enemy["hp"] if enemy else 0


def damaged(handle, initial=100000):
    return initial - health(handle)


def wait_for(predicate, ticks=300, label="weapon effect"):
    return st.wait_until(predicate, ticks=ticks, chunk=1, message=label,
                         state=lambda: st.snapshot())


def test_all_18_weapons_all_8_ranks_match_executed_javascript_getters():
    """JS expectations stay independent; evolutions add authored CSV tuning."""
    checked = 0
    recipes = evolution_tuning()
    for name, rows in ORACLE["weapons"].items():
        for row in rows:
            evolved = name in EVOLUTIONS
            weapon = EVOLUTIONS.get(name, name)
            spec = setup(weapon, row["rank"], evolved)
            stats = st.snapshot(full=False)["stats"]
            tuning = recipes[weapon] if evolved else {"damage": 1, "cooldown": 1, "area": 1, "range": 1}
            receives_damage_bonus = name not in ("plague_mortar", "snake_nest", "hydra_brood")
            assert spec["damage_bonus"] is receives_damage_bonus, (name, spec)
            for getter, raw in row["getters"].items():
                field, expected = None, raw
                if getter == "get_cooldown":
                    field, expected = "cooldown", raw / 1000 * tuning["cooldown"] * stats["cooldown"]
                elif getter in ("get_damage", "get_tick_damage", "get_spit_damage", "get_laser_damage"):
                    field = "damage"
                    multiplier = round(stats["damage"], 6) if receives_damage_bonus else 1
                    expected = math.floor(raw * tuning["damage"] * multiplier + .5)
                elif getter in ("get_fang_count", "get_beam_count", "get_lash_count", "get_tongue_count"):
                    field = "amount"
                elif getter == "get_spawn_count":
                    field = "brood_count"
                elif getter == "get_max_bounces":
                    field = "bounces"
                elif getter == "get_range":
                    field, expected = "range", raw * st.TILE * tuning["range"] * stats["range"]
                elif getter in ("get_pool_radius", "get_radius", "get_pull_radius", "get_breathing_radius"):
                    field, expected = "area", raw * st.TILE * tuning["area"] * stats["area"]
                elif getter in ("get_duration", "get_pit_duration"):
                    field, expected = "duration", raw * stats["duration"]
                elif getter == "get_max_ticks":
                    field, expected = "duration", raw * .08 * stats["duration"]
                if field:
                    close(spec[field], expected, (name, row["rank"], getter))
                    checked += 1
    assert checked >= 250, checked


def test_venom_shot_fires_three_forward_fangs_and_requires_a_nearby_enemy():
    spec = setup("venom_shot")
    target(-64)
    state = st.step(1, full=True)
    shots = state["projectiles"]
    assert len(shots) == 3, shots
    assert all(p["velocity"][0] > 0 for p in shots), shots
    assert min(p["velocity"][1] for p in shots) < 0 < max(p["velocity"][1] for p in shots)
    for shot in shots:
        close(math.hypot(*shot["velocity"]), 40 * st.TILE, "venom speed")
    setup("venom_shot")
    assert st.step(120)["counts"]["projectiles"] == 0
    close(spec["duration"], .55, "venom lifetime")


def test_viper_fangs_home_on_targets_and_expire_on_source_schedule():
    spec = setup("viper_fangs")
    handle = target(56, 16)
    state = st.step(1, full=True)
    assert len(state["projectiles"]) == 1
    assert state["projectiles"][0]["target_id"] == handle
    close(spec["turn_speed"], 10, "viper steering")
    close(spec["duration"], .6, "viper lifetime")
    wait_for(lambda: damaged(handle) > 0, 60, "homing fang hit")
    assert st.step(45)["counts"]["projectiles"] == 0


def test_plague_mortar_lobs_to_a_fixed_ground_target_then_ticks_poison():
    setup("plague_mortar")
    handle = target(64)
    state = st.step(1, full=True)
    assert len(state["projectiles"]) == 1
    shot = state["projectiles"][0]
    close(shot["flight_duration"], .9, "mortar flight")
    landing = shot["target"]
    st.set_enemy(handle, position=[landing[0], landing[1] + 64])
    assert st.step(30)["counts"]["zones"] == 0
    state = st.step(25, full=True)
    pools = [z for z in state["zones"] if z["pattern"] == "mortar"]
    assert len(pools) == 1 and pools[0]["position"] == landing, pools
    close(pools[0]["max_life"], 4, "poison pool duration")
    close(pools[0]["tick_interval"], .35, "poison tick")
    assert damaged(handle) == 0, "Moving away from the landing cell must avoid the pool"
    st.set_enemy(handle, position=landing)
    wait_for(lambda: damaged(handle) > 0, 30, "poison damage")


def test_snake_nest_egg_hatches_three_persistent_grid_snakes():
    setup("snake_nest")
    target(64)
    state = st.step(1, full=True)
    assert state["counts"]["projectiles"] == 0 and len(state["nest_eggs"]) == 1
    egg = state["nest_eggs"][0]
    close(egg["flight_duration"], .75, "nest flight")
    close(egg["hatch_delay"], .5, "nest hatch delay")
    assert egg["count"] == 3
    st.unequip("snake_nest")
    assert st.step(60)["counts"]["mini_snakes"] == 0
    wait_for(lambda: st.snapshot()["counts"]["mini_snakes"] == 3, 30, "nest hatch")
    state = st.snapshot()
    assert all(m["segment_count"] == 3 for m in state["mini_snakes"])
    close(egg["snake_life"], 5.6, "summon lifetime")
    assert st.step(15)["counts"]["mini_snakes"] == 3
    st.invariants()


def test_venom_nova_is_one_instant_pulse_and_not_a_damage_over_time_pool():
    spec = setup("venom_nova")
    near, far = target(16), target(40)
    st.step(1)
    close(damaged(near), spec["damage"], "single nova damage")
    assert damaged(far) == 0
    st.unequip("venom_nova")
    st.step(60)
    close(damaged(near), spec["damage"], "nova must not repeat damage each .3s")


def test_sidewinder_tracks_one_target_for_2_5_seconds_with_0_2_second_ticks():
    setup("sidewinder_beam")
    handle = target(40)
    state = st.step(1, full=True)
    assert len(state["beams"]) == 1 and state["beams"][0]["target"] == handle
    close(state["beams"][0]["duration"], 2.5, "beam duration")
    close(state["beams"][0]["tick_interval"], .2, "beam tick")
    wait_for(lambda: damaged(handle) > 0, 20, "tracking beam tick")
    p = st.enemy(handle)["position"]
    st.set_enemy(handle, position=[p[0], p[1] + 16])
    state = st.step(1, full=True)
    assert state["beams"][0]["position"] == st.enemy(handle)["position"]
    st.step(150)
    assert not st.snapshot()["beams"]
    previous = health(handle)
    st.step(30)
    close(health(handle), previous, "expired beam must stop damaging")


def test_ricochet_hits_distinct_enemies_and_increases_damage_between_hits():
    setup("ricochet_fang")
    handles = [target(x) for x in (24, 40, 56)]
    st.step(120)
    damages = sorted(damaged(handle) for handle in handles)
    assert all(value > 0 for value in damages), damages
    assert damages[0] < damages[-1], damages
    assert st.snapshot()["counts"]["projectiles"] == 0


def test_cobra_pit_lands_spawns_stationary_cobras_and_fires_spit():
    setup("cobra_pit")
    handle = target(64)
    state = st.step(1, full=True)
    assert state["projectiles"][0]["pattern"] == "pit"
    close(state["projectiles"][0]["flight_duration"], .75, "cobra pit flight")
    assert not state["cobras"]
    wait_for(lambda: st.snapshot()["cobra_pits"], 60, "cobra pit landing")
    state = st.snapshot()
    close(state["cobra_pits"][0]["life"], 6, "pit lifetime")
    close(state["cobra_pits"][0]["cobra_life"], 5, "cobra lifetime")
    wait_for(lambda: st.snapshot()["cobras"], 45, "cobra rising")
    cobra = st.snapshot()["cobras"][0]
    assert cobra["spit_cooldown"] > 0
    wait_for(lambda: damaged(handle) > 0, 180, "cobra spit damage")
    assert st.snapshot()["cobra_pits"], "The pit must persist after the first shot"


def test_tongue_lash_hits_only_its_forward_cone_and_slows_survivors():
    spec = setup("tongue_lash")
    front, behind, outside = target(16), target(-16), target(40)
    st.step(1)
    close(damaged(front), spec["damage"], "front lash damage")
    assert damaged(behind) == 0 and damaged(outside) == 0
    enemy = st.enemy(front)
    assert enemy["slow"] > 1.4
    close(enemy["slow_factor"], .4, "lash slow factor")
    st.step(30)
    assert not any(z["pattern"] == "lash" for z in st.snapshot()["zones"])


def test_miasma_is_a_single_persistent_following_half_second_damage_cloud():
    setup("venom_nova", 8, True)
    handle = target(32)
    state = st.step(1, full=True)
    clouds = [z for z in state["zones"] if z["pattern"] == "nova" and z["evolved"]]
    assert len(clouds) == 1
    close(clouds[0]["tick_interval"], .5, "miasma tick")
    wait_for(lambda: damaged(handle) > 0, 40, "miasma damage")
    st.player(position=[808, 808])
    state = st.step(1, full=True)
    clouds = [z for z in state["zones"] if z["pattern"] == "nova" and z["evolved"]]
    assert len(clouds) == 1 and clouds[0]["position"] == state["player"]["position"]


def test_hydra_parents_have_five_segments_and_split_into_three_segment_brood():
    setup("snake_nest", 8, True)
    target(64)
    st.step(1)
    st.unequip("snake_nest")
    wait_for(lambda: st.snapshot()["counts"]["mini_snakes"] == 6, 90, "Hydra hatch")
    assert all(m["segment_count"] == 5 and m["generation"] == 0 for m in st.snapshot()["mini_snakes"])
    wait_for(lambda: any(m["generation"] == 1 for m in st.snapshot()["mini_snakes"]), 360, "Hydra splitting")
    brood = [m for m in st.snapshot()["mini_snakes"] if m["generation"] == 1]
    assert len(brood) == 12 and all(m["segment_count"] == 3 for m in brood), brood


def test_serpents_gatling_has_two_rapid_piercing_homing_fangs():
    spec = setup("viper_fangs", 8, True)
    target(48)
    state = st.step(1, full=True)
    assert len(state["projectiles"]) == 2
    assert all(p["gatling"] and p["pierce"] == 2 for p in state["projectiles"])
    close(spec["cooldown"], .18 * evolution_tuning()["viper_fangs"]["cooldown"], "configured gatling cadence")
    close(spec["speed"], 24 * st.TILE, "gatling speed")
    st.step(12)
    assert st.snapshot()["events"]["shot"] >= 2


def test_consumption_is_always_on_and_gives_growth_not_healing_on_kill():
    setup("sidewinder_beam", 8, True)
    st.player(hp=40)
    initial = st.player()["length"]
    target(32, hp=10)
    wait_for(lambda: st.snapshot(full=False)["kills"] == 1, 30, "consumption kill")
    state = st.snapshot()
    assert state["player"]["length"] >= initial + 3
    close(state["player"]["hp"], 40, "Consumption must not turn into a healing beam")
    assert any(beam["evolved"] for beam in state["beams"])


def test_persistent_evolutions_keep_offensive_passive_investment_on_actual_ticks():
    """Authored native balance rules retain prior cooldown/duration investment."""
    setup("venom_nova", 8, True)
    st.equip("rapid_fire", 2)
    st.equip("coiled_volley", 2)
    st.equip("chronofield", 2)
    victim = target(32)
    # Two duration ranks produce two additional half-waves in the base Nova.
    # Its persistent evolution concentrates those waves, three Volley payloads,
    # and Rapid Fire into one aura tick, without creating extra aura actors.
    expected = math.floor(320 * evolution_tuning()["venom_nova"]["damage"]
                          * 3 * 2 / (.85 ** 2) * 1.15 + .5)
    st.step(31)
    close(damaged(victim), expected, "Miasma retained Volley, Chronofield, and Rapid Fire")
    clouds = [zone for zone in st.snapshot()["zones"] if zone["pattern"] == "nova" and zone["evolved"]]
    assert len(clouds) == 1 and clouds[0]["tick_interval"] == .5

    setup("sidewinder_beam", 8, True)
    st.equip("rapid_fire", 2)
    st.equip("chronofield", 2)
    victim = target(32)
    expected = math.floor(100 * evolution_tuning()["sidewinder_beam"]["damage"]
                          * 1.5 / (.85 ** 2) * 1.15 + .5)
    st.step(7)
    close(damaged(victim), expected, "Consumption retained duration and cooldown investment")
    beams = [beam for beam in st.snapshot()["beams"] if beam["evolved"] and not beam["ancient"]]
    assert len(beams) == 1


def test_singularity_pulls_during_the_well_then_detonates_once():
    setup("plague_mortar", 8, True)
    target(64)
    state = st.step(1, full=True)
    close(state["projectiles"][0]["flight_duration"], .65, "configured singularity flight")
    landing = state["projectiles"][0]["target"]
    st.equip("plague_mortar", 8, True, cooldown=100)
    moving = target(80, 16)
    before = st.enemy(moving)["position"]
    wait_for(lambda: any(z["evolved"] and z["pattern"] == "mortar" for z in st.snapshot()["zones"]), 75, "singularity landing")
    st.step(24)
    after = st.enemy(moving)["position"]
    assert math.dist(after, landing) < math.dist(before, landing), (before, after, landing)
    assert st.enemy(moving)["slow"] > 0
    wait_for(lambda: not any(z["evolved"] and z["pattern"] == "mortar" for z in st.snapshot()["zones"]), 150, "singularity detonation")
    damage = damaged(moving)
    st.step(12)
    close(damaged(moving), damage, "well must detonate only once")


def test_shatter_critical_impacts_create_targeted_splinters():
    setup("ricochet_fang", 8, True)
    st.equip("dead_eye", rank=8)
    for x, y in ((24, 0), (40, 8), (48, -16), (64, 0), (32, 24)):
        target(x, y)
    wait_for(lambda: any(p["generation"] > 0 for p in st.snapshot()["projectiles"]), 90, "critical shatter splinters")
    shards = [p for p in st.snapshot()["projectiles"] if p["generation"] > 0]
    assert all(p["target_id"] and p["pattern"] == "ricochet" for p in shards)
    assert all(0 < p["generation"] <= 3 for p in shards)


def test_ancient_pit_creates_one_absorbing_laser_leviathan():
    setup("cobra_pit", 8, True)
    handle = target(64)
    wait_for(lambda: st.snapshot(full=False)["leviathan"]["active"], 75, "leviathan landing")
    first = st.snapshot(full=False)["leviathan"]["growth"]
    wait_for(lambda: damaged(handle) > 0, 180, "leviathan laser")
    state = st.snapshot()
    assert any(beam["ancient"] for beam in state["beams"])
    wait_for(lambda: st.snapshot(full=False)["leviathan"]["growth"] > first, 360, "absorbing the next ancient pit")
    assert st.snapshot(full=False)["leviathan"]["growth"] <= 10


def test_reckoning_extends_captures_pulls_and_devours_without_stuck_grabs():
    setup("tongue_lash", 8, True)
    handle = target(56, hp=500)
    state = st.step(1, full=True)
    assert state["grapples"] and state["grapples"][0]["phase"] == 0
    wait_for(lambda: (st.enemy(handle) or {}).get("grabbed", False), 60, "grapple capture")
    wait_for(lambda: st.enemy(handle) is None, 180, "grapple devour")
    assert not any(enemy["grabbed"] for enemy in st.snapshot()["enemies"])


def test_ouroboros_replaces_the_gun_with_44_persistent_orbit_fangs():
    spec = setup("venom_shot", 8, True)
    assert spec["amount"] == ORACLE["constants"]["ouroboros"]["NUM_FANGS"] == 44
    target(48)
    initial = st.snapshot(full=False)["orbit_phase"]
    state = st.step(120, full=True)
    assert state["orbit_phase"] != initial
    assert state["counts"]["orbit_fangs"] == 44
    assert len({tuple(round(v,3) for v in p) for p in state["orbit_fangs"]}) > 35
    assert not any(p["pattern"] == "shot" for p in state["projectiles"])
    assert len([z for z in state["zones"] if z["pattern"] == "shot" and z["evolved"]]) == 1
    assert state["kills"] > 0 or any(e["hp"] < e["max_hp"] for e in state["enemies"])


def test_passives_compose_using_the_source_attack_duration_area_and_crit_rules():
    setup("sidewinder_beam", 5)
    st.equip("rapid_fire", 3)
    st.equip("chronofield", 2)
    st.equip("toxic_expanse", 2)
    st.equip("dead_eye", 4)
    state = st.snapshot(full=False)
    close(state["stats"]["cooldown"], .85 ** 3, "Rapid Fire multiplicative stacking")
    close(state["stats"]["duration"], 1.5, "Chronofield")
    close(state["stats"]["area"], 1.3, "Toxic Expanse")
    close(state["stats"]["crit"], .5, "Dead Eye starts at zero")
    close(state["owned"][0]["spec"]["duration"], 3.75, "beam duration bonus")


def test_serpents_scales_absorbs_a_contact_instead_of_reducing_damage():
    st.reset()
    st.equip("serpents_scales", 1)
    st.configure(invulnerable=False)
    state = st.step(1)
    assert state["shield"]["charges"] > 0
    before = state["player"]["hp"]
    target(0, hp=100000)
    state = st.step(1)
    close(state["player"]["hp"], before, "shield absorbs contact")
    assert state["shield"]["charges"] == 0


def test_live_upgrades_refresh_original_fang_beam_and_nova_damage_stages():
    """Upgrade through the API while attacks survive; expected stages are JS literals."""
    setup("viper_fangs")
    handle = target(48)
    st.step(1)
    assert len(st.snapshot()["projectiles"]) == 1
    st.equip("viper_fangs", rank=3, cooldown=100)
    st.equip("ravenous_maw", rank=8)
    wait_for(lambda: damaged(handle) > 0, 45, "upgraded existing fang impact")
    close(damaged(handle), math.floor(300 * 1.5 * 1.15 + .5),
          "original fang reads current rank and Maw on impact")

    setup("sidewinder_beam")
    handle = target(48)
    st.step(1)
    assert len(st.snapshot()["beams"]) == 1
    st.equip("sidewinder_beam", rank=6, cooldown=100)
    st.equip("dead_eye", rank=8)
    wait_for(lambda: damaged(handle) > 0, 20, "upgraded existing beam tick")
    close(damaged(handle), math.floor(90 * 1.15 + .5) * 2,
          "Sidewinder rounds103 before doubling the critical hit")

    setup("venom_nova")
    st.equip("chronofield", rank=1)
    handle = target(16)
    st.step(1)
    main = damaged(handle)
    close(main, math.floor(100 * 1.15 + .5), "original Nova main wave")
    st.equip("venom_nova", rank=3, cooldown=100)
    st.equip("ravenous_maw", rank=8)
    st.step(17)
    close(damaged(handle) - main, math.floor(200 * (1.5 * 1.15) * .5 + .5),
          "delayed aftershock reads upgraded damage before halving and rounding")
    st.invariants()


for _name, _function in list(globals().items()):
    if _name.startswith("test_") and callable(_function):
        _function.timeout_ticks = 1800
