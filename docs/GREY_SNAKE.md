# Grey snake barriers

The original behavior comes from the unmodified `src/survivors/grey_snake.js`, its
integration in `src/core/battle_royale_app.js`, and the grey snake/minimap drawing
in `src/survivors/survivors_renderer.js` at JavaScript reference commit
`b6cb72e66eebc1291d57efe63ebbe93ec602e863`.
Adaptive scheduling and route interception intentionally extend that behavior
to punish long straight runs, back-and-forth lanes, and repeated circuits.

## Behavior

- Varied movement keeps the original 90-second interval. Predictable movement
  can bring the first spawn forward to ten seconds, and subsequent spawns to
  thirty seconds after the previous attempt. At most one head is alive. An
  obstructed spawn consumes its interval rather than retrying every frame.
- Only successful player grid moves enter a fixed 512-step history. Straight
  pressure rises from twenty to forty-eight consecutive cells. Reused-lane
  pressure rises from eight to thirty-two repeated cells and decays twice as
  fast on fresh cells. The last sixteen steps are excluded from lane matching;
  parallel lanes within two cells still count. Turning at the corners of the
  same rectangle does not clear this pressure. Movement that has been held for
  over one simulation second contributes no active pressure. No keypress,
  render, cosmetic, or wall-clock state is used.
- Ordinary heads spawn 25–35 cells ahead of the player's current heading, clamped to cells
  1–198. Try the source's bounded offsets through radius ten if old walls block
  the spawn. The first cell blocks immediately.
- When pressure is at least one half, spawn sixteen cells to the side of a
  predicted crossing instead. Prefer a matching recorded route's future,
  including its turns, or extrapolate the current heading if none matches.
  Look about 1.8 seconds of player travel ahead, clamped to 12-32 cells. Approach
  a point six cells before the crossing and sweep twelve cells perpendicular
  to its expected travel direction. Replanning repeats this prediction.
- All spawns keep twelve cells of Manhattan distance from the player head and
  one cell of clearance from every player segment. Targeted spawns also keep
  at least twelve cells of lateral approach before the crossing. Try the other
  side and bounded offsets if obstructed; skip the attempt if no safe site fits.
- Move one cardinal cell every 60 ms for ten seconds. Every visited cell stays
  occupied permanently. The moving head cannot be targeted or damaged by weapons.
- Ordinary heads intercept a point 15–24 cells ahead of the player's heading. On arrival within
  one cell, sweep a twelve-cell wall perpendicular to that heading, with a random
  side, then plan another intercept.
- Ordinary heads flee within eight cells of the player; targeted heads use
  three cells so their cross-lane sweep can finish. Fleeing aborts a nearby wall
  sweep. Reverse a sweep at the boundary and try perpendicular steps when obstructed.
- Keep one cell of clearance, including diagonals, from other snakes' permanent
  trails and the player's head/tail when moving. A snake can cross its own trail.
- Every runtime spawn and new wall cell must preserve escape space. The open
  cardinal neighbors of that cell must connect around it in the surrounding
  eight-cell ring, and each affected free neighbor must retain at least two
  exits. Starting with the open arena, these rules preserve one connected free
  region and prevent grey-made dead ends, closed loops, or pockets sealed
  against the arena edge. This deliberately conservative rule may leave a wider
  opening than strictly necessary. A head takes a safe perpendicular alternative
  or waits if no safe step fits; it may still retrace its existing wall.
  Player body collisions and ordinary movement decisions retain their rules.
- Attempting to move into either a living or retired grey barrier starts the
  existing one-second collision grace period. The full player snake holds while
  combat continues. A safe turn cancels grace; expiry kills the player. Shields
  and enemy-contact immunity cannot absorb this collision. A head already on a
  barrier dies immediately. Boss chests search for a landing cell outside barriers.

Live trails are `#666`, retired trails `#555`, with a 14-source-pixel connected
body. Living heads have `#eee` eyes and `#111` pupils. Only the head interpolates;
historical wall cells stay fixed. The existing quantized camera and integer
presentation apply to both. The minimap shows all permanent cells and a brighter
living head. Visual settings never change collision, AI, or timing.

## Native timing and storage

All clocks advance only inside the 60 Hz simulation. The original wall-clock
timestamps become gameplay seconds, so pauses, level choices, reward reveals,
focus caps, and display refresh cannot advance the grey snake independently.
The 60 ms cadence alternates between fixed updates and retains fractional time.
When a move is blocked, the prior head edge finishes instead of replaying it.
Retirement finishes the last interpolated edge before showing its static wall.

