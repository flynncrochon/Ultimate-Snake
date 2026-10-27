"""Bounded native-update benchmarks; rendering is measured separately."""
import json
import os
from pathlib import Path

import snake_test as st

TIER = "bench"


def _measure(name, weapons, count):
    st.reset(seed=0xBEEF)
    for weapon, evolved in weapons:
        st.equip(weapon, 8, evolved)
    for passive in ("rapid_fire", "coiled_volley", "toxic_expanse", "chronofield"):
        st.equip(passive, 2 if passive == "coiled_volley" else 8)
    x, y = st.player()["position"]
    st.spawn_enemies({"x": x + ((i % 35) - 17) * 8,
                      "y": y + (((i // 35) % 23) - 11) * 8,
                      "hp": 1000000000000, "speed": 24, "kind": "red"}
                     for i in range(count))
    timing = st.benchmark(900, warmup=360, validate=True)
    result = {"scenario": name, "enemies": st.snapshot(full=False)["counts"]["enemies"],
              "weapons": weapons, "timing": timing}
    assert result["enemies"] == count, "Dense benchmark lost its target population"
    output = Path(__file__).resolve().parents[1] / "artifacts/performance" / (name + ".json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result))
    # Keep simulation comfortably below a 60 Hz frame, including sustained crowding.
    budget = float(os.environ.get("SNAKE_UPDATE_P99_US", "4000"))
    assert timing["p99_us"] < budget, (name, timing, budget)
    st.invariants()


def test_dense_evolved_projectiles_pools_and_hydra_update_budget():
    _measure("dense_hydra", [("viper_fangs", True), ("snake_nest", True),
                             ("venom_nova", True), ("plague_mortar", True)], 1800)


def test_dense_tracking_beams_orbits_and_stationary_turrets_update_budget():
    _measure("dense_tracking", [("sidewinder_beam", True), ("venom_shot", True),
                                ("cobra_pit", True), ("ricochet_fang", True)], 1800)


test_dense_evolved_projectiles_pools_and_hydra_update_budget.timeout_ticks = 1800
test_dense_tracking_beams_orbits_and_stationary_turrets_update_budget.timeout_ticks = 1800
