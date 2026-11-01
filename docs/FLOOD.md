# Flood enemy

The flood uses the selected original pixel-water material: joined blue water,
wandering pale current lines and fine foam against solid bodies. Its original
fluid motion is restored as well.

## Natural arrivals

Normal runs enable the flood director automatically. A flood arrives every
**60 seconds of simulation** (60, 120, 180, and so on). Pauses, level choices
and reward reveals do not advance that clock.
The arrival notice reads `FLOOD INCOMING - SHOOT IT BACK`.

Natural waves start small and grow with simulation time: **128 samples at
1:00**, then **32 more each minute**, reaching the full **640 samples at
17:00** and holding that volume for the rest of the run. Clearing water does
not reset this progression; explicit preview/test waves keep their requested
volume.

The director places each surge approximately 175 world units
from the player with a 1.5-second visible warning. Each wave lasts 90 seconds
and drains over the following second. Consecutive waves therefore overlap for
30 seconds and join when they meet: they share pressure, cohesion, viscosity,
weapon impulses and one continuous rendered surface.

Each sample retains its source wave, age and warning. An arrival never resets
old water, makes it harmless or restarts its lifetime. New water remains
stationary and harmless during its own warning. Older water fades and retires
independently without erasing a younger wave or its targeting handle.

The preallocated shared pool holds at most 1,536 samples; normal minute
arrivals peak at 1,280 (two full waves). Excess fixture volume is rejected
without changing existing water or allocating. A full ordinary enemy pool
yields one slot for the first targeting handle, preserving bosses and the
1,800-enemy cap. Joining waves reuse that handle without evicting more enemies.

The preview command is optional. Starting a normal run after a preview resets
the director and enables natural arrivals again.

## Interactions

The player's body and tail deflect the water. Grey barriers and arena walls
also obstruct it. Red, Swift, Splitter and boss heads and body segments pass
through without deflecting the water or taking contact damage. Player head
contact deals **10 HP damage**, reduced by armor to a minimum of 3 HP. It uses
the ordinary one-second contact immunity after damage; overlapping samples
and joined waves cannot stack hits during that time. Existing invulnerability
blocks water damage, and Serpent's Scales absorbs a hit with one charge and
grants its usual quarter-second immunity. A run ends only when water depletes
the remaining HP. Water does not damage other enemies.

Shooting pushes water back and damages the contacted samples, each with 20 HP.
Destroyed samples disappear and count as kills, dropping one XP fruit apiece
and using the ordinary 2.5% chance of a 20-health heart. Drops stay on grid cells
and use the existing bounded XP aggregation when the pickup pool is full.
Warnings, harmless pushback and natural expiry never produce rewards.
Swept projectile impacts
are ordered with ordinary enemy impacts, so a stopped shot cannot hit through
an earlier obstacle. Piercing and ricochet shots retain their hit history.
Homing weapons and beams target the nearest surface. Mortar pools and blasts,
nova pulses, orbiting fangs, summons, cobras and lashes also damage and repel water.
Grapples splash and damage it instead of grabbing or devouring the target.
Existing weapon formulas are unchanged.

The single `kind: flood` enemy handle supports targeting; contact uses the
actual liquid surface. Damage clears only the affected samples, preserving the
rest of a wave and any joined waves. Destroying the final sample removes the
target. Water never draws as a snake, splits into offspring or leaves snake corpses.

## Simulation and presentation

`src/flood_system.cpp` owns fixed-step inertia, local pressure, cohesion,
viscosity, rolling currents and collision foam. Two bounded substeps run per
60 Hz simulation tick. A uniform index limits pressure interactions to 48
local neighbors per sample, including corner-compressed populations. A cell
mask collects the player's body once per tick; grey barriers use their existing
occupancy map. Damage queries use the same local sample index. Destroyed samples
are compacted once after combat, with no growing contact or reward list. Event
storage covers a simultaneous maximum enemy and water clear. Storage, speeds and
populations are bounded. The water director and motion do not consume combat RNG;
weapon criticals and loot rolls do.

`src/flood_art.cpp` merges samples into the original seven-tone blue material.
It emits integer two-source-pixel spans through the existing raster batch,
using the shared quantized camera and 800x500 point-filtered framebuffer.
Positions interpolate between fixed updates without changing simulation.
Effects settings never hide the hazardous water. Density, foam and per-pixel
opacity storage, kernel and sine lookup are prepared before play. Overlapping
water keeps the most opaque contribution, so a new warning or older wave's
fade cannot reduce live water's opacity. Normal rendering has no file I/O,
shader compilation, GPU readback, texture uploads or growing storage.

## Review and verification

