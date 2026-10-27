// Run in a browser against an unmodified Snake-Survivors source server.
// collectReference("http://127.0.0.1:8008/") evaluates the original getters;
// constants are exported from temporary in-memory module copies, never edited on disk.
export async function collectReference(baseURL) {
  const modules=[
    ["venom_shot","bullet_manager","BulletManager"],["viper_fangs","fang_barrage","FangBarrage"],
    ["plague_mortar","poison_mortar","PoisonMortar"],["snake_nest","snake_nest","SnakeNest"],
    ["venom_nova","venom_nova","VenomNova"],["sidewinder_beam","sidewinder_beam","SidewinderBeam"],
    ["ricochet_fang","ricochet_fang","RicochetFang"],["cobra_pit","cobra_pit","CobraPit"],
    ["tongue_lash","tongue_lash","TongueLash"],["miasma","miasma","Miasma"],
    ["hydra_brood","hydra_brood","HydraBrood"],["serpent_gatling","serpent_gatling","SerpentGatling"],
    ["consumption_beam","consumption_beam","ConsumptionBeam"],["singularity_mortar","singularity_mortar","SingularityMortar"],
    ["shatter_fang","shatter_fang","ShatterFang"],["ancient_brood_pit","ancient_brood_pit","AncientBroodPit"],
    ["serpents_reckoning","serpents_reckoning","SerpentsReckoning"],["ouroboros","ouroboros","Ouroboros"]
  ];
  const weapons={},constants={};
  for(const [id,file,className] of modules) {
    const url=new URL("src/survivors/"+file+".js",baseURL);
    const original=await fetch(url).then(r=>{if(!r.ok)throw Error(url+": "+r.status);return r.text();});
    const names=Array.from(original.matchAll(/^const ([A-Z][A-Z_0-9]*)\s*=/gm),m=>m[1]);
    const absolute=original.replace(/(from\s*['"])([^'"]+)(['"])/g,(_,a,path,b)=>a+new URL(path,url).href+b);
    const instrumented=absolute+"\nexport const __referenceConstants={"+names.join(",")+"};";
    const blob=URL.createObjectURL(new Blob([instrumented],{type:"text/javascript"}));
    let module;try{module=await import(blob);}finally{URL.revokeObjectURL(blob);}
    constants[id]=Object.fromEntries(Object.entries(module.__referenceConstants).filter(([,v])=>typeof v==="number"));
    weapons[id]=[];
    for(let rank=1;rank<=8;rank++) {
      const instance=new module[className]();instance.level=rank;
      const getters={};
      for(const key of Object.getOwnPropertyNames(module[className].prototype).filter(k=>k.startsWith("get_"))) {
        getters[key]=instance[key]();
      }
      weapons[id].push({rank,getters});
    }
  }
  return {reference_commit:"b6cb72e66eebc1291d57efe63ebbe93ec602e863",
          source:"Original JavaScript classes/getters and constants executed in a browser; no native formulas used.",
          units:{time:"milliseconds for get_cooldown; seconds for duration",distance:"original cells; native simulation cell=8"},
          weapons,constants};
}
