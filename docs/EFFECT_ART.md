# Native pixel effect overhaul

Weapon effects use new native raster artwork while retaining the original
simulation, weapon actors, targeting, damage, lifetimes and fixed 60 Hz update.
The player geometry, 800x500 framebuffer and Pixel Operator font are unchanged.

`src/effect_art.cpp` contains eleven authored masks: venom, gatling, homing fang,
ricochet crystal, mortar, two eggs, cobra head and three orbit blade sizes.
Sixteen directions, four glint phases and critical palettes are prepared before
play in a point-filtered 320x1760 atlas. Every runtime draw uses a native 20x20
cell, with transparent padding; there is no runtime texture rotation or scaling.
Ouroboros retains all 44 independently positioned fangs and their size variation.

`src/world_renderer.cpp` adds layered poison bubbles and foam, stepped violet
singularity spirals, nova facets, beam cores and impact glints, forked tongues,
hatch details, scale highlights on base nest snakes, plain purple Hydra snakes,
rounded cobra hoods, gold Ancient beams, shield highlights and particle
fragments. Detail counts are bounded per actor.
Projectile wakes are batched before one contiguous texture pass. Graphics
settings still affect presentation only.

Hydra Brood summons use plain regular snake bodies with a single head and no
added scale detail. Their brood eggs and hatch cues share the purple palette.

## Serpent's Reckoning pull and snap

The evolved tongue follows the JavaScript reference's distinct extension,
retraction and white-jaw bite. A shaded pink ribbon tightens when latched;
forked hooks surround the interpolated prey and three small ripples travel
toward the mouth. The bite has two long ivory jaw banks, five teeth per bank,
a fast clamp, a brief clench, an opening fade, impact sparks and a fading
shockwave. Its heading is retained at devour, independent of later player turns.

Eight grid headings and thirteen openings are prepared as integer pixel spans
in `prepareWorldEffects()`. This bounded immutable cache occupies about 416 KiB
and uses the existing ordered raster batch. No additional textures, shaders,
GPU owners or per-attack allocations are needed. The effect uses simulation
age plus presentation interpolation, so pause freezes it and rendering never
uses gameplay RNG. Reduced weapon effects retain a simple visible tether.
Damage, grid pulls, capture protection, devour rewards and actor caps are unchanged.

```powershell
python scripts/showcase_reckoning.py --record before --exe PATH_TO_OLD_EXE
# Rebuild after the change.
python scripts/showcase_reckoning.py --record after
python scripts/build_reckoning_preview.py
python -m game_tests.run --native --single test_reckoning_visual
./build/bin/snake_render_benchmark.exe 1200 --reckoning --diagnose
```

The comparison records three real native clips and 372 before/after gameplay
checkpoints. The visual checks cover all eight headings, the open-to-clamped
silhouette, expiry, turning, pause, interpolation boundaries, repeat captures
and future combat/RNG. Four unrelated full-quality reference scenes retain
identical pixels; the combined grapple/bite scene intentionally changes.

Local Release measurements over 1,200 frames after 60 warmup frames, including
presentation and excluding simulation, are below in mean / p99 / worst ms:

- 1 tongue and 1 bite, 300 enemies and 200 projectiles: before
  0.1486 / 0.7425 / 1.7679; after 0.1440 / 0.6998 / 1.4566.
- 16 tongues and 16 bites, 1,800 enemies, 1,600 projectiles and 60 summons:
  before 0.6611 / 0.9870 / 1.4040; after 0.8640 / 1.1709 / 1.4061.
- The same dense scene with all 128 effect slots used for bites: before
  0.9237 / 1.2003 / 1.3952; after 1.3870 / 2.2160 / 2.9162.

The new dense and capacity scenes' first submissions measured 1.1366 and
1.6105 ms. Total presentation preparation measured 28.4429 ms before and
35.4287 ms after, including the new CPU art cache before gameplay. No measured
frame exceeded 16.67 ms in these runs. These local observations do not establish
cold OS/driver behavior or hitch-free play on every machine. Reports and the
standalone comparison are under ignored `artifacts/tongue-evolution`.

## Snake deaths

`src/death_art.cpp` dissolves retained cardinal body geometry on the native
pixel grid. A 45 ms palette-tinted impact leads into an erosion wave from the
head, then four small chips (six for bosses). Each opaque 2x2 source-pixel cell
uses a stable integer hash; whole-pixel camera translation preserves the exact
pattern. The shader quantizes its output to 8-bit colors. There is no blur,
antialiasing, fractional positioning, texture scaling, or simulation RNG use.
Normal enemies take 0.42 s, bosses 0.68 s, and nest/Hydra summons 0.38 s.
Successful splitters use the 0.30 s offspring transition described below.
The four player palettes finish at 0.72 s before results appear at 0.8 s;
combat is already stopped. Standing cobras retain their existing sinking
animation. Devoured enemies remain part of the consuming weapon's effect.

The simulation retains up to 2,048 compact death poses in storage reserved at
reset; saturated waves replace slots without allocating. Fixed updates age
these poses, and pause freezes them. The renderer batches all corpse geometry
behind loot and living actors. The particles preference controls flying chips;
the body dissolve remains readable. Existing generic death particles still
consume exactly the same simulation RNG and storage budget, but are hidden.

