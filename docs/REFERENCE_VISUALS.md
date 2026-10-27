# JavaScript presentation reference

Reference: Flynn Crochon's Snake-Survivors, commit `b6cb72e66eebc1291d57efe63ebbe93ec602e863`. The read-only local checkout at `C:/Users/elite/Documents/GitHub/web_snake` matches that exact commit. The requested port should reproduce this composition, replacing vector artwork with discrete pixel artwork and using the font from Warforge.

## Global composition

The original is intentionally sparse: pure black `#000`, white `#fff`, gray labels and outlines, green venom, cyan progression details. There is no textured terrain, visible tile grid, decorative frame, permanent sidebar, header strip, or footer strip. The arena fills the entire gameplay viewport and the HUD floats over it.

Desktop character selection is a square centered in the window, 90% of the smaller window dimension. Gameplay changes to the entire window's aspect ratio. In gameplay, the smaller viewport dimension spans 25 world cells. Each player's body square is 70% of a cell, ordinary enemy squares 55%, and boss squares 75%. Camera tracking centers the interpolated head and clamps at world boundaries.

## Character selection

Source: `src/core/battle_royale_app.js`, `_menu_card_rects` and `render_character_select`.

Four simultaneous tall cards form one centered row. The square's side is `size`:

- Heading `SNAKE SURVIVORS`, centered at `(size/2, size*0.08)` in white.
- Subtitle `Choose your serpent`, centered at height 13%, color `#555`.
- Card width 20% of size, height 60%, gap 2.5%, top at 18%. Four-card row centered horizontally.
- Selected background white at 8% opacity; ordinary background `rgba(30,30,30,0.5)`. Selected outline white and 3 pixels thick; ordinary outline `#444`, one pixel.
- Inside each card, content is centered: small downward selection triangle at 3% of card height; four-block white snake preview at 10%; character name at 20%; subtitle at 27%; divider at 32%; weapon icon from 34%; weapon name at 52%; wrapped description from 58% to 76%; divider at 80%; passive name at 85%; passive description from 91%.
- Selected names are white; other names `#888`. Selected secondary labels `#999`; others `#555`. Weapon names are `#ffaa00` selected / `#886600` otherwise. Passive names `#44ff44` selected / `#227722` otherwise; descriptions `#88dd88` / `#446644`.
- Footer instructions, centered, `#555`: `< > to select, Space to start` at 84%, `WASD / Arrows to move in-game` at 89%, `F2 - Evolution Menu` at 94%. Best survival, if present, at 97%.
- Clicking an unselected card selects it; clicking the selected card starts. Left/right changes selection; Enter or Space starts.

Character copy:

| Character | Subtitle | Weapon description | Passive |
|---|---|---|---|
| COBRA | The Spitting Menace | Rapid venom projectiles seek out prey | Venom Potency: +15% damage on all weapons |
| TAIPAN | The Fang Slayer | Homing fangs track and shred enemies | Apex Predator: +10% attack speed on all weapons |
| MAMBA | The Swift Fang | Whipping tongue slows and damages foes | Coiling Strike: +15% AOE radius on all effects |
| BASILISK | The Plague Bringer | Toxic bombs leave deadly poison zones | Pestilence: +20% duration on pools and timed effects |

## Gameplay HUD

Source: `render_survivors_hud`. These are original desktop CSS-pixel coordinates; scale the composition coherently to the port's source framebuffer.

- Transparent black backdrops only behind top-left stats `(0,0,190,120)`, centered time `(w/2-50,0,100,30)`, and top-right length `(w-130,0,130,30)`, opacity 55%.
- `Kills: N`, white, top-left `(12,12)`, original 16-pixel bold monospace.
- Optional `Best: N kills`, `#ccc`, `(12,34)`, 11-pixel monospace.
- Five hearts across the top-left at approximately y50. Filled `#ff2244` (`#f44` at one remaining heart), empty outlined `#666`.
- Weapons form four square icon boxes starting `(12,68)`; each is 24x24 with four-pixel gaps and 20x20 icon. Boxes `#444`. Rank number under an owned item when above rank one.
- Passives form a second identical row at y106.
- Timer `m:ss`, white, centered at `(w/2,12)`, 14-pixel bold monospace.
- `Length: N`, white, right-aligned at `(w-12,12)`, 12-pixel monospace.
- XP bar is **bottom center**, 160x6, at `((w-160)/2,h-18)`. Background white10%, fill `#0f8`, outline white25%. `Lv N` just to its left.
- Minimap is **large and square**, 220x220 at `(w-230,h-230)`, black70%, outline `#444`, ten-pixel bottom/right margins. No labels. Red ordinary dots, blue splitter dots, purple boss dots, gold chests, white snake trail, cyan player head.

