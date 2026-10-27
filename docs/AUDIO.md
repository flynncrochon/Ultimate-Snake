# Sound design

The sound set uses quiet, rounded sine plucks, low impact tones, filtered air,
and short consonant reward chords. It contains no square-wave oscillators.
Each gameplay cue has a soft attack and release, fixed conservative gain, and three
deterministic timbre/pitch variants. The variation is subtle enough to preserve
the identity of each sound.

Ordinary attacks use a faint woody tick. Hits and kills use short low thuds,
fruit uses a small glassy pluck, and damage uses a warm downward tone.
Level, chest, evolution, and victory each have distinct
ascending chords. Evolution replaces simultaneous chest/level sounds with one
reward chord; victory takes precedence over other reward sounds.

`audio_design.cpp` generates 44.1 kHz mono PCM at startup. It and the event
director have no raylib or audio-device dependency. Playback gains stay at or
below 0.60, ordinary combat is quieter still, and the mixed sound set retains ample
headroom. Sounds are synthesized once, never inside the per-frame update.

The main loop aggregates events after every simulation tick and UI update,
then advances audio once per rendered frame. That preserves feedback when a
render frame contains multiple fixed ticks, while preventing old events from
replaying between ticks at high refresh rates. Thousands of same-frame hits
collapse into one pending category. A shared 105 ms combat gate, category
cooldowns, three-voice limit, and reward ducking keep busy builds restrained.
Important feedback can replace a less important combat voice. Dropped feedback
never queues up to play late after combat ends.

The chest lottery has one additional, preloaded UI voice for an original 25 ms
mechanical click at constant pitch and 0.2 gain. Each square's native pixel center
crossing triggers it, using the exact integer offset that renders the reel.
The full 24-cell roll produces 24 clicks and bypasses the combat cooldown.
The six-second roll, skip, mute, settings/codex pause, and slow-frame coalescing
are described in [loot](LOOT.md).
Its waveform is synthesized here rather than using Valve's recording.

Muting stops active voices immediately and discards pending feedback. Starting
a run resets the director and active voices. Hidden test windows do not open an
audio device. Enabled state and master/category volumes are saved in
`snake-settings.json`; an existing profile's enabled state migrates when that
file is absent. Muting remains immediate when persistence fails. Volume changes
stop old voices so their previous gain cannot continue after applying a new level.

## Validate and audition

`snake_audio_tests` runs twelve device-free groups covering sample continuity,
energy and headroom, reproducible variants, burst aggregation, event replay,
combat rate limits, voice priority, reward precedence, and mute/reset behavior.
The additional groups check the mechanical click waveform, all 24 squares just
before/on the marker, 20–240 Hz presentation, stalls, and skip/freeze/reset.

```powershell
./build/snake_audio_tests.exe --export artifacts/audio
```

The export writes individual WAV files at their actual playback gains and an
11-second `combat_and_rewards.wav` example. It combines dense combat, pickup,
damage, level, chest, evolution, and victory cues through the same director
used in the game. The export does not require an audio device.
`reel_tick.wav` auditions the individual click; `reel_open_evolution.wav` includes
the complete square-synchronized roll and evolution reveal at their playback gains.
`reel_tick_times.json` lists crossings sampled from the real tracker for demo use.
