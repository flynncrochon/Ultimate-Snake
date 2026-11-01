# Vampire Survivors progression research and Snake adaptation

Researched 2026-11-01. The reference is **normal Mad Forest**, a standard
30-minute Vampire Survivors stage, without Hyper, Hurry, Inverse, Endless,
Golden Eggs, Limit Break, DLC characters, or Arcana modifiers. These modes
cannot be mixed into a single baseline damage/XP curve.

This describes run pacing. The subsequently implemented account-wide wallet,
permanent Shedskin bonuses and save migration are documented in
[permanent upgrades](PERMANENT_UPGRADES.md). The stage keeps thirty full
one-minute waves; it does not compress the schedule into twenty minutes.

## Evidence and its limits

- [poncle's official demo](https://poncle.itch.io/vampire-survivors) establishes
  the survival/collection loop and recommends concentrating upgrades into two
  or three weapons. Its listed session length is about half an hour. It is an
  older demo, not evidence of current retail internals.
- [Mad Forest's documented waves](https://vampire.survivors.wiki/w/Mad_Forest)
  provide the minute-by-minute population minima, spawn intervals, and boss
  times. Snake uses the 30 ordinary-wave minima and intervals from this table.
- [Enemy spawning and skills](https://vampire.survivors.wiki/w/Enemies) describe
  quota refill, a 300-enemy periodic-spawn limit, and HP x Level being evaluated
  **once at spawn** for enemies with that skill.
- [Level-up requirements](https://vampire.survivors.wiki/w/Level_up) describe
  the initial 5 XP requirement, successive 10/13/16 XP increments, and the
  600/2,400 XP transition surcharges. The explicit tier equations below follow
  the [published historical calculation](https://www.reddit.com/r/VampireSurvivors/comments/svtmvx/).
  The current wiki describes increments rather than publishing executable
  equations. These are a documented interpretation, not a verification against
  the closed-source current retail executable.
- [Growth](https://vampire.survivors.wiki/w/Growth) documents the temporary
  additive +100% Growth at levels 20 and 40, removed at 21 and 41.
- [Might](https://vampire.survivors.wiki/w/Might) describes percentage damage
  bonuses; [weapon stats](https://vampire.survivors.wiki/w/Weapons) distinguish
  damage, amount, cadence, hitbox delay, and weapon-specific exclusions.
  [Whip's ranks](https://vampire.survivors.wiki/w/Whip) are a concrete example:
  its base damage changes from 10 to 40 across eight ranks, while amount and
  area also change. Character level alone is not a universal damage multiplier.
- [Evolution rules](https://vampire.survivors.wiki/w/Evolution) document the
  usual 10-minute evolution-chest rule and its exceptions, including Mad
  Forest's minute-one chest. It would be inaccurate to apply a universal
  ten-minute lock to every chest.
- [Permanent PowerUps](https://vampire.survivors.wiki/w/PowerUps) have separate
  budgets: Might is +5% per rank for five ranks; Growth is +3% per rank for five
  ranks; Cooldown is -2.5% per rank for two ranks. The permanent Shedskin system
  uses these budgets for purchased bonuses inherited at the start of a run.

The wiki is community-maintained, manually compiled reference data. Its source
pages are indexed but direct retrieval is restricted. The implementation does
not reproduce Vampire Survivors art, source code, or a claimed universal DPS
or character-level-versus-time trajectory.

## Runtime rules

### XP and levels

`experienceForLevel(L)` is the cost from the **current** level L to L+1:

| Current level | Next-level cost |
|---|---|
| 1-19 | `10*L - 5` |
| 20 | `195 + 600 = 795` |
| 21-39 | `13*L - 6` |
| 40 | `514 + 2400 = 2914` |
| 41 onward | `16*L - 8` |

For example, 1->2 costs 5, 2->3 costs 15, 19->20 costs 185, 21->22
costs 267, and 41->42 costs 648. Levels 20 and 40 add **one** to the Growth
multiplier; a 1.25 multiplier becomes 2.25 at that level, then returns to 1.25.
The breakpoint is applied on collection, not to XP already banked.

Fractional XP carries between pickups. Four one-point fruit at 1.25 Growth
give 5 XP, just like a single four-point pickup. Overflow queues level choices
without losing XP or advancing combat. Raw fruit alone changes body length;
Growth and queued choices do not create extra segments. Length remains capped
at 120. Damage, XP, and inventory do not receive artificial catch-up grants.

### Damage and enemy strength

The nine weapon rank formulas, passive interactions, character bonuses, actor
lifetimes, and evolved weapon contracts are unchanged. Their actual damage
units are different from Vampire Survivors' weapon units. Copying an unrelated
weapon's displayed DPS would break Snake's established attacks.

Wave health, movement speed, contact damage, and XP rewards are authored in
`src/progression.cpp` for Snake's existing attacks. Early weak targets take one
or a few starter hits. Stronger waves alternate with numerous weak enemies at
11:00 and 21:00. Ordinary HP is independent of player level until the last two
elite waves; bosses and those late elites snapshot their HP x Level on spawn.
Leveling never heals or strengthens an existing enemy. Enemy recycling retains
the same HP, damage, and XP reward. Splitters retain their two-child lifecycle.

Most ordinary rewards are 1-5 raw XP; the final elite waves give 10. Splitters
give twice their wave's base reward; bosses give 30 and their normal chest.
Existing hand-built fixtures retain their legacy kind-based rewards/contact
damage unless they use real director-created enemies.

Nominal `damage * amount / cooldown` only describes a simple attack against
suitable targets. Pools, repeated hits, piercing, coverage, travel, missed
shots, summons, and executes make it inadequate for comparing whole builds.
The new damage ledger measures actual HP removed, capped at the victim's
remaining HP. Flood repulsion is not HP damage. Devours count remaining victim
HP, so their observed DPS must not be presented as ordinary raw attack damage.

### Waves and timer

The stage has 30 waves, each lasting 60 **simulation** seconds. A double
accumulator prevents the former repeated-float clock drift. At 60 fixed updates
per second, wave two begins on update 3,600 and victory occurs on update
108,000. Countdown, pause, item selection, and chest reveals are outside the
survival timer. Render rate and interpolation cannot advance it.

Each row defines a population minimum and a spawn interval. At each interval,
the director refills toward that minimum; above it, it introduces a small
weighted mix instead. Periodic ordinary spawning stops at 300 live ordinary
enemies. Splitter children and bosses have the existing shared hard limit of
1,800 actors. A quota refill is spread over updates, with **at most eight**
ordinary spawns per update. There is no unbounded catch-up loop or growing
spawn queue. Suppressing the director also clears pending refills.

Nineteen native boss waves are scheduled at minutes
1, 3, 5, 7, 8, 9, 10, 12, 14, 15, 16, 18, 20, 22, 23, 24, 25, 27, 29.
Boss health is authored per scheduled wave and multiplied by spawn-time player
level. A full ordinary pool yields a slot to a due boss. A full special-only
pool leaves it pending. No more than one boss is introduced per update.

Deliberate adaptations: Snake's existing red/blue/swift species substitute for
Mad Forest's larger roster. Its flood and grey barriers remain independent
native hazards. Arcana-only chest waves and scripted bat/flower events are not
implemented by this director. Existing guaranteed boss chests and immediate
recipe eligibility are retained. Reaching 30:00 awards native victory rather
than spawning Vampire Survivors' Reaper. Four weapon/four passive slots and
the fixed pickup radius remain unchanged.

## Measuring expected pacing

Vampire Survivors does not specify one guaranteed level or DPS at each minute.
Those outcomes depend on character, upgrades, build choices, routing, and
collection efficiency. This implementation schedules enemies, not the player's
level. The offline experiment measures what the real native combat produces:

```powershell
./scripts/build.ps1 -CoreOnly -Configuration Release -SkipTests
cmake --build build-tests --target snake_progression_sim --parallel
./build-tests/bin/snake_progression_sim.exe --seeds 3
./build-tests/bin/snake_progression_sim.exe --seeds 3 --mortal --output artifacts/pacing/progression-mortal.json
```

Run the build and optional-target commands in the same compiler-enabled shell.
The PowerShell build script imports MSVC for its process. From a separate
ordinary shell, run the build script first to import that environment again.

Each experiment starts all four characters at level one with their real starter.
The recorded baseline runs have zero permanent upgrades.
A deterministic collector uses cardinal shortcuts on a Hamiltonian circuit in
the central 100x100 cells, retaining room for future body growth. It collects
actual ground loot at the normal radius and chooses two weapons, then owned
ranks, recipe passives, Growth, and cooldown. It receives no free XP, ranks,
evolutions, teleports, or stat grants. The default run has explicit enemy-contact
immunity; `--mortal` enables normal contact damage. Both disable flood and grey
hazards because this route planner does not avoid them. Separate lifecycle
regressions exercise those hazards for the full 30 minutes.

JSON samples report each minute's actual level, effective DPS, kills/sec,
collected raw XP/sec, XP generated versus collected, equipment, population,
and first evolution time. These are finite scripted scenarios, not human
survival predictions. Timing covers headless `Game::update` only; it excludes
steering, choices, rendering, Python, protocol I/O, and report writing. Mean,
p99, worst, and first-update measurements cannot establish hitch-free play.

The GameTest API also exposes `snapshot.progression`: wave/remaining seconds,
run duration, population/cadence, scheduled bosses, effective damage and raw XP
ledgers. Enemy snapshots include their authored reward/contact damage, and
player snapshots expose fractional XP. `hello.run_seconds` supports fixtures
whose budgets/capacities depend on the stage length.

Regression coverage is in `tests/progression_tests.cpp` and
`game_tests/test_progression.py`, alongside the existing weapon-oracle, loot,
fixed-step, grey, flood, and dense-combat tests. Reports and captures belong in
ignored `artifacts/pacing`, not version control.

### Measured first-pass results

Windows x64 Release, MSVC 14.51. Twelve full-length contact-immune runs
(four characters x three seeds) all reached 30:00. The following are sample
means, with **first-evolution minimum/maximum**, not guaranteed player targets:

| Character | Level at 5:00 | At 10:00 | At 20:00 | At 30:00 | First evolution, minutes |
|---|---:|---:|---:|---:|---:|
| Cobra | 12.3 | 24.0 | 49.0 | 182.0 | 7.51-9.26 |
| Taipan | 11.7 | 23.7 | 67.7 | 140.3 | 7.20-14.70 |
| Mamba | 12.7 | 26.7 | 136.0 | 265.3 | 5.61-7.84 |
| Basilisk | 12.0 | 25.7 | 96.7 | 230.7 | 7.27-15.51 |

Equipment, XP, kills, and effective DPS were sampled every minute in
`artifacts/pacing/progression.json`; `run-curves.html` plots the means and
three-seed ranges. End levels span 72-364. The later spread is substantial:
Growth investment, evolving area weapons, devours, delayed collection of
merged XP, and repeated healing/gold choices all matter. This result does not
establish parity between the four starters or retail Vampire Survivors builds.

All twelve contact-enabled collector runs also completed their experiment;
none won the stage. They survived 1.81-29.39 minutes. This controller seeks
loot and preserves its body order but does not steer around hostile heads,
so these outcomes diagnose the limits of this controller and do **not** prove
that the stage is either fair or impossible for a human. Human survival and
the interaction with both native lethal hazards still require playtesting.

Verification: all 17 headless CTest targets passed, including 198 Python logic
checks. The native progression/weapon selection passed 136 checks; native
framebuffer, text, and presentation checks passed and three wave captures
were opened for review. The C++ progression tests cover eight groups, including
both same-tick breakpoint transitions and exactly 108,000 updates to victory.

The full-quality 1,800-enemy dense fixture used the same 300 fixed updates
before and after. Headless update timings, in microseconds:

| Measurement | Before | After |
|---|---:|---:|
| Mean | 94.88 | 96.93 |
| p99 | 229.00 | 219.60 |
| Worst | 264.40 | 265.50 |

The current 30-minute closed-body flood lifecycle, with 1,800 obstacles and
overlapping waves, measured mean 543.79 us, p99 922.40 us, and worst 7,067.30 us.
That workload incorporates the concurrent flood changes and is **not** a
matched before/after comparison with the earlier single-wave flood workload.
Its storage remained stable, samples stayed finite, and all lifecycle checks
passed. Neither set of CPU-only timings includes presentation or proves that
every frame on every machine is free of hitches. Raw logs are
`artifacts/pacing/before-stress.txt` and `verified-stress.txt`.
