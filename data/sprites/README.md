# Chest sprites

`chests.png` contains two independently authored native-resolution chest sprites:

| Use | Atlas rectangle | Draw size | Center offset |
|---|---|---|---|
| World pickup | `(0, 0, 16, 16)` | 16×16 source pixels | `(-8, -8)` |
| Reward reveal | `(16, 0, 32, 32)` | 32×32 source pixels | `(-16, -16)` |

The atlas is 48×32, eight-bit RGBA. Its unused lower-left 16×16 region is fully
transparent. Every sprite has a transparent outer border, an opaque one-pixel
outline, native wood-grain detail, gold trim and rivets, and a red jeweled latch.
The warm wood/gold/red palette follows the original JavaScript chest design.

Each map character is one source pixel. The 32×32 reveal artwork has its own
pixel map and detail; it is not a magnified copy of the pickup. Both regions
should be blitted at their native dimensions with point filtering. The game's
final framebuffer enlargement applies the same integer scale to all artwork.
Do not add fractional scaling, smooth filtering, antialiased edges, or a separate
per-sprite pixel scale.

Load the PNG once with the other art assets and reuse the texture. Do not
generate or reload it during drawing.

The fixed palette and authored maps live in
[`scripts/generate_chest_sprites.py`](../../scripts/generate_chest_sprites.py).
The generator uses only Python's standard library:

```powershell
python scripts/generate_chest_sprites.py
python scripts/generate_chest_sprites.py --preview
```

`--preview` writes an enlarged nearest-neighbor review image under
`artifacts/chest-sprites`; that preview is not a runtime asset. The generator
validates row sizes, palette characters, transparent outer edges, single-pixel
detail, and separately authored reveal art before writing the atlas.
