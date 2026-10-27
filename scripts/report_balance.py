"""Validate native balance measurements and render the checked-in Markdown report.

This only summarizes observed runs. Combat and damage accounting live in the
optional snake_balance_sim executable, which calls the real Game::update.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import statistics


ROOT = Path(__file__).resolve().parents[1]


def source_fingerprint():
    paths = sorted([*ROOT.glob("src/*.cpp"), *ROOT.glob("src/*.hpp"),
                    *ROOT.glob("data/*.csv"), ROOT / "tests/balance_sim.cpp"])
    digest = hashlib.sha256()
    for path in paths:
        digest.update(path.relative_to(ROOT).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def mean(values):
    return statistics.mean(values)


def spread(values):
    return statistics.stdev(values) if len(values) > 1 else 0.0


def cell(runs):
    dps = [r["damage"] / r["seconds"] for r in runs]
    kills = [r["kills"] for r in runs]
    return f"{mean(dps):,.0f} ± {spread(dps):,.0f} / {mean(kills):.1f} ± {spread(kills):.1f}"


def load_and_validate(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    assert data["fixed_hz"] == 60 and data["method_version"] == 1
    assert data["validation"]["all_runs_passed"] is True
    assert data["validation"]["repeat_verified"] is True
    assert data["validation"]["zero_damage_controls_passed"] is True
    builds = {b["id"]: b for b in data["builds"]}
    scenarios = {s["id"]: s for s in data["scenarios"]}
    assert set(scenarios) == {"early_moving", "late_moving", "late_hold"}
    seeds = data["seed_values"]
    assert len(seeds) >= 5 and len(set(seeds)) == len(seeds), "Need five distinct seeds"
    grouped = defaultdict(list)
    seen = set()
    for run in data["runs"]:
        key = (run["build_id"], run["scenario_id"], run["seed"])
        assert key not in seen, f"Duplicate run: {key}"
        seen.add(key)
        assert run["build_id"] in builds and run["scenario_id"] in scenarios
        assert run["seed"] in seeds
        assert run["seconds"] == scenarios[run["scenario_id"]]["seconds"]
        assert 0 <= run["damage"] <= run["total_spawn_hp"] + 1e-5, run
        assert run["kills"] >= 0 and run["remaining"] >= 0, run
        assert all(run["validation"][name] is True for name in
                   ("finite_grid_equipment_caps", "hp_ledger", "kill_ledger"))
        assert run["kills"] + run["remaining"] == run["spawned"]
        assert run["spawned"] == run["external_spawned"] + run["splitter_children"]
        assert abs(run["total_spawn_hp"] - run["remaining_hp"] - run["damage"]) < 1e-5
        assert abs(run["cold_damage"] + run["steady_damage"] - run["damage"]) < 1e-5
        assert run["cold_kills"] + run["steady_kills"] == run["kills"]
        assert abs(run["cold_seconds"] + run["steady_seconds"] - run["seconds"]) < 1e-5
        grouped[(run["build_id"], run["scenario_id"])].append(run)
    assert len(seen) == len(builds) * len(scenarios) * len(seeds), "Incomplete matrix"
    singles = [b for b in builds.values() if len(b["weapons"]) == 1]
    pairs = [b for b in builds.values() if len(b["weapons"]) == 2]
    base = [b for b in singles if not b["weapons"][0]["evolved"]]
    evolved = [b for b in singles if b["weapons"][0]["evolved"]]
    assert len(base) == 27 and len(evolved) == 9 and len(pairs) == 144, "Incomplete build coverage"
    assert Counter(w["rank"] for b in base for w in b["weapons"]) == {1: 9, 4: 9, 8: 9}
    assert len({tuple(sorted((w["id"], w["evolved"]) for w in b["weapons"]))
                for b in pairs}) == 144
    assert all(len({w["id"] for w in b["weapons"]}) == len(b["weapons"]) for b in pairs)
    assert all(len(b.get("passives", [])) <= 4 for b in builds.values())
    presets = [b for b in builds.values() if len(b["weapons"]) == 4]
    assert len(presets) == 4 and len(builds) == 184
    assert len(data["control_runs"]) == len(scenarios) * len(seeds)
    assert all(r["damage"] == 0 and r["kills"] == 0 for r in data["control_runs"])
    assert len({(r["scenario_id"], r["seed"]) for r in data["control_runs"]}) == len(data["control_runs"])
    return data, builds, scenarios, grouped, singles, pairs


def render(data, builds, scenarios, grouped, singles, pairs, raw_path):
    scenario_list = list(scenarios.values())
    late = next(s for s in scenario_list if s["id"] == "late_moving")
    hold = next(s for s in scenario_list if s["id"] == "late_hold")
    early = next(s for s in scenario_list if s["id"] == "early_moving")

    def score(build, scenario=late):
        return mean(r["damage"] / r["seconds"] for r in grouped[(build["id"], scenario["id"])])

    best_single = max((b for b in singles if b["weapons"][0]["rank"] == 8), key=score)
    best_pair = max(pairs, key=score)
    run_seconds = sum(r["seconds"] for r in data["runs"])
    lines = [
        "# Simulated weapon DPS and kills",
        "",
        f"Measured {datetime.now(timezone.utc).date().isoformat()} using the real C++17 combat simulation at 60 Hz. "
        f"{len(builds)} fixed loadouts × {len(scenarios)} scenarios × {len(data['seed_values'])} seeds = "
        f"**{len(data['runs']):,} fights, {run_seconds / 3600:.1f} simulated combat hours**. "
        "These are executed fights, including projectile travel, misses, retargeting, area damage, summons, splitting, slows, knockback, and executions.",
        "",
        f"In the late moving fixture, the highest mean single was **{best_single['label']}** "
        f"({score(best_single):,.0f} effective DPS) and the highest mean pair was **{best_pair['label']}** "
        f"({score(best_pair):,.0f} effective DPS). Use the scenario-specific tables below: target supply and movement can change the ranking.",
        "",
        "## Reading the numbers",
        "",
        "Every result cell is **effective DPS ± sample standard deviation / kills ± sample standard deviation**, "
        f"over {len(data['seed_values'])} matched seeds. Kills are the mean **total for that column's fight duration**, not kills per second. "
        "Fractional kill counts come from averaging integer results. To get kills/minute, multiply kills by 60/fight seconds. "
        "The ± values describe seed variation, not confidence intervals.",
        "",
        "Effective DPS = (all HP supplied − HP still alive) / exact simulated seconds. "
        "Each target's contribution stops at zero HP: overkill is excluded, while partial damage to surviving enemies is included. Native splitter children add their actual max HP to the supply ledger after they spawn. "
        "Reckoning's devour counts the victim's remaining HP; it is an execute, so this metric must not be read as its raw hit damage. "
        "Damage is the whole loadout's observed output; a pair is simulated together, never calculated by adding single-weapon DPS.",
        "",
        "## Scenarios and fixed assumptions",
        "",
        "| Fixture | Fight length | Starting run clock / player level | Player movement |",
        "|---|---:|---|---|",
    ]
    for scenario in scenario_list:
        movement = "Square route; turn every 150 ticks (2.5 s)" if scenario["moving"] else "Stationary; normal facing"
        lines.append(f"| {scenario['name']} | {scenario['seconds']:g} s | {scenario['elapsed']:g} s / level {scenario['level']} | {movement} |")
    lines += [
        "",
        "- All builds use **Cobra** and its native ×1.15 direct-damage bonus. Indirect poison and nest bites keep their normal exclusions. Character comparisons are outside this matrix.",
        "- Base singles test ranks **1, 4, and 8**, with no passives. Pairs use rank-8 weapons. Evolutions replace the base family and include their required recipe passive at **rank 1**. Evolved pairs own the union of those recipe passives, which affect the whole build. Thus pair gains include shared passive effects as well as weapon interaction; these tables do not isolate additive synergy or equal upgrade cost.",
        "- A separate unarmed native world records the actual director's timed red, swift, and splitter spawns. That schedule is reused for every loadout of the same scenario and seed. Enemy HP, speed, radius, and bodies come from that native world; weapon RNG cannot change the supplied wave schedule. Real splitter children still spawn inside each tested fight.",
        "- Fights begin with no enemies or lingering weapon actors, and attacks ready to fire. No artificial prewarming. Enemy pursuit, target recycling, actor caps, pickup behavior, and contact displacement stay active. No adaptive steering.",
        "- The player uses a large HP pool with **normal contact damage enabled**, preserving hit cadence and knockback. XP is collected, but a huge next-level threshold fixes level and equipment. This measures offensive output, not survival probability or a human's optimal play.",
        "- **Boss spawns and chests are excluded** to prevent random evolution/rank changes. Results therefore describe swarm combat, not isolated boss DPS, boss time-to-kill, or a complete 20-minute progression run. Four example full loadouts are included; the remaining three/four-weapon space, every passive investment, other characters, and optimized routes need separate simulations.",
        "- Finite target supply limits measured DPS. A strong build can run out of reachable enemies; a weak one builds a denser crowd. High area DPS and kills reflect that actual crowd interaction. Neither these measurements nor simulation speed establish rendering FPS.",
        "",
        "## Balance signals from these fights",
        "",
    ]
    plague = builds["plague_mortar_r8"]
    singularity = builds["plague_mortar_evolved"]
    viper = builds["viper_fangs_r8"]
    gatling = builds["viper_fangs_evolved"]
    ancient = builds["cobra_pit_evolved"]
    beam = builds["sidewinder_beam_r8"]
    ancient_runs = grouped[(ancient["id"], late["id"])]
    ancient_dps = [r["damage"] / r["seconds"] for r in ancient_runs]
    lines += [
        f"- **Evolution payoff:** Singularity Mortar changes effective DPS by {100 * (score(singularity) / score(plague) - 1):+.0f}% "
        "relative to rank-8 Plague Mortar in late moving combat. The separate "
        "[evolution upgrade checks](EVOLUTION_UPGRADES.md) compare identical passive investment and include durable boss targets.",
        f"- **Damage versus finishes:** Serpent's Gatling improves late moving effective DPS by "
        f"{100 * (score(gatling) / score(viper) - 1):.0f}% over rank-8 Viper Fangs; mean kills change from "
        f"{mean(r['kills'] for r in grouped[(viper['id'], late['id'])]):.1f} to "
        f"{mean(r['kills'] for r in grouped[(gatling['id'], late['id'])]):.1f}. Partial damage and kills tell different balance stories.",
        f"- **Setup and seed sensitivity:** Ancient Brood Pit ranges from {min(ancient_dps):,.0f} to "
        f"{max(ancient_dps):,.0f} late moving DPS. Its first {late['cold_seconds']:g}s averages "
        f"{mean(r['cold_damage'] / r['cold_seconds'] for r in ancient_runs):,.0f} DPS, versus "
        f"{mean(r['steady_damage'] / r['steady_seconds'] for r in ancient_runs):,.0f} later. "
        "Setup deserves separate attention from mature output.",
        f"- **Crowd specialization:** Rank-8 Plague Mortar averages {score(plague):,.0f} late moving DPS versus "
        f"{score(beam):,.0f} for Sidewinder Beam. AoE repeatedly benefits from accumulated crowds; "
        "use the separate matched boss checks to assess single-target evolution payoff.",
        "",
        "## Base weapons at ranks 1, 4, and 8",
        "",
    ]

    def table(items, include_passives=False):
        header = ["Loadout", f"Early moving ({early['seconds']:g}s)",
                  f"Late moving ({late['seconds']:g}s)", f"Late hold ({hold['seconds']:g}s)"]
        if include_passives:
            header.append("Recipe passive(s), rank 1")
        lines.append("| " + " | ".join(header) + " |")
        lines.append("|" + "---|" * len(header))
        for build in items:
            row = [build["label"]] + [cell(grouped[(build["id"], s["id"])]) for s in (early, late, hold)]
            if include_passives:
                row.append(", ".join(p.get("name", p["id"]).replace("_", " ") for p in build.get("passives", [])) or "—")
            lines.append("| " + " | ".join(row) + " |")
        lines.append("")

    table([b for b in singles if not b["weapons"][0]["evolved"]])
    lines += ["## Evolutions with their recipe passive", ""]
    table([b for b in singles if b["weapons"][0]["evolved"]], include_passives=True)

    lines += ["### Evolution change relative to rank-8 base", "",
              "Observed change in effective DPS with the same fixture and seeds. The evolved build also gains its "
              "rank-1 recipe passive; the base has no passive. Negative values flag a weaker evolution in this specific "
              "swarm fixture, not a universal downgrade or an isolated measurement of the evolution mechanic.", "",
              "| Evolution | Early moving | Late moving | Late hold |",
              "|---|---:|---:|---:|"]
    for build in singles:
        weapon = build["weapons"][0]
        if not weapon["evolved"]:
            continue
        base = next(b for b in singles if b["weapons"][0] == {"id": weapon["id"], "rank": 8, "evolved": False})
        changes = [100 * (score(build, scenario) / score(base, scenario) - 1) for scenario in (early, late, hold)]
        lines.append("| " + build["label"] + " | " + " | ".join(f"{value:+.0f}%" if abs(value) >= .5 else "0%" for value in changes) + " |")
    lines.append("")

    lines += ["## Startup versus later output", "",
              f"Late moving fight only: the first {late['cold_seconds']:g}s includes travel and setup; "
              f"the remaining {late['seconds'] - late['cold_seconds']:g}s retains every actor and accumulated enemy. "
              "Later output includes density changes, so this is not a target-independent sustained-DPS estimate.", "",
              "| Rank-8 weapon / evolution | First-window DPS | Later-window DPS | Total DPS seed range | Total kill seed range |",
              "|---|---:|---:|---:|---:|"]
    for build in singles:
        if build["weapons"][0]["rank"] != 8:
            continue
        runs = grouped[(build["id"], late["id"])]
        cold = [r["cold_damage"] / r["cold_seconds"] for r in runs]
        steady = [r["steady_damage"] / r["steady_seconds"] for r in runs]
        dps = [r["damage"] / r["seconds"] for r in runs]
        kills = [r["kills"] for r in runs]
        lines.append(f"| {build['label']} | {mean(cold):,.0f} | {mean(steady):,.0f} | {min(dps):,.0f}–{max(dps):,.0f} | {min(kills)}–{max(kills)} |")
    lines += ["", "## Every legal two-weapon combo", "",
              "All 144 pairs of different families are below: 36 base/base, 36 evolution/evolution, and 72 mixed. "
              "A base weapon and its own evolution cannot coexist. Ordering is by measured late moving DPS within each group; "
              "the native equipment/update order follows catalog order and is held constant across seeds.", ""]
    for number_evolved, title in ((0, "Base + base (36)"), (2, "Evolution + evolution (36)"), (1, "Base + evolution (72)")):
        lines += [f"### {title}", ""]
        items = [b for b in pairs if sum(w["evolved"] for w in b["weapons"]) == number_evolved]
        table(sorted(items, key=score, reverse=True))

    lines += ["## Four-weapon example loadouts", "",
              "These four presets are additional executed fights, not an exhaustive search for the best full build. "
              "Base weapons are rank 8 without passives; evolved presets own their four recipe passives at rank 1.", ""]
    table(sorted((b for b in builds.values() if len(b["weapons"]) == 4), key=score, reverse=True), include_passives=True)

    lines += ["## Enemy pressure and measurement checks", "",
              "These diagnostics help distinguish damage ceilings from limited target availability. "
              "Remaining enemies are alive at the end. Supply includes actual splitter children, so it can differ between builds.", "",
              "| Fixture | Best rank-8 single by DPS | DPS | Kills | Enemies remaining | HP supply removed |",
              "|---|---|---:|---:|---:|---:|"]
    for scenario in scenario_list:
        build = max((b for b in singles if b["weapons"][0]["rank"] == 8), key=lambda b: score(b, scenario))
        runs = grouped[(build["id"], scenario["id"])]
        lines.append(f"| {scenario['name']} | {build['label']} | {score(build, scenario):,.0f} | "
                     f"{mean(r['kills'] for r in runs):.1f} | {mean(r['remaining'] for r in runs):.1f} | "
                     f"{100 * mean(r['damage'] / r['total_spawn_hp'] for r in runs):.1f}% |")
    lines += ["", "Recorded external waves, before each fight's additional splitter children:", "",
              "| Fixture | Supplied enemies (seed range) | External target HP range | Enemy speed range (units/s) |",
              "|---|---:|---:|---:|"]
    for scenario in scenario_list:
        summaries = [w["source_summary"] for w in data["workloads"] if w["scenario_id"] == scenario["id"]]
        lines.append(f"| {scenario['name']} | {min(w['spawned'] for w in summaries)}–{max(w['spawned'] for w in summaries)} | "
                     f"{min(w['min_hp'] for w in summaries):,.0f}–{max(w['max_hp'] for w in summaries):,.0f} | "
                     f"{min(w['min_speed'] for w in summaries):.1f}–{max(w['max_speed'] for w in summaries):.1f} |")
    lines += ["", f"Seeds: `{', '.join(str(s) for s in data['seed_values'])}`.", "",
              "Native validation results:", "", "```json", json.dumps(data.get("validation", {}), indent=2), "```", "",
              "The executable rejects non-finite state, invalid grid paths, lost HP accounting, changed equipment, "
              "unexpected phases, and exceeded pool bounds. Unarmed controls must deal exactly zero damage and score zero kills. "
              "Every armed fight is repeated to check deterministic damage, kills, and actor totals; "
              "repeats are additional validation executions and are excluded from the tabulated fight/hour totals.", "",
              "## Reproduce", "",
              "From the repository root, configure a Release build using the normal compiler environment, then:", "",
              "```powershell", "cmake --build build --config Release --target snake_balance_sim", 
              "./build/bin/snake_balance_sim.exe --data data --output artifacts/balance/results.json --seeds 5 --seconds 120 --verify-repeat",
              "python scripts/report_balance.py --input artifacts/balance/results.json --output docs/BALANCE_SIMULATIONS.md", "```", "",
              "`snake_balance_sim` is an optional target and is excluded from normal builds. "
              "A headless Release build (`SNAKE_BUILD_GAME=OFF`) works too; use its build directory. "
              "On Windows, `snake.ps1` can locate Blender's bundled Python; the report script needs only Python's standard library.", "",
              f"Raw per-seed measurements: `{raw_path}` (ignored local artifact). "
              "The Markdown preserves the aggregate results; rerunning regenerates the raw evidence. "
              "Rebuild after simulation or content changes before using this report to make balance decisions.", "",
              f"Source fingerprint (SHA-256 of sorted `src/*.cpp`, `src/*.hpp`, `data/*.csv`, and the harness with paths): `{source_fingerprint()}`.", "",
              "Measurement implementation: [`tests/balance_sim.cpp`](../tests/balance_sim.cpp). "
              "Aggregation and matrix checks: [`scripts/report_balance.py`](../scripts/report_balance.py). "
              "Combat rules: [`WEAPON_PARITY.md`](WEAPON_PARITY.md), [`DESIGN.md`](DESIGN.md).", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "artifacts/balance/results.json")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/BALANCE_SIMULATIONS.md")
    args = parser.parse_args()
    data, builds, scenarios, grouped, singles, pairs = load_and_validate(args.input)
    report = render(data, builds, scenarios, grouped, singles, pairs, args.input.as_posix())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(report, encoding="utf-8")
    print(f"Validated {len(data['runs'])} fights; wrote {args.output}")


if __name__ == "__main__":
    main()
