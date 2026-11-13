// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joakim (joakimskurk)
// The lab's promise (web/lab/): its sound is the firmware's, bit for bit, and what it exports is the patch it played.
//  1. The browser's firmware (build/web/stardistroyer.wasm) plays tests/scripts/lab.txt a millisecond at a time,
//     event for event as tests/host.c plays it on this computer (tests/run_host.sh tests/scripts/lab.txt ->
//     build/host/stardistroyer.wav), and the two sounds must be the same, sample for sample.
//  2. The field table, as the page's worklet (web/app/worklet.js) reads it, covers the patch: every byte a field's
//     but the padding, no two fields on one byte, no name twice in a group.
//  3. A field set through the page is what the engine plays (the sound changes; set back, it is the same again), and
//     a factory patch the lab loads is the one PRESETS + EDIT loads.
//  4. The C the page exports (web/lab/patch.js: toC), compiled against firmware/src/sd_engine.h, is the patch's
//     bytes: every factory patch, and one with every field at a random value of its range. Its JSON loads back
//     exactly.
//  5. The worklet itself, as the browser runs it: a knob on the panel comes back to the page as the patch, a patch
//     from the page is the firmware's, and ● REC's recording is the frames the firmware made, as they left it.
//  6. The sequencer the same way: a pattern from the page is the firmware's, LENGTH runs SEQ's own, a setting lands,
//     and the frames carry the transport, TURING and the looper.
//   node tests/lab_test.mjs build/web/stardistroyer.wasm build/host/stardistroyer.wav
import { readFileSync, writeFileSync, mkdtempSync, rmSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { get, set, toC, toJSON, fromJSON, GROUPS } from '../web/lab/patch.js';

const [wasmPath, wavPath] = process.argv.slice(2);
if (!wasmPath || !wavPath) { console.error('usage: node tests/lab_test.mjs WASM HOST.wav'); process.exit(2); }
const wasm = readFileSync(wasmPath);
const here = (p) => fileURLToPath(new URL(p, import.meta.url));
let fails = 0;
const check = (ok, what) => { if (!ok) { console.log('FAIL ' + what); fails++; } };

async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const x = instance.exports;
  x.web_init();
  return x;
}
const str = (x, p) => { const m = new Uint8Array(x.memory.buffer); let s = ''; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
const patOf = (x) => new Uint8Array(x.memory.buffer, x.web_pat(), x.web_pat_size()).slice();
// the script, as tests/host.c reads it: keys (a mask), btn (a tap), enc (detents), end
function script(path) {
  const ev = [];
  let end = 6500;
  for (const line of readFileSync(path, 'utf8').split('\n')) {
    const m = line.trim().split(/\s+/);
    if (!m[0] || m[0].startsWith('#')) continue;
    const t = +m[0];
    if (m[1] === 'end') end = t;
    else if (m[1] === 'keys') ev.push({ t, keys: Number(m[2]) });
    else if (m[1] === 'btn') ev.push({ t, btn: +m[2] });
    else if (m[1] === 'enc') ev.push({ t, enc: +m[2], n: +m[3] });
  }
  return { ev: ev.sort((a, b) => a.t - b.t), end };
}
function take(x, out) {                          // the frames the passes left, clamped as the host's WAV is
  let n = x.web_avail();
  while (n > 0) {
    const k = Math.min(n, 1024);
    x.web_run(k);
    const o = new Int32Array(x.memory.buffer, x.web_out_ptr(), 2 * k);
    for (let i = 0; i < 2 * k; i++) out.push(Math.max(-32767, Math.min(32767, o[i])));
    n -= k;
  }
}
async function play(sc, poke) {                  // the WebAssembly on the host's time: an event before its pass
  const x = await boot();
  const out = [];
  let keys = 0, i = 0;
  take(x, out);
  while (x.web_ms() < sc.end) {
    const t = x.web_ms() + 1;                    // the pass about to run
    for (; i < sc.ev.length && sc.ev[i].t <= t; i++) {
      const e = sc.ev[i];
      if (e.keys !== undefined) {
        for (let k = 0; k < 27; k++) {
          const b = 1 << k;
          if ((e.keys & b) !== (keys & b)) x.web_key(k, e.keys & b ? 1 : 0);
        }
        keys = e.keys;
      } else if (e.btn !== undefined) { x.web_btn(e.btn, 1); x.web_btn(e.btn, 0); }
      else if (e.enc !== undefined) x.web_enc(e.enc, e.n);
    }
    if (poke) poke(x, t);
    x.web_pass();
    take(x, out);
  }
  return { x, out };
}
function wav(path) {                             // the host's: 16-bit stereo, its data chunk
  const b = readFileSync(path);
  let p = 12;
  while (p < b.length) {
    const id = b.toString('ascii', p, p + 4), n = b.readUInt32LE(p + 4);
    if (id === 'data') return new Int16Array(b.buffer.slice(b.byteOffset + p + 8, b.byteOffset + p + 8 + n));
    p += 8 + n;
  }
  throw new Error(path + ': no data');
}

// the worklet as the browser runs it: its globals, a port that keeps what it posts
const posted = [];
globalThis.sampleRate = 44100;
globalThis.AudioWorkletProcessor = class { constructor() { this.port = { postMessage: (m) => posted.push(m) }; } };
let Proc = null;
globalThis.registerProcessor = (_name, c) => { Proc = c; };
await import(new URL('../web/app/worklet.js', import.meta.url));
const wk = new Proc();
await wk.message({ type: 'boot', wasm });
const said = (type) => posted.filter((m) => m.type === type);
wk.message({ type: 'lab-init' });
const init = said('lab-init')[0];
check(init && init.fields.length > 200 && init.patches.length > 20, 'the worklet gave the page no field table');
const fields = init.fields, size = init.bytes.length;

// 1. the browser's firmware against the host's
const sc = script(here('./scripts/lab.txt'));
const host = wav(wavPath);
const { x, out } = await play(sc);
{
  const n = Math.min(host.length, out.length);
  let first = -1, diff = 0, loud = 0;
  for (let i = 0; i < n; i++) {
    if (out[i] !== host[i]) { diff++; if (first < 0) first = i; }
    if (Math.abs(host[i]) > loud) loud = Math.abs(host[i]);
  }
  console.log(`lab: the browser's firmware against the host's, ${(n / 2 / 44100).toFixed(2)} s (the script's keys, PLAY, ` +
              `PRESETS, a knob; its peak ${loud}): ${diff ? diff + ' samples differ, the first at frame ' + (first >> 1) :
              'the same, sample for sample'}${host.length !== out.length ? ` (lengths ${host.length / 2} / ${out.length / 2})` : ''}`);
  check(diff === 0 && loud > 1000, `the browser's sound is not the host's (${diff} samples differ; peak ${loud})`);
  check(Math.abs(host.length - out.length) <= 2 * 256, `the lengths differ: ${host.length / 2} against ${out.length / 2}`);
}

// 2. the field table: inside the patch, every byte a field's but the padding (SdOscPatch's pad byte, the compiler's)
{
  const cov = new Uint8Array(size), names = new Set();
  let bad = 0, dup = 0, nopath = 0;
  for (const f of fields) {
    const w = f.type >= 3 ? 2 : 1;
    if (f.off + w > size || f.lo > f.hi) bad++;
    for (let k = 0; k < w; k++) cov[f.off + k]++;
    if (names.has(f.group + f.name)) dup++;
    names.add(f.group + f.name);
    if (!/^\.[a-z_]/.test(f.path) || f.group >= GROUPS.length) nopath++;
  }
  const over = cov.filter((c) => c > 1).length, none = cov.reduce((a, c, i) => (c ? a : a.concat(i)), []);
  check(x.web_nfields() === fields.length && x.web_pat_size() === size, 'the worklet read another table than the firmware has');
  console.log(`lab: ${fields.length} fields over the patch's ${size} bytes (a field ${x.web_field_size()} B in the table): ` +
              `${none.length} held by none (the oscillators' pads), ${over} by two, ${dup} names twice in a group, ` +
              `${bad} past its end, ${nopath} without a C path or a group`);
  check(bad === 0 && over === 0 && dup === 0 && nopath === 0, 'the field table overlaps, repeats a name, or runs past the patch');
  check(none.length <= 24, `${none.length} bytes of the patch have no field: ${none.join(' ')}`);
  // each field's range holds what the factory patches hold (the page clamps an edit to it: a range too narrow would
  // cut a patch's value the first time it is touched). A 0 under a range that starts above it is a field the patch
  // leaves unset, which the engine reads as the range's floor (a Reactor pitch or a split at SD_R_LO, a cell's 1 ms)
  const xx = await boot(), out = [];
  for (const p of init.patches) {
    xx.web_pat_load(p.i);
    const b = patOf(xx);
    for (const f of fields) {
      const v = get(b, f);
      if ((v < f.lo && !(v === 0 && f.lo > 0)) || v > f.hi) out.push(`${p.name} ${f.path} = ${v} (${f.lo}..${f.hi})`);
    }
  }
  console.log(`lab: the ${init.patches.length} factory patches inside every field's range: ${out.length ? out.length + ' outside, ' + out.slice(0, 3).join('; ') : 'all'}`);
  check(out.length === 0, `factory values outside the table's ranges: ${out.slice(0, 6).join('; ')}`);
}

// 3. a field set through the page is what the engine plays; set back, the sound is the same again
{
  const xm = fields.find((f) => f.path === '.xm');
  const x0 = await boot();
  const base = get(patOf(x0), xm);
  const poke = (xx, t) => {
    const p = new DataView(xx.memory.buffer, xx.web_pat(), xx.web_pat_size());
    if (t === 2000) { p.setUint16(xm.off, 60000, true); xx.web_pat_apply(); }
    if (t === 2300) { p.setUint16(xm.off, base, true); xx.web_pat_apply(); }
  };
  const { out: o2 } = await play(sc, poke);
  const a0 = Math.round(2000 * 44.1) * 2, a1 = Math.round(2300 * 44.1) * 2;
  let d1 = 0;
  for (let i = a0; i < a1; i++) if (o2[i] !== out[i]) d1++;
  console.log(`lab: XM set through the page from ${base} to 60000 for 0.3 s: ${d1} of ${a1 - a0} samples different there`);
  check(d1 > (a1 - a0) / 4, 'a field set through the page does not change the sound');
  // a factory patch from the lab is PRESETS + EDIT's (VOWEL: the next after DISTROY, as the script loads it)
  const xx = await boot();
  xx.web_pat_load(3);
  check(xx.web_pat_now() === 3 && str(xx, xx.web_pat_name(3)) === 'VOWEL', `web_pat_load(3): ${str(xx, xx.web_pat_name(xx.web_pat_now()))}`);
}

// 4. the C the page exports, compiled: the patch's bytes, every factory patch and one with every field set at random
{
  const xx = await boot();
  const pats = [];
  for (const p of init.patches) { xx.web_pat_load(p.i); pats.push({ name: p.name, bytes: patOf(xx) }); }
  let seed = 0x5d1ab;
  const rnd = () => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return seed / 4294967296; };
  const wild = pats[3].bytes.slice();
  for (const f of fields) set(wild, f, f.lo + Math.floor(rnd() * (f.hi - f.lo + 1)));
  pats.push({ name: 'WILD', bytes: wild });
  const dir = mkdtempSync(join(tmpdir(), 'sd-lab-'));
  try {
    const src = join(dir, 'lab_c.c'), bin = join(dir, 'lab_c'), dat = join(dir, 'lab_c.bin');
    writeFileSync(src, '#include <stdio.h>\n#include "sd_engine.h"\nstatic const SdPatch T[] = {\n' +
      pats.map((p) => toC(p.bytes, fields, p.name, p.name)).join('') +
      '};\nint main(int argc, char **argv) {\n  FILE *f = fopen(argv[1], "wb");\n  (void)argc;\n' +
      '  fwrite(T, sizeof T, 1, f);\n  fclose(f);\n  printf("%u\\n", (unsigned)sizeof(SdPatch));\n  return 0;\n}\n');
    execFileSync('cc', ['-std=c11', '-O0', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-parameter',
                        '-Wno-unused-const-variable', '-Wno-unused-variable', '-I' + here('../firmware/src'), '-o', bin, src, '-lm'],
                 { stdio: ['ignore', 'pipe', 'inherit'] });
    const hostSize = Number(execFileSync(bin, [dat]).toString().trim()), got = readFileSync(dat);
    check(hostSize === size, `SdPatch is ${hostSize} B on this computer and ${size} B in the browser`);
    let bad = 0, firstBad = '';
    pats.forEach((p, k) => {
      for (let i = 0; i < size; i++) {
        if (got[k * hostSize + i] !== p.bytes[i]) {
          bad++;
          if (!firstBad) firstBad = `${p.name} byte ${i} (${(fields.find((f) => f.off === i || f.off + 1 === i) || {}).path || 'a pad'})`;
        }
      }
    });
    console.log(`lab: the page's C for ${pats.length} patches (the ${pats.length - 1} factory ones, and one with all ` +
                `${fields.length} fields at random) compiled against sd_engine.h: ${bad ? bad + ' bytes differ, the first ' + firstBad : 'the same bytes'}`);
    check(bad === 0, `the C the page exports is not the patch (${bad} bytes; ${firstBad})`);
  } finally { rmSync(dir, { recursive: true, force: true }); }
  const back = fromJSON(toJSON(wild, fields, 'WILD', 'VOWEL'), size);
  check(back.bytes.every((v, i) => v === wild[i]) && back.name === 'WILD', 'the page\'s JSON does not load back the same patch');
}

