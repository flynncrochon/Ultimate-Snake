# Settings

Open **Settings** from character selection, the pause screen, or its keyboard
shortcut (F10 by default). Settings pause the run while preserving the current
displayed pose. Use the mouse, or Tab to change pages, Up/Down to select a row,
Left/Right to change it, Enter to activate it, and Escape to cancel.

Changes remain drafts until **Apply**. **Cancel** discards the draft. **Defaults**
selects the defaults as a draft, so it can be reviewed before applying.

## Display

The display controls follow Warforge's windowing and frame-pacing model:

- **Window mode:** Windowed, Borderless Fullscreen, or Fullscreen. Borderless and
  fullscreen use the selected display's native dimensions. The remembered window
  size is restored when returning to Windowed.
- **Display:** choose a connected monitor. Fullscreen refresh choices come from
  the native modes reported by that monitor.
- **Window size:** changes the windowed client size. It is retained while using
  a fullscreen mode. Oversized windows fit the selected display's usable desktop
  area, including the native title bar and borders at that display's DPI scale.
  With Auto pixel scale, windows that reach the desktop limits use a whole
  canvas-sized client, avoiding large unused borders when decorations force a
  lower scale. Smaller custom sizes and explicit manual scales are retained.
- **Refresh rate:** Automatic keeps the display's desktop refresh; explicit
  supported rates are available in Fullscreen.
- **Frame rate limit:** Uncapped, or the listed frame-rate presets. This controls
  rendering; simulation remains fixed at 60 Hz.
- **VSync:** follows the display refresh and takes precedence over the software
  FPS limit. The frame-rate control is disabled while VSync is enabled.
- **Limit to display refresh:** caps rendering at the monitor refresh when
  VSync is disabled, avoiding work on frames the display cannot show.
- **Pixel scale:** Auto chooses the largest whole-number enlargement that fits.
  A manual scale is limited to what fits the window. The logical framebuffer
  remains 800×500 on desktop, with square pixels and letterboxing. The browser
  uses that baseline to choose zoom, then reveals more arena across its available
  canvas. Its HUD follows the expanded edges and its pixels retain integer scale.
- **Background frame rate:** caps rendering while unfocused or minimized.
  **Pause when unfocused** also stops simulation while the game lacks focus.

Monitor, mode, window-size, and refresh changes need confirmation.
Choose **Keep** within 15 seconds, or use **Revert**; an unanswered preview
automatically restores the previous display setup.

## Graphics

High uses the available effect population. Medium displays up to 600 particles
and 90 damage numbers; Low displays up to 200 and 40. The independent particle
and damage-number switches can hide those effects entirely. Weapon Effects
controls decorative fills, rings, trails, shadows, and beam detail while keeping
attacks readable. FPS Counter and Minimap can be toggled independently.

These options affect presentation, not weapon damage, collisions, enemy counts,
or rewards.

## Audio

Sound can be disabled globally. Master volume multiplies Game Effects, Combat
Effects, and Interface / Rewards. Combat remains subject to the sound director's
quiet cadence and voice limits. The mute shortcut works immediately even if the
settings file cannot be saved.

## Controls

Select an action and press Enter, or click its key, then press the replacement
key. Escape cancels capture. Apply rejects duplicate or overlapping bindings.
Arrow key alternatives can be disabled before assigning arrows to other actions.
Pause retains its P alternative while bound to Escape; the evolution menu retains
F2 while bound to Tab. Changing those bindings releases their alternatives.

## Persistence and tests

Applied settings are saved to `snake-settings.json` beside the executable.
`--settings PATH` selects another file. The existing run/profile save remains
separate; a missing settings file can migrate the old sound preference.
Version 1 settings retain their other bindings when the removed dash slot is
discarded. New saves use version 2 and the ten remaining actions.

Headless settings tests cover persistence and frame-pacing rules. Native display
tests cover window transitions and rollback; native settings UI tests cover real
mouse routing, draft/apply/cancel, display confirmation, audio controls, duplicate
bindings, and failed-save muting. Native tests require a desktop OpenGL context.
