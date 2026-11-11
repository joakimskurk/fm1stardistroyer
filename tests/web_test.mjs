// SPDX-License-Identifier: GPL-3.0-only
// The page's player (web/app/): the firmware as WebAssembly, run here as the page's audio thread runs it. It boots
// (the splash), a key plays, a press and release in one quantum still count, SCL held is held, a button turns a
// page, an encoder changes the tempo, SAVE writes the store, a stored store comes back after a restart, a MIDI clock
// (as the page's MIDI button passes it on) leads the tempo and START plays, and the same clock on two inputs leads
// at its tempo, not twice it; SEQ goes round its own pages, OCT is the octave on SEQ too and SCL + OCT its bank;
// MOD's DEST shows a list as it turns, and REC on ARP keeps what it played; SEQ B and ORBIT switched on while A
// plays wait for A's one, and SCL + SEQ makes B the master, A then waiting for B's one. Writes the screens it saw as PPM
// (OUT-*.ppm) to look at.
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

let clkT = 0, nextP = 0;                           // a MIDI clock at 100 BPM, its pulses between the quanta
function runClock(x, ms, bpm) {
  for (let t = 0; t < ms; t += 128 / 44.1) {
    x.web_run(128);
    clkT += 128 / 44.1;
    while (nextP <= clkT) { x.web_midi(0xF8, 0, 0); nextP += 60000 / bpm / 24; }
  }
}
const stat = (x) => lcd(x).slice(222 * 240);
const st0 = stat(y);
runClock(y, 1500, 100);
const st1 = stat(y);
check(differ(st0, st1) > 20, `a MIDI clock leads: its tempo, CLK, on the status line (${differ(st0, st1)} pixels)`);
y.web_midi(0xFA, 0, 0);
runClock(y, 600, 100);
check(differ(st1, stat(y)) > 10, `MIDI START plays: the status line's square (${differ(st1, stat(y))} pixels)`);
ppm(y, 'clock');

const z = await boot(store);                       // the same clock on two inputs (two ports, a DAW's sync to both)
run(z, 2500);
let zT = 0, zP = 0;
for (let t = 0; t < 1500; t += 128 / 44.1) {
  z.web_run(128);
  zT += 128 / 44.1;
  while (zP <= zT) { z.web_midi(0xF8, 0, 0, 1); z.web_midi(0xF8, 0, 0, 2); zP += 60000 / 100 / 24; }
}
check(differ(st1, stat(z)) === 0,
      `the same clock on two inputs leads at its tempo, not twice it (${differ(st1, stat(z))} pixels off one clock's)`);
ppm(z, 'clock2');

// SEQ goes round its own pages (2026-11-10: the fourth press went back to the page before it, SPACE); OCT is the
// octave on SEQ too, SCL + OCT the step bank
const w = await boot(store);
run(w, 2500);
const top = (x) => lcd(x).slice(0, 240 * 60);       // the title, the patch, the pages' line
for (let i = 0; i < 3; i++) { tap(w, BTN.FX); run(w, 150); }   // REACTOR, DISTROY, SPACE
const space = top(w);
tap(w, BTN.SEQ); run(w, 150);
const seqA = top(w);
for (let i = 0; i < 3; i++) { tap(w, BTN.SEQ); run(w, 150); }  // SEQ B, ORBIT, SEQ A
check(differ(seqA, top(w)) === 0 && differ(space, seqA) > 0,
      `SEQ goes SEQ A, SEQ B, ORBIT, SEQ A (not back to SPACE: ${differ(seqA, top(w))}, ${differ(space, seqA)} pixels)`);
const so = stat(w);
tap(w, BTN.OCT_UP); run(w, 150);
const su = stat(w);
tap(w, BTN.OCT_DOWN); run(w, 150);
check(differ(so, su) > 0 && differ(so, stat(w)) === 0, `OCT on SEQ moves the octave (+1 and back: ${differ(so, su)} pixels)`);
w.web_btn(BTN.SEL, 1); run(w, 150);                 // SCL: LENGTH (K1) to 32, then SCL + OCT+ the second bank
w.web_enc(ENC.K1, 16); run(w, 300);
const b1 = lcd(w);
tap(w, BTN.OCT_UP); run(w, 150);
const b2 = lcd(w);
w.web_btn(BTN.SEL, 0); run(w, 150);
check(differ(b1, b2) > 0 && differ(so, stat(w)) === 0, `SCL + OCT on SEQ turns the bank, the octave stays (${differ(b1, b2)} pixels)`);
ppm(w, 'bank');

