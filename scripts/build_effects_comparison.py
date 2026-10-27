"""Build an interactive, integer-pixel viewer for the real effect captures.

The default output embeds every PNG and can be opened offline. --references
writes a compact companion HTML document whose absolute image paths can be
inlined by T3's html_preview/html_render tools. No generated or mock artwork is
used: all images come from showcase_effects.py's native captures.
"""
from __future__ import annotations

import argparse
import base64
from html import escape
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Native source-pixel focus points, never rescaled screenshot derivatives.
DETAILS = {
    "venom_shot": ((450, 250), "Carved venom bolts, dark outlines and ivory glints."),
    "ouroboros": ((400, 250), "All 44 orbiting fangs retain their own direction and native size."),
    "viper_fangs": ((440, 250), "Hooked ivory fangs with cool blue shade clusters."),
    "serpent_gatling": ((460, 250), "Layered jade bolts with bright cores and brass accents."),
    "mortar_flight": ((485, 165), "A shaded poison vessel with a bright rim and a visible lob trail."),
    "plague_mortar": ((520, 250), "Toxic bubbles, broken foam shelves and clustered surface highlights."),
    "singularity_mortar": ((520, 250), "Violet spiral arms, orbiting fragments and a dark singularity core."),
    "nest_egg": ((468, 130), "A speckled amber shell with a curved highlight and dark keyline."),
    "snake_nest": ((500, 250), "Hunting hatchlings gain scales and highlights along their grid paths."),
    "hydra_egg": ((468, 130), "A purple brood egg with native shell shading and a dark keyline."),
    "hydra_brood": ((500, 250), "Plain purple snakes with a single head and undecorated bodies."),
    "venom_nova": ((400, 250), "Layered stepped shock rings, bright radial teeth and sharp glints."),
    "miasma": ((400, 250), "Bubbling venom, broken foam and rising pixel clusters."),
    "sidewinder_beam": ((460, 250), "A thicker blue energy spine with dark edges and bright current nodes."),
    "consumption_beam": ((460, 250), "Twin ember strands, shaded edges and a pale energy core."),
    "ricochet_fang": ((450, 250), "A jagged violet fang with teal inlays and ivory highlights."),
    "shatter_fang": ((490, 250), "Distinct crystal fangs and splinters replace single-pixel streaks."),
    "pit_flight": ((485, 135), "A detailed pit seed follows the original arcing flight."),
    "cobra_pit": ((520, 230), "Shaded cobra hoods, patterned scales and layered pit rims."),
    "ancient_brood_pit": ((520, 260), "An ornate gold hood, segmented belly and a layered ancient laser."),
    "tongue_lash": ((450, 250), "A fleshy shaded strike with a clear forked tip."),
    "serpents_reckoning": ((340, 290), "A outlined pink grapple with bright sinew and a forked hook."),
}


def build_html(root: Path | str, embed: bool = True) -> str:
    """Return a complete viewer; root is the repository root directory."""
    folder = Path(root).resolve() / "artifacts/effects-overhaul"
    before = json.loads((folder / "before/result.json").read_text(encoding="utf-8"))
    after = {row["scene"]: row for row in
             json.loads((folder / "after/result.json").read_text(encoding="utf-8"))}
    scenes, images = [], []
    for index, old in enumerate(before):
        name = old["scene"]
        if name not in after:
            raise ValueError(f"Missing after capture: {name}")
        new = after[name]
        focus, detail = DETAILS.get(name, ((400, 250), "Native pixel-art detail."))
        scenes.append({"name": name, "title": old["title"], "ticks": old["ticks"],
                       "evolved": old["evolved"], "focus": focus, "detail": detail,
                       "identical": bool(new.get("simulation_identical")),
                       "pixels": new.get("different_pixels", 0)})
        for phase in ("before", "after"):
            path = folder / phase / f"{name}.png"
            source = ("data:image/png;base64," + base64.b64encode(path.read_bytes()).decode("ascii")
                      if embed else path.as_posix())
            images.append(f'<img id="{phase[0]}{index}" src="{escape(source, quote=True)}" alt="">')
    if len(scenes) != 22:
        raise ValueError(f"Expected 22 matched showcase scenes, got {len(scenes)}")
    rows = [[s[key] for key in ("name", "title", "ticks", "evolved", "focus", "detail", "identical", "pixels")]
            for s in scenes]
    data = json.dumps(rows, separators=(",", ":")).replace("</", "<\\/")
    return TEMPLATE.replace("__SCENES__", data).replace("__IMAGES__", "\n".join(images))


