# Validation results

Validated locally on 26 October 2026, Windows x64, MSVC 14.51, raylib 5.5.
The JavaScript reference is commit `b6cb72e66eebc1291d57efe63ebbe93ec602e863`.

## Correctness

Grey barrier snakes were validated on 27 October 2026 against the same pinned
JavaScript reference. All 22 Release CTest targets and all 13 headless
AddressSanitizer targets pass. The new checks execute eleven original JavaScript
AI cases, a complete twenty-minute barrier lifecycle without storage growth,
lethal collision grace and recovery, shield/immunity bypass, loot exclusion,
pause/reset, and thirteen-trail capacity limits. Ten grey Python cases pass in
the actual raylib host, including 61 independent silhouette/frame comparisons
and minimap/collision-warning checks. Gameplay, retired walls, long trails, and
turn/retirement captures were opened and reviewed. The existing two combat
endurance scenarios and three seeded combat hunts also pass; the scripted
combat routes disable grey spawning, while the separate barrier lifecycle run
keeps it enabled. See [grey snake contracts](GREY_SNAKE.md).

Five full-quality weapon scenes retain zero changed pixels. At the same
600-frame full-quality dense workload (1,800 enemies, 1,600 projectiles, 64 zones,
60 summons), native frame mean/p99/worst measured 0.9018/1.4584/1.6703 ms before
and 0.9161/1.2459/1.4632 ms afterward. Adding thirteen visible grey trails measured
1.0260/1.4081/1.6161 ms, with a 1.2734 ms first frame and 1.3845 ms warmup worst.
The moving 1,800-enemy combat fixture with grey trails measured
0.6982/1.3518/1.4881 ms, including 60 Hz simulation and presentation at virtual
240 Hz. The 10,000-enemy render-only overload with grey trails had a 5.0485 ms
worst frame. No sampled frame exceeded 16.67 ms, including first use/warmup.
These local wall times include presentation, vary with system load, and do not
establish hitch-free behavior on other machines. Raw reports are under
`artifacts/performance/grey-snake`; reviewed captures and logic/hunt reports are
under `artifacts/grey-snake` and `artifacts/gametest/grey-snake`.

| Check | Result |
|---|---|
| Live coverage of every weapon/rank and evolution | 132 added combat/progression tests: 396 headless passes across three repeats and 132 native passes; [coverage map](WEAPON_TEST_COVERAGE.md) |
| Ten C++ regression suites | 107 groups pass in Release, Debug, and AddressSanitizer builds |
| Native presentation suite | Eight groups pass for boss drops, lottery skip/continue guards, FIFO clocks, overlay pauses, reset, and unchanged seeded choices |
| Native settings and display | Six UI groups and four display groups pass, including real hidden window resize/borderless rollback, VSync/focus, mute/save failures, and drafts |
| Native raster equivalence | Four groups pass, including clipping, alpha overlap, texture ordering, and 35,000-quad rollover; isolated AddressSanitizer run also passes |
| Full-quality effect parity | Five before/after scenes match exactly, with zero changed pixels |
| Native chest bitmap art | Both 16×16 pickup and 32×32 reward sprites match every asset pixel and transparent border; three repeats pass (six cases) |
| Native chest lottery | Six groups pass; all 18 upgrades and nine evolutions land on their exact native icon pixels. Treasure, clipping, deterministic samples, snapshot isolation, malformed timing recovery, and real pixel centers before/on the marker pass. With both chest sprite cases, eight native checks pass. |
| Square-synchronized clicks and indicator removal | Twelve audio groups pass, including all 24 square centers, 20–240 Hz cadence, skip/mute/reset, and click headroom. Opening/landed before/after poses each differ only in the 233 removed progress-line/top-pointer pixels. |
| Python logic through the headless host | 39 pass per run; three repeats give 117 passes; the native capture check is skipped |
| Python logic through the native raylib host | All 40 pass, including boss loot and recovery after a malformed capture request |
| Original JavaScript weapon oracle | All 18 variants at ranks 1–8 pass getter/constant comparisons |
| Native movement pixels | Seven groups, 521 framebuffer comparisons pass: 320 silhouettes and 201 temporal/camera frames |
| Seeded combat hunts | Three groups, 28 seeds, 16,910 ticks pass |
| Endurance | Two complete 20-minute simulations reach victory and schedule/defeat all ten bosses; Hydra reaches 60 summons |
| Dense combat | 1,800 enemies remain finite, grid aligned, bounded, and take weapon damage |
| Native smoke runs | 300 gameplay frames and 600 nest frames pass |

