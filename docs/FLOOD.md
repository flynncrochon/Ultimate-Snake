# Flood enemy

The flood uses the selected original pixel-water material: joined blue water,
wandering pale current lines and fine foam against solid bodies. Its original
fluid motion is restored as well.

## Natural arrivals

Normal runs enable the flood director automatically. A flood arrives every
**60 seconds of simulation** (60, 120, 180, and so on). Pauses, level choices
and reward reveals do not advance that clock.
The arrival notice reads `FLOOD INCOMING - SHOOT IT BACK`.

The director places a broad 640-sample surge approximately 175 world units
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

Body and tail contact deflect the water. Red, Swift, Splitter and boss heads
and body segments, grey barriers and arena walls also obstruct it. Head
contact immediately ends the run, including through HP, invulnerability and
Serpent's Scales. Water does not damage other enemies.

Shooting pushes water back instead of reducing HP. Swept projectile impacts
are ordered with ordinary enemy impacts, so a stopped shot cannot hit through
an earlier obstacle. Piercing and ricochet shots retain their hit history.
Homing weapons and beams target the nearest surface. Mortar pools and blasts,
nova pulses, orbiting fangs, summons, cobras and lashes also repel water.
Grapples splash it instead of grabbing or devouring an immortal target.
Existing weapon formulas are unchanged.

The single `kind: flood` enemy handle supports targeting; contact uses the
actual liquid surface. Water never draws as a snake, yields kills/loot, splits
into offspring or leaves snake corpses.

## Simulation and presentation

`src/flood_system.cpp` owns fixed-step inertia, local pressure, cohesion,
viscosity, rolling currents and collision foam. Two bounded substeps run per
60 Hz simulation tick. A uniform index limits pressure interactions to 48
local neighbors per sample, including corner-compressed populations. A cell
mask collects enemy bodies once per tick. Storage, speeds and populations are
bounded, and the water director and motion do not consume combat RNG.

`src/flood_art.cpp` merges samples into the original seven-tone blue material.
It emits integer two-source-pixel spans through the existing raster batch,
using the shared quantized camera and 800x500 point-filtered framebuffer.
Positions interpolate between fixed updates without changing simulation.
Effects settings never hide the lethal water. Density, foam and per-pixel
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

The single-wave material still matches all 120 archived original native
frames exactly in RGBA pixels. Lifecycle/schedule metadata changes as intended.
A 3,600-tick normal-run test
uses the real director, starter weapon, ordinary enemies and progression to
verify the first natural arrival without a `spawn_flood` fixture command.
Another API test advances through both actual minute arrivals, verifies two
full waves sharing one target and then observes only the older wave retire.
Native captures check a joined surface without a gap, the separate incoming
warning and an older wave fading while the younger surface remains opaque.
A presentation regression checks that starting a normal run after the preview
re-enables the director.

Native pixel coverage also checks animation, settings, render purity,
interpolation, retirement, arena corners and viewport changes.

The Release build passed all 29 configured CTest suites and all 17 headless
AddressSanitizer suites. The Python suite's wall-clock allowance is 120 seconds
to include the real two-arrival/expiry fixture under sanitizer instrumentation;
individual tick budgets remain bounded. The stress host passed two 30-minute
runs, dense combat and a 30-minute flood lifecycle with 1,800 enemies. The
flood lifecycle reached two full waves while retaining the original reserved
particle storage and preserving the closed player-body barrier.

Paired Release native timings used 1,200 frames per scene at 800x500 and full
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

This is a bounded stylized liquid model. Development-host measurements do not
establish 300+ FPS on low-end laptops or hitch-free play on every machine.
Earlier native benchmarks showed intermittent presentation stalls both with
and without water. Native frame timings include presentation; headless
simulation timings must not be presented as full-game FPS.
