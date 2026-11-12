// SPDX-License-Identifier: GPL-3.0-only
// Star Distroyer's lab: the instrument as one window, every section of the patch on it at once, in the order the
// sound goes through them. The firmware runs in the page's player (../app/player.js, worklet.js, stardistroyer.wasm;
// its FM-1 panel behind the FM-1 button); this page reads its field table and its patch through the worklet
// (lab-init, lab-set, lab-load, lab-rec) and keeps them in step both ways: a control moved here is the patch the
// firmware plays, a knob turned on the panel moves the control here. A: the patch as loaded, B: as edited.
import { get, set, toC, toJSON, fromJSON } from './patch.js';
import { bind, controls, h, knob, seg, toggle, pick, stepper, tabs, navPad, navWeights, envGraph, filterGraph,
  reactorGraph, distroyGraph, lfoGraph, turGraph, echoGraph, loopGraph, lives, hz, hzText, noteText, envMs, msText, p99 } from './ui.js';
import { buildSeq, LS } from './seq.js';

const $ = (id) => document.getElementById(id);
const LIB = 'sd-lab-library';                    // this browser's library: { name: { bytes (base64), from, at } }
let post = null;                                 // to the worklet (the player hands it over when it runs)
let fields = [], byPath = new Map(), enums = {}, patches = [];
let bytes = null, orig = null, from = '', nowI = -1;   // the patch as edited, as loaded, its name, its number
let ab = 'B';
const undo = [], redo = [];
let lastPush = 0, sendQueued = false, built = false;
let seq = null, layout = [], playNow = [0, 0, 0], playheads = null;   // the sequencer: the firmware's, as last read
const tur = { hist: Array(16).fill(0), reg: 0 };   // TURING's last values and its register; the looper as it plays
let lpNow = null;
// a setting the player sets beside the patch (the looper's, FREEZE): web_lab.h's LAB_LS_*, signed where it is
const ls = (i, name, lo, hi, fmt, signed, def) => ({
  f: { path: name, name, list: 0 }, lo, hi, text: fmt || String, changed: () => false,
  reset: def === undefined ? null : () => Q.setting(i, def >>> 0),
  get: () => (seq ? (signed ? seq.set[i] | 0 : seq.set[i]) : 0),
  set: (v) => Q.setting(i, Math.round(Math.max(lo, Math.min(hi, v))) >>> 0),
});
const LDIV15 = ['1/32', '1/16T', '1/16', '1/8T', '1/16D', '1/8', '1/4T', '1/8D', '1/4', '1/2T', '1/4D', '1/2', '1 BAR', '2 BAR', '4 BAR'];

const b64 = (u8) => { let s = ''; for (const c of u8) s += String.fromCharCode(c); return btoa(s); };
const unb64 = (s) => { const t = atob(s), u = new Uint8Array(t.length); for (let i = 0; i < t.length; i++) u[i] = t.charCodeAt(i); return u; };
const same = (a, b) => a && b && a.length === b.length && a.every((v, i) => v === b[i]);
const status = (t) => { $('st').innerHTML = t; };
const shown = () => (ab === 'A' ? orig : bytes);

// ---- to the firmware: the patch (A or B), once for each burst of edits
function send() {
  if (sendQueued || !post) return;
  sendQueued = true;
  setTimeout(() => {
    sendQueued = false;
    const b = shown().slice();
    post({ type: 'lab-set', bytes: b }, [b.buffer]);
  }, 0);
}
// an undo step: the patch and the patterns (a, b, ORBIT), once per gesture (a pause of 0.6 s starts a new one)
const snap = () => ({ p: bytes.slice(), s: seq ? { a: seq.a.slice(), b: seq.b.slice(), orb: seq.orb.slice() } : null });
function remember() {
  const now = performance.now();
  if (now - lastPush > 600) { undo.push(snap()); if (undo.length > 200) undo.shift(); redo.length = 0; }
  lastPush = now;
}
function restore(e) {                            // an undo step back: the patch, and the patterns to the firmware
  bytes = e.p;
  if (e.s && seq && post) {
    for (const k of [0, 1]) {
      const b = (k ? e.s.b : e.s.a).slice();
      if (same(b, k ? seq.b : seq.a)) continue;
      if (k) seq.b = b; else seq.a = b;
      post({ type: 'lab-seq-set', k, bytes: b.slice() });
    }
    if (!same(e.s.orb, seq.orb)) { seq.orb = e.s.orb.slice(); post({ type: 'lab-orb-set', bytes: seq.orb.slice() }); }
  }
}
const tap = (id) => { if (!post) return; post({ type: 'btn', id, down: true }); setTimeout(() => post({ type: 'btn', id, down: false }), 60); };
// the sequencer's way to the firmware (web/lab/seq.js)
const Q = {
  seq: () => seq, play: () => playNow, refresh: () => refresh(), tap, mod: (id, t, o) => mod(id, t, o),
  row: (c, ...e) => row(c, ...e), lbl: (t) => lbl(t), get layout() { return layout; }, get enums() { return enums; },
  writeTrack(k, b) { if (!post) return; remember(); if (k) seq.b = b; else seq.a = b; post({ type: 'lab-seq-set', k, bytes: b.slice() }); refresh(); },
  writeOrb(b) { if (!post) return; remember(); seq.orb = b; post({ type: 'lab-orb-set', bytes: b.slice() }); refresh(); },
  setting(i, v) { if (!post) return; seq.set[i] = v; post({ type: 'lab-lset', i, v }); refresh(); },
  op(k, op, a = 0, b = 0) { if (!post) return; remember(); post({ type: 'lab-seq-op', k, op, a, b }); },
};
function edit(list) {                            // fields set from the page: [[field, value], ...]
  if (ab === 'A' || !bytes) return;
  if (list.every(([f, v]) => get(bytes, f) === v)) return;
  remember();
  for (const [f, v] of list) set(bytes, f, v);
  send();
  refresh();
}
bind({ field: (p) => byPath.get(p), edit, bytes: shown, orig: () => orig, get enums() { return enums; } });

