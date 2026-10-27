# Boss loot contract

Every defeated boss preserves one chest reward. The dropped box occupies the
nearest free in-bounds grid cell to the death location, avoiding the player's
head/body and other chest cells. At the pickup storage limit, rewards merge into
an existing chest quantity rather than disappearing. Chests do not expire or
drift with XP attraction.

The head collects a box by crossing within six simulation units of its center.
Collection sweeps the full head path of an update, so fast movement or a long frame
cannot skip a box between its starting and ending cells. A box in the adjacent
grid cell remains visible until the head actually reaches it.

Opening a chest first evolves an eligible maxed weapon whose paired passive is
owned. Otherwise it grants a rank to an eligible owned upgrade. A complete build
receives 25 gold and up to 25 restored health. Every unit in a merged chest is
resolved once in order; an evolution from the first opening is reflected when
resolving the next opening.

The simulation grants rewards immediately and appends `ChestReward` records.
Each record contains a monotone serial, reward type, definition, resulting rank,
actual gold/healing amounts, title, and description. This lets the presentation
show and acknowledge rewards without relying on a short-lived event or granting
them a second time. The last 32 records are retained; normal runs schedule ten
bosses. Reset clears the ledger. Pause stops chest collection and combat.

The desktop game pauses combat for a lottery reel showing each pending record.
After a 0.22-second opening guard, the reel scrolls for six seconds with cubic
deceleration, stopping with the actual recorded reward beneath the center marker.
Enter, Space, Escape, or a click skips the spin. A separate press continues after
a 0.18-second landing guard. Each square passing the center marker makes a dry
mechanical click; the quiet chest/evolution chord plays once on landing. A simultaneous level-up follows
the chest reveal. Settings and the evolution codex freeze the animation.

The reel uses nine bounded visible cells, native raster icons, integer positions,
and a fixed scissor. Decoys come from a hash of the reward serial and cell index;
animation, skip timing, and render FPS never consume simulation RNG or reroll the
award. Every merged reward starts a fresh animation and acknowledgement never
grants it again. Native API captures can sample `reward_time=0..30` seconds
without advancing the live clock or simulation.

## Clicks follow the squares

The renderer and click tracker share one integer `reelScrollPixels` offset.
Each next square clicks once when its rendered center reaches the lower marker.
The starting square is already centered and does not click. A complete 24-cell
roll produces 24 crossings, and spacing between clicks naturally grows as the
reel slows. There is no independent click timer. Quantization is shared so the
last square cannot visually stop before its click occurs.

The [current Valve sound-event definition](https://github.com/SteamTracking/GameTracking-CS2/blob/master/game/csgo/pak01_dir/soundevents/game_sounds_ui.vsndevts#L6331-L6374)
uses a constant pitch, 0.2 gain, and one instance. Those parameters are verified
for current CS2, not the legacy recording. This game uses an original 25 ms
procedural click at 0.2 gain; it does not include Valve's audio sample or claim
an identical waveform.

The click is synthesized and uploaded once before play. Its own single UI voice
bypasses the combat sound cooldown and follows master/interface volume and mute.
Settings/codex pause the clock; skipped or overdue crossings are consumed without
burst playback. At normal render rates each square has its own click; a stalled
frame can coalesce several crossings into one. No tick changes
simulation events, reward odds, RNG, or the fixed 60 Hz combat clock. The former
upper progress line and pointer are removed; the lower center marker remains.

The box artwork uses the original JavaScript wood/gold/red palette. The
16×16 pickup and independently authored 32×32 reveal are native bitmap sprites,
blitted one-to-one on the shared pixel grid. See [`data/sprites`](../data/sprites/README.md).

The C++ loot suite covers actual boss deaths, arena-edge drops, body avoidance,
distinct simultaneous boxes, collection, exactly-once grants, evolution,
multi-cell movement, merged quantities, pause/reset, and bounded reward history.
`game_tests/test_boss_loot.py` exercises the same live paths through the Python
API and checks malformed pickup fixtures leave the native world intact.
