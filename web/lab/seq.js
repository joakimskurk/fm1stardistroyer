// SPDX-License-Identifier: GPL-3.0-only
// The lab's SEQUENCER and ORBIT: the firmware's own two patterns (SEQ A and B), ORBIT and the settings the tracks and
// the arpeggiator play by, on the window beside the patch. The bytes are SdPattern's and SdOrbit's, read at the
// offsets the firmware gives (web/app/web_lab.h: web_seq_layout); LENGTH, a segment's cut, EVEN, a segment's length,
// RND and CLR run the firmware's own functions (web_seq_op), so the window edits a pattern as SEQ's keys do.
import { controls, h, knob, seg, toggle, pick, stepper, noteText } from './ui.js';

// web_seq_layout's order (LAB_SEQ_OFF)
const NAMES = ['stepSize', 'note', 'vel', 'gate', 'type', 'ratchet', 'prob', 'chord', 'step', 'len', 'div', 'swing',
  'segCount', 'segSplit', 'segRep', 'segOrder', 'segSkip', 'scale', 'patSize', 'oOn', 'oLen', 'oRate', 'oGlide', 'oDst',
  'oXa', 'oXb', 'oLane', 'oMac', 'orbSize'];
export const LS = { BPM: 0, MODE: 1, ARP: 2, AMODE: 3, ADIV: 4, AOCT: 5, AGATE: 6, AHOLD: 7, ON_A: 8, ON_B: 9, MASTER: 10, CHROM: 11,
  LOOP: 12, REV: 13, LLEN: 14, LLVL: 15, LSPEED: 16, LGLIDE: 17, FREEZE: 18 };   // (web_lab.h: LAB_LS_*)
export const OP = { LEN: 1, CUT: 2, EVEN: 3, SEGLEN: 4, RND: 5, CLEAR: 6 };
const T = { NOTE: 0, SLIDE: 1, TIE: 2, REST: 3, SKIP: 4 };
const SEGS = 'ABCDEF';
const s8 = (v) => (v << 24) >> 24;

// a value that is not the patch's (ui.js: acc's o.src)
const src = (name, lo, hi, get, put, fmt, def) => ({
  f: { path: name, name, list: 0 }, lo, hi, get: () => get(),
  set: (v) => put(Math.round(Math.max(lo, Math.min(hi, v)))),
  text: fmt || String, changed: () => false, reset: def === undefined ? null : () => put(def),
});