```powershell
./snake.ps1 build
./scripts/run.ps1
./build/bin/snake_survivors.exe --scene flood
./snake.ps1 unit -R flood
./snake.ps1 gametest --single test_flood
./snake.ps1 bench 1200 --flood --diagnose
./snake.ps1 bench 1200 --flood-merged --diagnose
```

The seven-tone material and raster renderer are unchanged. Earlier restoration
validation matched all 120 archived original native frames exactly in RGBA
pixels. Enemy pass-through and weapon destruction now intentionally change
the simulated surface from that archive. Native damage captures check local
clearing, visible drops and removal of all blue pixels after the final sample.
A 3,600-tick normal-run test
uses the real director, starter weapon, ordinary enemies and progression to
verify the first natural arrival without a `spawn_flood` fixture command.
Another API test advances through both actual minute arrivals, verifies two
growing waves sharing one target and then observes only the older wave retire.
Pacing regressions cover every minute through the late-run volume cap and
verify that retiring waves never resets the ramp. Native captures compare the
small first arrival with the full-size arrival at 17:00.
Native captures check a joined surface without a gap, the separate incoming
warning and an older wave fading while the younger surface remains opaque.
A presentation regression checks that starting a normal run after the preview
re-enables the director.

Native pixel coverage also checks animation, settings, render purity,
interpolation, retirement, arena corners and viewport changes.
Contact regressions cover warnings, corner/swept hits, shared damage immunity,
shields, armor and the flood-specific death reason when HP reaches zero.

Before the damage/collision change, the Release build passed all 29 configured
CTest suites and all 17 headless
AddressSanitizer suites. The Python suite's wall-clock allowance is 120 seconds
to include the real two-arrival/expiry fixture under sanitizer instrumentation;
individual tick budgets remain bounded. The stress host passed two 30-minute
runs, dense combat and a 30-minute flood lifecycle with 1,800 enemies. The
flood lifecycle reached two full waves while retaining the original reserved
particle storage and preserving the closed player-body barrier.

Those earlier paired Release native timings used 1,200 frames per scene at
800x500 and full
visual quality. The 768-sample moving-combat workload measured mean / p99 /
worst frame times of 1.202 / 2.331 / 3.020 ms before and 1.179 / 2.225 / 2.532 ms
after, with matching enemy checksums. Two joined waves (1,280 samples) with
1,800 moving combatants measured 1.514 / 3.059 / 3.552 ms, including
presentation. Its first frame took 2.557 ms; resource preparation before play
took 39.576 ms. These single development-host runs include scheduling variance.

Captures, original-reference comparisons, natural-arrival evidence and timings
are retained under the ignored `artifacts/flood-merge` directory. Re-run an
exact native restoration comparison with:

```powershell
python scripts/showcase_flood_texture.py --record after --allow-flow-changes --require-pixel-parity --output artifacts/flood-merge
```

The current moving flood benchmark uses a closed player-body bend to protect
the head, since ordinary enemies now let water pass. Its water samples have
the same durable fixture HP as the ordinary benchmark targets, keeping the
full sample count throughout the measured combat; JSON marks this with
`water_durable_fixture`. These moving scenes differ from the earlier fixture.
The stationary full-quality scenes retain their original workload.

Damage/pass-through validation passed all 31 configured Release CTest suites
(including 207 Python logic checks), all 18 headless AddressSanitizer suites,
and the complete stress host. Native damage captures were opened and reviewed.
Maximum-clear regressions cover 1,536 water samples together with 1,799 ordinary
enemies and a pickup pool full of hearts, retaining XP and reserved storage.
The 1,200-frame native merged-water combat fixture retained 1,280 samples and
1,800 targets: mean / p99 / worst frame times were 1.136 / 2.433 / 2.738 ms;
its first frame took 2.295 ms. These include presentation and use the updated
durable fixture, so they are not an exact comparison with the earlier moving
scene. All six measured scenes stayed below the 60 Hz frame budget.

The long headless flood stress run recorded one 29.723 ms wall-clock outlier
while other development work was running. An isolated dense 5,000-tick follow-up
did not reproduce it: mean / p99 / worst simulation times were 0.766 / 0.947 /
1.183 ms, with a 1.088 ms first tick. This does not establish the cause of the
earlier outlier or full-game frame timing. Logs and paired native timings are
under ignored `artifacts/water-loot`; native captures are under
`artifacts/gametest/flood`.

This is a bounded stylized liquid model. Development-host measurements do not
establish 300+ FPS on low-end laptops or hitch-free play on every machine.
Earlier native benchmarks showed intermittent presentation stalls both with
and without water. Native frame timings include presentation; headless
simulation timings must not be presented as full-game FPS.
