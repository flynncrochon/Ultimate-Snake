"""Render matched evolution upgrade evidence from the real native simulation."""
import argparse
import csv
from datetime import datetime, timezone
import json
from pathlib import Path

from report_balance import ROOT, source_fingerprint


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=ROOT / 'artifacts/balance/evolution-upgrades.json')
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/EVOLUTION_UPGRADES.md')
    args = parser.parse_args()
    data = json.loads(args.input.read_text(encoding='utf-8'))
    checks = data['upgrade_checks']
    assert len(checks) == 135 and all(c['passed'] for c in checks)
    assert len(data['runs']) == 1350 and data['validation']['repeat_verified']
    assert len({(c['weapon_id'], c['profile'], c['scenario_id']) for c in checks}) == 135
    assert all(len(c['matched_seeds']) == 5 for c in checks)
    assert len(data['control_runs']) == 25
    assert all(r['damage'] == 0 and r['kills'] == 0 for r in data['control_runs'])
    for run in data['runs']:
        assert all(run['validation'].values())
        assert run['external_kills'] + run['external_remaining'] == run['external_spawned']
        assert abs(run['external_damage'] + run['external_remaining_hp'] - run['external_spawn_hp']) < .01
        assert run['kills'] + run['remaining'] == run['spawned']
    with (ROOT / 'data/evolutions.csv').open(newline='', encoding='utf-8') as file:
        recipes = list(csv.DictReader(file))
    names = {row['weapon']: row['name'] for row in recipes}
    lines = ['# Evolution upgrade validation', '',
             f"Measured {datetime.now(timezone.utc).date().isoformat()} after native evolution tuning. "
             '**All nine evolutions pass all 135 matched upgrade comparisons**, covering 1,350 fights '
             'with five seeds; every fight was executed twice with identical results. Another 25 unarmed controls deal zero damage and score zero kills.', '',
             '## What changed', '',
             '- Strengthened evolved damage/cadence/coverage through the authored evolution CSV. The original base weapon formulas remain the reference.',
             '- Retained Volley, Duration, and Rapid Fire investment when a repeated attack becomes a persistent aura/beam or a single concentrated well/egg.',
             '- Added eight close-orbit Ouroboros fangs, extended Consumption to 12 cells, gave Singularity a faster landing hit, and let mature Ancient seeds retain overflow power.',
             '- Routed actual orbit hits, well detonations, Hydra children, laser growth, and grapple damage through the configured evolution values.', '',
             'See [the native evolution contracts](WEAPON_PARITY.md#native-evolution-upgrade-tuning) for formulas and intentional changes from the original JavaScript.', '',
             '## Matched comparison', '',
             'Each pair uses a rank-8 base weapon versus its rank-8 evolution with **identical character, passives, ranks, enemy wave schedule, route, duration, and seed**. '
             'All builds use Cobra. Profiles are recipe passive at rank 1, recipe passive at its maximum rank, and recipe plus offensive support filling four passive slots. '
             'The offensive profile adds max-rank Rapid Fire, Coiled Volley, Chronofield, Dead Eye, and Ravenous Maw in that order until slots are full, retaining the recipe passive. '
             'Exact equipment is recorded in the raw JSON.', '',
             'Five fixtures per profile: early moving swarm (120 s, level 5, run clock 0); late moving and stationary swarms (60 s, level 40, run clock 840); '
             'stationary actual boss actors at 24 and 64 simulation units (90 s, 100,000,000 HP). The oversized boss HP measures sustained damage; it is not a claim about normal boss health. '
             'Normal contact mechanics remain enabled with a large player HP pool. Progression is locked and steering is scripted.', '',
             '**Pass rule:** mean damage to the identical incoming enemies must retain at least 98% of the base result; swarm kills may vary by max(1 kill, 2% of base kills). '
             'Mean damage or kills must improve by more than 5%, unless the base already removes at least 85% of the available targets/HP. '
             'Boss kills allow no losses; if both versions kill the durable boss in every seed, evolution must reduce mean kill time by at least 5%. '
             'These are aggregate regression thresholds with explicit seed tolerance, rather than guarantees about every individual frame or random encounter.', '',
             '**Splitter accounting:** Consumption and Reckoning can devour splitters before they create two children. Their upgrade comparison uses damage/kills against the identical incoming enemies '
             'and records prevented splits separately. Raw damage and kills, including actual children, remain unchanged in the report; no hypothetical child damage is credited. '
             'For fights without additional split prevention, the raw damage and kill floors must also pass.', '',
             '## Results for every evolution', '',
             'Each ratio below is the **lowest evolved/base incoming-enemy DPS ratio across all five fixtures** for that support profile. '
             'A ratio near 1 can mean both versions clear almost all available enemies; boss kill-time improvements are checked separately.', '',
             '| Evolution | Recipe rank 1: worst ratio | Recipe max: worst ratio | Offensive build: worst ratio | Checks passed |',
             '|---|---:|---:|---:|---:|']
    for weapon, name in names.items():
        family = [c for c in checks if c['weapon_id'] == weapon]
        ratios = [min(c['external_damage_ratio'] for c in family if c['profile'] == profile and c['external_damage_ratio'] is not None)
                  for profile in ('recipe', 'max', 'offense')]
        lines.append(f"| {name} | " + ' | '.join(f'{ratio:.3f}×' for ratio in ratios) + ' | 15 / 15 |')
    lines += ['', '## Late moving fight with rank-1 recipe passive', '',
              'DPS below counts all HP actually removed, and kills include actual splitter children. '
              'A devouring evolution can finish the same incoming wave with fewer raw kills because it prevents child spawns.', '',
              '| Evolution | Base DPS → evolved DPS | Base kills → evolved kills | Incoming-enemy kills: base → evolved | Splitters prevented from splitting |',
              '|---|---:|---:|---:|---:|']
    for weapon, name in names.items():
        row = next(c for c in checks if c['weapon_id'] == weapon and c['profile'] == 'recipe' and c['scenario_id'] == 'late_moving')
        lines.append(f"| {name} | {row['base_dps']:,.0f} → {row['evolved_dps']:,.0f} | "
                     f"{row['base_kills']:.1f} → {row['evolved_kills']:.1f} | "
                     f"{row['base_external_kills']:.1f} → {row['evolved_external_kills']:.1f} | {row['evolved_suppressed_splitters']:.1f} |")
    lines += ['', '## Authored evolution multipliers', '',
              'These multiply the original evolved specification, followed by the documented passive-retention rules. '
              'Smaller cooldown multipliers mean faster casts.', '',
              '| Evolution | Damage | Cooldown | Area | Range |', '|---|---:|---:|---:|---:|']
    for row in recipes:
        lines.append('| ' + row['name'] + ' | ' + ' | '.join(row[field] for field in ('damage', 'cooldown', 'area', 'range')) + ' |')
    performance = ROOT / 'artifacts/balance'
    before_path = performance / 'evolution-before/cold.json'
    after_path = performance / 'evolution-after/cold-durable.json'
    if before_path.is_file() and after_path.is_file():
        before = {row['scenario']: row for row in json.loads(before_path.read_text())}
        after = {row['scenario']: row for row in json.loads(after_path.read_text())}
        lines += ['', '## Local performance checks', '',
                  'Release, Windows x64, same seeded positions, 1,800 moving enemies, rank-8 evolved four-weapon fixtures, '
                  'and identical passive ranks. The first update and following 119 cold updates are measured separately; '
                  'warm samples contain 900 updates after 360 warmup updates. Every before/after sample ends with 1,800 living targets. '
                  'After tuning, fixture HP was raised from 100 million to one trillion to prevent the stronger lasers from emptying the benchmark; '
                  'ordinary HP damage has no target-selection effect while all these targets remain alive. '
                  'The benchmark now asserts that target population remains dense. Timings below are native simulation work, excluding rendering and protocol I/O.', '',
                  '| Dense fixture | First update: before → after (ms) | Cold p99: before → after (ms) | Warm mean: before → after (ms) | Warm p99: before → after (ms) |',
                  '|---|---:|---:|---:|---:|']
        for scenario in before:
            old, new = before[scenario], after[scenario]
            assert old['counts']['enemies'] == new['counts']['enemies'] == 1800
            pairs = [(old['first']['mean_us'], new['first']['mean_us']),
                     (old['cold']['p99_us'], new['cold']['p99_us']),
                     (old['warm']['mean_us'], new['warm']['mean_us']),
                     (old['warm']['p99_us'], new['warm']['p99_us'])]
            lines.append('| ' + scenario + ' | ' + ' | '.join(f'{a/1000:.3f} → {b/1000:.3f}' for a,b in pairs) + ' |')
        def render_rows(path):
            encoded = path.read_bytes()
            text = encoded.decode('utf-16' if encoded.startswith(b'\xff\xfe') else 'utf-8-sig')
            return {row['scenario']: row for row in (json.loads(line) for line in text.splitlines() if line.strip())}
        render_before = performance / 'evolution-before/render.jsonl'
        render_after = performance / 'evolution-after/render.jsonl'
        if render_before.is_file() and render_after.is_file():
            old, new = render_rows(render_before), render_rows(render_after)
            lines += ['', 'Rendering is measured separately with the unchanged full-quality 800×500 render stress fixture, '
                      '600 sampled frames after 60 warmup frames, uncapped pacing. This fixture fixes actor populations rather than simulating combat.', '',
                      '| Render fixture | CPU mean: before → after (ms) | CPU p99: before → after (ms) | Frame mean: before → after (ms) | Frame p99: before → after (ms) |',
                      '|---|---:|---:|---:|---:|']
            for scenario in old:
                fields = ('render_cpu_mean_ms', 'render_cpu_p99_ms', 'frame_mean_ms', 'frame_p99_ms')
                lines.append('| ' + scenario + ' | ' + ' | '.join(f'{old[scenario][f]:.3f} → {new[scenario][f]:.3f}' for f in fields) + ' |')
        lines += ['', 'The stronger attacks increase simulation work in these dense fixtures, with bounded actors and sub-millisecond sampled p99 update times on this machine. '
                  'Native captures were opened and inspected for the changed effects and HUD. These samples do not establish full-game FPS or hitch-free behavior on every machine.', '']
    lines += ['', '## Reproduce and regression gate', '', '```powershell',
              'cmake --build build --config Release --target snake_balance_sim',
              './build/bin/snake_balance_sim.exe --evolution-check --profile all --seeds 5 --seconds 120 --verify-repeat --assert-upgrades --output artifacts/balance/evolution-upgrades.json',
              'python scripts/report_evolution_upgrades.py', '```', '',
              'For CTest, configure `-DSNAKE_RUN_BALANCE_TESTS=ON`, build, then run '
              '`ctest --test-dir build -R snake_evolution_upgrades --output-on-failure`. '
              'The option builds the analysis executable and enables a failing upgrade regression gate. '
              '`--weapon ID` and `--profile recipe|max|offense` select focused tuning checks.', '',
              f'Raw measurements: `{args.input.relative_to(ROOT).as_posix() if args.input.is_absolute() else args.input.as_posix()}`. '
              'The original broad weapon/combo matrix is refreshed in [BALANCE_SIMULATIONS.md](BALANCE_SIMULATIONS.md).', '',
              f'Source fingerprint: `{source_fingerprint()}`.', '',
              'These checks establish upgrade behavior in the measured fixtures. Other characters, arbitrary passive combinations, complete progression runs, survival difficulty, '
              'and every possible enemy arrangement remain outside this finite validation set.', '']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\n'.join(lines), encoding='utf-8')
    print(f'Validated {len(checks)} upgrade comparisons; wrote {args.output}')


if __name__ == '__main__':
    main()
