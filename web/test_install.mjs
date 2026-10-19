// SPDX-License-Identifier: GPL-3.0-only
// The installer (install.html: fm1pkg.js, fm1ota.js) against a simulated FM-1 on WebMIDI, with the real package:
// its identity and image as tools/fm1_install.py reads them, a whole install (running DAKHMEH -> Felucca's loader ->
// DAKHMEH, every byte the device asks for checked), resuming a cut-off install, refusing other loaders, unplugging.
// After SARDAB's web/test_web.mjs (FakeFM1). Run after tools/build.py --release $(cat VERSION):
//   node web/test_install.mjs [build/dakhmeh-0.3.fwsc]
import { readFileSync, existsSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { productOf, logicalImage } from "./fm1pkg.js";
import { Updater, pack7, unpack7, SARDAB_LOADER, STOCK_LOADER } from "./fm1ota.js";

const version = readFileSync(new URL("../VERSION", import.meta.url), "utf8").trim();
const pkgPath = process.argv[2] || `build/dakhmeh-${version}.fwsc`;
let fails = 0;
const ok = (c, what) => { console.log(`  ${c ? "ok  " : "FAIL"}  ${what}`); if (!c) fails++; };
const eq = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
if (!existsSync(pkgPath)) { console.log(`test_install: ${pkgPath} missing (tools/build.py --release ${version})`); process.exit(1); }
const raw = new Uint8Array(readFileSync(pkgPath));

/* ---- the package: as the command-line installer reads it */
const py = execFileSync("python3", ["-c", `import sys; sys.path.insert(0, "tools"); import fm1_install as f
p, img = f.load_package(sys.argv[1], False); sys.stdout.buffer.write(p.encode() + b"\\n" + img)`, pkgPath]);
const nl = py.indexOf(10), pyProduct = py.subarray(0, nl).toString(), pyImage = new Uint8Array(py.subarray(nl + 1));
const image = logicalImage(raw);
ok(productOf(raw) === "FM-1_966" && pyProduct === "FM-1_966", `package identity FM-1_966 (JS and Python)`);
ok(eq(image, pyImage), `logical image: JS == tools/fm1_install.py (${image.length} B)`);

/* ---- an FM-1: identity, then "device asks, host answers" reads */
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
class FakeFM1 {
  constructor(img, { running = "FM-1_966", loader = SARDAB_LOADER, after = "FM-1_966", unplugAfter = Infinity } = {}) {
    this.image = img; this.served = 0; this.bad = 0; this.bytes = 0; this.unplugAfter = unplugAfter;
    this.loaderId = loader; this.after = after; this.written = new Uint8Array(img.length);
    this.access = { inputs: new Map(), outputs: new Map() };
    this.boot(running, running.startsWith("ota-") ? "FM-1 Update" : "FM-1");
  }
  boot(identity, name) {
    this.identity = identity; this.waiting = null; this.queue = [];
    for (const m of [this.access.inputs, this.access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    const id = Math.random().toString(36).slice(2);
    this.input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {} };
    this.output = { id: "o" + id, name, state: "connected", open: async () => {}, send: (d) => {
      if (this.output.state !== "connected") throw new Error("InvalidStateError");
      setTimeout(() => this.rx(Array.from(d)), 0);
    } };
    this.access.inputs.set(this.input.id, this.input);
    this.access.outputs.set(this.output.id, this.output);
  }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 0); }
  rx(d) {
    if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(this.identity)];
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      if (this.identity.startsWith("ota-")) {   /* the loader: the whole image, 512 B a request, then done */
        this.queue = [];
        for (let a = 0; a < this.image.length; a += 512) this.queue.push([a, Math.min(512, this.image.length - a)]);
        this.queue.push([0xF0000000, 8]);
      } else this.queue = [[0, 64], [0x40, 160], [0x1000, 512], [0xE0000000, 8]];   /* the running firmware: a check */
      this.next();
    } else if (this.waiting) {
      const u = unpack7(d.slice(1, -1)), [addr, len] = this.waiting;
      const got = u.slice(14, 14 + (addr >= 0xE0000000 ? 8 : len));
      const want = addr >= 0xE0000000 ? [...new TextEncoder().encode("success"), 0] : Array.from(this.image.subarray(addr, addr + len));
      if (!eq(got, want)) this.bad++;
      else if (addr < 0xE0000000 && this.identity.startsWith("ota-")) { this.written.set(got, addr); this.bytes += len; }
      this.waiting = null;
      if (++this.served >= this.unplugAfter) { this.input.state = this.output.state = "disconnected"; return; }
      if (addr === 0xE0000000) setTimeout(() => this.boot(this.loaderId, "FM-1 Update"), 50);
      else if (addr === 0xF0000000) setTimeout(() => this.boot(this.after, "FM-1"), 50);
      else this.next();
    }
  }
  next() {
    const r = this.queue.shift();
    if (!r) return;
    this.waiting = r;
    const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0;
    for (let i = 6; i < 14; i++) s += u[i];
    u.push(~s & 0xFF);
    this.tx([0xF0, ...pack7(u), 0xF7]);
  }
}