A run reserves `RunDuration / 30` trails for the maximum pressure schedule
(sixty for the current thirty-minute stage). Each has 168 reserved
path entries (spawn plus at most 166 successful moves). Storage is reserved at
reset. A 200×200 byte ownership grid provides constant-time collision and nine
cell neighbor checks; no growing sets, strings, allocations, file access, or
resource creation occur during grey updates/rendering. Rendering culls complete
off-screen trails and uses the existing raster batch.
History matching scans at most 512 entries per successful player move or
interception plan, never the arena or the enemy population. New route and
capacity storage is prepared at reset, including the earliest pressure spawn.
Escape checks examine at most four neighboring cells and their exits, plus
eight surrounding cells and seven fixed-size bit-mask expansions. They use
no arena flood fill, pathfinding, allocations, or cached connectivity. Tests
independently flood-fill the whole arena to check the global invariant.
Explicitly authored test paths bypass runtime placement safety so fixtures can
still reproduce dangerous and pre-existing enclosure cases.

## Regression coverage

`game_tests/grey_reference_probe.mjs` executes the original JavaScript with a
controlled clock and random stream. Its independently generated
`fixtures/javascript_grey_snake_oracle.json` covers eleven approach, sweep,
replan, flee, boundary, tail/other-wall avoidance, own-crossing, and blocked cases.
`test_grey_snake.py` drives those cases through the actual native host, plus
scheduling, lifetime, pause, collision grace, immunity/shields, loot, reset,
validation, and capacity limits.
Additional native tests exercise early straight-run pressure, ordinary timing
on fresh routes, repeated thin and rectangular circuits, prediction through
corners and nearby lane shifts, pressure decay, bounded history, and an actual
cross-lane block followed by a successful escape turn. Python integration tests
drive the real player around a rectangle and verify interception after its
next turn. The JavaScript oracle continues to cover ordinary-head decisions;
it does not define the intentional adaptive extensions.
Enclosure tests drive the player through preserved loop and arena-edge
doorways. A 5,000-proposal seeded wall fixture checks local placement against
an independent whole-map connectivity and minimum-exit oracle, which also runs
through both full-stage spawn schedules. Native pixel assertions verify that
the preserved doorway remains visible between the surrounding walls.

`snake_grey_snake_tests` runs full-stage lifecycles, checks permanent
occupancy and unchanged storage, compares render cadences, and samples a head
turn and retirement without diagonal shortcuts or head snaps. The two existing scripted combat endurance
runs disable grey spawning because their routes do not steer around permanent
obstacles and barrier collisions deliberately bypass their contact immunity.

`test_grey_snake_visual.py` compares actual native pixels against an independent
square-stamping oracle: a 160-cell wall, four head directions, five movement
phases, retired paths, 25 samples crossing a cardinal turn, and eleven retirement
frames at unchanged head speed. It also checks
minimap brightness, collision warning pixels, and capture/state independence.
Native CTest includes both grey Python modules when `SNAKE_RUN_NATIVE_TESTS=ON`.
`snake_render_benchmark 600 --grey --diagnose` adds thirteen visible trails to the
same full-quality static/dense/moving scenarios; its first-use, mean, p99, and
worst results remain workload measurements, not a hitch-free guarantee.
`--grey-capacity` instead populates the full adaptive capacity, including off-screen
trails, to check the larger reserved population and culling.

## Measured validation (2026-11-01)

The Release build passed all 29 configured CTest suites, including the native
grey pixel oracles, and all 17 headless AddressSanitizer suites. Both grey
full-stage lifecycle schedules and the existing core stress scenarios passed.
`scripts/showcase_grey_prediction.py` captures actual straight-run interception,
escape, and a learned rectangle's next-corner interception. The native PNGs were
opened and reviewed; captures and snapshots are under `artifacts/grey-prediction`.

The 1,200-frame full-quality capacity benchmark at 800x500, with sixty grey
trails and 1,800 moving combat enemies, measured 1.4039 ms mean, 2.6629 ms p99,
and 5.7186 ms worst including presentation. Its first frame was 1.5843 ms and
warmup worst was 1.8707 ms. No frame in that sample exceeded 16.67 ms. These
are development-host measurements with the benchmark's fixed 60 Hz simulation
and virtual 240 Hz rendering, including off-screen trail culling.

The benchmark holds the player stationary, so movement-history recording was
measured separately in the actual native straight-run fixture: its first 602
updates, including the first targeted spawn, measured 2.074 microseconds mean,
2.1 microseconds p99, and 28.8 microseconds worst. Those are isolated simulation
timings without dense combat or rendering, not full-game frame rates. Neither
sample establishes hitch-free play on other hardware. Raw before/after
thirteen-trail timings and the separate capacity timings are retained beside
the captures.

After adding escape preservation, all 29 configured CTest suites and all 17
AddressSanitizer suites passed again, including the new enclosure/edge escape
tests, doorway pixels, and independent global-connectivity checks throughout
both full-stage grey schedules. Native captures of a seven-segment player
escaping both preserved openings were opened and reviewed. These captures,
snapshots, and fresh before/after sixty-trail timings are retained under
`artifacts/grey-escape`.

The post-change 1,200-frame sixty-trail moving-combat sample at 800x500 measured
1.1692 ms mean, 1.7990 ms p99, and 2.8984 ms worst including presentation; first
frame and warmup worst were both 1.4865 ms. No scenario in that sample exceeded
16.67 ms. The same fixed-workload and hardware limitations above apply; these
measurements do not establish hitch-free play on every machine.