// 5. the worklet: a knob comes back as the patch, a patch from the page is the firmware's, ● REC is its frames
{
  const L = new Float32Array(128), R = new Float32Array(128), outL = [], outR = [];
  const quanta = (n) => { for (let i = 0; i < n; i++) { wk.process([], [[L, R]]); outL.push(...L); outR.push(...R); } };
  quanta(700);                                  // past the boot screen (2 s), as the script's first key
  wk.message({ type: 'lab-rec', on: true });
  const r0 = outL.length;
  wk.message({ type: 'key', k: 7, down: true });
  quanta(150);
  const before = posted.length;
  wk.message({ type: 'enc', id: 2, n: 6 });     // K1 on the Q page: XM
  quanta(60);
  const pat = posted.slice(before).filter((m) => m.type === 'frame' && m.pat).at(-1);
  const xm = fields.find((f) => f.path === '.xm');
  check(pat && get(pat.pat, xm) !== get(init.bytes, xm),
        'a knob turned on the panel did not come back to the page as the patch');
  const mine = init.bytes.slice();
  set(mine, xm, 12345);
  wk.message({ type: 'lab-set', bytes: mine });
  check(get(wk.pat(), xm) === 12345, 'a patch from the page is not the firmware\'s');
  wk.message({ type: 'key', k: 7, down: false });
  quanta(100);
  wk.message({ type: 'lab-rec', on: false });
  const rec = said('lab-rec')[0].frames, n = outL.length - r0;
  let diff = 0, peak = 0;
  for (let i = 0; i < n; i++) {
    const a = Math.max(-32767, Math.min(32767, Math.round(outL[r0 + i] * 32768)));
    const b = Math.max(-32767, Math.min(32767, Math.round(outR[r0 + i] * 32768)));
    if (rec[2 * i] !== a || rec[2 * i + 1] !== b) diff++;
    peak = Math.max(peak, Math.abs(rec[2 * i]));
  }
  console.log(`lab: the worklet: K1 came back as XM ${pat ? get(pat.pat, xm) : '?'} (from ${get(init.bytes, xm)}); ● REC kept ` +
              `${rec.length / 2} frames of the ${n} it played (${(n / 44100).toFixed(2)} s, peak ${peak}): ` +
              `${diff ? diff + ' differ' : 'the same frames'}`);
  check(rec.length === 2 * n && diff === 0 && peak > 1000, `● REC's recording is not what the firmware played (${rec.length / 2} of ${n}, ${diff} differ, peak ${peak})`);
}
// 6. the sequencer through the worklet: a pattern from the page is the firmware's, LENGTH runs SEQ's own (the
// segments scaled), a setting is set as its button sets it, and the frames carry the transport, TURING and the looper
{
  const L = init.layout, at = (n) => L[['stepSize', 'note', 'vel', 'gate', 'type', 'ratchet', 'prob', 'chord', 'step', 'len',
    'div', 'swing', 'segCount', 'segSplit'].indexOf(n)];
  const x = wk.x, seqOf = (k) => new Uint8Array(x.memory.buffer, x.web_seq(k), x.web_seq_size()).slice();
  check(L && L.length === 29 && init.seq && init.seq.a.length === x.web_seq_size(), 'the worklet gave the page no sequencer');
  const a = init.seq.a.slice(), o = at('step') + 3 * at('stepSize');
  a[o + at('note')] = 64; a[o + at('type')] = 0;
  wk.message({ type: 'lab-seq-set', k: 0, bytes: a });
  const got = seqOf(0);
  check(got[o + at('note')] === 64 && got.every((v, i) => v === a[i]), 'a pattern from the page is not the firmware\'s');
  wk.message({ type: 'lab-seq-op', k: 0, op: 1, a: 24 });
  const g2 = seqOf(0), sc = g2[at('segCount')], last = sc > 1 ? g2[at('segSplit') + sc - 2] : 0;
  check(g2[at('len')] === 24 && last > 12 && last < 24, `LENGTH 24 through SEQ's own: len ${g2[at('len')]}, the last segment from ${last}`);
  wk.message({ type: 'lab-lset', i: 0, v: 100 });
  wk.message({ type: 'lab-lset', i: 15, v: 40000 });   // L-LEVEL: the looper armed
  check(x.web_lset(0) === 100 && x.web_lset(15) === 40000, `the settings: TEMPO ${x.web_lset(0)}, L-LEVEL ${x.web_lset(15)}`);
  const before = posted.length;
  const L2 = new Float32Array(128), R2 = new Float32Array(128);
  for (let i = 0; i < 40; i++) wk.process([], [[L2, R2]]);
  const fr = posted.slice(before).filter((m) => m.type === 'frame' && m.play && m.tur && m.lp);
  check(fr.length > 0 && fr.at(-1).lp.total > 0, `the frames carry no transport, TURING or looper (${fr.length}; the looper's memory ${fr.length ? fr.at(-1).lp.total : '?'})`);
  console.log(`lab: the sequencer through the worklet: a step set from the page is the firmware's; LENGTH 24 scaled the ` +
              `segments (${sc}, the last from step ${last + 1}); TEMPO ${x.web_lset(0)}; the looper armed (${fr.length ? fr.at(-1).lp.total : 0} samples of memory)`);
}
console.log(fails ? `lab_test: ${fails} FAILED` : 'lab_test: all passed');
process.exit(fails ? 1 : 0);