The weapon checks observe actual hits, targeting, timing, splitting, devouring,
shields, and upgrades applied while attacks remain alive. Independent expected
values come from executing the original JavaScript, rather than the C++ formulas.
See [weapon contracts](WEAPON_PARITY.md) for the intentional native differences.

Pixel checks cover straight, L, S, tight, and spiral paths, long bodies, growth,
wall clamps, speed boosts, rotated faces, seven interpolation fractions, and real movement
history. The Python oracle stamps squares along the historical centerline; it
does not call the native geometry implementation.

Temporal checks measure actual head and camera displacement at 60/120/144/240 Hz,
including cell boundaries. They catch duplicate 60 Hz poses on faster displays,
which static silhouette checks alone cannot detect. Speed-change progress, pause,
and entity reordering have additional pure C++ regressions. See [movement](MOVEMENT.md).

Dash removal was checked on 27 October 2026: all 18 CTest targets, eight native
movement/collision checks, three seeded combat hunts, and a 300-frame native smoke
run pass. Five full-quality combat scenes have zero changed pixels; the menu
comparison changes only its movement hint. Legacy settings migrate their ten
surviving bindings and preserve the other preferences.
The same 1,200-frame dense workload measured 1.1988/1.5437/2.2156 ms
mean/p99/worst before and 0.8151/1.1533/2.0672 ms after, including presentation.
Afterward its first frame and warmup worst were both 1.0176 ms. Other shared
raster/geometry edits were present during these runs, so timing differences cannot
be attributed solely to dash removal or establish hitch-free play on every machine.
Reports and reviewed captures are under `artifacts/remove-dash`.

Chest lottery validation also ran all 18 registered CTest targets successfully.
The 1,200-frame native benchmark includes the same full-quality dense scene with
and without a pending reward. With the reel, frame mean/p99/worst were
1.2833/1.5915/1.9332 ms; without it, 1.2605/1.5908/2.1464 ms. First lottery use
was 1.2347 ms and the 60-frame warmup worst was 1.8644 ms. These are local wall
times including presentation, not GPU timer queries or a universal hitch guarantee.
Raw samples: `artifacts/performance/lottery-final.jsonl`.

The later case-click update retains all 18 passing CTest targets. Its 1,200-frame
full-quality dense-reel benchmark measured 1.3112 ms mean, 2.1932 ms p99, and
2.5058 ms worst, versus 1.2995/2.0463/2.7667 ms before. First use was 1.4142 ms;
warmup worst was 2.1443 ms. The hidden render benchmark excludes audio-device
playback; the click is prepared once and reuses one voice. These small timing
differences do not establish a universal performance gain or absence of hitches.
Reports are `artifacts/performance/case-click-{before,after}.jsonl`; the exact
native pixel comparison is in `artifacts/case-click-demo/manifest.json`.

The subsequent square-synchronized version replaces the fixed click schedule
with one shared integer scroll offset for rendering and audio. All 18 CTest
targets and eight native Python chest checks pass. The same 1,200-frame dense
reel workload measured 1.4221 ms mean, 2.2235 ms p99, and 2.5594 ms worst, with
1.3829 ms first use and 2.7215 ms warmup worst. The render benchmark excludes
audio-device playback; small timing differences between runs are not evidence
of a performance improvement. See `artifacts/performance/square-click-after.jsonl`
and `artifacts/square-click-native-results.json`.

The broad world-image baseline predates other world-effect edits in the shared
workspace and differs from the final captures. It does not establish full-world
visual parity for the lottery change. The new lottery tests instead compare each
awarded icon against its native asset and prove repeated preview captures and
gameplay snapshots stay identical.

Boss checks include adjacent and arena-edge drops, fast swept collection,
actual upgrade/evolution rewards, merged quantities, and an acknowledged native
reveal that does not grant twice. Eight sound groups cover soft waveform
endpoints, conservative mix energy, event aggregation, cadence/voice limits,
reward priority, variations, and mute/reset. See [loot](LOOT.md) and [audio](AUDIO.md).

