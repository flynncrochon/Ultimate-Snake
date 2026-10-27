# Third-party notices

The original game concept, weapon and character names, visual design, and upgrade artwork are by Flynn Crochon: [Snake Survivors](https://github.com/flynncrochon/Snake-Survivors), reference commit `b6cb72e66eebc1291d57efe63ebbe93ec602e863`. The native C++ simulation is new. `data/icons/original-icons.png` is converted from the original game's `src/rendering/powerup_icons.js` SVG definitions; the original artwork and colors are retained in discrete raster variants. See [`data/icons/README.md`](../data/icons/README.md) for atlas details.

The weapon reference fixture [`game_tests/fixtures/javascript_weapon_oracle.json`](../game_tests/fixtures/javascript_weapon_oracle.json) records values obtained by executing the original JavaScript classes, getters, and module constants from that commit. [`game_tests/reference_probe.mjs`](../game_tests/reference_probe.mjs) contains the browser probe; [`WEAPON_PARITY.md`](WEAPON_PARITY.md) describes the resulting native contracts and intentional fixes. This fixture is reference data for tests, not a second implementation of the original game.

## Pixel Operator 8

`data/fonts/PixelOperator8.ttf` is the exact face used by Warforge, unchanged release `2018.10.04-1` by Jayvee Enaguas (HarvettFox96), copyright 2009–2018. It is distributed under Creative Commons Zero (CC0) 1.0. The original copyright notice and complete CC0 legal text accompany it in [`data/fonts`](../data/fonts).

Source: [Pixel Operator on DaFont](https://www.dafont.com/pixel-operator.font).

The native game rasterizes the font at its intended eight-pixel size with monochrome pixels and point filtering. This changes its rendering pipeline, not the supplied font file.

## raylib 5.5

Source: https://github.com/raysan5/raylib/tree/5.5

Copyright (c) 2013-2024 Ramon Santamaria (@raysan5)

This software is provided "as-is", without any express or implied warranty. In no event
will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial
applications, and to alter it and redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you
   wrote the original software. If you use this software in a product, an acknowledgment
   in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented
   as being the original software.
3. This notice may not be removed or altered from any source distribution.

raylib bundles dependencies including GLFW, miniaudio, stb libraries, and other modules. Their original notices remain in the fetched raylib source distribution under `build/_deps` or the configured local raylib source checkout.

## nlohmann/json 3.11.3

Source: [nlohmann/json v3.11.3](https://github.com/nlohmann/json/tree/v3.11.3).

Copyright (c) 2013-2023 Niels Lohmann.

The unmodified single header is vendored at [`third_party/nlohmann/json.hpp`](../third_party/nlohmann/json.hpp). It parses and serializes the developer GameTest JSON-lines protocol and persistent settings. The complete MIT license is included at [`third_party/nlohmann/LICENSE.MIT`](../third_party/nlohmann/LICENSE.MIT); retain it when redistributing this dependency.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
