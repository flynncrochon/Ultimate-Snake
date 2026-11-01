# Shedskin permanent upgrades

Shedskin is the shared permanent upgrade system alongside the full 30-minute
run balance. Its bonuses are purchased between runs and inherited by every
character; the one-minute wave schedule spans all thirty minutes.

Open **UPGRADES [U]** from character selection. Buy a rank with Enter or click
its row; use Up/Down to select and Escape/Back to return. If U is already bound
to a control, the button remains available without taking that binding.

Upgrades apply to every character at the start of a new run. Earned run gold
is banked once after death, victory, returning to the menu from pause, or
gracefully closing an unfinished desktop run. This uses the game's existing
`gems`/gold counter: raw collected fruit and existing treasure/fallback gold.
The XP multiplier does not multiply banked gold. There is no new world pickup,
currency sprite, snake skin, weapon artwork, or gameplay HUD layout.

| Upgrade | Per rank | Ranks | Initial price |
|---|---|---:|---:|
| Might | +5% direct damage | 5 | 200 |
| Vitality | x1.10 maximum/starting HP | 3 | 200 |
| Armor | -1 contact damage | 3 | 600 |
| Recovery | +0.1 HP per simulation second | 5 | 200 |
| Cooldown | -2.5% weapon cooldown | 2 | 900 |
| Growth | +3% XP | 5 | 900 |
| Reroll | +2 rerolls each run | 5 | 1,000 |
| Banish | +2 banishes each run | 5 | 100 |