Regressions also cover damage rounding, fractional cobra damage, independent
projectile caps, full gun volleys, centered Singularity victims, duplicate fixture
handles, bad requests, timeout cleanup, and restarting a failed test host.

Enemy hit-motion smoothing was checked on 27 October 2026. All 20 registered
CTest targets pass, including 15 temporal groups. Two new native groups inspect
151 actual lash/death captures across 60/120/144/240 Hz. The prior motion-history
behavior reproduces a 60-source-pixel jump; the correction traverses intermediate
pixels without changing damage or logical knockback. Contact, blocked shoves,
stacked hits, hash collisions, pool reordering, recycling, pause, raw captures,
and deaths during knockback have focused C++ coverage. The normal desktop
Release executable was rebuilt and its four focused combat suites also pass.

An isolated comparison reuses identical compiled simulation, geometry and
presentation objects, replacing only movement-history behavior. All 30 ordinary
swarm captures match exactly, as do the five original full-quality effect scenes.
Across the six 1,200-frame benchmark cases, enemy-state checksums match. Moving
1,800-enemy combat measures 0.7204/1.4994/2.0166 ms mean/p99/worst before and
0.7234/1.4027/1.7866 ms after. Static dense rendering measures
0.8502/1.1092/1.1968 ms before and 0.8991/1.2026/1.5210 ms after. The final
moving-combat first submitted frame and 60-frame warmup worst both measure
0.9699 ms. These local wall times include presentation; first submission follows
resource preparation and does not establish cold OS/driver-cache behavior or
hitch-free play on every machine. Reports, comparison sources and reviewed
captures are under `artifacts/enemy-hit-motion`; the isolated final timing files
are `final-render-{before,after}.jsonl`. Earlier broad swarm baselines include
concurrent camera edits and are superseded by `isolated-swarm-parity`.

## Performance

Release measurements on this machine, in milliseconds. Runs were measured
separately, without concurrent compilation. These are workload timings, not
cross-machine FPS promises.

| Workload | Mean | 99th percentile |
|---|---:|---:|
| Simulation: 1,800 enemies, evolved fang/Hydra/Miasma/Singularity build | 0.0970 | 0.2505 |
| Simulation: 1,800 enemies, Consumption/Ouroboros/Ancient/Shatter build | 0.0355 | 0.1318 |
| Native render: 300 enemies, 200 projectiles, 12 effects | 0.1536 | 0.6761 |
| Native render: 1,800 enemies, 1,600 projectiles, 64 effects, 60 summons | 0.6423 | 0.8390 |

Simulation uses 360 warmup ticks and 900 measured ticks, excluding Python,
serialization, protocol I/O, and rendering. Both cases pass the 4 ms p99 update
budget. Native rendering uses the actual 800×500 framebuffer, 60 warmup frames
and 1,200 measured frames with the frame limiter disabled. Its wall time includes
drawing, active motion-history capture/sampling, driver submission, and presentation;
it is not a GPU timer query.

For the same 1,200-frame full-quality workload, the previous renderer measured
2.6753 ms mean / 3.1523 ms p99 in the dense scene; the optimized renderer measured
0.6423 ms / 0.8390 ms. This is about 4.2× faster on average. Representative mean
rendering improved from 0.3777 to 0.1536 ms; its p99 varied from 0.5872 to
0.6761 ms, so the small-scene tail did not improve in this run. Both raw reports
are under `artifacts/performance/settings-{before,after}-render.jsonl`.

The change streams ordered integer quads through a bounded GPU batch instead of
repeating general shape setup for each pixel/span. It uses about 352 KiB of GPU
buffer storage and 256 KiB of CPU staging memory per presentation instance.
Shaders and buffers are prepared once; no GPU objects are created per frame.
Reference scenes and raster-equivalence tests preserve full-quality rendering.
Lower effect budgets remain optional settings rather than the source of this gain.

## GUI readability

Native GUI coverage now checks all eight equipment ranks against bright gameplay,
clear spacing between equipment rows, dark backing for long counters, and visible
upgrade headings/cards on every catalog page and the Second Wind fallback.
Captures cover all four characters, all 18 upgrades, settings tabs, evolution
requirements, pause, results, and chest rewards. Secondary text is brighter;
repeated instructions and duplicate passive-stat explanations were removed.
The Pixel Operator 8 font and integer pixel presentation are unchanged.