// ---- the window: one section an engine, in the order the sound goes
const rack = $('rack');
function mod(id, title, o = {}) {
  const s = h('section', `mod m-${id}`), hd = h('header', '', `<span class="sl">///</span><h2>${title}</h2><span class="n"></span>`);
  if (o.fx) hd.append(toggle('.fx_off', 'ON', { bits: [Math.log2(o.fx), 1], inv: true, cls: 'pwr', title: `${title} on or off (its settings kept, as EDIT on the FM-1)` }));
  const body = h('div', 'body');
  s.append(hd, body);
  rack.append(s);
  return body;
}
const row = (cls, ...els) => { const r = h('div', `row${cls ? ' ' + cls : ''}`); r.append(...els); return r; };
const lbl = (t) => h('span', 'lbl', t);
const pct = (v) => `${v >= 0 ? '+' : ''}${Math.round((v * 100) / 32767)} %`;
const LDIV = ['1/16', '1/8', '1/8D', '1/4', '1/4D', '1/2', '1/2D', '1 BAR', '1.5 BAR', '2 BAR', '3 BAR', '4 BAR'];
const SDIV = ['1/32', '1/16T', '1/16', '1/8T', '1/16D', '1/8', '1/4T', '1/8D', '1/4', '1/2T', '1/4D', '1/2'];
const bit = (path, k) => (get(shown(), byPath.get(path)) >> k) & 1;