These budgets and the price rule follow the documented
[Vampire Survivors PowerUps](https://vampire.survivors.wiki/w/PowerUps).
For an upgrade with `r` owned ranks and `n` total purchased ranks:

```
price = initial_price * (r + 1) + fee(n)
fee(0) = 0
fee(n) = floor(20 * 1.1^n), n > 0
```

For example, Might first costs 200; with that one rank bought, its second rank
costs 422 and a first Armor rank costs 622. Additive global fees make total
cost independent of purchase order. The full set costs 47,910 gold.
**REFUND ALL** returns the actual recorded investment and clears purchased
ranks. No fee or currency is lost on refunds. Purchases and refunds are only
available between runs; an active run keeps its inherited snapshot.

## Native adaptations and visual contract

- Might adds to existing damage bonuses before the character multiplier.
  Existing weapon-specific exclusions remain: base mortar poison and nest
  bites do not gain direct-damage bonuses. Their evolved forms retain their
  authored contracts. Zero purchased ranks reproduce the prior weapon stats.
- Cooldown's permanent multiplier composes with the existing Rapid Fire and
  character cadence, preserving all existing limits and weapon lifecycles.
- Vitality reaches 133.1 HP at rank three. The same five heart sprites show
  proportional health; it does not add hearts or alter the snake's appearance.
- Armor keeps the native minimum hit of 3. Shields still absorb whole hits.
  Extra HP, Armor and Recovery do not protect against lethal snake, wall,
  grey-barrier or flood collisions.
- Recovery advances only in active 60 Hz simulation. Pause, choices, chest
  reveals and background pause cannot heal, and death cannot be revived.
- Growth is additive with existing XP bonuses and the temporary level-20/40
  boost. Fractional XP is retained, so 100 one-point fruit at +3% give 103 XP.
  Bonus XP does not add length or gold.
- Reroll and Banish add to the native three uses of each, reaching thirteen.
  The four weapon/four passive slots and offer/rank eligibility stay intact.
- There are no permanent area, duration, amount, projectile-speed, movement,
  pickup-radius, character-color or cosmetic upgrades. The same raster font,
  native artwork, source framebuffer and integer presentation remain in use.

## Saving and migration

Normal play uses `snake-survivors.save` beside the executable. `--profile FILE`
selects a different profile. The original `SS1` best-kill/run/time/victory/sound
fields migrate into `SS2`, followed by gold, recorded investment, and eight
ranks. Legacy saves start with zero permanent currency because historical
gold was not saved. Existing `snake-settings.json` preferences remain separate.

Startup reads the profile before play. Native changes are copied to an
eight-slot SPSC mailbox; a background worker coalesces complete snapshots,
writes a temporary file, and atomically replaces the previous save only after
the new write succeeds. The update/render thread does not access the file,
wait on the worker, or take its mutex. A full mailbox retains the latest dirty
profile for a later update; shutdown flushes the final snapshot and joins the
worker outside gameplay. Currency has a bounded 64-bit limit, with room reserved
to refund all invested gold.

A write failure preserves the previous file and shows a retry action. In-memory
purchases remain available while storage is repaired. Malformed or unknown
save versions are left untouched; that session's progress is temporary and
the upgrade screen reports the problem. Atomic replacement protects against a
partial write, not every possible device or power failure.

The web build schedules the small MEMFS write outside its frame callback,
then uses the existing asynchronous IndexedDB flush. Shutdown cancels any
pending timer before destroying its owner. Abrupt process/tab termination can
still interrupt an unfinished save; desktop graceful shutdown is covered by
the persistence tests.

Automated scenes, captures, smoke runs and default test/benchmark presentations
use temporary profiles. Passing `--profile` to a scene/capture loads that file
read-only. These tools cannot farm rewards into, refund, or overwrite a normal
player profile. The deterministic GameTest `reset` accepts an explicit
`permanent` rank object; ordinary fixture resets always clear inherited bonuses.

```powershell
./build/bin/snake_survivors.exe --scene permanent --capture artifacts/permanent/menu.png
./build/bin/snake_survivors.exe --profile artifacts/permanent/test.save
./snake.ps1 gametest --single test_permanent
```

## Verification

`snake_permanent_tests` covers prices, caps, purchase-order invariance, refunds,
overflow reserves, SS1 migration, SS2 round trips, malformed/future files,
failed atomic replacement, retry, mailbox coalescing, graceful shutdown,
read-only previews, stat budgets, source exclusions, fractional XP and pause.
`snake_permanent_ui_tests` exercises actual keyboard/mouse purchases, banking,
reopening, refunds and save failure/retry in a hidden native window.

The UI tests compare every pixel of an unarmed gameplay scene with zero and
maximum permanent bonuses. All 400,000 source pixels match, including the
five-heart HUD. Shop, result and error captures are saved beneath
`artifacts/gametest/permanent` and opened for visual review. Geometry/stat tests
also check that every weapon's area, range, projectile speed, duration and
amount remain unchanged.

For matched full-quality native workloads with zero versus maximum bonuses:

```powershell
./build/bin/snake_render_benchmark.exe 600 --diagnose
./build/bin/snake_render_benchmark.exe 600 --diagnose --permanent
```

The static dense fixture holds actor counts equal; moving combat can change
attack occupancy because the purchased cadence/damage actually applies. Native
timings include presentation; headless timing excludes rendering. These finite
checks do not establish human survival balance or universal hitch-free play.

Validated locally on 1 November 2026 with Release MSVC/raylib 5.5:

- All 18 headless and 31 desktop CTest targets pass, including the permanent
  model, native UI and Python API cases. All 18 headless AddressSanitizer targets
  also pass. The pinned Emscripten 4.0.23 web build compiles successfully.
- Five full-quality combat captures from the pre-upgrade executable match the
  new executable with zero purchased ranks: zero changed pixels in every scene.
  The shop, maximum-bonus gameplay, results and save-failure captures were
  opened for native visual review.
- Both complete 108,000-tick combat runs pass with all permanent ranks enabled.
  Simulation update mean/p99/worst measured 7.38/12.10/172.90 microseconds for
  the scripted build and 13.22/55.00/226.40 microseconds for Snake Nest/Hydra.
  These are simulation-only timings with scripted collection and immunity.

For the same 600 measured frames after 60 warmup frames, the native dense scene
contains 1,800 enemies, 1,600 projectiles, 64 zones and 60 summons:

| Profile | Frame mean / p99 / worst (ms) | First submitted frame (ms) |
|---|---:|---:|
| Zero permanent ranks | 0.8826 / 1.1331 / 1.3554 | 1.2675 |
| Maximum permanent ranks | 0.8970 / 1.2182 / 1.4381 | 1.2440 |

This is a profile comparison using the same new executable, rather than a
code-revision timing comparison. Across its six workloads, including moving
combat, the chest lottery and a 10,000-enemy render-only overload, no sampled
frame exceeded 16.67 ms. The maximum measured frame was 3.8843 ms with zero
ranks and 3.8822 ms with maximum ranks. Resource preparation took
38.7989/39.2654 ms before play; it is outside these frame measurements. First
submission follows resource preparation, rather than a cold OS/driver-cache
test. These finite local observations do not establish hitch-free behavior
on other machines.

Raw benchmark reports, effect comparisons, web/ASan logs and endurance results
are beneath `artifacts/permanent`. Reproduce maximum-bonus endurance with
`build-tests/snake_core_stress.exe --permanent`; the default stress mode retains
its separate dense-combat and flood-lifecycle checks.