The Release build and six focused suites pass: GUI text, settings UI,
presentation, world raster, display settings, and settings logic. Five combat
scenes match exactly between the previous and revised GUI with identical world
rendering; the HUD comparison changes 13,961 pixels, all within its expected
regions. Concurrent weapon-art work was included equally on both sides of this
comparison.

For 1,200 full-quality native render frames, GUI-only before/after measurements
are below in milliseconds. These include presentation, exclude simulation, and
do not establish performance on other machines.

| Workload | Before mean / p99 / worst | After mean / p99 / worst |
|---|---:|---:|
| 300 enemies, 200 projectiles, 12 effects | 0.2337 / 0.6203 / 1.0212 | 0.2437 / 0.6464 / 0.8794 |
| 1,800 enemies, 1,600 projectiles, 64 effects, 60 summons | 1.2431 / 1.5372 / 1.9068 | 1.2469 / 1.5936 / 2.1678 |

The first submitted frame measured 0.7553 / 0.3473 ms before/after in the smaller
scene and 1.3107 / 1.2986 ms in the dense scene. These are first-render timings
after resource preparation, not startup/loading measurements. Raw comparisons,
captures, and timing reports are under `artifacts/ui-review`.

## Dense moving swarm optimization (27 October 2026)

The enemy renderer now caches projected world paths and quantized source-pixel
geometry independently of the camera. Full-quality pixel spans use 12-byte
rectangle instances instead of four 8-byte vertices, with 16 preallocated GPU
upload slots. Fully visible effect lines bypass clipping divisions. This uses
about 2.8 MiB of additional CPU cache storage, 96 KiB of raster staging, and
1.5 MiB of GPU instance buffers. Startup prepares the resources; normal frames
do not allocate these buffers or perform readback.

An isolated baseline restores the previous geometry, raster, and enemy-render
paths while retaining the same surrounding game/control code. Both Release
executables use the same full-quality fixtures, assets, and benchmark source.
Each case measures 6,000 frames after 60 warmup frames on Windows x64/MSVC 14.51.
The moving cases use 60 Hz updates and virtual 240 Hz presentation sampling;
the benchmark itself runs uncapped. All six enemy-state checksums and peak
actor populations match between builds. Times below are milliseconds and
include presentation; moving cases include their fixed updates.

| Workload | Before mean / p99 / worst | After mean / p99 / worst |
|---|---:|---:|
| 300 enemies, 200 projectiles, 12 effects | 0.2153 / 0.4081 / 0.6886 | 0.1486 / 0.3414 / 1.3667 |
| 1,800 enemies, 1,600 projectiles, 64 effects, 60 summons | 1.2753 / 2.0015 / 2.7215 | 0.8317 / 1.2985 / 1.7542 |
| Same dense scene with chest lottery | 1.2808 / 1.9305 / 2.5621 | 0.8418 / 1.3332 / 1.7984 |
| 1,800 moving, full-length enemies | 0.6866 / 1.2175 / 1.6822 | 0.2807 / 0.4937 / 0.8178 |
| 1,800 moving enemies and evolved weapons, reaching 60 summons | 1.7090 / 2.5530 / 146.7636 | 1.0313 / 1.7320 / 2.2990 |
| 10,000 snakes, rendering only | 3.1876 / 5.0269 / 6.3404 | 2.5788 / 4.0360 / 112.2412 |

Moving-swarm mean frame time improves about 2.45x and p99 falls about 59%.
First submitted moving-swarm frames measure 0.6688/0.5840 ms before/after;
full moving-combat first frames measure 0.9296/1.0469 ms. Preparation is measured
separately at 16.8692/13.0992 ms. These are first submissions after preparation,
not proof of cold OS/driver-cache behavior. The 10,000-snake fixture deliberately
exceeds the geometry cache and tests bounded collision handling; it does not
advance simulation or increase the 1,800-enemy gameplay cap.

