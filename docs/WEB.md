# Web build and hosting

The same C++17 game can compile to WebAssembly with Emscripten and raylib's
WebGL2 backend. Desktop builds remain the default. A desktop browser with a
keyboard and WebGL2 is required; this does not add mobile controls.

## Toolchain

Install CMake 3.20+, Ninja, and the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html).
The web CI uses **Emscripten 4.0.23**. In an emsdk checkout:

```sh
./emsdk install 4.0.23
./emsdk activate 4.0.23
source ./emsdk_env.sh
```

On Windows, use `./emsdk.bat install 4.0.23`, `./emsdk.bat activate 4.0.23`,
then `./emsdk_env.ps1` in the PowerShell session used for the build.
The SDK supplies its compiler and Node runtime; MSVC is not needed for web.
The first configure downloads the same pinned raylib 5.5 archive as desktop.

## Build

From the repository root, after loading the Emscripten environment:

```powershell
./snake.ps1 web
# Optional: ./snake.ps1 web -Configuration Debug -RaylibSource C:/path/to/raylib
```

This builds in `build-web` and packages a static site in `build-web/site`.
Rebuild after changing any assets; CMake tracks the preloaded data and HTML shell.
The equivalent commands on Linux/macOS or from an Emscripten shell are:

```sh
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web --parallel
cmake --install build-web --prefix build-web/site --component game
```

For an offline raylib checkout add `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=/path/to/raylib`
to configure. Native GLFW, native test hosts, and Python GameTest executables are
excluded from the web target. Keep using the desktop/headless builds for those tests.

## Test locally and host

```sh
python -m http.server 8080 --directory build-web/site
```

Visit `http://localhost:8080`. Opening `index.html` with `file://` will not work.
Upload the **contents** of `build-web/site` to any static web host, such as GitHub
Pages, Netlify, Cloudflare Pages, or an ordinary HTTP server. Keep `index.html`,
`index.js`, `index.wasm`, and `index.data` together. Paths are relative, so hosting
under a repository subdirectory works. The `docs` and license files are also
included in the package. Serve `.wasm` as `application/wasm` and `.data` as
`application/octet-stream`. Enable HTTP compression for faster downloads. No
backend, Node server, pthreads, or cross-origin isolation headers are required.
Embedded hosts must allow scripts, WebAssembly, audio, keyboard focus, and
fullscreen if desired (for an iframe, `allow="autoplay; fullscreen"`).

The GitHub Actions **Build, test, and publish** workflow compiles the web game
with pinned Emscripten 4.0.23 and uploads a `snake-survivors-web` static-site
artifact on pushes and pull requests. After a successful web build, pushes to
the repository's default branch also publish `build-web/site` to GitHub Pages.
Pull requests, other branches, and tags do not deploy. A manual run of the
workflow on the default branch also publishes the site.

One-time repository setup: in **Settings → Pages → Build and deployment**,
select **GitHub Actions** as the source. No personal access token or additional
secret is required. The `github-pages` deployment environment exposes the final
site URL in the workflow run. For this repository the default project address
is `https://flynncrochon.github.io/Ultimate-Snake/`, unless a custom domain is
configured. Forks publish under their own owner/repository path.

The deployment job waits for the web build, has only Pages/OIDC write
permissions, and uses GitHub's configure/upload/deploy Pages actions. Runs for
each Git ref are serialized, with the latest pending run retained and an active
deployment allowed to finish.

## Browser behavior

- Browser animation callbacks drive rendering; the existing bounded fixed-step
  accumulator still advances combat at **60 Hz**. The browser controls render
  cadence and background throttling. Native monitor, refresh-rate, VSync, and FPS
  controls are disabled in web Settings. Pause when unfocused remains available.
- **800×500** remains the baseline for choosing integer pixel zoom. Larger
  browser canvases reveal additional arena rather than stretching that image.
  The shared quantized camera, native font, square pixels, point filtering, and
  full-quality effects remain intact. HUD elements anchor to the expanded
  viewport edges; menus and settings stay centered, with matching mouse routing.
  The browser renders directly at integer zoom, reusing its prepared shaders and
  batches without allocating render textures on resize. Canvas dimensions and
  GLFW's window metadata stay synchronized through resize, DPI, and fullscreen
  changes. The page applies no fractional CSS scaling. Viewports below 800×500
  scroll/crop instead of shrinking pixels. The Fullscreen button uses the
  browser fullscreen API.
- Click the game to focus keyboard input and unlock audio under browser autoplay
  rules. Enter starts a run; Escape pauses. Escape in the character menu keeps the
  web game open. Browser-reserved shortcuts may require rebinding in Settings.
- Settings and profile records live in an in-memory filesystem, loaded from
  **IndexedDB before startup**. Saves are coalesced and flushed asynchronously.
  Storage is scoped to this site's origin; private browsing, blocked storage,
  clearing site data, or changing host can remove it. Storage failure displays a
  warning and allows play with preferences lasting for the current visit.
- All assets are preloaded and shaders/audio are prepared before play. The build
  reserves **256 MiB** of WebAssembly memory, including an 8 MiB stack, and disables
  heap growth to avoid growth pauses. This is not a guarantee of hitch-free play
  across browsers and hardware. Out-of-memory failures require investigating the
  workload before raising the fixed budget.

For a release, review a cold load, menu, gameplay, settings/reload persistence,
audio after interaction, focus loss/return, window resize, and fullscreen in the
target browsers. Native tests alone do not verify browser rendering or pacing.

## Verification recorded for this port

The Emscripten 4.0.23 Release build and static-site installation were run locally.
Chromium/WebGL2 checks covered startup, gameplay, the item picker, settings,
IndexedDB persistence across reloads, resize, and entering/exiting fullscreen.
The desktop Release build passed all 15 configured CTest checks (including four
native graphics/UI checks) and its 300-frame smoke run. All five deterministic
full-quality render-parity scenes matched the original desktop shader with zero
different pixels when using the same assets.

For the existing 1,200-frame dense desktop render workload (1,800 enemies,
1,600 projectiles, 64 zones), before/after frame means were 0.6620/0.6624 ms and
p99 values were 1.0911/1.1078 ms on the local machine. These are native rendering
measurements, not browser FPS. A dense browser benchmark and a full browser
playthrough have not been completed; the browser checks above establish basic
functionality, not universal frame-time guarantees.

The expanded viewport was checked on 31 October 2026. All 23 desktop/headless
CTest targets pass, including six new native viewport groups. They verify newly
visible actors on all four sides, unchanged pixels in the original central
view, integer enlargement of world/HUD/menu/settings/chest pixels, and aligned
mouse/upgrade routing. Five original-resolution combat captures have zero
changed pixels. Headless Edge WebGL2 checks cover six browser sizes (including
odd dimensions and DPR 2), a centered settings click, fullscreen entry/exit,
and audio activation without JavaScript errors. Native and browser captures
were opened for review. See [validation](VALIDATION.md) for timings and limits.

`scripts/check_web_viewport.mjs` runs these browser checks with Node 22+ against
a running game and an isolated Chromium/Edge DevTools endpoint. Its optional
arguments are the game URL and DevTools HTTP endpoint; defaults are
`http://127.0.0.1:18765/index.html` and `http://127.0.0.1:18766`. It writes
captures and a report beneath `artifacts/web-responsive/browser`.
