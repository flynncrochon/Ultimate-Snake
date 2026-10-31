// Browser regression through an isolated Chromium/Edge DevTools endpoint.
// Requires Node 22+, a running web build, and --remote-debugging-port=18766.
import {mkdir,writeFile} from 'node:fs/promises';
import path from 'node:path';
const url=process.argv[2]??'http://127.0.0.1:18765/index.html';
const endpoint=process.argv[3]??'http://127.0.0.1:18766';
const output=path.resolve('artifacts/web-responsive/browser');await mkdir(output,{recursive:true});
const pages=await(await fetch(endpoint+'/json/list')).json();
const page=pages.find(p=>p.type==='page');if(!page)throw Error('No isolated browser page is available.');
const socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject;});
let serial=0;const pending=new Map(),errors=[];
socket.onmessage=event=>{
    const value=JSON.parse(event.data);const request=pending.get(value.id);
    if(request){pending.delete(value.id);clearTimeout(request.timer);value.error?request.reject(Error(JSON.stringify(value.error))):request.resolve(value.result);}
    if(value.method==='Runtime.exceptionThrown')errors.push(value.params.exceptionDetails);
};
function send(method,params={}){return new Promise((resolve,reject)=>{
    const id=++serial,timer=setTimeout(()=>{pending.delete(id);reject(Error(method+' timed out'));},20000);
    pending.set(id,{resolve,reject,timer});socket.send(JSON.stringify({id,method,params}));
});}
async function evaluate(expression,extra={}){
    const r=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true,...extra});
    if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;
}
const frames=(count=2)=>evaluate(`new Promise(resolve=>{let n=${count};function frame(){if(--n<=0)resolve(true);else requestAnimationFrame(frame);}requestAnimationFrame(frame);})`);
async function click(x,y){
    await send('Input.dispatchMouseEvent',{type:'mouseMoved',x,y});
    await send('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',clickCount:1});await frames();
    await send('Input.dispatchMouseEvent',{type:'mouseReleased',x,y,button:'left',clickCount:1});await frames();
}
async function capture(name){const image=await send('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});await writeFile(path.join(output,name+'.png'),Buffer.from(image.data,'base64'));}
function require(value,message){if(!value)throw Error(message);}
try{
    await send('Page.enable');
    await send('Page.addScriptToEvaluateOnNewDocument',{source:`
        let gameModule;Object.defineProperty(window,'Module',{configurable:true,get(){return gameModule;},set(value){value.arguments=['--scene','pause'];gameModule=value;}});
    `});
    await send('Emulation.setDeviceMetricsOverride',{width:1280,height:800,deviceScaleFactor:1,mobile:false});
    await send('Page.navigate',{url});
    await send('Runtime.enable');
    await evaluate(`new Promise((resolve,reject)=>{const until=performance.now()+15000;function ready(){if(window.Module?.calledRun&&document.querySelector('#loading')?.hidden)resolve(true);else if(performance.now()>until)reject(Error('Game startup timed out'));else requestAnimationFrame(ready);}ready();})`);
    const results=[];
    for(const [width,height,dpr] of [[800,500,1],[1280,800,1],[1920,1080,1],[1921,1081,1],[2560,1080,1],[1024,768,2]]){
        await send('Emulation.setDeviceMetricsOverride',{width,height,deviceScaleFactor:dpr,mobile:false});await frames(3);
        const state=await evaluate(`new Promise(resolve=>requestAnimationFrame(()=>{
            const c=document.querySelector('#canvas'),r=c.getBoundingClientRect(),gl=c.getContext('webgl2');
            const n=Math.min(200,c.width),h=30,pixels=new Uint8Array(n*h*4);gl.readPixels(c.width-n,c.height-h,n,h,gl.RGBA,gl.UNSIGNED_BYTE,pixels);
            let white=0;for(let i=0;i<pixels.length;i+=4)if(pixels[i]>70&&pixels[i+1]>70&&pixels[i+2]>70)++white;
            resolve({width:innerWidth,height:innerHeight,canvasWidth:c.width,canvasHeight:c.height,cssWidth:r.width,cssHeight:r.height,left:r.left,top:r.top,viewport:Array.from(gl.getParameter(gl.VIEWPORT)),white,failed:document.querySelector('#status').textContent,filter:getComputedStyle(c).imageRendering});
        }))`);
        require(state.canvasWidth===width&&state.canvasHeight===height,'Canvas missed browser dimensions: '+JSON.stringify(state));
        require(state.cssWidth===width&&state.cssHeight===height&&state.left===0&&state.top===0,'Canvas was CSS-stretched or offset');
        require(state.viewport[2]===width&&state.viewport[3]===height,'WebGL viewport became stale');
        await capture(`viewport-${width}x${height}-dpr${dpr}`);
        require(state.white>10,'HUD did not reach the expanded right edge: '+JSON.stringify(state));
        results.push({width,height,dpr,...state});
    }
    await send('Emulation.setDeviceMetricsOverride',{width:1280,height:800,deviceScaleFactor:1,mobile:false});await frames();
    await click(640,449);await capture('settings-click');
    const settings=await evaluate(`new Promise(resolve=>requestAnimationFrame(()=>{const c=document.querySelector('#canvas'),gl=c.getContext('webgl2'),p=new Uint8Array(4);gl.readPixels(296,c.height-181,1,1,gl.RGBA,gl.UNSIGNED_BYTE,p);resolve(Array.from(p));}))`);
    require(settings[0]===68&&settings[1]===68&&settings[2]===68,'Expanded settings button missed its panel');
    await click(640,597); // Centered Cancel.
    await evaluate('document.body.requestFullscreen().then(()=>true)',{userGesture:true});await frames(3);
    require(await evaluate('Boolean(document.fullscreenElement)'),'Fullscreen did not enter');await capture('fullscreen');
    await evaluate('document.exitFullscreen().then(()=>true)');await frames(3);
    require(!await evaluate('Boolean(document.fullscreenElement)'),'Fullscreen did not exit');
    require(errors.length===0,'Browser runtime exceptions: '+JSON.stringify(errors));
    await writeFile(path.join(output,'results.json'),JSON.stringify({url,results,settings,errors},null,2));
    console.log(JSON.stringify({passed:results.length+3,viewports:results.map(r=>`${r.width}x${r.height}@${r.dpr}`),runtimeErrors:errors.length}));
}finally{socket.close();}