Rare presentation stalls remain unresolved. The longer diagnostic run records
one 146.7636 ms old-renderer frame with 1.9043 ms drawing and 0 ms reported thread
CPU, and one 112.2412 ms new-renderer frame with 2.5235 ms drawing and 15.625 ms
reported thread CPU. Neither frame contains a simulation update. The large
remainder is in the final presentation section; Windows thread CPU accounting
is coarse and does not identify whether the cause is driver waiting or OS
scheduling. An earlier paired run also captured a 105.3757 ms new-renderer dense
frame and an 881.5665 ms old-renderer 10,000-snake frame. These outliers are
retained in the reports and prevent a hitch-free claim. `--diagnose` records up
to 64 slow frames, including warmup, without adding gameplay instrumentation.

All 18 registered CTest targets pass; all 11 AddressSanitizer targets pass.
Native raster coverage now includes 150,000 translucent rectangles across upload
ring rollover and repeated GPU owner lifetimes. Eleven temporal C++ groups
include 1,800 live enemies at 60/120/144/240 Hz; three native Python groups check
342 actual framebuffers. Five full-quality combat scenes and 30 additional
moving-swarm/camera/lifecycle captures have zero changed pixels. The web Release
build and an actual WebGL2 paused-combat capture pass; both shaders link and the
graphics error state is clear. Both 1,800-enemy headless combat benchmarks also
pass (p99 0.2677/0.2080 ms), excluding rendering and protocol/Python overhead.

Reports and captures are under `artifacts/performance/swarm`, including
`extended-{before,after}.jsonl`, the earlier `aligned-{before,after}.jsonl`,
`diagnostic.jsonl`, `smooth-motion.json`, `simulation-bench.json`, `parity`, and
`motion-parity`. `snake.ps1` now forwards positional benchmark counts correctly
and binds `-CoreOnly`/`-SkipTests` as build switches instead of Python arguments.
Focused CTest `-R` filters also reach CTest instead of binding `-RaylibSource`.

## Pixel death animation (27 October 2026)

Twenty registered CTest targets, 44 native Python visual checks, 12 headless
AddressSanitizer targets, and both complete 20-minute endurance runs pass.
The final death shader additionally passes six native pixel groups and four
focused integration checks. Coverage includes initial silhouette parity,
binary alpha, monotone 2x2 erosion, whole-pixel camera translation, all four
player palettes, actual weapon kills, pause/reset, a saturated 2,048-pose pool,
70,000-instance GPU ring rollover, and resource-owner recreation.

Four full-quality effect scenes match the recorded old renderer exactly. The
fifth differs at seven pixels belonging to old generic particles for consumed
victims, which are intentionally hidden by the new effect. Twelve old/new
simulation checkpoints across Nova, Hydra, and Singularity match completely
after excluding timing and the new cosmetic death count; each build kills
the same 256 targets. Repeated death captures also preserve future combat and
loot. Raw reports are under `artifacts/death-effects`.

The same 1,200-frame dense render scene measures mean/p99/worst
0.8307/1.1904/1.6914 ms before and 0.8708/1.3267/1.6583 ms after, including
presentation. Other shield/motion work was present in the shared workspace
during these runs; changes in timing cannot be attributed solely to deaths.

The final dedicated benchmark adds corpses to 1,800 enemies, 1,600 projectiles,
64 zones, and 60 summons at full quality. With 2,048 retained bodies it measures
1.4160 ms mean, 1.9813 ms p99, and 3.5592 ms worst over 1,200 frames after 60
warmup frames. That scene's first submission is 1.2301 ms and warmup worst is
1.6441 ms. The first death-shader submission in the one-corpse case is
1.1005 ms; preparation before rendering is 23.0840 ms. These are local wall
times including presentation, excluding simulation and fixture setup. They
do not establish cold OS/driver-cache behavior or hitch-free play on every
machine. See `death-bench-final.jsonl` for all four populations.

The Emscripten Release build also passes. An actual WebGL2 page links the
death shader successfully with an empty program log and graphics error 0.

## Tracking-camera jitter follow-up

The enemy/camera rounding reproduction recorded seven one-pixel reverse steps
in 120 native frames. Combining continuous motion with the shared camera origin
before rasterization removes all seven. New native coverage checks 564 frames
across four directions at 60/120/144/240 Hz, with no reverse steps or excess
pixel jumps. Pure geometry checks additionally cover equal-speed actors at
fractional-pixel separation and elbows crossing half-pixel boundaries.