function build() {
  rack.innerHTML = '';
  controls.length = 0;
  lives.length = 0;
  // VOICE: the nav, the four banks, the Q page
  {
    const b = mod('nav', 'NAV');
    b.append(navPad());
    b.append(row('', knob('.width', 'WIDTH'), seg('.course', 'COURSE', { cls: 'course' })));
    b.querySelector('.seg.course').title = 'COURSE: the nav travels its four cells. ON: from each note, once round; LOOP: round and round on its own, from PLAY';
    b.append(row('cells', ...[0, 1, 2].map((k) => knob(`.cell_ms[${k}]`, `${k + 1} › ${k + 2}`, { size: 's', taper: 'log', fmt: msText }))));
  }
  {
    const b = mod('osc', 'OSCILLATORS');
    const pane = (k) => {
      const p = h('div', 'bank'), L = 'ABCD'[k], oscs = h('div', 'oscs');
      for (let j = 0; j < 4; j++) {
        const o = `.bank[${k}].osc[${j}]`, st = h('div', 'osc');
        st.append(row('oh', h('i', 'oid', `${L}${j + 1}`), seg(`${o}.role`, '', { cls: 'role' })),
          stepper(`${o}.harm`, 'HARMONIC', { fmt: (v) => `×${v}` }),
          row('', knob(`${o}.level`, 'LEVEL'), knob(`${o}.fine`, 'FINE', { size: 's', fmt: (v) => `${v > 0 ? '+' : ''}${v} ct` })));
        oscs.append(st);
      }
      p.append(oscs, row('ofs', lbl(`BANK ${L} +`), knob(`.bank[${k}].xm`, 'XM', { size: 's', fmt: pct }), knob(`.bank[${k}].wp`, 'WARP', { size: 's', fmt: pct }),
        knob(`.bank[${k}].fd`, 'FOLD', { size: 's', fmt: pct }), knob(`.bank[${k}].dt`, 'DETUNE', { size: 's', fmt: pct })));
      return p;
    };
    const t = tabs(['A', 'B', 'C', 'D'].map((x) => `BANK ${x}`), [0, 1, 2, 3].map(pane), { cls: 'banks' });
    const bars = [...t.querySelectorAll('.tabbar button')].map((bt) => { const i = h('i', 'w'); bt.append(i); return i; });
    controls.push({ update(bb) { navWeights(bb).forEach((w, k) => { bars[k].style.width = `${Math.round(w * 100)}%`; }); } });
    b.append(t);
  }
  {
    const b = mod('q', 'VOICE');
    b.append(row('big', knob('.xm', 'XM', { size: 'l' }), knob('.warp', 'WARP', { size: 'l' })),
      row('big', knob('.fold', 'FOLD', { size: 'l' }), knob('.dt', 'DETUNE', { size: 'l' })),
      row('', lbl('ENV 2 ›'), knob('.e2_warp', 'WARP', { fmt: pct }), knob('.e2_fold', 'FOLD', { fmt: pct })));
  }
  {
    const b = mod('mac', 'MACROS');
    b.append(row('big', knob('.mac[0]', 'MUTATE', { size: 'l' }), knob('.mac[1]', 'MOVE', { size: 'l' }), knob('.mac[2]', 'BREAK', { size: 'l' }),
      knob('.mac[3]', 'MACRO 4', { size: 'l' })), row('', seg('.mac4', 'MACRO 4 IS')));
  }
  // the envelopes
  const env = (pre, id, title) => {
    const b = mod(id, title), t = (v) => msText(envMs(v));
    b.append(envGraph(pre), row('', knob(`.${pre}_a`, 'A', { size: 's', fmt: t }), knob(`.${pre}_d`, 'D', { size: 's', fmt: t }),
      knob(`.${pre}_s`, 'S', { size: 's', fmt: (v) => `${Math.round((v * 100) / 127)} %` }), knob(`.${pre}_r`, 'R', { size: 's', fmt: t })));
  };
  env('e1', 'env1', 'ENV 1 · LEVEL');
  env('e2', 'env2', 'ENV 2 · FM');
  // the filter and its envelope
  {
    const b = mod('filt', 'FILTER', { fx: 8 });
    b.append(filterGraph(), row('', seg('.f_type', 'TYPE'),
      knob('.f_cut', 'CUTOFF', { size: 'l', fmt: (v) => hzText(hz(16 + (v * 114) / 65535)) }), knob('.f_res', 'RES', { size: 'l' })),
    row('', knob('.f_env', 'ENV 3', { fmt: (v) => `${v >= 0 ? '+' : ''}${((v * 60) / 32767).toFixed(1)} st` }),
      knob('.f_key', 'KEY', { size: 's', fmt: (v) => `${v} %` }), knob('.f_vel', 'VEL', { size: 's', fmt: (v) => `${v} %` })));
  }
  env('e3', 'env3', 'ENV 3 · FILTER');
  // the LFOs
  {
    const b = mod('lfo', 'LFO');
    for (let k = 0; k < 2; k++) {
      const c = h('div', 'lfoc'), tg = h('div', 'tg');
      tg.append(toggle(`.lfo_mode[${k}]`, 'SYNC', { bits: [0, 1] }), toggle(`.lfo_mode[${k}]`, 'KEY', { bits: [1, 1], title: 'restart on a note' }),
        toggle(`.lfo_mode[${k}]`, 'ONCE', { bits: [2, 1], title: 'with KEY: one cycle a note, then held' }));
      c.append(h('i', 'oid', `LFO ${k + 1}`), lfoGraph(k), seg(`.lfo_shape[${k}]`, '', { names: ['SIN', 'TRI', 'SAW', 'RAMP', 'SQR', 'S&H'], cls: 'wrap' }),
        row('', knob(`.lfo_rate[${k}]`, 'RATE', { fmt: (v) => (bit(`.lfo_mode[${k}]`, 0) ? LDIV[(v * 12) >> 16] : `${(0.02 * 2000 ** (v / 65536)).toFixed(2)} Hz`) }), tg));
      b.append(c);
    }
  }
  // the Reactor
  {
    const b = mod('react', 'REACTOR', { fx: 1 });
    let st = null;
    const g = reactorGraph((k) => st.choose(k));
    const stage = (k) => {
      const p = h('div', 'stg');
      for (let j = 0; j < 4; j++) {
        const fp = h('div', 'fpair');
        fp.append(knob(`.stage[${k}].pitch[${j}]`, `F${j + 1}`, { size: 's', fmt: (v) => hzText(hz(v / 256)) }),
          knob(`.stage[${k}].level[${j}]`, 'LVL', { size: 's', fmt: (v) => `${(v / 32768).toFixed(2)}×` }));
        p.append(fp);
      }
      return p;
    };
    st = tabs(['1', '2', '3', '4', '5', '6'], [0, 1, 2, 3, 4, 5].map(stage), { cls: 'stages', onChoose: (k) => g.select && g.select(k), extra: lbl('STAGE') });
    const side = h('div', 'col');
    side.append(seg('.r_model', 'MODEL', { cls: 'wrap' }), row('', toggle('.r_track', 'KEY', { bits: [0, 1], title: 'TRACK: the filters follow the key' }),
      toggle('.r_track', 'HARM', { bits: [1, 1], title: 'TRACK: onto the note\'s harmonics' })));
    b.append(row('gr', g, side));
    b.append(row('', knob('.r_pos', 'POS', { size: 'l', fmt: (v) => (1 + v / 256).toFixed(2) }), knob('.r_res', 'RES'), knob('.r_slew', 'SLEW'),
      knob('.r_drive', 'DRIVE', { fmt: (v) => `${(v / 16384).toFixed(2)}×` }), knob('.r_mix', 'MIX'),
      knob('.r_freq', 'FREQ', { fmt: (v) => `${v >= 0 ? '+' : ''}${(v / 256).toFixed(1)} st` }),
      knob('.r_low', 'LOW', { size: 's', fmt: (v) => (v ? `${50 + ((v * 350) >> 16)} Hz` : 'OFF') })));
    b.append(row('', lbl('CLOCK'), stepper('.r_div', 'DIV', { fmt: (v) => (v ? String(v) : 'OFF') }), seg('.r_cmode', 'MODE'),
      stepper('.r_min', 'FROM', { fmt: (v) => String(v + 1) }), stepper('.r_max', 'TO', { fmt: (v) => String(v + 1) })),
    row('', lbl('PAN'), ...[0, 1, 2, 3].map((k) => knob(`.r_pan[${k}]`, `F${k + 1}`, { size: 'xs' }))));
    b.append(st);
    g.select(0);
  }
  // DISTROY
  {
    const b = mod('dist', 'DISTROY', { fx: 2 });
    const side = h('div', 'col');
    side.append(seg('.d_xover', 'XOVER'), knob('.d_mix', 'MIX', { size: 'l' }));
    b.append(row('gr', distroyGraph(), side));
    b.append(row('', lbl('SPLIT'), ...[0, 1, 2].map((k) => knob(`.d_split[${k}]`, `${k + 1}`, { size: 's', fmt: (v) => hzText(hz(v / 256)) })),
      lbl('LOOP'), knob('.d_fb', 'FDBK', { size: 's' }), knob('.d_ftime', 'TIME', { size: 's', fmt: (v) => `${(1 + (v * 19) / 65535).toFixed(1)} ms` }),
      knob('.d_spread', 'SPREAD', { size: 's' }), knob('.d_tame', 'TAME', { size: 's', fmt: (v) => (v ? p99(v) : 'OFF') })));
    const bands = h('div', 'bands');
    for (let k = 0; k < 4; k++) {
      const c = h('div', `bandc b${k}`);
      c.append(h('i', 'oid', `BAND ${k + 1}`), pick(`.d_type[${k}]`, ''),
        knob(`.d_gain[${k}]`, 'DRIVE', { size: 's', fmt: (v) => `+${((v * 36) / 65535).toFixed(1)} dB` }),
        knob(`.d_level[${k}]`, 'LEVEL', { size: 's', fmt: (v) => `${(v / 32768).toFixed(2)}×` }),
        knob(`.d_alt[${k}]`, 'ALT', { size: 's' }), knob(`.d_bias[${k}]`, 'BIAS', { size: 's', fmt: (v) => `${v >= 0 ? '+' : ''}${((v * 50) / 32767).toFixed(0)} %` }),
        knob(`.d_pan[${k}]`, 'PAN', { size: 's' }),
        knob(`.d_shape[${k}]`, 'PHAT·SHARP', { size: 's', fmt: (v) => (v ? `${v < 0 ? 'PHAT' : 'SHARP'} ${Math.round((Math.abs(v) * 99) / 32767)}` : '0') }),
        knob(`.d_body[${k}]`, 'BODY', { size: 's', fmt: (v) => `${(-54 + (v * 48) / 65535).toFixed(0)} dB` }));
      bands.append(c);
    }
    b.append(bands);
  }
  // TIME, SPACE
  {
    const b = mod('time', 'TIME · DELAY · LOOPER', { fx: 16 });
    b.append(echoGraph(() => (seq ? seq.set[LS.BPM] : 120)),
      row('', knob('.s_time', 'TIME', { fmt: (v) => (bit('.s_mode', 0) ? SDIV[(v * 12) >> 16] : `${1 + Math.floor((v * 999) / 65535)} ms`) }),
        knob('.s_fb', 'FDBK'), knob('.s_dtone', 'TONE'), knob('.s_dlevel', 'LEVEL'),
        toggle('.s_mode', 'SYNC', { bits: [0, 1], title: 'TIME (and the looper\'s LENGTH and GLIDE) on the tempo' }), toggle('.s_mode', 'PING-PONG', { bits: [1, 1] })));
    if (seq) {                                   // the looper: the player's (not the patch's), as SCL on TIME sets it
      const sync = () => bit('.s_mode', 0);
      b.append(loopGraph(() => lpNow, () => ({ lvl: seq.set[LS.LLVL] })),
        row('', toggle(null, 'LOOP', { src: ls(LS.LOOP, 'LOOP', 0, 1), title: 'LOOP: the last LENGTH round and round (L-LEVEL above 0 arms it)' }),
          toggle(null, 'REV', { src: ls(LS.REV, 'REVERSE', 0, 1), title: 'REVERSE: the loop backwards' }),
          knob(null, 'L-LEVEL', { src: ls(LS.LLVL, 'L-LEVEL', 0, 65535, p99), title: 'L-LEVEL: above 0 the looper armed (it borrows the delay line\'s second half: the delay 0.5 s)' }),
          knob(null, 'LENGTH', { src: ls(LS.LLEN, 'LENGTH', 0, 65535, (v) => (sync() ? LDIV15[(v * 15) >> 16] : `${1 + Math.floor((v * 999) / 65535)} ms`)) }),
          knob(null, 'SPEED', { src: ls(LS.LSPEED, 'SPEED', -3072, 3072, (v) => `${v >= 0 ? '+' : ''}${(v / 256).toFixed(1)} st`, true, 0), title: 'SPEED: −12 .. +12 semitones (double-click: as recorded)' }),
          knob(null, 'GLIDE', { src: ls(LS.LGLIDE, 'GLIDE', 0, 65535, (v) => (sync() ? ((v * 16) >> 16 ? LDIV15[((v * 16) >> 16) - 1] : 'OFF') : v ? `${Math.round((v * 4000) / 65535)} ms` : 'OFF')) })));
    }
  }
  {
    const b = mod('space', 'SPACE · PLATE', { fx: 4 });
    b.append(row('', knob('.s_size', 'SIZE', { size: 'l' }), knob('.s_decay', 'DECAY', { size: 'l' })),
      row('', knob('.s_rtone', 'TONE'), knob('.s_rlevel', 'LEVEL'), knob('.s_mode', 'SHIMMER', { bits: [2, 6], fmt: (v) => String(Math.round((v * 99) / 63)) })));
    if (seq) b.append(row('', toggle(null, 'FREEZE', { src: ls(LS.FREEZE, 'FREEZE', 0, 1), title: 'FREEZE: the tail held for ever (the player\'s, not the patch\'s)' })));
  }
  // movement: TURING, the matrix
  {
    const b = mod('tur', 'TURING');
    const dn = enums[8] || [];
    b.append(turGraph(() => tur));
    const fS = byPath.get('.tur_set'), fD = byPath.get('.tur_dst');
    const turOn = {                              // ON as EDIT on TURING: with no DEST yet, XM, and CHANGE 25 % if it was 0
      f: { path: '.tur_set (bit 7)', name: 'ON', list: 0 }, lo: 0, hi: 1, text: String, reset: null,
      get: () => (get(shown(), fS) >> 7) & 1,
      changed: (b, o) => ((get(b, fS) >> 7) & 1) !== ((get(o, fS) >> 7) & 1),
      set(v) {
        let ns = v ? get(bytes, fS) | 0x80 : get(bytes, fS) & 0x7F;
        const list = [];
        if (v && !(ns & 127)) ns |= 25;
        list.push([fS, ns]);
        if (v && !get(bytes, fD)) list.push([fD, 2]);   // (SD_MD_XM)
        if (v && !get(bytes, byPath.get('.tur_amt'))) list.push([byPath.get('.tur_amt'), 30]);
        edit(list);
      },
    };
    b.append(row('', toggle(null, 'ON', { src: turOn }), knob('.tur_set', 'CHANGE', { bits: [0, 7], hi: 100, fmt: (v) => `${v} %` }),
      knob('.tur_amt', 'AMOUNT', { fmt: (v) => `${v > 0 ? '+' : ''}${v} %` }),
      stepper('.tur_ld', 'LENGTH', { bits: [0, 5], lo: 2, hi: 16, dec: (v) => (v ? Math.max(2, Math.min(16, v)) : 8), fmt: String })),
    row('', pick('.tur_dst', 'DEST', { names: [...dn, 'NOTES'], values: [...dn.map((_, i) => i), 128] })),
    row('', seg('.tur_ld', 'DIV', { bits: [5, 3], lo: 0, hi: 5, names: ['1/32', '1/16T', '1/16', '1/8T', '1/8', '1/4'], values: [0, 1, 2, 3, 4, 5],
      dec: (v) => (v ? Math.min(5, v - 1) : 2), enc: (v) => v + 1, cls: 'wrap' })));
  }
  {
    const b = mod('mtx', 'MOD MATRIX');
    const t = h('div', 'mtx');
    for (let k = 0; k < 8; k++) t.append(row('slot', h('i', 'oid', String(k + 1)), pick(`.mod[${k}].src`, ''), h('span', 'to', '›'), pick(`.mod[${k}].dst`, ''),
      knob(`.mod[${k}].amt`, '', { size: 'xs', fmt: pct })));
    b.append(t);
  }
  // the sequencer: SEQ A and B, ARP, ORBIT (not the patch's: the firmware's patterns and settings)
  if (seq && layout.length) playheads = buildSeq(Q);
  built = true;
}

