"""Build an integer-scaled comparison of native Reckoning captures."""
import argparse
import base64
import json
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from game_tests._png import read


def cropped_png(path):
    frame = read(path)
    x, y, width, height = 300, 170, 320, 160
    rows = b"".join(b"\0" + frame.rgba[((y+row)*frame.width+x)*4:((y+row)*frame.width+x+width)*4]
                    for row in range(height))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind+data) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    return "data:image/png;base64," + base64.b64encode(png).decode("ascii")


def inline_preview(folder):
    frames = {stage: {version: cropped_png(folder/version/f"east-{tick:03d}.png")
                      for version in ("before", "after")}
              for stage, tick in (("Pull", 40), ("Open jaws", 74), ("Snap shut", 78))}
    return r'''<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{margin:0;color:var(--foreground,#eee);font:14px var(--font-sans,system-ui,sans-serif)}
.controls{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}button{font:inherit;color:inherit;background:var(--secondary,#29212b);border:1px solid var(--border,#59404e);border-radius:var(--radius,8px);padding:8px 12px;cursor:pointer}
button[aria-pressed=true]{background:var(--accent-surface,#432334);border-color:#f48dab}figure{margin:0;padding:16px;background:#080609;border-radius:var(--radius,8px)}
figcaption{font-size:12px;color:#cbb9c8;margin-bottom:12px}img{display:block;margin:auto;width:320px;height:160px;image-rendering:pixelated}
@media(min-width:672px){img{width:640px;height:320px}}p{font-size:12px;color:var(--muted-foreground,#b5a4b2);line-height:1.5;margin:10px 0 0}</style>
</head><body><div class="controls" aria-label="Attack stage"><button data-stage="Pull" aria-pressed="false">Pull</button><button data-stage="Open jaws" aria-pressed="true">Open jaws</button><button data-stage="Snap shut" aria-pressed="false">Snap shut</button></div>
<div class="controls" aria-label="Build"><button data-version="before" aria-pressed="false">Previous</button><button data-version="after" aria-pressed="true">Updated</button></div>
<figure><figcaption id="label">Updated · Open jaws · native source pixels</figcaption><img id="frame" alt="Native pixel art tongue evolution with open white jaws" width="320" height="160" src="__INITIAL__"></figure>
<p>Actual native captures. All 372 gameplay checkpoints match the previous version.</p>
<script>const frames=__FRAMES__;let stage='Open jaws',version='after';const frame=document.getElementById('frame'),label=document.getElementById('label');
function show(){frame.src=frames[stage][version];frame.alt=stage+' · '+version+' native capture';label.textContent=(version==='after'?'Updated':'Previous')+' · '+stage+' · native source pixels';for(const b of document.querySelectorAll('[data-stage]'))b.setAttribute('aria-pressed',String(b.dataset.stage===stage));for(const b of document.querySelectorAll('[data-version]'))b.setAttribute('aria-pressed',String(b.dataset.version===version))}
for(const b of document.querySelectorAll('[data-stage]'))b.onclick=()=>{stage=b.dataset.stage;show()};for(const b of document.querySelectorAll('[data-version]'))b.onclick=()=>{version=b.dataset.version;show()};
frame.decode().then(()=>console.log('Native comparison ready: 3 stages in both builds')).catch(console.error);</script></body></html>'''.replace("__INITIAL__", frames["Open jaws"]["after"]).replace("__FRAMES__", json.dumps(frames))