// Q, the page: seq() (the state: a, b, orb, set), layout, enums, mod, row, lbl, writeTrack(k, bytes),
// writeOrb(bytes), setting(i, v), op(k, op, a, b), tap(button)
export function buildSeq(Q) {
  const L = {};
  NAMES.forEach((n, i) => { L[n] = Q.layout[i]; });
  const E = (l) => Q.enums[l] || [];
  const tr = (k) => (k ? Q.seq().b : Q.seq().a);
  const so = (i) => L.step + i * L.stepSize;
  const len = (k) => Math.max(1, Math.min(64, tr(k)[L.len]));
  const bounds = (k) => {                        // sd_seg_bounds: b[0] = 0 .. b[sc] = len
    const p = tr(k), n = len(k);
    let sc = Math.max(1, Math.min(6, p[L.segCount], n));
    const b = [0];
    for (let i = 0; i + 1 < sc; i++) { const lo = b[i] + 1, hi = n - (sc - 1 - i), v = p[L.segSplit + i]; b.push(Math.max(lo, Math.min(hi, v))); }
    b.push(n);
    return b;
  };
  const putT = (k, fn) => { const b = tr(k).slice(); fn(b); Q.writeTrack(k, b); };
  const putO = (fn) => { const b = Q.seq().orb.slice(); fn(b); Q.writeOrb(b); };
  const sel = { k: 0, i: 0 };
  const cut = [false, false];
  const views = [];                              // what the transport moves: { k, cells }

  // ---- the transport, the voices, ARP
  const body = Q.mod('seq', 'SEQUENCER');
  const sset = (i, name, lo, hi, fmt) => src(name, lo, hi, () => Q.seq().set[i], (v) => Q.setting(i, v), fmt);
  const play = h('button', 'tog play', '<i></i>▶ PLAY');
  play.type = 'button';
  play.title = 'PLAY: both tracks from the one (Space)';
  play.addEventListener('click', () => Q.tap(12));
  body.append(Q.row('trans', play,
    stepper(null, 'TEMPO', { src: sset(LS.BPM, 'TEMPO', 40, 240, (v) => `${v} BPM`), title: 'TEMPO: 40 .. 240 BPM (drag, scroll)' }),
    seg(null, 'VOICES', { src: sset(LS.MODE, 'VOICES', 0, 2), names: E(21) }),
    seg(null, 'THE ONE FROM', { src: sset(LS.MASTER, 'MASTER', 0, 1), names: ['SEQ A', 'SEQ B'] }),
    seg(null, 'TRANSPOSE', { src: sset(LS.CHROM, 'TRANSPOSE', 0, 1), names: ['IN KEY', 'CHROM'] }),
    h('span', 'vsep'), Q.lbl('ARP'),
    toggle(null, 'ON', { src: sset(LS.ARP, 'ARP', 0, 1), title: 'ARP on (it needs a voice the tracks leave free)' }),
    seg(null, 'ORDER', { src: sset(LS.AMODE, 'ARP MODE', 0, 4), names: E(20) }),
    pick(null, 'RATE', { src: sset(LS.ADIV, 'ARP RATE', 0, 11), names: E(19) }),
    stepper(null, 'OCT', { src: sset(LS.AOCT, 'ARP OCT', 1, 4) }),
    knob(null, 'GATE', { size: 's', src: sset(LS.AGATE, 'ARP GATE', 5, 100, (v) => `${v} %`) }),
    toggle(null, 'HOLD', { src: sset(LS.AHOLD, 'ARP HOLD', 0, 1) })));
  controls.push({ update() { play.classList.toggle('on', !!Q.play()[0]); } });

  // ---- a track: its settings, its steps, its segments
  const tracks = h('div', 'tracks'), insp = h('div', 'insp');
  for (let k = 0; k < 2; k++) {
    const t = h('div', `trk t${k}`), N = k ? 'SEQ B' : 'SEQ A';
    const pf = (key, name, lo, hi, fmt, dec) => src(`${N} ${name}`, lo, hi, () => (dec ? dec(tr(k)[L[key]]) : tr(k)[L[key]]),
      (v) => putT(k, (b) => { b[L[key]] = v; }), fmt);
    const btn = (label, title, fn) => { const b = h('button', 'tog', label); b.type = 'button'; b.title = title; b.addEventListener('click', fn); return b; };
    const cb = btn('CUT', 'Click a step to start a segment there, or join it to the one before', () => { cut[k] = !cut[k]; cb.classList.toggle('on', cut[k]); t.classList.toggle('cutting', cut[k]); });
    t.append(Q.row('th', h('i', 'oid', N),
      toggle(null, 'ON', { src: sset(k ? LS.ON_B : LS.ON_A, `${N} ON`, 0, 1), title: `${N} on or off (it keeps time, silent)` }),
      stepper(null, 'LENGTH', { src: src(`${N} LENGTH`, 1, 64, () => len(k), (v) => { tr(k)[L.len] = v; Q.op(k, OP.LEN, v); }),
        title: 'LENGTH: 1 .. 64 steps (the segments scale with it)' }),   // (the length now, before the firmware's answer: a quick click counts from it)
      pick(null, 'DIV', { src: pf('div', 'DIV', 0, 11), names: E(19) }),
      knob(null, 'SWING', { size: 's', src: pf('swing', 'SWING', 50, 75, (v) => `${v} %`, (v) => Math.max(50, v)) }),
      pick(null, 'SCALE', { src: pf('scale', 'SCALE', 0, 6), names: E(18) }),
      btn('RND', 'RANDOMIZE: a new line in the scale, now (UNDO takes it back)', () => Q.op(k, OP.RND)),
      btn('CLR', 'Every step a rest (UNDO takes it back)', () => Q.op(k, OP.CLEAR)),
      btn('EVEN', 'The segments as near equal as they go', () => Q.op(k, OP.EVEN)), cb));
    const grid = h('div', 'sgrid');
    t.append(grid);
    const cells = [];
    const cell = (i) => {
      const c = h('button', 'scell', '<i class="sn"></i><em class="sg"></em><b></b><span class="sv"><u></u></span><small></small>');
      c.type = 'button';
      c.tabIndex = -1;
      let drag = null;
      c.addEventListener('pointerdown', (e) => { if (e.button === 0) drag = { y: e.clientY, n: tr(k)[so(i) + L.note], moved: false }; });
      c.addEventListener('pointermove', (e) => {
        if (!drag || !(e.buttons & 1)) return;
        const d = Math.round((drag.y - e.clientY) / 6);
        if (!drag.moved && Math.abs(drag.y - e.clientY) < 4) return;
        drag.moved = true;
        try { c.setPointerCapture(e.pointerId); } catch (_) { /* (gone) */ }
        if (tr(k)[so(i) + L.type] <= T.SLIDE) putT(k, (b) => { b[so(i) + L.note] = Math.max(0, Math.min(127, drag.n + d)); });
      });
      c.addEventListener('pointerup', () => {
        const moved = drag && drag.moved;
        drag = null;
        if (moved || i >= len(k)) return;
        if (cut[k]) Q.op(k, OP.CUT, i);
        else { sel.k = k; sel.i = i; Q.refresh(); }
      });
      c.addEventListener('dblclick', () => {    // NOTE <-> REST
        if (i >= len(k) || cut[k]) return;
        putT(k, (b) => { const o = so(i) + L.type; b[o] = b[o] <= T.SLIDE || b[o] === T.TIE ? T.REST : T.NOTE; });
      });
      c.addEventListener('wheel', (e) => {       // the note, a semitone (Shift: an octave)
        e.preventDefault();
        if (i >= len(k) || tr(k)[so(i) + L.type] > T.SLIDE) return;
        const n = Math.sign(-e.deltaY || e.deltaX) * (e.shiftKey ? 12 : 1);
        putT(k, (b) => { b[so(i) + L.note] = Math.max(0, Math.min(127, b[so(i) + L.note] + n)); });
      }, { passive: false });
      return c;
    };
    // the segments: each one's steps, REPEAT, ORDER, SKIP and length
    const segs = h('div', 'segs'), chips = [];
    for (let g = 0; g < 6; g++) {
      const c = h('div', 'chip');
      const sk = src(`${N} SEGMENT ${SEGS[g]} SKIP`, 0, 1, () => (tr(k)[L.segSkip] >> g) & 1,
        (v) => putT(k, (b) => { b[L.segSkip] = v ? b[L.segSkip] | (1 << g) : b[L.segSkip] & ~(1 << g); }));
      c.append(h('i', 'oid', SEGS[g]), h('span', 'rg', ''),
        stepper(null, 'REPEAT', { src: src(`${N} ${SEGS[g]} REPEAT`, 1, 4, () => Math.max(1, tr(k)[L.segRep + g]), (v) => putT(k, (b) => { b[L.segRep + g] = v; })) }),
        seg(null, 'ORDER', { src: src(`${N} ${SEGS[g]} ORDER`, 0, 3, () => tr(k)[L.segOrder + g] % 4, (v) => putT(k, (b) => { b[L.segOrder + g] = v; })), names: E(17) }),
        stepper(null, 'STEPS', { src: src(`${N} ${SEGS[g]} STEPS`, 1, 64, () => { const bd = bounds(k); return g + 1 < bd.length ? bd[g + 1] - bd[g] : 1; }, (v) => Q.op(k, OP.SEGLEN, g, v)) }),
        toggle(null, 'SKIP', { src: sk }));
      segs.append(c);
      chips.push(c);
    }
    t.append(segs);
    tracks.append(t);
    const view = { k, cells };
    views.push(view);
    controls.push({
      update() {
        const p = tr(k), n = len(k), rows = Math.ceil(n / 16), bd = bounds(k), sc = bd.length - 1;
        while (cells.length < rows * 16) { const c = cell(cells.length); cells.push(c); grid.append(c); }
        cells.forEach((c, i) => { c.hidden = i >= rows * 16; });
        for (let i = 0; i < rows * 16; i++) {
          const c = cells[i], o = so(i), ty = p[o + L.type], out = i >= n;
          let g = 0;
          while (g + 1 < sc && bd[g + 1] <= i) g++;
          const chord = [0, 1, 2].filter((j) => s8(p[o + L.chord + j]) !== 0).length;
          c.className = `scell${out ? ' out' : ''}${ty === T.SLIDE ? ' slide' : ''}${ty === T.TIE ? ' tie' : ''}${ty === T.REST ? ' rest' : ''}` +
            `${ty === T.SKIP ? ' skip' : ''}${(p[L.segSkip] >> g) & 1 ? ' segskip' : ''}${g % 2 ? ' odd' : ''}` +
            `${!out && bd.includes(i) && i < n ? ' start' : ''}${sel.k === k && sel.i === i ? ' sel' : ''}${c.classList.contains('now') ? ' now' : ''}`;
          c.querySelector('.sn').textContent = String(i + 1);
          c.querySelector('.sg').textContent = !out && bd.indexOf(i) >= 0 && bd.indexOf(i) < sc ? SEGS[bd.indexOf(i)] : '';
          c.querySelector('b').textContent = out ? '' : ty <= T.SLIDE ? noteText(p[o + L.note]) : ty === T.TIE ? '—' : ty === T.REST ? '·' : '×';
          c.querySelector('u').style.width = ty <= T.SLIDE && !out ? `${Math.round((p[o + L.vel] / 127) * 100)}%` : '0';
          const rt = Math.max(1, p[o + L.ratchet]), pr = p[o + L.prob];
          c.querySelector('small').textContent = out || ty > T.SLIDE ? '' : `${chord ? `+${chord} ` : ''}${rt > 1 ? '•'.repeat(rt) : ''}${pr < 100 ? ` ${pr}%` : ''}`;
          c.title = out ? '' : `${N} step ${i + 1}, segment ${SEGS[g]}: ${E(16)[ty] || ty}${ty <= T.SLIDE ? ` ${noteText(p[o + L.note])}, velocity ${p[o + L.vel]}, gate ${p[o + L.gate]} %` : ''}` +
            '. Click: edit it; drag or scroll: its note (Shift: an octave); double-click: a note or a rest';
        }
        chips.forEach((c, g) => {
          c.hidden = g >= sc;
          if (g < sc) c.querySelector('.rg').textContent = `${bd[g] + 1}–${bd[g + 1]}`;
          c.classList.toggle('skipped', !!((p[L.segSkip] >> g) & 1));
        });
        t.classList.toggle('muted', !Q.seq().set[k ? LS.ON_B : LS.ON_A]);
      },
    });
  }
  // ---- the step picked: everything it holds
  const sf = (key, name, lo, hi, fmt, dec, signed) => src(name, lo, hi, () => {
    const v = tr(sel.k)[so(sel.i) + L[key]];
    return dec ? dec(signed ? s8(v) : v) : signed ? s8(v) : v;
  }, (v) => putT(sel.k, (b) => { b[so(sel.i) + L[key]] = v & 255; }), fmt);
  const ititle = h('div', 'ititle', '');
  const chordF = (j) => src(`CHORD ${j + 1}`, -24, 24, () => s8(tr(sel.k)[so(sel.i) + L.chord + j]),
    (v) => putT(sel.k, (b) => { b[so(sel.i) + L.chord + j] = v & 255; }), (v) => (v ? `${v > 0 ? '+' : ''}${v}` : 'OFF'), 0);
  insp.append(ititle,
    seg(null, 'TYPE', { src: sf('type', 'TYPE', 0, 4), names: E(16), cls: 'wrap' }),
    Q.row('', stepper(null, 'NOTE', { src: sf('note', 'NOTE', 0, 127, noteText), title: 'NOTE: drag, scroll or the arrows' }),
      seg(null, 'RATCHET', { src: sf('ratchet', 'RATCHET', 1, 3, String, (v) => Math.max(1, v)), names: ['1', '2', '3'], values: [1, 2, 3] })),
    Q.row('', knob(null, 'VEL', { size: 's', src: sf('vel', 'VELOCITY', 1, 127) }), knob(null, 'GATE', { size: 's', src: sf('gate', 'GATE', 1, 100, (v) => `${v} %`) }),
      knob(null, 'PROB', { size: 's', src: sf('prob', 'PROBABILITY', 0, 100, (v) => `${v} %`) })),
    Q.row('chord', Q.lbl('CHORD'), ...[0, 1, 2].map((j) => stepper(null, '', { src: chordF(j), title: `CHORD ${j + 1}: semitones from the note (0: none)` }))));
  controls.push({
    update() {
      if (sel.i >= len(sel.k)) sel.i = 0;
      ititle.innerHTML = `<i class="oid">${sel.k ? 'SEQ B' : 'SEQ A'}</i> STEP ${sel.i + 1} <span>of ${len(sel.k)}</span>`;
    },
  });
  body.append(Q.row('tw', tracks, insp));

  // ---- ORBIT: its settings, then its lanes, a bar a step
  const ob = Q.mod('orb', 'ORBIT · SEQ A\'S');
  const orb = () => Q.seq().orb;
  const of = (key, name, lo, hi, fmt, dec, at = 0) => src(name, lo, hi, () => (dec ? dec(orb()[L[key] + at]) : orb()[L[key] + at]),
    (v) => putO((b) => { b[L[key] + at] = v; }), fmt);
  const olen = () => { const v = orb()[L.oLen]; return v ? Math.max(2, Math.min(8, v)) : 4; };
  const gl = (bit, name) => src(name, 0, 1, () => (orb()[L.oGlide] >> bit) & 1, (v) => putO((b) => { b[L.oGlide] = v ? b[L.oGlide] | (1 << bit) : b[L.oGlide] & ~(1 << bit); }));
  const dn = E(8);
  ob.append(Q.row('', toggle(null, 'ON', { src: of('oOn', 'ORBIT', 0, 1), title: 'ORBIT on: it comes in at the one' }),
    stepper(null, 'STEPS', { src: of('oLen', 'ORBIT STEPS', 2, 8, String, (v) => (v ? Math.max(2, Math.min(8, v)) : 4)) }),
    seg(null, 'A STEP LASTS', { src: of('oRate', 'ORBIT RATE', 0, 2, String, (v) => Math.min(2, v)), names: ['1 LOOP', '2 LOOPS', '4 LOOPS'], values: [0, 1, 2] }),
    Q.lbl('GLIDE'), toggle(null, 'LANE 1', { src: gl(0, 'GLIDE LANE 1') }), toggle(null, 'LANE 2', { src: gl(1, 'GLIDE LANE 2') }),
    toggle(null, 'MACROS', { src: gl(2, 'GLIDE MACROS') }),
    pick(null, 'LANE 1 TO', { src: of('oDst', 'LANE 1 DEST', 0, dn.length - 1), names: Q.dord.map((d) => dn[d]), values: Q.dord }),
    pick(null, 'LANE 2 TO', { src: of('oDst', 'LANE 2 DEST', 0, dn.length - 1, String, undefined, 1), names: Q.dord.map((d) => dn[d]), values: Q.dord })));
  const lanes = h('div', 'lanes');
  const LANES = [['XPOSE A', 'oXa', 0, 24, (v) => `${v > 0 ? '+' : ''}${v}`], ['XPOSE B', 'oXb', 0, 24, (v) => `${v > 0 ? '+' : ''}${v}`],
    ['LANE 1', 'oLane', 0, 100, (v) => `${v > 0 ? '+' : ''}${v}%`], ['LANE 2', 'oLane', 8, 100, (v) => `${v > 0 ? '+' : ''}${v}%`],
    ['MUTATE', 'oMac', 0, 99, String], ['MOVE', 'oMac', 8, 99, String], ['BREAK', 'oMac', 16, 99, String], ['MACRO 4', 'oMac', 24, 99, String]];
  for (const [name, key, at, hi, fmt] of LANES) {
    const r = h('div', 'lane');
    r.append(h('span', 'lbl', name));
    const bars = [];
    for (let s = 0; s < 8; s++) {
      const c = h('div', 'bar', '<i></i><output></output>');
      c.title = `${name}, ORBIT step ${s + 1}: drag or scroll (−${hi} .. +${hi}); double-click: 0`;
      const get = () => s8(orb()[L[key] + at + s]);
      const put = (v) => putO((b) => { b[L[key] + at + s] = Math.max(-hi, Math.min(hi, Math.round(v))) & 255; });
      let drag = null;
      c.addEventListener('pointerdown', (e) => { e.preventDefault(); try { c.setPointerCapture(e.pointerId); } catch (_) { /* (gone) */ } drag = { y: e.clientY, v: get() }; });
      c.addEventListener('pointermove', (e) => { if (drag) put(drag.v + ((drag.y - e.clientY) * hi) / 40); });
      c.addEventListener('pointerup', () => { drag = null; });
      c.addEventListener('wheel', (e) => { e.preventDefault(); put(get() + Math.sign(-e.deltaY || e.deltaX)); }, { passive: false });
      c.addEventListener('dblclick', () => put(0));
      r.append(c);
      bars.push({ c, get });
    }
    lanes.append(r);
    controls.push({
      update() {
        const n = olen();
        bars.forEach((b, s) => {
          const v = b.get(), q = Math.abs(v) / hi;
          const bar = b.c.querySelector('i');
          bar.style.height = `${(q * 50).toFixed(1)}%`;
          bar.style.top = v >= 0 ? `${(50 - q * 50).toFixed(1)}%` : '50%';
          bar.classList.toggle('neg', v < 0);
          b.c.querySelector('output').textContent = v ? fmt(v) : '·';
          b.c.classList.toggle('out', s >= n);
        });
      },
    });
  }
  ob.append(lanes);

  // the transport, moved on its own (each frame): the step each track plays lit
  return function playheads(pl) {
    for (const v of views) {
      const now = pl[0] ? pl[1 + v.k] : -1;
      v.cells.forEach((c, i) => c.classList.toggle('now', i === now));
    }
    play.classList.toggle('on', !!pl[0]);
  };
}