// ---- the window's state: the controls, the changes, the bar
function refresh() {
  if (!built || !bytes) return;
  const b = shown();
  for (const c of controls) c.update(b, orig);
  let changed = 0;
  for (const f of fields) if (get(bytes, f) !== get(orig, f)) changed++;
  for (const m of rack.querySelectorAll('.mod')) { const n = m.querySelectorAll('.body .ch').length; m.querySelector('header .n').textContent = n ? `${n} changed` : ''; }
  $('undo').disabled = !undo.length; $('redo').disabled = !redo.length;
  document.body.classList.toggle('ab-a', ab === 'A');
  status(`<b>${$('name').value || from}</b> · ${ab === 'A' ? '<b>A</b>: as loaded' : `B: ${changed} field${changed === 1 ? '' : 's'} changed`}`);
}
function lists(now) {
  const sel = $('patch');
  sel.innerHTML = patches.map((p) => `<option value="${p.i}">${String(p.i + 1).padStart(2, '0')} ${p.name}</option>`).join('');
  sel.value = String(now);
  library();
}
function library() {
  let lib = {};
  try { lib = JSON.parse(localStorage.getItem(LIB) || '{}'); } catch (_) { /* (none) */ }
  $('lib').innerHTML = '<option value="">LIBRARY</option>' + Object.keys(lib).sort().map((n) => `<option value="${n}">${n}</option>`).join('');
  return lib;
}
function take(b, name, src) {                    // a patch in: it is B, and A is what it was loaded as
  orig = b.slice(); bytes = b.slice(); from = src || name;
  $('name').value = name;
  undo.length = 0; redo.length = 0; lastPush = 0;
  ab = 'B'; $('ab').classList.remove('on');
  send(); refresh();
}