All 20 registered CTest targets pass, the full 12-target AddressSanitizer run
passes, and focused geometry/motion sanitizer checks pass after the final tie
rounding adjustment. Native hit/death-motion tests pass. Five full-quality raw
reference scenes retain zero changed pixels. Simulation positions, combat and
RNG remain unchanged; live relative raster positions intentionally change.

For 1,200 full-quality native frames, moving-swarm mean/p99/worst measured
0.3661/0.8958/2.5318 ms before and 0.4093/0.7457/1.5030 ms afterward. Moving
combat measured 0.7435/1.4641/1.7114 ms before and 0.7744/1.5055/1.8979 ms
afterward. The dense static scene measured 0.8491/1.2938/1.6136 ms before and
0.9261/1.4455/1.6767 ms afterward. Mean rendering cost increases modestly in
these runs; concurrent cosmetic hit-motion updates are also included in the
final build, so the differences cannot be attributed solely to camera rounding.
These measurements include presentation and do not establish hitch-free play.
Reports, native traces and captures are in `artifacts/performance/enemy-jitter`.

## Splitter material handoff (27 October 2026)

The splitter's blue body now becomes its two actual Swift offspring over
0.30 s. Nine core death groups cover inherited poses, connected intermediate
spans, exact final pixels, pause, consumed splitters, reset, and the original
enemy-cap spawn limit. Six focused native integration checks pass, including
identical pixels on both sides of a fixed-update boundary at the final handoff.
The source pose is retained for one extra fixed tick so interpolation completes
the last color and position without an early switch to the normal renderer.

All 20 registered CTest targets and 12 headless AddressSanitizer targets pass;
both complete endurance simulations pass. Desktop and Emscripten Release builds
pass. The native comparison records 93 before/after gameplay snapshots across
straight, curved, and arena-edge paths. All states match, and all three final
images have zero changed pixels after the transition. The self-contained
comparison is generated by `scripts/build_splitter_preview.py`.

For the same 1,200-frame full-quality legacy dense fixture with 2,048 retained
corpses, before/after mean/p99/worst are 1.4982/2.3186/3.1355 ms and
1.3709/1.9304/2.1511 ms. These small differences are local workload observations,
not a demonstrated performance gain. The dedicated split fixture uses 900
parents, 1,800 actual offspring, 1,600 projectiles, 64 zones, and 60 summons.
Over 1,200 measured frames after 60 warmup frames it measures 1.3293 ms mean,
1.9841 ms p99, and 2.3254 ms worst, including presentation. First morph use in
that scene is 1.6252 ms; warmup worst is 2.0219 ms. The one-parent case's first
morph is 0.5197 ms. Fixture setup and simulation are excluded. No new GPU
resources or shaders are created for a birth; the added actor history is
bounded to 48 bytes per enemy. These checks do not establish hitch-free play
on every machine. Reports and captures are under `artifacts/splitter-death`.

## Splitter Y separation revision (27 October 2026)

The two inherited body banks now peel along opposite screen-Y arcs, reaching
26 source pixels of elevation each at the midpoint. The elevation returns to
zero at the exact handoff, and vertical parents keep their lateral opening.
Ten core death groups pass, including upward/downward separation for all four
headings and exact landing. All six focused native death checks pass, including
the adjacent fixed-update handoff comparison. The 93 straight, curved, and
arena-edge snapshots remain identical to the previous version; all three final
framebuffers still have zero changed pixels. Reviewed native captures and the
larger comparison viewport are under `artifacts/splitter-elevation`.

At the same full-quality 900-parent / 1,800-offspring workload over 1,200 measured
frames after 60 warmup frames, before/after mean/p99/worst are
1.3475/2.1928/3.0337 ms and 1.3591/2.5002/2.7631 ms, including presentation.
The revised scene's first morph measures 2.2105 ms and warmup worst 2.3776 ms.
These local samples exclude simulation and setup; they do not demonstrate a
performance improvement or prove hitch-free play. No simulation, actor storage,
shader resource, or allocation behavior changes for this revision.
The focused ten-group death suite also passes under AddressSanitizer, and the
Emscripten Release build passes.

## Startup window borders (31 October 2026)

The saved 3840×2019 window exceeded the primary display's decorated client
limit of 3814×1993. Auto presentation dropped to 3× while retaining 707 pixels
of padding per side and 246/247 above/below. Windows reaching the usable desktop
limits now fit their integer-scaled canvas: 2400×1500 in this case. Smaller
custom windows, manual pixel scales, and fullscreen modes retain their behavior.

