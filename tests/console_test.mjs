// SPDX-License-Identifier: GPL-3.0-only
// The CONSOLE's frames (firmware/src/sd_console.h) as the page reads them (web/console/frame.js): the player's
// firmware (WebAssembly) makes them, frame.js reads every field of every slow part to its end; each fits the FM-1's
// USB queue; what they say follows the firmware (the boot's patch, the tempo, PLAY, SEQ A's steps moving, a key's
// voice, a MOD slot, the sound with the frames on the same as with them off); and frame.js's names are stardistroyer.c's.
//   node tests/console_test.mjs build/web/stardistroyer.wasm
import { readFileSync } from 'node:fs';
import { newState, decode, SRC_NAME, DST_NAME, LSHAPE, SCALE, ROUTE, MODS } from '../web/console/frame.js';

const [wasmPath] = process.argv.slice(2);
if (!wasmPath) { console.error('usage: node tests/console_test.mjs WASM'); process.exit(2); }
const bytes = readFileSync(wasmPath);
let fails = 0;
const check = (ok, what) => { console.log(`${ok ? 'ok  ' : 'FAIL'}  ${what}`); if (!ok) fails++; };
const BTN = { PLAY: 12, SEQ: 11 };

async function boot() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const x = instance.exports;
  x.web_init();
  return x;
}
function run(x, ms, sink) {                      // ms of sound; every 40 ms a frame into sink (if one)
  const out = [];
  for (let f = 0, next = 0; f < ms * 44.1; f += 128) {
    x.web_run(128);
    const o = new Int32Array(x.memory.buffer, x.web_out_ptr(), 256);
    for (let i = 0; i < 256; i++) out.push(o[i]);
    if (sink && f >= next) { next += 40 * 44.1; sink(frame(x)); }
  }
  return out;
}
const frame = (x) => { const n = x.web_con_frame(); return new Uint8Array(x.memory.buffer, x.web_con_ptr(), n).slice(); };

// the names: frame.js's lists as stardistroyer.c has them
{
  const c = readFileSync(new URL('../firmware/src/stardistroyer.c', import.meta.url), 'utf8');
  const arr = (name) => JSON.parse('[' + c.match(new RegExp(`static const char \\*const ${name}\\[[^\\]]*\\]\\s*=\\s*\\{([\\s\\S]*?)\\};`))[1] + ']');
  const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
  check(same(arr('SRC_NAME'), SRC_NAME) && same(arr('DST_NAME'), DST_NAME) && same(arr('LSHAPE_NAME'), LSHAPE) &&
        same(arr('SCALE_NAME'), SCALE) && same(arr('ALG_ROUTE'), ROUTE), 'frame.js names the sources, destinations, shapes, scales and routes as the FM-1 does');
}

const x = await boot();
const st = newState();
let bad = 0, longest = 0, n = 0, read = 0;
const take = (f) => { n++; longest = Math.max(longest, f.length); if (decode(f, st)) { read++; if (st.len !== f.length) bad++; } else bad++; };
run(x, 1000, take);
check(read === n && n >= 20 && !bad, `${n} frames, every one read to its end (${bad} not)`);
check(longest + 6 <= 192, `each fits the FM-1's USB queue (the longest ${longest + 6} bytes with its SysEx, of 192)`);
check(st.ok, 'every slow part came round (the sound, MOD, SEQ A, SEQ B, ORBIT, ARP and the banks)');
check(st.name.length > 0 && st.tempo === 120 && !st.play, `the boot: ${st.name}, ${st.tempo} BPM, stopped`);
x.web_btn(BTN.PLAY, 1); run(x, 30); x.web_btn(BTN.PLAY, 0);
const steps = new Set();
run(x, 2000, (f) => { take(f); steps.add(st.step[0]); });
check(st.play && steps.size >= 8, `PLAY: SEQ A's step moved through ${steps.size} places`);
x.web_btn(BTN.PLAY, 1); run(x, 30); x.web_btn(BTN.PLAY, 0); run(x, 300, take);
x.web_midi(0x90, 60, 100, 0);
run(x, 200, take);
check(st.voices.includes(60), `a key: its voice in the frame (${st.voices.join(' ')})`);
x.web_midi(0x80, 60, 0, 0);
check(st.mod.length === MODS && MODS === 12 && st.banks.length === 4 && st.seq[0].steps.length === 16 && st.orbit.steps.length === 16,
      'MOD\'s twelve slots, the four banks, SEQ A\'s 16 steps and ORBIT\'s 16');
// EDIT: a field written where the lab's table puts it (as the console does, the player's way), the frames show it
{
  const dv = new DataView(x.memory.buffer), m = new Uint8Array(x.memory.buffer), nf = x.web_nfields(), f0 = x.web_fields(), sz = x.web_field_size();
  const str = (p) => { let s = ''; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
  const fl = {};
  for (let i = 0; i < nf; i++) { const b = f0 + i * sz; fl[str(dv.getUint32(b + 20, true))] = { off: dv.getUint16(b, true), type: dv.getUint8(b + 2) }; }
  const poke = (path, v) => { const f = fl[path]; const p = new Uint8Array(x.memory.buffer, x.web_pat(), x.web_pat_size()); p[f.off] = v & 255; if (f.type >= 3) p[f.off + 1] = v >> 8; };
  const need = ['.x', '.y', '.mac[1]', '.d_level[0]', '.r_pos', '.s_rlevel', '.s_dlevel'];
  check(need.every((k) => fl[k]), `the console's fields in the lab's table (${need.filter((k) => !fl[k]).join(' ') || 'all'})`);
  poke('.mac[1]', 65535); poke('.mac[2]', 0); x.web_pat_apply();
  run(x, 300, take);
  check(st.mac[1] >= 126 && st.mac[2] === 0, `EDIT: MOVE written to the top, BREAK to 0, the frames show ${st.mac[1]} and ${st.mac[2]}`);
}
// the sound the same with the frames on as with them off (they only read)
{
  const a = await boot(), b = await boot();
  a.web_midi(0x90, 57, 110, 0); b.web_midi(0x90, 57, 110, 0);
  const sa = run(a, 1500), sb = run(b, 1500, () => {});
  let same = sa.length === sb.length;
  for (let i = 0; same && i < sa.length; i++) same = sa[i] === sb[i];
  check(same, 'the sound sample for sample the same with the frames on');
}
console.log(fails ? `console_test: ${fails} FAILED` : 'console_test: all ok');
process.exit(fails ? 1 : 0);