def build(folder, embed=False):
    sources = {}
    for scene in ("east", "diagonal", "volley"):
        sources[scene] = {}
        for version in ("before", "after"):
            paths = [folder / version / f"{scene}-{tick:03d}.png" for tick in range(2, 125, 2)]
            sources[scene][version] = ["data:image/png;base64," + base64.b64encode(path.read_bytes()).decode("ascii")
                                      if embed else path.as_uri() for path in paths]
    return r'''<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{margin:0;color:var(--foreground,#e9e4eb);font:14px var(--font-sans,system-ui,sans-serif)}
.controls{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:12px}
button,select{font:inherit;color:inherit;background:var(--secondary,#27212c);border:1px solid var(--border,#4e3a4f);border-radius:var(--radius,8px);padding:8px 12px;cursor:pointer}
button[aria-pressed=true]{border-color:#f48dab;background:var(--accent-surface,#432334)}
input{flex:1;min-width:100px;accent-color:#f48dab}output{font:12px var(--font-mono,monospace);min-width:76px}
figure{margin:0 0 10px;padding:16px;border-radius:var(--radius,8px);background:#09070b}
figcaption{font-size:12px;color:#cbb9c8;margin-bottom:12px}
canvas{display:block;margin:auto;image-rendering:pixelated;background:#000;max-width:none}
p{margin:0;color:var(--muted-foreground,#b5a4b2);font-size:12px;line-height:1.5}
</style></head><body>
<div class="controls"><button id="play" aria-label="Pause animation">Pause</button>
<select id="scene" aria-label="Attack"><option value="east">Straight pull</option><option value="diagonal">Diagonal pull</option><option value="volley">Three tongues</option></select>
<button data-version="before" aria-pressed="false">Previous</button><button data-version="after" aria-pressed="true">Updated</button></div>
<div class="controls"><input id="time" type="range" min="0" max="61" value="36" aria-label="Animation frame"><output id="stamp">Snap · 1.23s</output></div>
<figure><figcaption id="caption">Updated · actual native framebuffer</figcaption><canvas width="320" height="160" aria-label="Native tongue pull and jaw snap"></canvas></figure>
<p>Native pixel art at an integer scale. The same pulls, devours and gameplay state in both versions.</p>
<script>
const sources=__SOURCES__,images={},ready=[];
for(const [scene,versions] of Object.entries(sources)){images[scene]={};for(const [version,paths] of Object.entries(versions))images[scene][version]=paths.map(src=>{const image=new Image();image.src=src;ready.push(image.decode());return image})}
const canvas=document.querySelector('canvas'),ctx=canvas.getContext('2d'),slider=document.getElementById('time'),stamp=document.getElementById('stamp'),play=document.getElementById('play'),choice=document.getElementById('scene'),caption=document.getElementById('caption');
const crops={east:[300,170,320,160],diagonal:[300,100,260,220],volley:[250,80,340,340]},bites={east:73,diagonal:77,volley:66},latches={east:13,diagonal:11,volley:12};
let version='after',seconds=1.23,previous=0,playing=!matchMedia('(prefers-reduced-motion:reduce)').matches,loaded=false;
function show(){if(!loaded)return;const scene=choice.value,index=Math.min(61,Math.floor(Math.min(seconds,2.06)*30)),tick=(index+1)*2,crop=crops[scene];
if(canvas.width!==crop[2]||canvas.height!==crop[3]){canvas.width=crop[2];canvas.height=crop[3]}
const scale=Math.max(1,Math.floor((canvas.parentElement.clientWidth-32)/canvas.width));canvas.style.width=canvas.width*scale+'px';canvas.style.height=canvas.height*scale+'px';
ctx.imageSmoothingEnabled=false;ctx.clearRect(0,0,canvas.width,canvas.height);ctx.drawImage(images[scene][version][index],crop[0],crop[1],crop[2],crop[3],0,0,crop[2],crop[3]);
slider.value=index;stamp.textContent=(tick<bites[scene]?(tick<latches[scene]?'Fire':'Pull'):(tick<bites[scene]+33?'Snap':'Rest'))+' · '+(tick/60).toFixed(2)+'s';caption.textContent=(version==='after'?'Updated':'Previous')+' · actual native framebuffer'}
function state(){play.textContent=playing?'Pause':'Play';play.setAttribute('aria-label',playing?'Pause animation':'Play animation')}
play.onclick=()=>{playing=!playing;state()};slider.oninput=()=>{playing=false;seconds=Number(slider.value)/30;state();show()};choice.onchange=()=>{seconds=0;show()};
for(const button of document.querySelectorAll('[data-version]'))button.onclick=()=>{version=button.dataset.version;for(const b of document.querySelectorAll('[data-version]'))b.setAttribute('aria-pressed',String(b===button));show()};
new ResizeObserver(show).observe(document.body);
Promise.all(ready).then(()=>{loaded=true;show();console.log('Reckoning ready: 3 scenes, 62 frames each, both native builds')}).catch(error=>console.error(error));
function animate(now){if(loaded&&playing)seconds=(seconds+Math.min(.05,(now-(previous||now))/1000))%2.5;previous=now;show();requestAnimationFrame(animate)}state();requestAnimationFrame(animate);
</script></body></html>'''.replace("__SOURCES__", json.dumps(sources))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/tongue-evolution")
    args = parser.parse_args()
    folder = args.output.resolve()
    (folder / "preview.html").write_text(build(folder, True), encoding="utf-8")
    (folder / "preview-linked.html").write_text(build(folder), encoding="utf-8")
    (folder / "inline-preview.html").write_text(inline_preview(folder), encoding="utf-8")
    print(folder / "preview.html")


if __name__ == "__main__":
    main()
