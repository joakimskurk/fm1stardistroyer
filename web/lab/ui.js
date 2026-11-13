// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joakim (joakimskurk)
// The lab's controls and pictures, each bound to a field of the patch by its C path (web/app/web_lab.h's table):
// a knob, a switch of a few, a toggle, a list, a stepper; the nav's field, an envelope, the filter, the Reactor's
// stages, DISTROY's bands, an LFO's shape. A control reads the patch when the page refreshes (update) and writes it
// through the page (edit), so the panel's knobs, undo and A/B all go through one place (lab.js).
import { get, show } from './patch.js';

export const controls = [];                      // { update(bytes, orig) }
let P = null;                                    // the page: field(path), edit([[f, v], ...]), bytes(), orig(), enums
export function bind(page) { P = page; }
export const h = (tag, cls, html) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (html != null) e.innerHTML = html;
  return e;
};
const grabPointer = (el, e) => { try { el.setPointerCapture(e.pointerId); } catch (_) { /* (a pointer gone already) */ } };
const NOTE = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
export const hz = (m) => 440 * 2 ** ((m - 69) / 12);
export const hzText = (f) => (f < 1000 ? `${f < 100 ? f.toFixed(1) : f.toFixed(0)} Hz` : `${(f / 1000).toFixed(f < 10000 ? 2 : 1)} kHz`);
export const noteText = (m) => { const n = Math.round(m); return `${NOTE[((n % 12) + 12) % 12]}${Math.floor(n / 12) - 1}`; };
export const envMs = (i) => 1000 * 0.001 * (10.0 / 0.001) ** (i / 127);   // tools/gen_tables.py: 1 ms .. 10 s
export const msText = (ms) => (ms < 1000 ? `${ms < 10 ? ms.toFixed(1) : ms.toFixed(0)} ms` : `${(ms / 1000).toFixed(ms < 10000 ? 2 : 1)} s`);
export const p99 = (v) => String(Math.round((v * 99) / 65535));

// a value of the patch: a field, or some of its bits; decoded and encoded where the field packs it. (o.src: a value
// that is not the patch's, as the sequencer's: its own get, set, text, lo, hi, reset)
export function acc(path, o = {}) {
  if (o.src) return o.src;
  const f = P.field(path);
  if (!f) throw new Error(`the lab has no field ${path}`);
  const sh = o.bits ? o.bits[0] : 0, mask = o.bits ? (1 << o.bits[1]) - 1 : null;
  const rd = (b) => { let v = get(b, f); if (mask !== null) v = (v >> sh) & mask; return o.dec ? o.dec(v) : v; };
  const a = {
    f, lo: o.lo ?? (mask !== null ? 0 : f.lo), hi: o.hi ?? (mask !== null ? mask : f.hi),
    get: rd,
    raw(v) {                                     // the field's value with this one in it
      v = Math.round(Math.max(a.lo, Math.min(a.hi, v)));
      let r = o.enc ? o.enc(v) : v;
      if (mask !== null) r = (get(P.bytes(), f) & ~(mask << sh)) | ((r & mask) << sh);
      return r;
    },
    set(v) { P.edit([[f, a.raw(v)]]); },
    text: (v) => (o.fmt ? o.fmt(v) : show(f, v, P.enums)),
    changed: (b, w) => rd(b) !== rd(w),
  };
  return a;
}

