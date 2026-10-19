// SPDX-License-Identifier: GPL-3.0-only
// DiArMaqAr data, the same documented API used by Tanghim. Browser-side only.
import {defaultTuning,encodeTuning} from './tuning.js';
export const ARCHIVE='https://diarmaqar.netlify.app/api';
export function archiveTuning(response, entry, variant, direction='ascending', prior=defaultTuning()) {
  if(entry.characteristics?.isOctaveRepeating === false) throw new Error('This maqam is not octave-repeating; the current firmware needs a separate mapping for it.');
  const ds=(variant||entry).maqamDegrees?.[direction];
  if(!Array.isArray(ds)||!ds.length) throw new Error('No documented degrees for this direction.');
  const pitches=ds.map(d=>{
    const m=/^(\d+)\s+([+-]?\d+(?:\.\d+)?)$/.exec(d.midiNotePlusCentsDeviation||'');
    if(!m)throw new Error('The archive did not supply paired MIDI-note and cents data.');
    const dc=Number.isFinite(d.centsDeviation)?Math.round(d.centsDeviation*10):Math.round(Number(m[2])*10);
    return {note:Number(m[1]),dc};
  });
  // Ascending tonic remains the reference even when loading a descending table.
  const tonicDegree=(variant||entry).maqamDegrees?.ascending?.[0];
  const tonic=/^(\d+)\s/.exec(tonicDegree?.midiNotePlusCentsDeviation||'');
  if(!tonic)throw new Error('No tonic reference.');
  const root=+tonic[1]%12, table=Array(12).fill(0), seen=new Map();let mask=0;
  for(const {note,dc} of pitches){
    const pc=(note-root+12)%12;
    if(seen.has(pc)&&Math.abs(seen.get(pc)-dc)>1)throw new Error('Two pitches need the same chromatic key. Choose another documented form.');
    seen.set(pc,dc);table[pc]=dc;mask|=1<<pc;
  }
  const system=response.tuningSystem, start=response.selectedStartingNote, tonicName=(variant||entry).tonic;
  const s={...structuredClone(prior),root,scale:40,custom:mask,mode:0,system:1,temper:0,trim:Array(12).fill(0),table,
    name:`${entry.maqam.displayName} / ${tonicName?.displayName||''} / ${direction}`,
    source:`DiArMaqAr — ${system.displayName}; ${start.displayName}; ${system.version}`,
    maqamIdentity:entry.maqam, tuningIdentity:system, startingNote:start, direction, characteristics:entry.characteristics,
    tonicIdentity:tonicName, degrees:ds};
  encodeTuning(s);return s;
}
export function mountArchive(parent,getState,onChange,onError){
  const el=(tag,text)=>{const e=document.createElement(tag);if(text)e.textContent=text;parent.append(e);return e;};
  el('h3','TANGHIM / DIARMAQAR TUNINGS');
  el('p','Choose a documented system, its starting note, a maqam and its form. Ascending and descending tables are explicit selections; melodic movement is not generated. Imported tuning remains on the device without internet.');
  const sys=el('select'),start=el('select'),load=el('button','Load maqams'),maq=el('select'),form=el('select'),dir=el('select'),apply=el('button','Apply tuning'),status=el('p');
  [sys,start,maq,form,dir].forEach((e,i)=>e.setAttribute('aria-label',['Tuning system','System starting note','Archive maqam','Documented tonic','Melodic direction'][i]));
  const fill=(e,items)=>{e.textContent='';items.forEach((t,i)=>{const o=document.createElement('option');o.value=i;o.textContent=t;e.append(o);});};
  fill(dir,['ascending','descending']); apply.disabled=true;
  let systems=[],data=null,variants=[];
  const chooseForm=()=>{const m=data.data[+maq.value];variants=[null,...(m.transpositions||[])];fill(form,[m.tonic.displayName,...variants.slice(1).map(v=>v.tonic.displayName)]);};
  maq.onchange=chooseForm;
  sys.onchange=()=>{fill(start,systems[+sys.value].startingNotes.displayNames);apply.disabled=true;};start.onchange=()=>apply.disabled=true;
  fetch('data/tuning-systems.json').then(r=>{if(!r.ok)throw new Error('Tuning catalog unavailable');return r.json();}).then(j=>{systems=j.data;fill(sys,systems.map(s=>s.tuningSystem.displayName));sys.onchange();start.value=1;}).catch(onError);
  load.onclick=async()=>{load.disabled=true;apply.disabled=true;try{
    const s=systems[+sys.value],id=s.tuningSystem.idName,st=s.startingNotes.idNames[+start.value];
    const url=id==='ronzevalle_1904'&&st==='yegah'?'data/ronzevalle_1904-yegah.json':`${ARCHIVE}/tuning-systems/${encodeURIComponent(id)}/${encodeURIComponent(st)}/maqamat?includeMaqamDegrees=true&includeTranspositions=true&includeDegreeDetails=true`;
    status.textContent='Loading documented tunings…';
    const r=await fetch(url,{signal:AbortSignal.timeout(30000)});if(!r.ok)throw new Error(`Archive HTTP ${r.status}`);data=await r.json();
    if(!Array.isArray(data.data)||!data.data.length)throw new Error('No maqams available for this system and starting note.');
    fill(maq,data.data.map(m=>m.maqam.displayName));chooseForm();apply.disabled=false;status.textContent=`${data.data.length} maqams available. Export a tuning after applying it to keep the source information.`;
  }catch(e){status.textContent='Could not load this system. The included Ronzevalle / yegah catalog works offline.';onError(e);}finally{load.disabled=false;}};
  apply.onclick=()=>{try{onChange(archiveTuning(data,data.data[+maq.value],variants[+form.value],dir.selectedOptions[0].textContent,getState()));}catch(e){onError(e);}};
}
