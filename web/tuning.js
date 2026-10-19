// SPDX-License-Identifier: GPL-3.0-only
// Tuning state is independent of sound records. Firmware tables use decicents.
import { parseScala } from './scala.js';
export const NOTES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
export const defaultTuning = () => ({root:0, scale:0, map:0, temper:0, trim:Array(12).fill(0), custom:4095,
  mode:0, system:0, retune:0, table:Array(12).fill(0), on:Array(12).fill(0), cents:Array(12).fill(-50),
  name:'Untitled tuning', source:''});
function range(x, lo, hi) { if (!Number.isInteger(x) || x < lo || x > hi) throw new Error(`Tuning value must be ${lo}…${hi}.`); return x; }
export function encodeTuning(s) {
  const b = new Uint8Array(59), v = new DataView(b.buffer);
  b[0]=range(s.root,0,11); b[1]=range(s.scale,0,49); b[2]=range(s.map,0,4); b[3]=range(s.temper,0,3);
  for (const k of ['trim','table','on','cents']) if (!Array.isArray(s[k]) || s[k].length!==12) throw new Error('Tuning needs twelve pitch classes.');
  s.trim.forEach((x,i)=>v.setInt8(4+i,range(x,-63,63)));
  v.setUint16(16,range(s.custom,0,4095),true);
  b[18]=range(s.mode,0,1); b[19]=range(s.system,0,1); b[20]=range(s.retune,0,2);
  s.table.forEach((x,i)=>v.setInt16(21+2*i,range(x,-2000,2000),true));
  s.cents.forEach((x,i)=>v.setInt8(45+i,range(x,-100,100)));
  s.on.forEach((x,i)=>{b[57+(i>>3)] |= range(x,0,1)<<(i%8);});
  return b;
}
export function decodeTuning(b) {
  if (b.length!==59) throw new Error('Invalid tuning size.');
  const s=defaultTuning(), v=new DataView(b.buffer,b.byteOffset,b.byteLength);
  [s.root,s.scale,s.map,s.temper]=b.slice(0,4);
  s.trim=s.trim.map((_,i)=>v.getInt8(4+i)); s.custom=v.getUint16(16,true);
  [s.mode,s.system,s.retune]=b.slice(18,21);
  s.table=s.table.map((_,i)=>v.getInt16(21+2*i,true)); s.cents=s.cents.map((_,i)=>v.getInt8(45+i));
  s.on=s.on.map((_,i)=>b[57+(i>>3)]>>(i%8)&1);
  encodeTuning(s); return s;
}
export function scalaTuning(text, prior=defaultTuning()) {
  const q=parseScala(text), s={...structuredClone(prior), mode:0, system:1, scale:40, custom:q.mask,
    temper:0, trim:Array(12).fill(0), table:q.cents.map(x=>10*x), name:q.name, source:'Scala import'};
  return {state:s,maxError:q.maxError};
}
export function exportTuning(s) { encodeTuning(s); return JSON.stringify({format:'sardab-tuning',version:1,tuning:s},null,2); }
export function importTuning(text) {
  if (text.length>65536) throw new Error('Tuning file too large.');
  const j=JSON.parse(text);
  if (j.format!=='sardab-tuning'||j.version!==1||!j.tuning) throw new Error('Not a DAKHMEH tuning preset.');
  encodeTuning(j.tuning);
  return {...j.tuning,name:String(j.tuning.name||'Imported tuning').slice(0,120),source:String(j.tuning.source||'').slice(0,500)};
}
export function drawTuning(parent,s,catalog,onChange,onError) {
  const add=(tag,text,host=parent)=>{const e=document.createElement(tag); if(text!==undefined)e.textContent=text;host.append(e);return e;};
  const update=()=>{try{encodeTuning(s);onChange(s);}catch(e){onError(e);}};
  const pick=(label,items,key)=>{const l=add('label',label+' '),e=add('select',undefined,l);items.forEach((n,i)=>{const o=add('option',n,e);o.value=i;});e.value=s[key];e.onchange=()=>{s[key]=+e.value;update();};return e;};
  const num=(host,value,min,max,step,change)=>{const e=add('input',undefined,host);e.type='number';e.min=min;e.max=max;e.step=step;e.value=value;e.style.width='100%';e.onchange=()=>{const v=Number(e.value);if(e.value===''||!Number.isFinite(v)||v<min||v>max){e.value=value;onError(new Error(`Use a value from ${min} to ${max}.`));return;}change(v);update();};return e;};
  add('h3','MAQAM & TUNING');
  pick('Mode',['Maqam tuning','Arabic keyboard / tashriq'],'mode');
  pick('Tonic',NOTES,'root');
  pick('Maqam / scale',catalog.map(x=>x.name),'scale');
  pick('Keys',['Chromatic','Snap','Snap + sequence','White keys','All keys'],'map');
  pick('Tuning table',['Factory approximation','Custom / imported table'],'system');
  pick('Temperament (factory table)',['Equal','Pythagorean','Meantone','Just'],'temper');
  pick('Sounding notes',['Retune immediately','Short glide','Hold until release'],'retune');
  add('p','Tashriq switches use absolute note names and ignore maqam offsets. Maqam tables and trims follow the tonic. Instrument changes retain tuning.');
  const grid=add('div');grid.className='pitch-grid';
  const temper=[[0,0,0,0,0,0,0,0,0,0,0,0],[0,-98,39,-59,78,-20,117,20,-78,59,-39,98],[0,171,-68,103,-137,34,-205,-34,137,-103,68,-171],[0,117,39,156,-137,-20,-98,20,137,-156,-39,-117]];
  const base=s.system?s.table:catalog[s.scale].offsets.map((x,i)=>x*10+temper[s.temper][i]);
  NOTES.forEach((n,i)=>{
    const c=add('div',undefined,grid);c.className='pitch-cell';
    const button=add('button',n+(s.on[i]?' ON':' OFF'),c);button.setAttribute('aria-pressed',String(!!s.on[i]));
    button.style.background=s.on[i]?'var(--accent, #cd976e)':'';
    button.onclick=()=>{s.on[i]^=1;update();};
    add('small',' Tashriq cents ',c);num(c,s.cents[i],-100,100,1,v=>s.cents[i]=Math.round(v));
    const pc=(i-s.root+12)%12;
    add('small',' Maqam table cents ',c);num(c,base[pc]/10,-200,200,0.1,v=>{s.table=[...base];s.table[pc]=Math.round(v*10);s.system=1;});
    add('small',' Additional trim ',c);num(c,s.trim[pc],-63,63,1,v=>s.trim[pc]=Math.round(v));
    const lab=add('label',' Custom scale member ',c),ch=add('input',undefined,lab);ch.type='checkbox';ch.checked=!!(s.custom>>pc&1);
    ch.onchange=()=>{const mask=s.custom^(1<<pc);if(!mask){ch.checked=true;return;}s.custom=mask;update();};
  });
  const p=add('p');p.textContent=`${s.name} — ${s.source || 'Editable tuning; factory maqams are approximations.'}`;
  const name=add('input');name.type='text';name.value=s.name;name.placeholder='Tuning preset name';name.onchange=()=>{s.name=name.value.slice(0,120);update();};
  const save=add('button','Export tuning');save.onclick=()=>{const a=document.createElement('a'),url=URL.createObjectURL(new Blob([exportTuning(s)],{type:'application/json'}));a.href=url;a.download='dakhmeh-tuning.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
  const load=add('input');load.type='file';load.accept='.json,.scl';load.setAttribute('aria-label','Import Scala or DAKHMEH tuning');load.onchange=async()=>{try{const f=load.files[0];if(!f)return;if(f.size>65536)throw new Error('Tuning file too large.');const text=await f.text();const next=f.name.toLowerCase().endsWith('.scl')?scalaTuning(text,s).state:importTuning(text);onChange(next);}catch(e){onError(e);}};
}