// ---- from the firmware
window.addEventListener('sd-ready', (e) => {
  post = e.detail.post;
  post({ type: 'lab-init' });
});
window.addEventListener('sd-lab', (e) => {
  const m = e.detail;
  if (m.type === 'lab-init') {
    fields = m.fields; enums = m.enums; patches = m.patches;
    byPath = new Map(fields.map((f) => [f.path, f]));
    layout = m.layout || []; seq = m.seq || null;
    const name = (patches.find((p) => p.i === m.now) || {}).name || 'PATCH';
    orig = m.bytes.slice(); bytes = m.bytes.slice(); from = name; $('name').value = name; nowI = m.now;
    document.body.classList.add('live');
    for (const id of ['patch', 'prev', 'next', 'ab', 'reset', 'keep', 'lib', 'imp', 'exp', 'cc', 'rec']) $(id).disabled = false;
    build(); lists(m.now); refresh();
  } else if (m.type === 'lab-loaded') {
    const p = patches.find((q) => q.i === m.now);
    nowI = m.now;
    take(m.bytes, p ? p.name : 'PATCH');
    $('patch').value = String(m.now);
  } else if (m.type === 'lab-pat') {             // the panel's knobs changed the patch, or PRESETS + EDIT loaded one
    if (ab === 'A') return;
    if (m.now !== undefined && m.now !== nowI) {
      const p = patches.find((q) => q.i === m.now);
      nowI = m.now; $('patch').value = String(m.now);
      take(m.bytes, p ? p.name : 'PATCH');
    } else if (!same(m.bytes, bytes)) { remember(); bytes = m.bytes.slice(); refresh(); }
  } else if (m.type === 'lab-seq') {             // the sequencer: the panel's SEQ (or an op) changed it; the transport
    if (m.seq && seq) {
      const pats = !same(m.seq.a, seq.a) || !same(m.seq.b, seq.b) || !same(m.seq.orb, seq.orb);
      if (pats && ab === 'B') remember();
      seq = m.seq;
      refresh();
    }
    if (m.play) { playNow = m.play; if (playheads) playheads(m.play); }
    if (m.tur) { for (const v of m.tur.vals) { tur.hist.shift(); tur.hist.push(v); } tur.reg = m.tur.reg; }
    if (m.lp) lpNow = m.lp;
    if ((m.tur || m.lp) && built && bytes) for (const f of lives) f();
  } else if (m.type === 'lab-rec') {             // the frames, as the firmware made them: a .wav
    const pcm = m.frames, n = pcm.length * 2, hd = new DataView(new ArrayBuffer(44));
    const w = (o, s) => { for (let i = 0; i < 4; i++) hd.setUint8(o + i, s.charCodeAt(i)); };
    w(0, 'RIFF'); hd.setUint32(4, 36 + n, true); w(8, 'WAVE'); w(12, 'fmt '); hd.setUint32(16, 16, true);
    hd.setUint16(20, 1, true); hd.setUint16(22, 2, true); hd.setUint32(24, 44100, true); hd.setUint32(28, 44100 * 4, true);
    hd.setUint16(32, 4, true); hd.setUint16(34, 16, true); w(36, 'data'); hd.setUint32(40, n, true);
    download(new Blob([hd, pcm], { type: 'audio/wav' }), `star_distroyer_${slug()}_${stamp()}.wav`);
    status(`<b>${$('name').value}</b> · recorded ${(pcm.length / 2 / 44100).toFixed(1)} s: the firmware's own frames`);
  }
});

