"""Build an integer-scaled native before/after splitter animation."""
import base64
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "artifacts/splitter-death"


def build(embed, folder=OUTPUT):
    sources = {}
    for scene in ("straight", "turn", "edge"):
        sources[scene] = {}
        for version in ("before", "after"):
            paths = [folder / version / f"{scene}-{frame:02d}.png" for frame in range(31)]
            sources[scene][version] = ["data:image/png;base64," + base64.b64encode(path.read_bytes()).decode("ascii")
                                      if embed else path.as_posix() for path in paths]
    return r'''<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><style>
body{margin:0;font:14px var(--font-sans,system-ui,sans-serif);color:var(--foreground,#e7eee9)}
.controls{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-bottom:12px}
button,select{font:inherit;color:inherit;background:var(--secondary,#202923);border:1px solid var(--border,#3a463e);border-radius:var(--radius,8px);padding:8px 12px;cursor:pointer}
input{flex:1;min-width:100px;accent-color:#98c1a5}output{font:12px var(--font-mono,monospace)}
figure{margin:0 0 12px;padding:16px;border-radius:var(--radius,8px);background:#090c0e}
figcaption{color:#bdcdc1;font-size:12px;margin-bottom:12px}
canvas{display:block;margin:auto;image-rendering:pixelated;background:#000}
p{font-size:12px;line-height:1.5;color:var(--muted-foreground,#a0b0a5);margin:0}
</style></head><body><div class="controls">
<button id="toggle">Pause</button><select id="scene" aria-label="Snake path"><option value="straight">Straight body</option><option value="turn">Curved body</option><option value="edge">Arena edge</option></select>
<input id="time" type="range" min="0" max="50" step="1" value="0" aria-label="Animation time"><output id="stamp">0.00 s</output>
</div><figure><figcaption>Previous</figcaption><canvas width="240" height="128" data-version="before" aria-label="Previous splitter death"></canvas></figure>
<figure><figcaption>Body splits into the two offspring</figcaption><canvas width="240" height="128" data-version="after" aria-label="New splitter death"></canvas></figure>
<p>Actual native captures. Both versions spawn and simulate the same two snakes at the same time.</p>
<script>const sources=__SOURCES__;
const images={};const ready=[];
for(const [scene,versions] of Object.entries(sources)){images[scene]={};for(const [version,paths] of Object.entries(versions))images[scene][version]=paths.map(src=>{const image=new Image();image.src=src;ready.push(image.decode());return image})}
const canvases=[...document.querySelectorAll('canvas')],slider=document.getElementById('time'),stamp=document.getElementById('stamp'),button=document.getElementById('toggle'),choice=document.getElementById('scene');
const crops={straight:[440,186],turn:[440,20],edge:[0,16]};let seconds=0,previous=0,playing=!matchMedia('(prefers-reduced-motion:reduce)').matches;
function show(){const frame=Math.min(30,Math.floor(Math.min(seconds,.5)*60+1e-6)),crop=crops[choice.value];for(const canvas of canvases){const image=images[choice.value][canvas.dataset.version][frame];if(!image.complete||!image.naturalWidth)continue;const ctx=canvas.getContext('2d');ctx.imageSmoothingEnabled=false;ctx.clearRect(0,0,240,128);ctx.drawImage(image,crop[0],crop[1],240,128,0,0,240,128)}slider.value=Math.min(50,Math.round(seconds*100));stamp.textContent=Math.min(seconds,.5).toFixed(2)+' s'}
function state(){button.textContent=playing?'Pause':'Play';button.setAttribute('aria-pressed',String(playing))}
button.onclick=()=>{playing=!playing;if(playing&&seconds>=.5)seconds=0;state()};slider.oninput=()=>{playing=false;seconds=Number(slider.value)/100;state();show()};choice.onchange=()=>{seconds=0;show()};
new ResizeObserver(()=>{for(const canvas of canvases){const scale=canvas.parentElement.clientWidth-32>=480?2:1;canvas.style.width=240*scale+'px';canvas.style.height=128*scale+'px'}}).observe(document.body);
Promise.all(ready).then(()=>{seconds=0;show();console.log('Native splitter preview ready: 3 paths, 31 frames, before/after')});
function animate(now){if(playing)seconds=(seconds+Math.min(.05,(now-(previous||now))/1000))%1.1;previous=now;show();requestAnimationFrame(animate)}state();requestAnimationFrame(animate);
</script></body></html>'''.replace("__SOURCES__", json.dumps(sources))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    folder = parser.parse_args().output.resolve()
    (folder / "preview.html").write_text(build(True, folder), encoding="utf-8")
    (folder / "preview-linked.html").write_text(build(False, folder), encoding="utf-8")
    print(folder / "preview.html")


if __name__ == "__main__":
    main()
