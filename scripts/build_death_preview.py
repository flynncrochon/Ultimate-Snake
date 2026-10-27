"""Build a self-contained animation from the native death-art test captures."""
from pathlib import Path
import base64
import json

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "artifacts/gametest/death"
OUTPUT = ROOT / "artifacts/death-effects/preview.html"


def main():
    frames = ["data:image/png;base64," + base64.b64encode(
        (SOURCE / f"frame-{frame}.png").read_bytes()).decode("ascii")
        for frame in range(49)]
    html = r'''<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{margin:0;color:var(--foreground,#e4eee8);font:14px var(--font-sans,system-ui,sans-serif)}
.controls{display:flex;align-items:center;gap:12px;margin:0 0 16px;flex-wrap:wrap}
button{border:1px solid var(--border,#3b4941);border-radius:var(--radius,8px);padding:8px 16px;background:var(--secondary,#1e2822);color:inherit;font:inherit;cursor:pointer;min-width:80px}
input{flex:1;min-width:140px;accent-color:#a4c9ac}output{font:12px var(--font-mono,monospace);min-width:64px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(224px,1fr));gap:12px}
figure{margin:0;background:#090c0e;border-radius:var(--radius,8px);padding:16px;display:flex;align-items:center;flex-direction:column}
figcaption{align-self:flex-start;color:#b5c5ba;font-size:12px;letter-spacing:.04em}
canvas{display:block;width:192px;height:148px;image-rendering:pixelated}
p{font-size:12px;line-height:1.5;color:var(--muted-foreground,#a1aea5);margin:12px 0 0}
@media(prefers-reduced-motion:reduce){button{outline:1px solid var(--border,#3b4941)}}
</style></head><body>
<div class="controls"><button id="toggle" type="button">Pause</button>
<input id="time" type="range" min="0" max="80" value="0" step="1" aria-label="Animation time">
<output id="stamp" for="time">0.00 s</output></div>
<div class="grid" id="specimens"></div>
<p>Actual native framebuffer captures at 2x. Drag the timeline to inspect the pixels.</p>
<script>
const sources=__FRAMES__;
const names=['Red','Blue palette','Swift','Boss','Nest hatchling','Hydra brood','Player'];
const images=sources.map(src=>{const image=new Image();image.src=src;return image});
const contexts=names.map(name=>{
 const figure=document.createElement('figure'),caption=document.createElement('figcaption'),canvas=document.createElement('canvas');
 caption.textContent=name;canvas.width=96;canvas.height=74;canvas.setAttribute('aria-label',name+' death animation');
 figure.append(caption,canvas);document.getElementById('specimens').append(figure);
 const context=canvas.getContext('2d');context.imageSmoothingEnabled=false;return context;
});
const slider=document.getElementById('time'),button=document.getElementById('toggle'),stamp=document.getElementById('stamp');
let playing=!matchMedia('(prefers-reduced-motion:reduce)').matches,seconds=0,previous=0;
function show(){
 const frame=Math.min(48,Math.floor(Math.min(seconds,.8)*60+1e-6)),image=images[frame];
 if(!image.complete || !image.naturalWidth)return;
 contexts.forEach((context,row)=>{context.clearRect(0,0,96,74);context.drawImage(image,380,21+row*65,96,74,0,0,96,74)});
 slider.value=Math.min(80,Math.round(seconds*100));stamp.textContent=Math.min(seconds,.8).toFixed(2)+' s';
}
function state(){button.textContent=playing?'Pause':'Play';button.setAttribute('aria-pressed',String(playing))}
button.onclick=()=>{playing=!playing;if(playing&&seconds>=.8)seconds=0;state()};
slider.oninput=()=>{playing=false;seconds=Number(slider.value)/100;state();show()};
Promise.all(images.map(image=>image.decode())).then(()=>{show();console.log('Native death preview ready: 49 frames, 7 families, 2x integer display')});
function animate(now){if(playing)seconds=(seconds+Math.min(.05,(now-(previous||now))/1000))%1.25;previous=now;show();requestAnimationFrame(animate)}
state();requestAnimationFrame(animate);
</script></body></html>'''.replace("__FRAMES__", json.dumps(frames))
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(html, encoding="utf-8")
    print(OUTPUT)


if __name__ == "__main__":
    main()