// ---- the bar
const slug = () => ($('name').value || 'patch').toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_|_$/g, '');
const stamp = () => new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-');
function download(blob, name) {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob); a.download = name;
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}
function load(i) {
  if (!same(bytes, orig) && !confirm('Load another patch? Your changes are kept only if you KEEP or EXPORT them first.')) {
    $('patch').value = String(nowI); return;
  }
  post({ type: 'lab-load', i });
}
$('patch').addEventListener('change', () => load(Number($('patch').value)));
const near = (d) => { const k = patches.findIndex((p) => p.i === nowI); return patches[(k + d + patches.length) % patches.length].i; };
$('prev').addEventListener('click', () => load(near(-1)));
$('next').addEventListener('click', () => load(near(1)));
$('ab').addEventListener('click', () => { if (!bytes) return; ab = ab === 'B' ? 'A' : 'B'; $('ab').classList.toggle('on', ab === 'A'); send(); refresh(); });
$('reset').addEventListener('click', () => { if (!bytes || ab === 'A') return; remember(); lastPush = 0; bytes = orig.slice(); send(); refresh(); });
function step(from_, to) { if (!from_.length || ab === 'A') return; to.push(snap()); restore(from_.pop()); lastPush = 0; send(); refresh(); }
$('undo').addEventListener('click', () => step(undo, redo));
$('redo').addEventListener('click', () => step(redo, undo));
document.addEventListener('keydown', (e) => {
  if (!(e.metaKey || e.ctrlKey) || e.key.toLowerCase() !== 'z' || !bytes) return;
  if (e.target.matches('input[type=text], textarea')) return;
  e.preventDefault();
  if (e.shiftKey) step(redo, undo); else step(undo, redo);
});
$('name').addEventListener('input', refresh);
$('keep').addEventListener('click', () => {
  const lib = library(), name = ($('name').value || from).toUpperCase().slice(0, 24);
  if (lib[name] && !confirm(`Replace ${name} in the library?`)) return;
  lib[name] = { bytes: b64(bytes), from, at: new Date().toISOString() };
  try { localStorage.setItem(LIB, JSON.stringify(lib)); } catch (_) { alert('This browser would not keep it (its storage is full or blocked): EXPORT it instead.'); }
  library(); $('lib').value = name;
  status(`<b>${name}</b> kept in this browser's library`);
});
$('lib').addEventListener('change', () => {
  const lib = library(), name = $('lib').value, e = lib[name];
  if (!e) return;
  const b = unb64(e.bytes);
  if (b.length !== bytes.length) { alert(`${name} was kept by a firmware whose patch is another size: it cannot load here.`); return; }
  take(b, name, e.from);
  $('lib').value = '';
});
$('imp').addEventListener('click', () => $('file').click());
$('file').addEventListener('change', async () => {
  const f = $('file').files[0];
  if (!f) return;
  try { const p = fromJSON(await f.text(), bytes.length); take(p.bytes, p.name, p.from); }
  catch (err) { alert(`${f.name}: ${err.message}`); }
  $('file').value = '';
});
$('exp').addEventListener('click', () => {
  download(new Blob([toJSON(bytes, fields, $('name').value || from, from)], { type: 'application/json' }), `${slug()}.json`);
});
$('cc').addEventListener('click', () => {
  $('dlgt').textContent = `${$('name').value || from}: as C, an entry of SD_PATCH[] in firmware/src/sd_patches.h (its name in SD_PATCH_NAME[])`;
  $('dlgx').value = toC(bytes, fields, ($('name').value || from).toUpperCase(), from);
  $('dlg').showModal();
});
$('dlgcopy').addEventListener('click', async () => { try { await navigator.clipboard.writeText($('dlgx').value); $('dlgcopy').textContent = 'COPIED'; setTimeout(() => { $('dlgcopy').textContent = 'COPY'; }, 1200); } catch (_) { $('dlgx').select(); } });
$('dlgclose').addEventListener('click', () => $('dlg').close());
$('rec').addEventListener('click', () => {
  if (!post) return;
  const on = !$('rec').classList.contains('on');
  $('rec').classList.toggle('on', on);
  $('rec').textContent = on ? '■ STOP' : '● REC';
  post({ type: 'lab-rec', on });
  if (on) status(`<b>${$('name').value}</b> · recording the firmware's frames…`);
});
// POWER: the player's own button (its panel may be hidden), on and then off and on
$('go').addEventListener('click', () => $('power').click());
$('pw').addEventListener('click', () => { const p = $('power'); if (!p.hidden) p.click(); else $('p-off').click(); });
// the FM-1's panel, behind its button
$('hwb').addEventListener('click', () => { const on = $('hw').hidden; $('hw').hidden = !on; $('hwb').classList.toggle('on', on); });
$('hwx').addEventListener('click', () => { $('hw').hidden = true; $('hwb').classList.remove('on'); });