// MOD's DEST from a list as it turns, gone 1.5 s later; ARP at 1/32, REC there KEEPs what it played into SEQ A
const v = await boot(store);
run(v, 2500);
const mid = (x) => lcd(x).slice(240 * 60, 240 * 180);  // the knobs and the picture
for (let i = 0; i < 4; i++) { tap(v, BTN.LFO); run(v, 150); }   // LFO 1, LFO 2, TURING, MOD
const m0 = mid(v);
v.web_enc(ENC.K2, 9); run(v, 200);
const m1 = mid(v);
ppm(v, 'dest-list');
run(v, 1700);
const m2 = mid(v);
check(differ(m0, m1) > 3000 && differ(m1, m2) > 3000 && differ(m0, m2) > 0,
      `MOD's DEST: a list as it turns, gone after it (${differ(m0, m1)}, ${differ(m1, m2)} pixels)`);
tap(v, BTN.SEQ); run(v, 200);
const sq0 = mid(v);
tap(v, BTN.ARP); run(v, 150); tap(v, BTN.EDIT); run(v, 150);   // ARP on
v.web_enc(ENC.K2, -2); run(v, 150);                            // RATE 1/16 -> 1/32
v.web_key(7, 1); v.web_key(11, 1); v.web_key(14, 1); run(v, 2200);
v.web_key(7, 0); v.web_key(11, 0); v.web_key(14, 0); run(v, 100);
const h0 = lcd(v).slice(240 * 186, 240 * 204);
tap(v, BTN.REC); run(v, 150);
const h1 = lcd(v).slice(240 * 186, 240 * 204);
tap(v, BTN.SEQ); run(v, 200);
ppm(v, 'kept');
check(differ(h0, h1) > 100 && differ(sq0, mid(v)) > 1000,
      `REC on ARP keeps what it played in SEQ A (the message ${differ(h0, h1)}, SEQ A ${differ(sq0, mid(v))} pixels)`);

// While the master plays, the other track and ORBIT switched on come in at its one (ON blinks till then); SCL + SEQ
// chooses the master, A or B (2026-11-10, the user, playing the player)
const u = await boot();
run(u, 2500);
let T = 0;                                         // ms since PLAY (as the quanta ran)
const go = (ms) => { run(u, ms); T += Math.ceil(ms * 44.1 / 128) * 128 / 44.1; };
const to = (ph) => go(((ph - T % 2000) + 2000) % 2000);   // to a place in A's loop (2 s: 16 16ths at 120 BPM)
const nav = (x) => lcd(x).slice(240 * 44, 240 * 62);  // the pages' line and the switch
const flips = () => { const s = []; for (let i = 0; i < 6; i++) { s.push(nav(u)); go(100); }
  return s.slice(1).filter((n, i) => differ(n, s[i]) > 0).length; };
tap(u, BTN.GLO); run(u, 150);                      // DUO: B may play
tap(u, BTN.SEQ); run(u, 150);                      // SEQ A
tap(u, BTN.PLAY);                                  // A's one now
go(150); tap(u, BTN.SEQ);                          // SEQ B
go(150); tap(u, BTN.EDIT);                         // B OFF
to(600);
const bOff = nav(u);
tap(u, BTN.EDIT); go(20);                          // B ON 0.6 s into A's loop
const bWait = flips();
to(300);                                           // past A's one
const bIn = flips();
check(bWait >= 2 && bIn === 0 && differ(bOff, nav(u)) > 0,
      `SEQ B ON while A plays: ON blinks till A's one (${bWait} flips), then lit (${bIn})`);
tap(u, BTN.SEQ); to(600);                          // ORBIT
tap(u, BTN.EDIT); go(20);                          // ORBIT on 0.6 s into A's loop
const oWait = flips();
to(300);
const oIn = flips();
check(oWait >= 2 && oIn === 0, `ORBIT on while A plays: ON blinks till A's one (${oWait} flips), then lit (${oIn})`);
tap(u, BTN.EDIT); go(100);                         // ORBIT off, SEQ A
tap(u, BTN.SEQ); go(150);
const navA = nav(u);
u.web_btn(BTN.SEL, 1); go(50);
const sclA = nav(u);
tap(u, BTN.SEQ); go(50);                           // SCL + SEQ: MASTER B
const sclB = nav(u);
u.web_btn(BTN.SEL, 0); go(50);
const navB = nav(u);
ppm(u, 'master');
check(differ(sclA, sclB) > 0 && differ(navA, navB) > 0 && differ(navA, sclA) > 0,
      `SCL + SEQ chooses the master: SCL shows it, its tab marked (${differ(sclA, sclB)}, ${differ(navA, navB)} pixels)`);
tap(u, BTN.EDIT); to(600);                         // A OFF, then ON 0.6 s into B's loop: in at B's one
tap(u, BTN.EDIT); go(20);
const aWait = flips();
to(300);
const aIn = flips();
check(aWait >= 2 && aIn === 0, `MASTER B, SEQ A ON while B plays: ON blinks till B's one (${aWait} flips), then lit (${aIn})`);

console.log(fails ? `web_test: ${fails} FAILED` : `web_test: all passed (${bytes.length} B of WebAssembly)`);
process.exit(fails ? 1 : 0);
