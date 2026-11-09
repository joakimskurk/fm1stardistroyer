// SPDX-License-Identifier: GPL-3.0-only
// The page's player (web/app/): the firmware as WebAssembly, run here as the page's audio thread runs it. It boots
// (the splash), a key plays, a press and release in one quantum still count, SCL held is held, a button turns a
// page, an encoder changes the tempo, SAVE writes the store, a stored store comes back after a restart. Writes the
// screens it saw as PPM (OUT-*.ppm) to look at.
//   node tests/web_test.mjs build/web/stardistroyer.wasm [OUT]
import { readFileSync, writeFileSync } from 'node:fs';

const [wasmPath, out = 'build/web/web_test'] = process.argv.slice(2);
if (!wasmPath) { console.error('usage: node tests/web_test.mjs WASM [OUT]'); process.exit(2); }
const bytes = readFileSync(wasmPath);
const BTN = { OCT_DOWN: 0, OCT_UP: 1, FX: 2, SEL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, SAVE: 9, ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };
const ENC = { SELECT: 0, ALGORITHM: 1, K1: 2, K2: 3, K3: 4, K4: 5, PRESETS: 6 };
let fails = 0;
const check = (ok, what) => { console.log(`${ok ? 'ok  ' : 'FAIL'}  ${what}`); if (!ok) fails++; };

async function boot(store) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const x = instance.exports;
  if (store) new Uint8Array(x.memory.buffer, x.web_store(), x.web_store_size()).set(store);
  x.web_init();
  return x;
}
// n ms of sound (in 128-frame quanta, as a worklet asks): its RMS and peak
function run(x, ms) {
  let sum = 0, peak = 0, n = 0;
  for (let f = 0; f < ms * 44.1; f += 128) {
    x.web_run(128);
    const o = new Int32Array(x.memory.buffer, x.web_out_ptr(), 256);
    for (let i = 0; i < 256; i++) { sum += o[i] * o[i]; peak = Math.max(peak, Math.abs(o[i])); n++; }
  }
  return { rms: Math.sqrt(sum / n), peak };
}
const lcd = (x) => new Uint16Array(x.memory.buffer, x.web_lcd(), 240 * 240).slice();
function ppm(x, name) {
  const p = lcd(x), b = Buffer.alloc(15 + 240 * 240 * 3);
  b.write('P6 240 240 255\n');
  for (let i = 0; i < 57600; i++) {
    const c = p[i];
    b[15 + 3 * i] = ((c >> 11) * 255 / 31) | 0; b[16 + 3 * i] = (((c >> 5) & 63) * 255 / 63) | 0; b[17 + 3 * i] = ((c & 31) * 255 / 31) | 0;
  }
  writeFileSync(`${out}-${name}.ppm`, b);
}
const differ = (a, b) => { let d = 0; for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) d++; return d; };
const tap = (x, id) => { x.web_btn(id, 1); x.web_btn(id, 0); };   // down and up before the next quantum

const x = await boot();
run(x, 400);
ppm(x, 'splash');
const splash = lcd(x);
check(splash.some((c) => c !== splash[0]), 'the splash draws');
const quiet = run(x, 1800);
const home = lcd(x);
ppm(x, 'home');
check(differ(splash, home) > 5000, `after the splash, the main screen (${differ(splash, home)} pixels changed)`);
check(quiet.peak < 200, `silent with nothing played (peak ${quiet.peak})`);

x.web_key(7, 1);                                   // C4
const note = run(x, 800);
x.web_key(7, 0);
run(x, 1500);
check(note.rms > 300, `a key plays (RMS ${note.rms.toFixed(0)}, peak ${note.peak})`);
check(((x.web_leds(0) >>> 14) & 0x7FFFFFF) === 0, 'its key light is off after it');

x.web_key(9, 1); x.web_key(9, 0);                  // a press and release in one quantum
const blip = run(x, 300);
check(blip.peak > 300, `a tap shorter than a pass plays (peak ${blip.peak})`);
run(x, 1500);

const before = lcd(x);
tap(x, BTN.FX);
run(x, 200);
ppm(x, 'reactor');
check(differ(before, lcd(x)) > 2000, 'FX turns the page (REACTOR)');
check(((x.web_leds(0) >>> BTN.FX) & 1) === 1, 'FX lit there');

x.web_btn(BTN.SEL, 1);                             // SCL held: the second layer (the names red)
run(x, 200);
const held = lcd(x);
ppm(x, 'reactor-scl');
x.web_btn(BTN.SEL, 0);
run(x, 200);
check(differ(held, lcd(x)) > 50, 'SCL held shows the second layer, let go the first');

tap(x, BTN.HOME);
run(x, 200);
const bpm0 = lcd(x);
x.web_enc(ENC.SELECT, 7);                          // the tempo, 120 -> 127
run(x, 200);
check(differ(bpm0, lcd(x)) > 20, 'SELECT turns the tempo');

const rev0 = x.web_store_rev();
tap(x, BTN.SAVE); run(x, 300); tap(x, BTN.SAVE); run(x, 4000);   // SAVE, SAVE: the patch to a place of its own
check(x.web_store_rev() !== rev0, `SAVE writes the store (rev ${rev0} -> ${x.web_store_rev()})`);
ppm(x, 'saved');
const store = new Uint8Array(x.memory.buffer, x.web_store(), x.web_store_size()).slice();

const y = await boot(store);                       // a reload with the store put back
run(y, 2500);
const set = new Uint8Array(y.memory.buffer, y.web_store(), 8);
check(set[0] === 1, 'after a restart the settings are there');
ppm(y, 'restart');

console.log(fails ? `web_test: ${fails} FAILED` : `web_test: all passed (${bytes.length} B of WebAssembly)`);
process.exit(fails ? 1 : 0);