// ---- the keys: the FM-1's 27 (F3 .. G5 at OCT 0), OCT and PLAY, as its own buttons
{
  const kb = $('keys'), WH = [0, 2, 4, 5, 7, 9, 11];
  const notes = [...Array(27)].map((_, k) => 53 + k), whites = notes.filter((n) => WH.includes(n % 12)).length;
  let wi = 0;
  const held = new Set(), els = [];
  const press = (k, down) => {
    if (!post || down === held.has(k)) return;
    if (down) held.add(k); else held.delete(k);
    post({ type: 'key', k, down });
    els[k].classList.toggle('down', down);
  };
  notes.forEach((n, k) => {
    const white = WH.includes(n % 12), e = h('button', white ? 'w' : 'b', white && n % 12 === 0 ? `<em>${noteText(n)}</em>` : '');
    e.type = 'button';
    e.tabIndex = -1;
    if (white) { e.style.left = `${(wi / whites) * 100}%`; e.style.width = `${100 / whites}%`; wi++; }
    else { e.style.left = `${((wi - 0.33) / whites) * 100}%`; e.style.width = `${66 / whites}%`; }
    e.addEventListener('pointerdown', (ev) => {   // (a touch captures its key: let it go, so a glide plays the next)
      ev.preventDefault();
      try { e.releasePointerCapture(ev.pointerId); } catch (_) { /* (none held) */ }
      press(k, true);
    });
    e.addEventListener('pointerenter', (ev) => { if (ev.buttons & 1) press(k, true); });
    e.addEventListener('pointerleave', () => press(k, false));
    e.addEventListener('pointerup', () => press(k, false));
    els.push(e);
    kb.append(e);
  });
  $('octd').addEventListener('click', () => tap(0));
  $('octu').addEventListener('click', () => tap(1));
  $('play').addEventListener('click', () => tap(12));
}
// the bar's height, for the window's sticky parts (it wraps on a narrow window)
new ResizeObserver(([e]) => document.documentElement.style.setProperty('--barh', `${Math.ceil(e.target.offsetHeight)}px`))
  .observe(document.querySelector('.top'));