/* (the 10 ms between answers: real; the test waits ~15 s for the whole image) */
{
  const dev = new FakeFM1(image), steps = [];
  const got = await new Updater(dev.access).install(image, "FM-1_966", (k) => steps.push(k), SARDAB_LOADER);
  ok(got === "FM-1_966" && dev.bad === 0 && dev.bytes === image.length && eq(dev.written, image) && steps.at(-1) === "done",
     `install: DAKHMEH -> loader ${SARDAB_LOADER} -> FM-1_966, all ${image.length} B written as the package (${dev.served} requests)`);
}
{
  const dev = new FakeFM1(image, { running: "FM-1_015" });   /* (from the official firmware: its identity, model FM-1) */
  const got = await new Updater(dev.access).install(image, "FM-1_966", null, SARDAB_LOADER);
  ok(got === "FM-1_966" && dev.bad === 0, "install from the official firmware");
}
{
  const dev = new FakeFM1(image, { running: SARDAB_LOADER });   /* an install cut off: the loader waits */
  const done = await new Updater(dev.access).resume(image, null, "FM-1_966", 0, SARDAB_LOADER);
  ok(done === true && dev.bad === 0 && eq(dev.written, image), "resume: the loader found, the write finished");
}
for (const id of [STOCK_LOADER, "ota-FM-1_123"]) {
  const dev = new FakeFM1(image, { running: id });
  const done = await new Updater(dev.access).resume(image, null, "FM-1_966", 0, SARDAB_LOADER);
  ok(done === false && dev.served === 0, `another loader (${id}): nothing sent`);
}
{
  const dev = new FakeFM1(image, { loader: STOCK_LOADER });
  const e = await new Updater(dev.access).install(image, "FM-1_966", null, SARDAB_LOADER).then(() => null, (x) => x);
  ok(e?.code === "wrongloader" && e.detail === STOCK_LOADER && dev.bytes === 0, "install refuses the official loader before writing");
}
{
  const dev = new FakeFM1(image, { after: "FM-1_900" });
  const e = await new Updater(dev.access).install(image, "FM-1_966", null, SARDAB_LOADER).then(() => null, (x) => x);
  ok(e?.code === "mismatch" && e.detail === "FM-1_900", "another identity after the install: reported");
}
{
  const dev = new FakeFM1(image, { unplugAfter: 2 });
  const e = await new Updater(dev.access).install(image, "FM-1_966", null, SARDAB_LOADER).then(() => null, (x) => x);
  ok(e?.code === "lost", "unplugged in step 1: 'lost', nothing written");
  const e2 = await new Updater({ inputs: new Map(), outputs: new Map() }).install(image, "FM-1_966").then(() => null, (x) => x);
  ok(e2?.code === "notfound", "no FM-1: 'notfound'");
}
/* ---- the page's script parses (its imports are the modules above) */
{
  const html = readFileSync(new URL("./install.html", import.meta.url), "utf8");
  const js = /<script type="module">([\s\S]*?)<\/script>/.exec(html)[1].replace(/^import .*$/gm, "");
  let parsed = true;
  try { new Function(`return async () => {${js}}`); } catch (e) { parsed = false; console.log(e); }
  ok(parsed && html.includes("firmware/latest.json"), "install.html: its script parses; it reads firmware/latest.json");
}
console.log(fails ? `test_install: ${fails} failed` : "test_install: all ok");
process.exit(fails ? 1 : 0);
