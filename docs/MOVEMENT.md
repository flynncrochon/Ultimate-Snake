# Smooth movement

The simulation still advances at 60 Hz and stores cardinal grid waypoints.
Presentation samples the interval between the previous and current simulation
updates at the display's cadence. This adds one fixed update of presentation
latency (about 16.7 ms), rather than predicting an unknown turn.

The renderer interpolates normalized movement progress across cell boundaries.
It selects the correct historical path on either side of a turn, including the
matching face direction. Clamping a predicted phase at the next cell boundary
would briefly stop movement every cell; the retained history avoids that hold.
Speed changes preserve progress through the current cell.

Players, enemies, and summoned snakes share this sampling model. Enemy and
summon history uses stable IDs and fixed hash storage, allocated once. Combat
shoves and pulls retain a cosmetic translation that settles at a minimum of 192
world pixels per second, so a three-cell lash knockback takes about 0.125 seconds
to display. Larger stacked shoves also finish within 0.125 seconds.
This includes unfinished movement progress and earlier shoves. The whole body,
face, attached beams and death pose share the translation, rounded once to source
pixels. Logical positions, damage, hit timing and RNG remain unchanged. Spawn,
recycling and arbitrary fixture relocations safely use their current pose. Projectile,
pickup, and particle positions interpolate their actual simulation endpoints;
egg arcs and standing/orbiting effects sample the same presentation time.
Two preallocated 8,192-slot enemy history tables preserve corrections across
pool reordering while accommodating live enemies and retained death poses.

The camera is derived from the sampled head and quantized once. Actors, paths,
and the camera share one integer source-pixel grid. The 800×500 framebuffer and
integer nearest-neighbor enlargement remain unchanged. Pixel art necessarily
holds a pixel across some high-refresh frames; it no longer jumps in 60 Hz
bursts on a faster display.

The camera's fractional remainder is retained until object and camera motion
have been combined, then their relative positions are rounded to source pixels.
Rounding those motions independently caused occasional one-pixel reverse steps
on enemies moving in the same direction as the player. Snakes, body elbows,
effects and attached beam endpoints now share this pixel origin. Numerical noise
at exact half-pixel ties gets one consistent answer, including equal-speed motion.
All final coordinates and strokes remain integer pixels.

Vsync follows the display refresh without a competing 144 FPS limiter. Pause,
the codex, and reward reveals preserve the current fractional render pose, so
resuming does not reset the image backwards. A new run clears history.

Enemy geometry uses a bounded 4,096-slot cache prepared before play. Each slot
retains projected world-space waypoints and its last source-pixel advances on
each axis. Frames reuse the path until its waypoints change and reuse rectangles
until those visible pixel advances change. Integer camera translation is applied
afterward; its fractional remainder participates in the shared quantization.
Lookup is direct and bounded;
collisions, changed widths, reused IDs, and invalid paths rebuild safely.
Interpolation still samples every display frame, independent of simulation.

## Regression checks

Pure C++ temporal groups cover 60/120/144/240 Hz, uneven frame cadence,
turn boundaries, speed changes, teleports, reordered entities, summon lifecycle,
pause, and raw fixture capture. They include 1,800 live enemies and enemy slow
transitions, contact and wall-clamped shoves, repeated hits through pool reordering,
and deaths during knockback. Native hit-motion tests additionally track actual
lash and death pixels. Four Python groups inspect 906 actual native frames
against an independent constant-velocity oracle, including a moving target in
a 1,800-snake swarm and simultaneous camera/enemy movement in all four cardinal
directions at 60/120/144/240 Hz. At 240 Hz
the head and camera advance by at most one source pixel per frame at base speed.
The existing 320 native silhouette comparisons continue to check clean turns.

`scripts/check_swarm_parity.py --record before|after` compares 30 additional
native swarm captures across fractional poses, moving-camera turns, speed
changes, pool compaction, and new IDs. Record `before` with the old executable
using `--exe PATH` before rebuilding. Generated captures remain in `artifacts`.

```powershell
./snake.ps1 gametest --single test_smooth_movement_visual --native
./snake.ps1 gametest --tier visual --native
```

Native `capture` requests can supply `render_alpha` in `[0,1]` after advancing
fixed ticks to test fractional presentation. Omitting it preserves exact raw
fixture capture. This setting changes rendering, never simulation state.