## Arena and actors

Source: `rendering/renderer.js`, `snake/snake_renderer.js`, `survivors/enemy.js`, and `survivors/survivors_renderer.js`.

- Arena background pure black. White border appears only at actual world bounds.
- Player is connected, pure white rectangular segments with simple distinct faces, no shaded scales or pastel outlines. Cardinal turns use connected L-shaped corners.
- Ordinary enemy head/body is red `#c00`; stronger enemies `#c22` or `#a00`. Splitters use `#2266cc`, `#3388dd`, or `#44aaee`. Boss head `#bb44ff` and body `#7722aa`, yellow eyes.
- Enemy eyes are white squares; boss eyes yellow. Boss HP strip uses dark purple `#440066`, filled `#bb44ff`.
- XP/fruit is a plain white square, 40% of a cell; it is not a diamond-shaped gem.
- Venom projectiles are green fangs/diamonds with a small pale green highlight and green trails. Preserve those colors and silhouettes when rasterizing.
- Gray snake barriers are gray `#555` / `#666` with pale eyes.
- Original upgrade icons are the actual 64x64 SVG definitions in `src/rendering/powerup_icons.js`; they can be rasterized to a coherent source-pixel size rather than redesigned.

## Level-up selection

Source: `rendering/ui_renderer.js`, `draw_item_picker`.

- Full-screen black90% overlay.
- White centered heading `CHOOSE AN ITEM`, original 22-pixel bold, at y60.
- Three centered **narrow, tall cards**, each 160x290 with 20-pixel gaps. Card top `(h-290)/2-20`.
- Selected white outline3px; other outline `#555`1px. Card inside `rgba(30,30,30,0.8)`.
- Centered item name y+30; bright green `NEW` badge upper-right for unowned items.
- Rank pips y+46: cyan owned ranks, white prospective rank, `#333` future ranks. `Lv current -> next` below in `#888`.
- Large48x48 upgrade icon at card y+68, horizontally centered.
- Centered wrapped description from y+130 in `#aaa`; actual numerical upgrade stats below in cyan `#0ff`.
- Key hint `[1]`, `[2]`, `[3]` near card bottom y+275, `#555`.
- The port's reroll/banish features may remain as unobtrusive secondary controls below this reference composition.

## Pause and results

- Pause: black60% full-screen overlay. Only white `PAUSED` at screen center y-20, 36-pixel bold, and gray `Press Space to resume` at center y+20, 14-pixel. No framed menu, buttons, invented headings, or flavor copy.
- Result: preserve the battle image; overlay a full-width black75% strip, 60 pixels tall, vertically centered. White centered20px bold text: `DEAD - N kills - m:ss - Press Space`, or `YOU WON! 20:00 - N kills - Press Space`. Space/Enter returns to character selection.

## Evolution menu

Source: `render_evolution_menu`.

Centered yellow-outlined black92% panel, three columns by three rows. Cards200x120, gap16; outer padding30, title50, footer30. Yellow centered `* EVOLUTIONS *` title. Card border gray locked / cyan ready / green unlocked; card fill dark gray or dark green. Icon32x32 at card `(8,8)`; yellow evolution name `(48,16)`; status under it `(48,32)`. Each requirement is a row at y54/72 with small14px icon, prerequisite name, and right-aligned current/target rank. Footer explains F2/Escape and opening a chest. Preserve this functional list presentation rather than inventing a separate codex theme.

## Fidelity priorities

1. Restore pure black full-window arena and floating original HUD placement.
2. Restore all four character cards and exact original copy.
3. Restore original saturated white/red/blue/purple/green actor palette and flat silhouettes.
4. Restore plain fruit, large bottom-right minimap, and bottom-center XP.
5. Restore `CHOOSE AN ITEM`, plain `PAUSED`, and the centered result strip.
6. Use Warforge's actual font while preserving the reference's relative type sizes.
7. Keep every source pixel on the same nearest-neighbor integer grid.