TEMPLATE = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ultimate Snake · Pixel effect comparison</title>
<style>
:root{color-scheme:dark;--ink:var(--foreground,#eaf0ed);--sub:var(--muted-foreground,#99aaa2);--line:var(--border,#29372f);--jade:#aff7a6}
*{box-sizing:border-box}html,body{margin:0;background:transparent;color:var(--ink);font:14px/1.5 var(--font-sans,system-ui,sans-serif)}
button,select,input{font:inherit}button,select{color:var(--ink);background:var(--secondary,#17221c);border:1px solid var(--line);border-radius:7px;min-height:36px}
button{padding:6px 11px;cursor:pointer}button:hover{border-color:#79bd89}button:focus-visible,select:focus-visible,input:focus-visible{outline:2px solid #aff7a6;outline-offset:3px}
button[aria-pressed=true]{color:#0c2312;background:var(--jade);border-color:var(--jade)}select{padding:6px 30px 6px 10px;width:100%;min-width:0}
.kicker{font:10px/1.4 var(--font-mono,monospace);letter-spacing:.11em;color:var(--sub);margin:0 0 8px}.heading{display:flex;gap:12px;align-items:baseline;justify-content:space-between;margin-bottom:14px}
h2{font-size:22px;line-height:1.2;letter-spacing:-.025em;font-weight:650;margin:0}.variant{font-size:11px;color:var(--sub);white-space:nowrap}
.toolbar{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-bottom:10px}.chooser{flex:1 1 235px;display:flex;gap:6px;min-width:0}.chooser select{flex:1}.arrow{width:34px;min-width:34px;padding:0;font-size:18px}
.seg{display:flex;gap:3px}.zoom{display:flex;gap:3px;margin-left:auto}.zoom button{padding-inline:8px}.stage{position:relative;background:#080e0b;border:1px solid #22382b;border-radius:var(--radius,12px);padding:16px;overflow:hidden}
#picture{display:block;max-width:none;image-rendering:pixelated;image-rendering:crisp-edges;touch-action:none;cursor:grab;background:#000;margin:0 auto}#picture:active{cursor:grabbing}
.badges{position:absolute;top:27px;left:27px;right:27px;display:flex;justify-content:space-between;pointer-events:none}.badge{font:10px/1 var(--font-mono,monospace);letter-spacing:.08em;border:1px solid #31483a;padding:7px 8px;background:#0b110ee8;border-radius:4px;color:#b3c0b7}.badge.new{color:#bdf9ae;border-color:#4a7852}.stage-note{position:absolute;bottom:24px;right:27px;color:#7f9487;font:10px var(--font-mono,monospace);pointer-events:none}
.wipe{display:flex;align-items:center;gap:10px;margin:9px 0 6px;color:var(--sub);font-size:11px}.wipe input{width:100%;height:18px;accent-color:#b4f5a6;cursor:ew-resize}.wipe span{white-space:nowrap}.wipe[hidden]{display:none}
.caption{display:flex;gap:6px 15px;flex-wrap:wrap;font-size:11px;color:var(--sub);margin:10px 0}.ok{color:#8ccf8f}.detail{font-size:14px;margin:0 0 17px;min-height:21px}.small{font-size:11px;color:var(--sub)}
.notes{border-top:1px solid var(--line);padding-top:14px;display:grid;grid-template-columns:repeat(3,1fr);gap:15px}.notes strong{display:block;font-size:12px;font-weight:600;margin-bottom:4px}.notes p{font-size:12px;color:var(--sub);margin:0}.footer{font-size:11px;color:var(--sub);margin:15px 0 0}.error{color:#f4ab9b;margin:10px 0}
@media(max-width:500px){h2{font-size:19px}.heading{gap:8px}.toolbar{gap:8px}.chooser{flex-basis:100%}.zoom{margin-left:auto}.notes{grid-template-columns:1fr;gap:10px}.notes strong{display:inline;margin-right:5px}.notes p{display:inline}.stage{padding:16px}.badges{left:23px;right:23px}.stage-note{right:23px}.kicker{font-size:9px}.variant{font-size:10px}}
</style></head><body>
<p class="kicker">22 MATCHED NATIVE CAPTURES · ALL 9 WEAPON FAMILIES</p>
<div class="heading"><h2 id="title">Singularity Mortar</h2><span class="variant" id="variant">EVOLVED</span></div>
<div class="toolbar">
 <div class="chooser"><button class="arrow" id="previous" aria-label="Previous effect">‹</button><select id="scene" aria-label="Choose weapon effect"></select><button class="arrow" id="next" aria-label="Next effect">›</button></div>
 <div class="seg" aria-label="Comparison mode"><button data-mode="before" aria-pressed="false">Before</button><button data-mode="split" aria-pressed="true">Split</button><button data-mode="after" aria-pressed="false">New</button></div>
 <div class="zoom" aria-label="Integer pixel zoom"><button data-zoom="1" aria-pressed="true">1×</button><button data-zoom="2" aria-pressed="false">2×</button><button data-zoom="3" aria-pressed="false">3×</button></div>
</div>
<div class="stage" id="stage"><canvas id="picture" width="640" height="400" role="img" aria-label="Before and new pixel-art effect comparison"></canvas>
 <div class="badges"><span class="badge" id="beforeBadge">BEFORE</span><span class="badge new" id="afterBadge">NEW PIXEL ART</span></div><span class="stage-note" id="stageNote">1 SOURCE PIXEL = 1 SCREEN PIXEL</span>
</div>
<div class="wipe" id="wipe"><span>Before</span><input id="slider" type="range" min="0" max="100" value="50" aria-label="Reveal new pixel artwork"><span>New</span></div>
<div class="caption"><span id="fixture"></span><span id="simulation" class="ok"></span><span id="changes"></span></div>
<p class="detail" id="detail"></p>
<div class="notes"><div><strong>Hand-authored pixels</strong><p>New silhouettes, clustered shading, dark keylines and sharp highlights.</p></div><div><strong>Every weapon family</strong><p>Projectiles, pools, beams, eggs, summons, cobras, nova and tongue strikes.</p></div><div><strong>Native presentation</strong><p>800 × 500 source frames. Integer zoom, square pixels and no smoothing.</p></div></div>
<p class="footer">Drag the divider to compare. Drag the image to inspect details; double-click to recenter. Each pair uses the same seeded combat and exact simulation tick.</p>
<p class="error" id="error" hidden></p>
<div hidden>__IMAGES__</div>
<script>
const scenes=__SCENES__.map(d=>({name:d[0],title:d[1],ticks:d[2],evolved:d[3],focus:d[4],detail:d[5],identical:d[6],pixels:d[7]}));
const $=id=>document.getElementById(id),canvas=$('picture'),ctx=canvas.getContext('2d',{alpha:false});
const state={index:Math.max(0,scenes.findIndex(s=>s.name==='singularity_mortar')),mode:'split',zoom:1,split:.5,focus:[520,250],ready:false};
for(let i=0;i<scenes.length;i++){const o=document.createElement('option');o.value=i;o.textContent=String(i+1).padStart(2,'0')+' · '+scenes[i].title;$('scene').append(o)}
function source(phase){return $(phase[0]+state.index)}
function draw(){
 if(!state.ready)return;
 const z=state.zoom,w=canvas.width,h=canvas.height;
 const x=Math.max(w-800*z,Math.min(0,Math.round(w/2-state.focus[0]*z)));
 const y=Math.max(h-500*z,Math.min(0,Math.round(h/2-state.focus[1]*z)));
 ctx.imageSmoothingEnabled=false;ctx.fillStyle='#000';ctx.fillRect(0,0,w,h);
 const paint=phase=>ctx.drawImage(source(phase),x,y,800*z,500*z);
 if(state.mode==='before')paint('before');else if(state.mode==='after')paint('after');
 else{paint('before');const divider=Math.round(w*state.split);ctx.save();ctx.beginPath();ctx.rect(divider,0,w-divider,h);ctx.clip();paint('after');ctx.restore();ctx.fillStyle='#b4f5a6';ctx.fillRect(divider,0,1,h);ctx.fillStyle='#cfffbd';ctx.fillRect(divider-8,Math.floor(h/2)-12,17,25);ctx.fillStyle='#25432d';ctx.fillRect(divider-3,Math.floor(h/2)-5,1,11);ctx.fillRect(divider+2,Math.floor(h/2)-5,1,11)}
 $('beforeBadge').hidden=state.mode==='after';$('afterBadge').hidden=state.mode==='before';$('wipe').hidden=state.mode!=='split';
 $('stageNote').textContent=z+'× INTEGER PIXELS · DRAG TO PAN';
 canvas.setAttribute('aria-label',scenes[state.index].title+', '+(state.mode==='split'?'before and new split comparison':state.mode==='after'?'new pixel art':'original effects')+', '+z+' times native pixel zoom');
}
function resize(){const width=Math.max(200,Math.floor($('stage').clientWidth-32));canvas.width=Math.min(1100,width);canvas.height=width<430?330:400;canvas.style.width=canvas.width+'px';canvas.style.height=canvas.height+'px';draw()}
function choose(index){state.index=(index+scenes.length)%scenes.length;const s=scenes[state.index];state.focus=[...s.focus];$('scene').value=state.index;$('title').textContent=s.title;$('variant').textContent=s.evolved?'EVOLVED':'BASE WEAPON';$('fixture').textContent='SEED 0xC0FFEE · TICK '+s.ticks+' · 60 Hz';$('simulation').textContent=s.identical?'✓ Simulation identical':'Recorded native fixture';$('changes').textContent=s.pixels.toLocaleString()+' updated pixels';$('detail').textContent=s.detail;draw()}
$('scene').addEventListener('change',e=>choose(Number(e.target.value)));$('previous').onclick=()=>choose(state.index-1);$('next').onclick=()=>choose(state.index+1);
document.querySelectorAll('[data-mode]').forEach(button=>button.onclick=()=>{state.mode=button.dataset.mode;document.querySelectorAll('[data-mode]').forEach(b=>b.setAttribute('aria-pressed',String(b===button)));draw()});
document.querySelectorAll('[data-zoom]').forEach(button=>button.onclick=()=>{state.zoom=Number(button.dataset.zoom);document.querySelectorAll('[data-zoom]').forEach(b=>b.setAttribute('aria-pressed',String(b===button)));draw()});
$('slider').oninput=e=>{state.split=Number(e.target.value)/100;draw()};
let dragging=null;
canvas.addEventListener('pointerdown',e=>{const rect=canvas.getBoundingClientRect(),x=e.clientX-rect.left;dragging={x:e.clientX,y:e.clientY,wipe:state.mode==='split'&&Math.abs(x-canvas.width*state.split)<18};canvas.setPointerCapture(e.pointerId)});
canvas.addEventListener('pointermove',e=>{if(!dragging)return;if(dragging.wipe){const rect=canvas.getBoundingClientRect();state.split=Math.max(0,Math.min(1,(e.clientX-rect.left)/canvas.width));$('slider').value=Math.round(state.split*100)}else{state.focus[0]=Math.max(0,Math.min(800,state.focus[0]-(e.clientX-dragging.x)/state.zoom));state.focus[1]=Math.max(0,Math.min(500,state.focus[1]-(e.clientY-dragging.y)/state.zoom))}dragging.x=e.clientX;dragging.y=e.clientY;draw()});
for(const event of ['pointerup','pointercancel','lostpointercapture'])canvas.addEventListener(event,()=>dragging=null);
canvas.ondblclick=()=>{state.focus=[...scenes[state.index].focus];draw()};
new ResizeObserver(resize).observe($('stage'));
Promise.all([...document.querySelectorAll('img')].map(img=>img.decode())).then(()=>{state.ready=true;choose(state.index);resize();console.log('Effect comparison ready: '+scenes.length+' matched native pairs; integer drawing; smoothing='+ctx.imageSmoothingEnabled)}).catch(error=>{$('error').hidden=false;$('error').textContent='Unable to load native captures: '+error.message;console.error(error)});
choose(state.index);resize();
</script></body></html>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--references", action="store_true",
                        help="Use absolute image references for T3's HTML tools")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = args.output or ROOT / "artifacts/effects-overhaul" / (
        "comparison.references.html" if args.references else "comparison.html")
    output.parent.mkdir(parents=True, exist_ok=True)
    html = build_html(ROOT, embed=not args.references)
    output.write_text(html, encoding="utf-8")
    print(f"{output.resolve()} ({len(html.encode('utf-8')):,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
