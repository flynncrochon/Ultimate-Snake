# Original Snake Survivors icons

`original-icons.png` is a raster atlas converted from Flynn Crochon's original
`src/rendering/powerup_icons.js`, reference commit
`b6cb72e66eebc1291d57efe63ebbe93ec602e863`:
https://github.com/flynncrochon/Snake-Survivors

The original artwork and colors are retained. The SVG sources were rasterized
once into native 8, 10, 16, 24, and 32 pixel variants; opacity is quantized to
transparent or opaque pixels. The game loads the PNG and uses point filtering.
It does not draw SVGs or resize icons to fractional pixel sizes at runtime.

The atlas is 864x90. There are 27 columns, with 32 pixel column stride.
Variant rows start at y=0, 8, 18, 34, and 58 respectively.

Column order: Venom Shot, Viper Fangs, Plague Mortar, Snake Nest, Venom Nova,
Sidewinder Beam, Ricochet Fang, Cobra Pit, Tongue Lash, Graviton, Rapid Fire,
Dead Eye, Ravenous Maw, Chronofield, Coiled Volley, Toxic Expanse, Serpent's
Reach, Serpent's Scales, Ouroboros, Serpent's Gatling, Singularity Mortar,
Hydra Brood, Miasma, Consumption Beam, Shatter Fang, Ancient Brood Pit,
Serpent's Reckoning. Column 9 is now used by **Gridlock**, the turn-charged
gravity trap replacing Graviton; its original gravity-coil artwork is retained.

## Application icon

`ultimate-snake.ico` contains native 16 and 24 pixel snake artwork, with integer
pixel enlargements through 256 pixels. `ultimate-snake.png` is the large preview.
The white head and gray tail follow the game's player palette. Regenerate both
files with `python scripts/generate_app_icon.py` (standard library only).

Windows builds embed the ICO as the `GLFW_ICON` resource. GLFW picks it up when
creating the window, and Windows uses the embedded sizes for the taskbar,
Alt+Tab, File Explorer, and shortcuts. This needs no per-frame work or runtime
asset loading. Headless and web builds do not compile the Windows resource.