// ---- a knob: drag up or down (Shift: finer), the wheel, the arrows; a double click: as loaded
const R = 15.5, ang = (p) => (-135 + 270 * p) * Math.PI / 180;
const pt = (p, r = R) => [20 + r * Math.sin(ang(p)), 20 - r * Math.cos(ang(p))];
function arc(p0, p1) {
  if (Math.abs(p1 - p0) < 0.002) return '';
  const [x0, y0] = pt(p0), [x1, y1] = pt(p1);
  return `M${x0.toFixed(2)} ${y0.toFixed(2)}A${R} ${R} 0 ${Math.abs(p1 - p0) * 270 > 180 ? 1 : 0} ${p1 > p0 ? 1 : 0} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}
export function knob(path, label, o = {}) {
  const a = acc(path, o), bip = o.bip ?? (a.lo < 0 && a.hi > 0);
  const lg = o.taper === 'log', lo = Math.max(lg ? 1 : a.lo, a.lo), span = a.hi - a.lo;
  const pos = (v) => (lg ? Math.log(Math.max(lo, v) / lo) / Math.log(a.hi / lo) : (v - a.lo) / span);
  const val = (p) => { p = Math.max(0, Math.min(1, p)); return lg ? lo * (a.hi / lo) ** p : a.lo + p * span; };
  const w = h('div', `kn ${o.size || 'm'}`);
  w.innerHTML = `<svg viewBox="0 0 40 40" aria-hidden="true"><path class="tr" d="${arc(0, 1)}"/><path class="va"/>` +
    `<circle class="cap" cx="20" cy="20" r="11"/><line class="kp" x1="20" y1="20" x2="20" y2="11.5"/></svg>` +
    `<b>${label}</b><output></output>`;
  w.tabIndex = 0;
  w.setAttribute('role', 'slider');
  w.setAttribute('aria-label', label);
  w.title = o.title || `${label} (${a.f.path}): drag or scroll; Shift finer; double-click back to the patch as loaded`;
  const va = w.querySelector('.va'), pn = w.querySelector('.kp'), out = w.querySelector('output');
  let cur = 0, drag = null;
  const step = Math.max(1, Math.round(span / 200));
  w.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    w.focus({ preventScroll: true });
    grabPointer(w, e);
    drag = { y: e.clientY, x: e.clientX, p: pos(cur) };
    w.classList.add('drag');
  });
  w.addEventListener('pointermove', (e) => {
    if (!drag) return;
    const d = (drag.y - e.clientY) + (e.clientX - drag.x) * 0.3;
    a.set(val(drag.p + d / (e.shiftKey ? 1600 : 200)));
  });
  const end = () => { drag = null; w.classList.remove('drag'); };
  w.addEventListener('pointerup', end);
  w.addEventListener('pointercancel', end);
  w.addEventListener('wheel', (e) => {
    e.preventDefault();
    const n = Math.sign(-e.deltaY || e.deltaX);
    if (!n) return;
    if (span <= 64 && !lg) a.set(cur + n);
    else a.set(val(pos(cur) + n / (e.shiftKey ? 400 : 100)));
  }, { passive: false });
  w.addEventListener('dblclick', () => (a.reset ? a.reset() : a.set(a.get(P.orig()))));
  w.addEventListener('keydown', (e) => {
    const n = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: 10, PageDown: -10 }[e.key];
    if (!n) return;
    e.preventDefault();
    if (span <= 64 && !lg) a.set(cur + n); else a.set(val(pos(cur) + n / 100));
  });
  controls.push({
    update(b, orig) {
      cur = Math.max(a.lo, Math.min(a.hi, a.get(b)));   // (a field left at 0 under its range: the engine's floor)
      const p = Math.max(0, Math.min(1, pos(cur)));
      va.setAttribute('d', bip ? (p >= 0.5 ? arc(0.5, p) : arc(p, 0.5)) : arc(0, p));
      const [x, y] = pt(p, 8.5);
      pn.setAttribute('x2', x.toFixed(2)); pn.setAttribute('y2', y.toFixed(2));
      out.textContent = a.text(cur);
      w.classList.toggle('ch', a.changed(b, orig));
      w.setAttribute('aria-valuetext', out.textContent);
    },
  });
  return w;
}

// ---- a switch of a few: its names (the field's list by default), one lit
export function seg(path, label, o = {}) {
  const a = acc(path, o), names = o.names || P.enums[a.f.list] || [], vals = o.values || names.map((_, i) => a.lo + i);
  const w = h('div', `seg${o.cls ? ' ' + o.cls : ''}`);
  if (label) w.append(h('b', '', label));
  const row = h('div', 'opts');
  const bs = names.map((n, i) => {
    const b = h('button', '', n);
    b.type = 'button';
    b.title = `${label || a.f.name}: ${n} (${a.f.path})`;
    b.addEventListener('click', () => a.set(vals[i]));
    row.append(b);
    return b;
  });
  w.append(row);
  controls.push({
    update(b, orig) {
      const v = a.get(b);
      bs.forEach((x, i) => x.classList.toggle('on', vals[i] === v));
      w.classList.toggle('ch', a.changed(b, orig));
    },
  });
  return w;
}
// ---- a toggle: a bit (or a field 0 / 1); inv: lit when the bit is clear (the engines' ON, fx_off's bits)
export function toggle(path, label, o = {}) {
  const a = acc(path, o), w = h('button', `tog${o.cls ? ' ' + o.cls : ''}`, `<i></i>${label}`);
  w.type = 'button';
  w.title = o.title || `${label} (${a.f.path})`;
  const on = (v) => (o.inv ? !v : !!v);
  w.addEventListener('click', () => a.set(on(a.get(P.bytes())) ? (o.inv ? 1 : 0) : (o.inv ? 0 : (o.onValue ?? 1))));
  controls.push({
    update(b, orig) {
      w.classList.toggle('on', on(a.get(b)));
      w.classList.toggle('ch', a.changed(b, orig));
    },
  });
  return w;
}
// ---- a list: a <select> of its names (values: what each one writes)
export function pick(path, label, o = {}) {
  const a = acc(path, o), names = o.names || P.enums[a.f.list] || [], vals = o.values || names.map((_, i) => a.lo + i);
  const w = h('label', `pick${o.cls ? ' ' + o.cls : ''}`);
  if (label) w.append(h('b', '', label));
  const s = h('select');
  s.innerHTML = names.map((n, i) => `<option value="${vals[i]}">${n}</option>`).join('');
  s.title = `${label || a.f.name} (${a.f.path})`;
  s.addEventListener('change', () => a.set(Number(s.value)));
  w.append(s);
  controls.push({
    update(b, orig) {
      const v = a.get(b);
      if (Number(s.value) !== v) s.value = String(v);
      w.classList.toggle('ch', a.changed(b, orig));
    },
  });
  return w;
}
// ---- a stepper: a number, its arrows, drag or the wheel
export function stepper(path, label, o = {}) {
  const a = acc(path, o), w = h('div', `stp${o.cls ? ' ' + o.cls : ''}`);
  w.innerHTML = `${label ? `<b>${label}</b>` : ''}<div class="box"><button type="button" aria-label="less">◂</button><output></output>` +
    `<button type="button" aria-label="more">▸</button></div>`;
  const [dn, up] = w.querySelectorAll('button'), out = w.querySelector('output');
  let cur = 0, drag = null;
  dn.addEventListener('click', () => a.set(cur - 1));
  up.addEventListener('click', () => a.set(cur + 1));
  out.title = o.title || `${label || a.f.name} (${a.f.path}): drag, scroll or the arrows; double-click: as loaded`;
  out.addEventListener('pointerdown', (e) => { e.preventDefault(); grabPointer(out, e); drag = { y: e.clientY, v: cur }; });
  out.addEventListener('pointermove', (e) => { if (drag) a.set(drag.v + Math.round((drag.y - e.clientY) / 8)); });
  out.addEventListener('pointerup', () => { drag = null; });
  out.addEventListener('wheel', (e) => { e.preventDefault(); a.set(cur + Math.sign(-e.deltaY || e.deltaX)); }, { passive: false });
  out.addEventListener('dblclick', () => (a.reset ? a.reset() : a.set(a.get(P.orig()))));
  controls.push({
    update(b, orig) {
      cur = a.get(b);
      out.textContent = a.text(cur);
      w.classList.toggle('ch', a.changed(b, orig));
    },
  });
  return w;
}
// ---- tabs: one of several panes shown, a dot on a tab whose pane holds a change
export function tabs(names, panes, o = {}) {
  const w = h('div', `tabs${o.cls ? ' ' + o.cls : ''}`), bar = h('div', 'tabbar'), box = h('div', 'panes');
  let sel = 0;
  const bs = names.map((n, i) => {
    const b = h('button', '', n);
    b.type = 'button';
    b.addEventListener('click', () => choose(i));
    bar.append(b);
    return b;
  });
  if (o.extra) bar.append(o.extra);
  panes.forEach((p) => box.append(p));
  function choose(i) {
    sel = i;
    bs.forEach((b, k) => b.classList.toggle('on', k === i));
    panes.forEach((p, k) => { p.hidden = k !== i; });
    if (o.onChoose) o.onChoose(i);
  }
  choose(0);
  w.append(bar, box);
  controls.push({ update() { panes.forEach((p, k) => bs[k].classList.toggle('ch', !!p.querySelector('.ch'))); } });
  w.choose = choose;
  w.sel = () => sel;
  return w;
}

// ---- the pictures
const svg = (w, ht, cls) => {
  const s = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  s.setAttribute('viewBox', `0 0 ${w} ${ht}`);
  s.setAttribute('class', cls);
  if (cls.startsWith('graph')) s.setAttribute('preserveAspectRatio', 'none');
  return s;
};
const node = (s, tag, attrs) => {
  const e = document.createElementNS('http://www.w3.org/2000/svg', tag);
  for (const k in attrs) e.setAttribute(k, attrs[k]);
  s.append(e);
  return e;
};

// the nav's field, as the FM-1 draws it: a diamond, A at the top, B right, C below, D left; the banks' mix in it
// (the engine's bilinear weights), the point (X/Y) to drag; COURSE's four cells when it is on
export function navPad() {
  const S = 200, cx = 100, cy = 100, Rr = 84;
  const s = svg(S, S, 'navpad'), X = acc('.x'), Y = acc('.y'), CO = acc('.course');
  node(s, 'path', { class: 'grid', d: `M${cx - Rr} ${cy}H${cx + Rr}M${cx} ${cy - Rr}V${cy + Rr}M${cx - Rr / 2} ${cy - Rr / 2}L${cx + Rr / 2} ${cy + Rr / 2}M${cx + Rr / 2} ${cy - Rr / 2}L${cx - Rr / 2} ${cy + Rr / 2}` });
  node(s, 'path', { class: 'edge', d: `M${cx} ${cy - Rr}L${cx + Rr} ${cy}L${cx} ${cy + Rr}L${cx - Rr} ${cy}Z` });
  const mix = node(s, 'path', { class: 'mix' });
  [['A', cx, cy - Rr - 6], ['B', cx + Rr + 8, cy + 4], ['C', cx, cy + Rr + 14], ['D', cx - Rr - 8, cy + 4]].forEach(([t, x, y]) => {
    const e = node(s, 'text', { x, y, class: 'corner', 'text-anchor': 'middle' });
    e.textContent = t;
  });
  const cells = [0, 1, 2, 3].map((k) => {
    const g = node(s, 'g', { class: 'cell' });
    node(g, 'circle', { r: 7 });
    const t = node(g, 'text', { 'text-anchor': 'middle', y: 3.5 });
    t.textContent = String(k + 1);
    return { g, x: acc(`.cell_x[${k}]`), y: acc(`.cell_y[${k}]`) };
  });
  const path = node(s, 'path', { class: 'route' });
  const dot = node(s, 'g', { class: 'dot' });
  node(dot, 'circle', { r: 9, class: 'halo' });
  node(dot, 'rect', { x: -4, y: -4, width: 8, height: 8 });
  const to = (x, y) => [cx + ((x + y - 65535) * Rr) / 65536, cy + ((x - y) * Rr) / 65536];
  const from = (px, py) => {                     // a point in the diamond back to X and Y (clamped to the field)
    const u = ((px - cx) / Rr) * 65536, v = ((py - cy) / Rr) * 65536;
    const c = (t) => Math.max(0, Math.min(65535, Math.round(t)));
    return [c((u + v) / 2 + 32767.5), c((u - v) / 2 + 32767.5)];
  };
  let grab = null;
  const at = (e) => { const r = s.getBoundingClientRect(); return [((e.clientX - r.left) / r.width) * S, ((e.clientY - r.top) / r.height) * S]; };
  s.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    grabPointer(s, e);
    const t = e.target.closest('.cell');
    grab = t ? cells.find((c) => c.g === t) : 'nav';
    move(e);
  });
  const move = (e) => {
    if (!grab) return;
    const [x, y] = from(...at(e));
    if (grab === 'nav') P.edit([[X.f, x], [Y.f, y]]);
    else P.edit([[grab.x.f, x], [grab.y.f, y]]);
  };
  s.addEventListener('pointermove', move);
  s.addEventListener('pointerup', () => { grab = null; });
  s.addEventListener('dblclick', () => { const o = P.orig(); P.edit([[X.f, X.get(o)], [Y.f, Y.get(o)]]); });
  s.setAttribute('aria-label', 'The nav: drag the point between the four banks');
  controls.push({
    update(b, orig) {
      const x = X.get(b), y = Y.get(b), xq = x / 65535, yq = y / 65535;
      const w = [(1 - xq) * yq, xq * yq, xq * (1 - yq), (1 - xq) * (1 - yq)];
      const n = Math.sqrt(Math.sqrt(w.reduce((t, q) => t + q * q, 0))) || 1;
      const q = w.map((v) => Math.min(1, v / n) * Rr);
      mix.setAttribute('d', `M${cx} ${cy - q[0]}L${cx + q[1]} ${cy}L${cx} ${cy + q[2]}L${cx - q[3]} ${cy}Z`);
      const [px, py] = to(x, y);
      dot.setAttribute('transform', `translate(${px.toFixed(1)} ${py.toFixed(1)})`);
      s.classList.toggle('ch', X.changed(b, orig) || Y.changed(b, orig));
      const on = CO.get(b) !== 0;
      let d = '';
      cells.forEach((c, k) => {
        const [qx, qy] = to(c.x.get(b), c.y.get(b));
        c.g.setAttribute('transform', `translate(${qx.toFixed(1)} ${qy.toFixed(1)})`);
        c.g.style.display = on ? '' : 'none';
        d += `${k ? 'L' : 'M'}${qx.toFixed(1)} ${qy.toFixed(1)}`;
      });
      path.setAttribute('d', on ? d + 'Z' : '');
    },
  });
  return s;
}
// the banks' weights at the nav's point (for the bank tabs' bars)
export function navWeights(b) {
  const xq = get(b, P.field('.x')) / 65535, yq = get(b, P.field('.y')) / 65535;
  const w = [(1 - xq) * yq, xq * yq, xq * (1 - yq), (1 - xq) * (1 - yq)];
  const n = Math.sqrt(Math.sqrt(w.reduce((t, q) => t + q * q, 0))) || 1;
  return w.map((v) => Math.min(1, v / n));
}

// an envelope: attack, decay, sustain, release, on a log time axis (1 ms .. 10 s each)
export function envGraph(pre) {
  const W = 220, H = 56, s = svg(W, H, 'graph env');
  const A = acc(`.${pre}_a`), D = acc(`.${pre}_d`), S = acc(`.${pre}_s`), Rl = acc(`.${pre}_r`);
  node(s, 'path', { class: 'base', d: `M0 ${H - 4}H${W}` });
  const fill = node(s, 'path', { class: 'fill' }), line = node(s, 'path', { class: 'line' });
  controls.push({
    update(b) {
      const seg = (v) => 6 + (v / 127) * 46;     // a segment's width: its time's log
      const a = seg(A.get(b)), d = seg(D.get(b)), r = seg(Rl.get(b)), hold = 34, top = 5, bot = H - 4;
      const sy = bot - (S.get(b) / 127) * (bot - top);
      const x1 = 2 + a, x2 = x1 + d, x3 = x2 + hold, x4 = Math.min(W - 2, x3 + r);
      const p = `M2 ${bot}L${x1.toFixed(1)} ${top}Q${(x1 + d * 0.25).toFixed(1)} ${sy.toFixed(1)} ${x2.toFixed(1)} ${sy.toFixed(1)}` +
        `L${x3.toFixed(1)} ${sy.toFixed(1)}Q${(x3 + r * 0.2).toFixed(1)} ${bot} ${x4.toFixed(1)} ${bot}`;
      line.setAttribute('d', p);
      fill.setAttribute('d', p + 'Z');
    },
  });
  return s;
}
// the filter: its lowpass as it stands (12 or 24 dB an octave), the resonance's peak at the cutoff
export function filterGraph() {
  const W = 260, H = 64, s = svg(W, H, 'graph filt');
  const T = acc('.f_type'), C = acc('.f_cut'), Q = acc('.f_res'), OFF = acc('.fx_off', { bits: [3, 1] });
  node(s, 'path', { class: 'grid', d: [100, 1000, 10000].map((f) => { const x = xf(f, W); return `M${x} 0V${H}`; }).join('') });
  const fill = node(s, 'path', { class: 'fill' }), line = node(s, 'path', { class: 'line' });
  controls.push({
    update(b) {
      const t = T.get(b), fc = hz(16 + (C.get(b) * 114) / 65535), q = Q.get(b) / 65535, off = !t || OFF.get(b);
      let d = '';
      for (let i = 0; i <= 64; i++) {
        const x = (i / 64) * W, f = fx(x, W), r = f / fc;
        let g = 1;
        if (!off) {
          const n = t === 2 ? 4 : 2, peak = 1 + q * 6 / (1 + ((r - 1) * 8) ** 2);
          g = (1 / Math.sqrt(1 + r ** (2 * n))) * peak;
        }
        const db = 20 * Math.log10(Math.max(1e-4, g)), y = 18 - db * 1.1;
        d += `${i ? 'L' : 'M'}${x.toFixed(1)} ${Math.max(2, Math.min(H - 2, y)).toFixed(1)}`;
      }
      line.setAttribute('d', d);
      fill.setAttribute('d', `${d}L${W} ${H}L0 ${H}Z`);
      s.classList.toggle('off', off);
    },
  });
  return s;
}
function xf(f, W) { return (Math.log(f / 20) / Math.log(20000 / 20)) * W; }   // 20 Hz .. 20 kHz across
function fx(x, W) { return 20 * (20000 / 20) ** (x / W); }

// the Reactor, as the FM-1 draws it: its six stages as columns, each one's four filters a block in its own lane at
// its pitch, on a grid (unlit cells around them); the stage POS plays framed red, the one it moves to dark red; the
// blocks of a filter at level 0 dark. Square cells: the picture keeps its own proportions.
export function reactorGraph(onStage) {
  const W = 420, H = 116, ROWS = 16, s = svg(W, H, 'graph react');
  s.setAttribute('preserveAspectRatio', 'xMidYMid meet');
  const POS = acc('.r_pos'), cols = [];
  const GAP = 6, CW = (W - 4 - 5 * GAP) / 6, PAD = 5, LG = 3, LW = (CW - 2 * PAD - 3 * LG) / 4, Y0 = 5, RH = 6;
  const row = (v) => {                           // a pitch to its row: SD_R_LO (16) at the bottom, SD_R_HI (130) at the top
    const t = (Math.max(4096, Math.min(33280, v)) / 256 - 16) / (130 - 16);
    return Math.round((1 - t) * (ROWS - 1));
  };
  for (let k = 0; k < 6; k++) {
    const x0 = 2 + k * (CW + GAP), g = node(s, 'g', { class: 'stage' });
    node(g, 'rect', { x: x0 + 0.5, y: 0.5, width: CW - 1, height: Y0 + ROWS * RH + 2, class: 'fr' });
    let grid = '';
    for (let j = 0; j < 4; j++) for (let r = 0; r < ROWS; r++) grid += `M${(x0 + PAD + j * (LW + LG)).toFixed(2)} ${Y0 + r * RH}h${LW.toFixed(2)}v${RH - 1}h${(-LW).toFixed(2)}Z`;
    node(g, 'path', { class: 'cells', d: grid });
    const blk = [0, 1, 2, 3].map((j) => ({
      r: node(g, 'rect', { x: (x0 + PAD + j * (LW + LG)).toFixed(2), width: LW.toFixed(2), height: RH - 1, class: 'blk' }),
      p: acc(`.stage[${k}].pitch[${j}]`), l: acc(`.stage[${k}].level[${j}]`),
    }));
    const t = node(g, 'text', { x: x0 + CW / 2, y: H - 2, 'text-anchor': 'middle' });
    t.textContent = String(k + 1);
    g.addEventListener('click', () => onStage && onStage(k));
    cols.push({ g, blk });
  }
  const cur = node(s, 'path', { class: 'pos' });
  const c = {
    update(b) {
      const pos = POS.get(b), at = pos >> 8, f = pos & 255, nx = at === 5 ? 0 : at + 1;
      const dom = f < 128 ? at : nx, sec = f ? (f < 128 ? nx : at) : -1;   // as the FM-1's screen marks them
      cols.forEach((q, k) => {
        q.g.classList.toggle('dom', k === dom);
        q.g.classList.toggle('sec', k === sec);
        q.blk.forEach((d) => {
          d.r.setAttribute('y', Y0 + row(d.p.get(b)) * RH);
          d.r.setAttribute('class', `blk${d.l.get(b) ? '' : ' zero'}`);
          d.r.style.opacity = d.l.get(b) ? (0.45 + 0.55 * Math.min(1, d.l.get(b) / 32768)).toFixed(2) : '';
        });
      });
      const x = 2 + (pos / 256) * (CW + GAP) + CW / 2, y = Y0 + ROWS * RH + 3;   // POS: a tick under the columns
      cur.setAttribute('d', `M${x.toFixed(1)} ${y}l-3 4h6Z`);
    },
    select(k) { cols.forEach((q, i) => q.g.classList.toggle('sel', i === k)); },
  };
  controls.push(c);
  s.select = (k) => c.select(k);
  return s;
}

// DISTROY: the spectrum cut in four bands at its three splits (drag one; they sort as the engine does); each band's
// shaper named in it and its drive as its height
export function distroyGraph() {
  const W = 520, H = 96, s = svg(W, H, 'graph dist');
  const SP = [0, 1, 2].map((k) => acc(`.d_split[${k}]`));
  const TY = [0, 1, 2, 3].map((k) => acc(`.d_type[${k}]`)), GA = [0, 1, 2, 3].map((k) => acc(`.d_gain[${k}]`));
  const LV = [0, 1, 2, 3].map((k) => acc(`.d_level[${k}]`));
  node(s, 'path', { class: 'grid', d: [50, 100, 200, 500, 1000, 2000, 5000, 10000].map((f) => `M${xf(f, W).toFixed(1)} 0V${H - 14}`).join('') });
  [[100, '100'], [1000, '1k'], [10000, '10k']].forEach(([f, t]) => { const e = node(s, 'text', { x: xf(f, W), y: H - 2, 'text-anchor': 'middle', class: 'ax' }); e.textContent = t; });
  const bands = [0, 1, 2, 3].map(() => ({ r: node(s, 'rect', { class: 'band' }), t: node(s, 'text', { class: 'bt', 'text-anchor': 'middle' }) }));
  const lines = SP.map((a, k) => {
    const g = node(s, 'g', { class: 'split' });
    node(g, 'line', { y1: 0, y2: H - 14 });
    node(g, 'rect', { x: -5, y: 0, width: 10, height: H - 14, class: 'hit' });
    const t = node(g, 'text', { y: 10, 'text-anchor': 'middle' });
    t.textContent = String(k + 1);
    return { g, a, t };
  });
  let grab = null;
  const at = (e) => { const r = s.getBoundingClientRect(); return ((e.clientX - r.left) / r.width) * W; };
  s.addEventListener('pointerdown', (e) => {
    const g = e.target.closest('.split');
    if (!g) return;
    e.preventDefault();
    grabPointer(s, e);
    grab = lines.find((l) => l.g === g);
  });
  s.addEventListener('pointermove', (e) => {
    if (!grab) return;
    const f = fx(Math.max(0, Math.min(W, at(e))), W), m = 69 + 12 * Math.log2(f / 440);
    grab.a.set(Math.round(m * 256));
  });
  s.addEventListener('pointerup', () => { grab = null; });
  controls.push({
    update(b) {
      const cl = (v) => Math.max(4096, Math.min(33280, v));   // as the engine reads a split (SD_R_LO .. SD_R_HI)
      const sp = SP.map((a) => cl(a.get(b))).sort((p, q) => p - q), xs = [0, ...sp.map((v) => xf(hz(v / 256), W)), W];
      const names = P.enums[TY[0].f.list] || [];
      bands.forEach((bd, k) => {
        const g = GA[k].get(b) / 65535, lv = Math.min(2, LV[k].get(b) / 32768), t = TY[k].get(b);
        const ht = 10 + g * 60 * Math.min(1, 0.35 + lv * 0.5), x0 = xs[k], x1 = xs[k + 1];
        bd.r.setAttribute('x', (x0 + 1).toFixed(1)); bd.r.setAttribute('width', Math.max(0, x1 - x0 - 2).toFixed(1));
        bd.r.setAttribute('y', (H - 14 - ht).toFixed(1)); bd.r.setAttribute('height', ht.toFixed(1));
        bd.r.setAttribute('class', `band b${k}${t ? '' : ' none'}`);
        bd.t.setAttribute('x', ((x0 + x1) / 2).toFixed(1)); bd.t.setAttribute('y', (H - 20 - ht).toFixed(1));
        bd.t.textContent = x1 - x0 > 34 ? names[t] || '' : '';
      });
      lines.forEach((l) => { const x = xf(hz(cl(l.a.get(b)) / 256), W); l.g.setAttribute('transform', `translate(${x.toFixed(1)} 0)`); });
    },
  });
  return s;
}
// an LFO's shape, drawn
export function lfoGraph(k) {
  const W = 120, H = 40, s = svg(W, H, 'graph lfo'), SH = acc(`.lfo_shape[${k}]`);
  const line = node(s, 'path', { class: 'line' });
  node(s, 'path', { class: 'base', d: `M0 ${H / 2}H${W}` });
  const f = [(t) => Math.sin(2 * Math.PI * t), (t) => 1 - 4 * Math.abs(((t + 0.25) % 1) - 0.5), (t) => 1 - 2 * (t % 1),
    (t) => 2 * (t % 1) - 1, (t) => ((t % 1) < 0.5 ? 1 : -1), (t) => [0.6, -0.3, 0.9, -0.8, 0.2, -0.5, 0.7, -0.1][Math.floor((t % 1) * 4 + Math.floor(t) * 4) % 8]];
  controls.push({
    update(b) {
      const g = f[SH.get(b)] || f[0];
      let d = '';
      for (let i = 0; i <= 96; i++) { const t = (i / 96) * 2, y = H / 2 - g(t) * (H / 2 - 4); d += `${i ? 'L' : 'M'}${((i / 96) * W).toFixed(1)} ${y.toFixed(1)}`; }
      line.setAttribute('d', d);
    },
  });
  return s;
}

// ---- what moves as it plays (redrawn each frame from the firmware's state, not only when the patch changes)
export const lives = [];                         // () => void, each frame

// TURING, as the FM-1 draws it: its last 16 values a stepped line (the newest red), the loop's bits under it (bit 0
// at the right, going left; the 8 the value reads brighter); dark while it is off
export function turGraph(st) {                   // st(): { hist (16 values, -32767 .. 32767), reg }
  const W = 256, H = 76, Y = 32, s = svg(W, H, 'graph tur');
  s.setAttribute('preserveAspectRatio', 'xMidYMid meet');
  const ON = acc('.tur_set', { bits: [7, 1] }), LN = acc('.tur_ld', { bits: [0, 5], dec: (v) => (v ? Math.max(2, Math.min(16, v)) : 8) });
  node(s, 'path', { class: 'base', d: `M0 ${Y}H${W}` });
  const joins = node(s, 'path', { class: 'joins' }), steps = node(s, 'path', { class: 'steps' }), last = node(s, 'rect', { class: 'last', width: 16, height: 3 });
  const bits = [...Array(16)].map((_, i) => node(s, 'rect', { x: (15 - i) * 16 + 1, y: 60, width: 14, height: 14, class: 'bit' }));
  const draw = () => {
    const b = P.bytes(), t = st(), on = !!ON.get(b), n = LN.get(b), y = (v) => (Y - (v * 26) / 32767).toFixed(1);
    let d = '', j = '';
    t.hist.forEach((v, i) => {
      if (i < 15) d += `M${i * 16} ${y(v)}h16`;
      if (i) j += `M${i * 16} ${y(t.hist[i - 1])}V${y(v)}`;
    });
    steps.setAttribute('d', d);
    joins.setAttribute('d', j);
    last.setAttribute('x', 240); last.setAttribute('y', (+y(t.hist[15]) - 1.5).toFixed(1));
    bits.forEach((r, i) => {
      r.style.display = i < n ? '' : 'none';
      r.setAttribute('class', `bit${(t.reg >> i) & 1 ? (i < 8 ? ' one' : ' one far') : ''}`);
    });
    s.classList.toggle('off', !on);
  };
  lives.push(draw);
  controls.push({ update: draw });
  return s;
}

// TIME: the echoes as they fall by FDBK along 2 s (ping-pong: in pairs, left red, right ice), as the FM-1 draws them
export function echoGraph(bpm) {                 // bpm(): the tempo, for a synced TIME
  const W = 300, H = 44, s = svg(W, H, 'graph echo');
  const TI = acc('.s_time'), FB = acc('.s_fb'), DL = acc('.s_dlevel'), MO = acc('.s_mode');
  const BEATS = [0.125, 1 / 6, 0.25, 1 / 3, 0.375, 0.5, 2 / 3, 0.75, 1, 4 / 3, 1.5, 2];   // 1/32 .. 1/2
  node(s, 'path', { class: 'base', d: `M0 ${H - 1}H${W}` });
  const red = node(s, 'path', { class: 'ered' }), ice = node(s, 'path', { class: 'eice' });
  controls.push({
    update(b) {
      const mo = MO.get(b), sync = mo & 1, pp = mo & 2;
      let ms = sync ? (60000 / Math.max(40, bpm() || 120)) * BEATS[(TI.get(b) * 12) >> 16] : 1 + Math.floor((TI.get(b) * 999) / 65535);
      while (sync && ms > 1000) ms /= 2;          // (a halving when long, as the engine plays it)
      const fb = (FB.get(b) * 31130) / 65536 / 32768;
      let h = (DL.get(b) * (H - 6)) / 65536, r = '', c = '';
      for (let i = 1; ms && i * ms < 2000 && h > 1; i++) {
        const x = ((i * ms * W) / 2000).toFixed(1), seg = `M${x} ${H - 1}V${(H - 1 - h).toFixed(1)}`;
        if (pp && i % 2 === 0) c += seg; else r += seg;
        if (!pp) h *= fb; else if (i % 2 === 0) h *= fb * fb;
      }
      red.setAttribute('d', r); ice.setAttribute('d', c);
    },
  });
  return s;
}

// the looper: its loop's outline (what LOOP holds, or would hold: the last LENGTH as it records), the head in it while
// it loops, and what it is doing
const muMag = (c) => { const ex = (c >> 4) & 7; return (((16 + (c & 15)) << (ex + 1)) + (1 << ex) - 33) << 2; };   // sd_mu_dec, Q14
export function loopGraph(lp, sets) {            // lp(): the looper's state (env: 240 peaks); sets(): LOOP REV L-LEVEL
  const W = 480, H = 84, M = 50, s = svg(W, H, 'graph loop');
  const wave = node(s, 'path', { class: 'wave' }), head = node(s, 'path', { class: 'head' });
  const tag = node(s, 'text', { x: 6, y: 13, class: 'tag' }), sub = node(s, 'text', { x: W - 6, y: 13, class: 'sub', 'text-anchor': 'end' });
  node(s, 'path', { class: 'base', d: `M0 ${M}H${W}` });
  let envKey = null;
  const draw = () => {
    const o = lp(), st = sets();
    if (!o) return;
    if (o.env && o.env !== envKey) {             // (a new outline: a few times a second)
      envKey = o.env;
      let d = '';
      const n = o.env.length;
      for (let i = 0; i < n; i++) {
        const v = muMag(o.env[i]) / 16384, db = v > 0 ? Math.max(0, (20 * Math.log10(v) + 48) / 48) : 0, hh = Math.min(1, db) * (H - M - 3);
        if (hh > 0.4) d += `M${((i + 0.5) * W / n).toFixed(1)} ${(M - hh).toFixed(1)}V${(M + hh).toFixed(1)}`;
      }
      wave.setAttribute('d', d);
    }
    const armed = st.lvl > 0, looping = o.on && armed;
    if (looping && o.ln) {
      const x = ((o.k / o.ln) * W).toFixed(1);
      head.setAttribute('d', `M${x} 18V${H - 2}`);
    } else head.setAttribute('d', '');
    s.classList.toggle('looping', looping);
    s.classList.toggle('idle', !armed);
    tag.textContent = !armed ? 'L-LEVEL 0: THE LOOPER NOT ARMED' : o.park ? 'LOOP PARKED: PLAY TAKES IT ON' : looping ? `LOOP${o.rev ? ' · REV' : ''}` :
      o.wait ? 'LOOP AT THE NEXT LINE…' : 'RECORDING: LOOP HOLDS THE LAST LENGTH';
    s.classList.toggle('waiting', !!o.wait);
    const sec = (looping ? o.ln : o.len) / 256 / 22050;
    sub.textContent = armed ? `${sec.toFixed(2)} s${looping && o.rate ? ` · ×${(o.rate / 65536).toFixed(2)}` : ''}` : '';
  };
  lives.push(draw);
  return s;
}