The Release settings, native display, and settings UI targets pass (25 groups).
Native first-frame captures cover fresh default and saved windows on both
connected monitors, including their different decoration DPI. Edge colors and
a one-source-pixel stripe verify the presented area and integer enlargement.
The captures and native menu were opened for review. A normal GUI launch with
a settings copy also fits its complete decorated frame; the original settings
file's SHA-256 remained unchanged. Five full-quality combat scenes have zero
changed pixels.

In separate 600-frame full-quality dense benchmarks, native frame
mean/p99/worst were 0.9371/1.3598/1.7326 ms before and
0.9643/1.5174/1.8732 ms after. The latter first frame was 1.2952 ms and warmup
worst 1.4614 ms. A separate run concurrent with native display tests had a
26.0545 ms spike; its raw report is retained alongside the isolated measurements
under `artifacts/startup-borders`. These local timings vary with system load
and do not establish hitch-free behavior. The fix adds only bounded window
fitting work during initialization and settings changes.

## Expanded browser viewport (31 October 2026)

The browser now reveals more arena across its available canvas at whole-number
pixel zoom. The desktop and source captures retain their 800×500 baseline.
Rendering into an expanded viewport preserves every pixel in the original
central view. Six native groups additionally cover newly visible actors on all
four sides, 2× world/HUD/menu/settings/chest pixels, scissor bounds, and centered
mouse/upgrade routing. All 23 configured CTest targets pass. Five full-quality
original-resolution combat scenes retain zero changed pixels.

Headless Edge WebGL2 passes nine browser checks: six sizes from 800×500 to
2560×1080 (including 1921×1081 and DPR 2), a settings click, and fullscreen
entry/exit. Resizing synchronizes GLFW and canvas dimensions so DPI changes
cannot restore stale startup dimensions. Clicking also exposed miniaudio's
dependency on `Module.HEAPF32`, now explicitly exported for Emscripten 4.
No JavaScript exceptions occur in the completed run. Actual native and browser
captures were opened and reviewed; generated reports are beneath
`artifacts/web-responsive`.
The common central region of the actual 1280×800 (1×) and 1920×1080 (2×)
browser captures also matches in all 1,328,000 compared output pixels, excluding
HUD regions that intentionally move to the viewport edges.

For the same 600-frame native full-quality dense workload, frame mean/p99/worst
were 0.8863/1.1501/1.6582 ms before and 0.8587/1.1140/1.3877 ms afterward. The
1280×800 expanded fixture measured 0.8681/1.1085/1.3335 ms, with a 1.2258 ms
first frame/warmup worst. Its 10,000-enemy overload worst was 3.5379 ms. These
measurements include native presentation, vary with local system load, and are
not browser FPS or proof of universal hitch-free behavior. Reproduce the
expanded workload with `./snake.ps1 bench 600 --wide`.

## Reproduce

```powershell
./snake.ps1 build
./snake.ps1 unit
./snake.ps1 gametest --tier logic --repeat 3
./snake.ps1 gametest --tier logic --native
./snake.ps1 gametest --tier visual --native
./snake.ps1 gametest --tier hunt
./snake.ps1 gametest --tier bench
./snake.ps1 bench 1200
./build/snake_core_stress.exe
```

For memory checks, run `scripts/check-asan.ps1 -Python PATH_TO_PYTHON` from
PowerShell. See the [GameTest guide](GAME_TEST_API.md) for selectors, fixture
controls, native captures, and structured JSON reports. Local reports are saved
under `artifacts`; CTest reports are in each build directory.

`build/bin/snake_presentation_tests.exe` runs the native loot UI checks. To
register it in CTest, configure with `-DSNAKE_RUN_NATIVE_TESTS=ON`; this requires
a working desktop OpenGL context. Headless CI leaves that option off.
The same option registers raster, display, and settings UI tests. Automated hosts
ignore ordinary saved preferences and never switch monitor modes; an explicit
`--settings FILE` may supply a read-only capture fixture. Startup loading was
checked with saved window, FPS, VSync, audio, and graphics values.

Passing these contracts and stress cases does not establish balance or prove
that every possible input sequence is bug free. A complete human balance pass
remains separate from this automated validation.
