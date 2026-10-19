import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {defaultTuning,encodeTuning,decodeTuning,scalaTuning,exportTuning,importTuning} from './tuning.js';
import {archiveTuning} from './maqam_archive.js';
import {Client,VirtualFM1,factoryObject,OBJ} from './editor_core.js';
const s=defaultTuning();s.root=7;s.mode=1;s.on[4]=1;s.cents[4]=-37;s.table[4]=-373;s.trim[9]=-13;s.retune=2;
assert.deepEqual(encodeTuning(decodeTuning(encodeTuning(s))),encodeTuning(s));
assert.deepEqual(importTuning(exportTuning(s)),s);
assert.throws(()=>encodeTuning({...s,table:[NaN]}));assert.throws(()=>importTuning('{"version":1}'));
const rast=scalaTuning('Rast\n7\n200.0\n350.0\n500.0\n700.0\n900.0\n1050.0\n2/1',defaultTuning());
assert.equal(rast.state.table[4],-500);assert.equal(rast.state.table[11],-500);assert.equal(rast.state.custom,0xab5);
assert.throws(()=>scalaTuning('bad\n2\n200.0\n3/1')); // no silent non-octave folding
const ar=JSON.parse(readFileSync('web/data/ronzevalle_1904-yegah.json'));
let valid=0,rejected=0;
for(const e of ar.data)for(const v of [null,...(e.transpositions||[])])for(const dir of ['ascending','descending']) {
  try {const t=archiveTuning(ar,e,v,dir);assert(t.maqamIdentity.idName);assert.equal(t.tuningIdentity.idName,'ronzevalle_1904');encodeTuning(t);valid++;}
  catch(e){if(e instanceof assert.AssertionError)throw e;rejected++;}
}
assert(valid>100);
const vf=await VirtualFM1.create(readFileSync('web/dist/grimm.wasm'));vf.warmUp();const cl=new Client(vf),info=await cl.info();
assert.equal(info.proto,3);assert.equal(info.fmt,4);
const initial=await cl.tuning(0);assert.equal(decodeTuning(initial).on.reduce((a,b)=>a+b),0);
assert.deepEqual(await cl.tuning(0,encodeTuning(s)),encodeTuning(s));
assert.deepEqual(await cl.tuning(0),encodeTuning(s));
assert.deepEqual(await cl.tuning(1),initial);
const sound=await cl.get(factoryObject(48),64);await cl.trackSet(0,sound);assert.deepEqual(await cl.tuning(0),encodeTuning(s));
for(let i=0;i<info.factory;i++)assert.equal((await cl.get(factoryObject(i),64))[3],vf.meta().factory[i].rec[3]);
await assert.rejects(()=>cl.put(factoryObject(51),sound,info.fmt));
await assert.rejects(()=>cl.tuning(0,new Uint8Array(2)));
console.log(`TUNING OK: Scala, preset roundtrip, ${valid} archive forms (${rejected} explicitly unsupported), live firmware, track independence, sound retention, all factory IDs`);