The shader and eight GPU upload slots are prepared before play, including
initial submissions. Its RAII owner releases them before the graphics context
closes. CPU death snapshots use approximately 328 KiB, staging 256 KiB, and GPU
instances 2 MiB. Rendering never creates shaders, buffers, or textures per death.

```powershell
./build/snake_death_tests.exe
./build/bin/snake_death_art_tests.exe
python scripts/build_death_preview.py
./snake.ps1 gametest --single test_death_visual --native
./build/bin/snake_death_benchmark.exe 1200
```

The native art suite exports 49 actual framebuffer samples to
`artifacts/gametest/death`. Pixel checks cover exact initial silhouettes,
monotone erosion, binary alpha, uniform 2x2 cells, camera translation,
determinism, 70,000-instance upload rollover, and GPU owner recreation.
The dedicated benchmark adds 0, 1, 512, or 2,048 full-length corpses to the same
full-quality 1,800-enemy / 1,600-projectile / 64-zone / 60-summon scene. It reports
first submission, warmup worst, mean, p99, and worst wall time, including
presentation. It does not advance simulation or measure GPU timer queries.

`scripts/build_death_preview.py` turns the captures into a self-contained,
scrubbable animation at `artifacts/death-effects/preview.html`, with integer
2x enlargement and no image smoothing.

## Splitter offspring

A splitter keeps the brief impact flash, then opens its blue body into two
narrow banks. Those banks bow apart, contract into the real moving offspring,
and turn red. One bank rises and the other drops along screen Y, reaching
26 source pixels of elevation each before settling. Vertical parents retain
their small horizontal opening as well. Faces inherit the parent's heading
until the new bodies take
shape. At 0.30 s the rendered silhouettes exactly match ordinary Swift snakes.
This replaces the additional generic dissolving corpse for a successful split.

The two Swift actors still spawn immediately at the original grid positions,
with unchanged health, speed, movement RNG, damage, loot, and cap behavior.
The animation never delays combat activation or creates extra snakes.
Devoured splitters do not spawn offspring. Each offspring owns 48 bytes of
compressed grid-pose history, approximately 84 KiB at the 1,800-enemy cap;
there is no per-frame allocation or lookup through the death pool. Pause
freezes the material clock, relocation clears it, and the shared camera still
quantizes every span to whole source pixels.

Ordered overlapping rectangles keep each intermediate body connected through
turns. The transition uses the existing prepared raster batch and shaders.

```powershell
python scripts/showcase_splitter.py --record before --exe PATH_TO_OLD_EXE
# Rebuild after the change.
python scripts/showcase_splitter.py --record after
python scripts/build_splitter_preview.py
./build/bin/snake_death_benchmark.exe 1200 --split
```

The comparison checks 93 native frames across straight, curved, and arena-edge
fixtures. Gameplay snapshots match before/after; final images match exactly.
The split benchmark exercises up to 900 simultaneous parents and 1,800 born
offspring alongside 1,600 projectiles, 64 zones, and 60 summons. Captures,
reports, and the standalone animation live in `artifacts/splitter-death`.

## Circle profiles

Circle profiles for radii 0 through 1024 occupy approximately 3 MiB of bounded
storage and are prepared at presentation startup. They require no per-radius
allocation or first-use sorting during play. Larger circles clip their work to
the source framebuffer. The compact profiles match the previous span cache
pixel-for-pixel in all 22 matched native effect fixtures.

## Reproduce the comparison

Before rebuilding the old renderer, run:

```powershell
python scripts/showcase_effects.py --record before
```

After rebuilding the new renderer:

```powershell
python scripts/showcase_effects.py --record after
python scripts/build_effects_comparison.py
python -m game_tests.run --single test_effects_visual --native
./build/bin/snake_effect_art_tests.exe
./build/bin/snake_render_benchmark.exe 1200
```

Captures, full gameplay snapshots and the standalone interactive comparison
are written under ignored `artifacts/effects-overhaul`. The comparison requires
identical gameplay state across the old and new renderer. These are actual
native framebuffer captures, with the same seeds, loadouts, targets and ticks.

## Local validation, 26 October 2026

All 22 original/new fixtures retain identical gameplay snapshots. The new
22-case native suite checks repeated pixels at interpolation 0, 0.5 and 1,
unchanged state, and identical future combat after rendering. Five atlas groups
check every direction, glint and critical palette, binary alpha, transparent
gutters, cardinal rotation, three blade sizes and texture owner recreation.
The existing movement, chest, raster, display and core regression checks also
passed during the overhaul.

Release native render measurements, 1200 measured frames after 60 warmup frames:

| Workload | Original mean / p99 / worst | New mean / p99 / worst |
|---|---|---|
| 300 enemies, 200 projectiles, 12 effects | 0.133 / 0.343 / 0.837 ms | 0.273 / 0.661 / 1.173 ms |
| 1800 enemies, 1600 projectiles, 64 effects, 60 summons | 0.643 / 0.844 / 1.030 ms | 1.279 / 1.679 / 2.345 ms |

The first representative draw measured 0.488 ms; the first dense workload draw
measured 1.425 ms on the already-created context. Presentation preparation,
including font/assets, effect atlas and circle profiles, measured 12.640 ms
before play. Raw reports are `before-render.jsonl` and `after-render.jsonl`.
These wall timings include rendering and presentation, exclude simulation, and
are local workload observations rather than cross-machine hitch-free claims.
