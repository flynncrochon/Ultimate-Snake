# Grey snake barriers

The behavior comes from the unmodified `src/survivors/grey_snake.js`, its
integration in `src/core/battle_royale_app.js`, and the grey snake/minimap drawing
in `src/survivors/survivors_renderer.js` at JavaScript reference commit
`b6cb72e66eebc1291d57efe63ebbe93ec602e863`.

## Behavior

- First spawn at 90 seconds of gameplay, then every 90 seconds, with at most
  one living head. An obstructed spawn consumes that interval.
- Spawn 25–35 cells ahead of the player's current heading, clamped to cells
  1–198. Try the source's bounded offsets through radius ten if old walls block
  the spawn. The first cell blocks immediately.
- Move one cardinal cell every 60 ms for ten seconds. Every visited cell stays
  occupied permanently. The moving head cannot be targeted or damaged by weapons.
- Intercept a point 15–24 cells ahead of the player's heading. On arrival within
  one cell, sweep a twelve-cell wall perpendicular to that heading, with a random
  side, then plan another intercept.
- Flee within eight cells of the player, aborting a nearby wall sweep. Reverse
  a sweep at the boundary and try perpendicular steps when obstructed.
- Keep one cell of clearance, including diagonals, from other snakes' permanent
  trails and the player's head/tail when moving. A snake can cross its own trail.
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

A twenty-minute run can spawn at most thirteen trails. Each has 168 reserved
path entries (spawn plus at most 166 successful moves). Storage is reserved at
reset. A 200×200 byte ownership grid provides constant-time collision and nine
cell neighbor checks; no growing sets, strings, allocations, file access, or
resource creation occur during grey updates/rendering. Rendering culls complete
off-screen trails and uses the existing raster batch.

## Regression coverage

`game_tests/grey_reference_probe.mjs` executes the original JavaScript with a
controlled clock and random stream. Its independently generated
`fixtures/javascript_grey_snake_oracle.json` covers eleven approach, sweep,
replan, flee, boundary, tail/other-wall avoidance, own-crossing, and blocked cases.
`test_grey_snake.py` drives those cases through the actual native host, plus
scheduling, lifetime, pause, collision grace, immunity/shields, loot, reset,
validation, and capacity limits.

`snake_grey_snake_tests` runs a full twenty-minute lifecycle, checks permanent
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
