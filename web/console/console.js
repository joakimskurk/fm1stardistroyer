// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 joakimskurk
// CONSOLE (W1): everything the FM-1 does, at once, a ship's bridge of 1600 x 900 scaled to the window. Its frames
// (frame.js) come from an FM-1 over USB MIDI (Web MIDI with SysEx: the page asks each second, the FM-1 sends 25 a
// second while asked) or from the player under it (the firmware in this browser, ../app/player.js). Read only.
import { newState, decode, isFrame, ASK, STOP, SRC_NAME, dstName, LSHAPE, SCALE, ROUTE, SHAPER, RMODEL, MAC4, MODE, KEY,
         SPDIV, SDIV, AMODE, PAGE } from './frame.js';

const $ = (id) => document.getElementById(id);
const ICE = '#cde6f0', DIM = '#8fa5b2', MUTE = '#546874', RED = '#eb1c1c', GRID = '#1a2530', LINE = '#2a3642', DARK = '#0d141b';
const st = newState();
let src = '', lastAt = 0, dirty = false;
const navTrail = [], spaceHist = [], echoHist = [];

// ---- the stage: 1600 x 900, as one piece to the window's width
const stage = $('stage'), box = $('stagebox');
function fit() {
  const k = Math.min(1, box.clientWidth / 1600);
  stage.style.transform = `scale(${k})`;
  box.style.height = `${900 * k}px`;
}
addEventListener('resize', fit); fit();
const SZ = {};
for (const id of ['nav', 'voice', 'react', 'dist', 'time', 'space', 'out', 'mac', 'seqa', 'seqb', 'orbit', 'arp', 'mod']) {
  const e = $(id); SZ[id] = [e.clientWidth || 200, e.clientHeight || 100];
}

// ---- drawing: SVG as text
const t = (x, y, s, c = DIM, sz = 9, a = 'start', w = 600) => `<text x="${x.toFixed(1)}" y="${y.toFixed(1)}" fill="${c}" font-size="${sz}" text-anchor="${a}" font-weight="${w}">${s}</text>`;
const L = (x1, y1, x2, y2, c = GRID, w = 1, d = '') => `<line x1="${x1.toFixed(1)}" y1="${y1.toFixed(1)}" x2="${x2.toFixed(1)}" y2="${y2.toFixed(1)}" stroke="${c}" stroke-width="${w}"${d ? ` stroke-dasharray="${d}"` : ''}/>`;
const R = (x, y, w, h, f = 'none', s = 'none') => `<rect x="${x.toFixed(1)}" y="${y.toFixed(1)}" width="${Math.max(0, w).toFixed(1)}" height="${Math.max(0, h).toFixed(1)}" fill="${f}" stroke="${s}"/>`;
const C = (x, y, r, f = 'none', s = 'none', w = 1) => `<circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${r}" fill="${f}" stroke="${s}" stroke-width="${w}"/>`;
const P = (pts, s = ICE, w = 1.4, f = 'none') => pts.length ? `<path d="M${pts.map((p) => `${p[0].toFixed(1)},${p[1].toFixed(1)}`).join('L')}" fill="${f}" stroke="${s}" stroke-width="${w}" stroke-linejoin="round"/>` : '';
const put = (id, h) => { $(id).innerHTML = h; };
const pct = (v) => Math.round(v * 99 / 127);
const hz = (m) => { const f = 440 * Math.pow(2, (m - 69) / 12); return f >= 1000 ? `${(f / 1000).toFixed(1)}K` : `${Math.round(f)}`; };
const nn = (n) => n ? `${KEY[n % 12]}${Math.floor(n / 12) - 1}` : '—';
const fill = (on, a = .55) => on ? `rgba(205,230,240,${a})` : 'rgba(84,104,116,.45)';

function header() {
  $('h-patch').innerHTML = `<small>${st.patch < 29 ? 'P' + String(st.patch + 1).padStart(2, '0') : 'U' + String(st.patch - 28).padStart(2, '0')}</small>${st.name || '—'}`;
  $('h-play').className = 'lamp red' + (st.play ? ' on' : '');
  $('h-rec').className = 'lamp red' + (st.rec ? ' on' : '');
  $('h-tempo').innerHTML = st.tempo ? `${Math.round(st.tempo)} <small>${st.clock ? 'CLK' : 'BPM'}</small>` : '---';
  const used = st.voices.filter((n) => n).length;
  $('h-voices').innerHTML = `${MODE[st.mode] || '?'} <small>${used}/4</small>`;
  $('h-key').innerHTML = `${KEY[st.key] || '?'} <small>${SCALE[st.scale] || ''}</small>`;
  $('h-page').textContent = PAGE[st.page] || '—';
  $('h-cpu').innerHTML = st.cpu === 16383 ? '--' :
    `${Math.round(st.cpu / 10)}% <small>MAX ${Math.round(st.cpuMax / 10)}${st.wait !== 16383 ? ` · CORE 2 ${Math.round(st.wait / 10)}` : ''}</small>`;
  $('h-usb').className = 'lamp' + (src === 'usb' ? ' on' : '');
  $('h-player').className = 'lamp' + (src === 'player' ? ' on' : '');
}

function nav() {
  const [w, h] = SZ.nav, s = Math.min(w, h) - 24, x0 = (w - s) / 2, y0 = 4;
  let g = R(x0, y0, s, s, 'none', LINE);
  for (let i = 1; i < 8; i++) g += L(x0 + s * i / 8, y0, x0 + s * i / 8, y0 + s) + L(x0, y0 + s * i / 8, x0 + s, y0 + s * i / 8);
  [['A', 0, 1], ['B', 1, 1], ['C', 1, 0], ['D', 0, 0]].forEach(([n, cx, cy], k) => {
    const x = x0 + cx * s, y = y0 + (1 - cy) * s, b = st.banks[k];
    g += R(x - 9, y - 9, 18, 18, '#0b1017', ICE) + t(x, y + 4, n, ICE, 11, 'middle', 800);
    if (b) g += t(x + (cx ? -14 : 14), y + (cy ? 22 : -12), b.filter((o) => o.level).map((o) => o.harm).join(':'), MUTE, 8, cx ? 'end' : 'start');
  });
  const px = x0 + st.navX * s, py = y0 + (1 - st.navY) * s;
  navTrail.push([px, py]); if (navTrail.length > 60) navTrail.shift();
  g += P(navTrail, MUTE, 1.2) + L(px, y0, px, y0 + s, '#3a4a58', 1, '2 3') + L(x0, py, x0 + s, py, '#3a4a58', 1, '2 3');
  g += C(px, py, 9, 'none', RED, 1.5) + C(px, py, 3.5, ICE);
  put('nav', g);
  $('n-nav').textContent = `${st.on.course ? 'COURSE · ' : ''}X ${st.navX.toFixed(2)} Y ${st.navY.toFixed(2)}`;
}

function voice() {
  const [w, h] = SZ.voice; let g = '';
  (st.banks.length ? st.banks : [[], [], [], []]).forEach((b, k) => {
    const y = 4 + k * (h - 14) / 4, hh = (h - 14) / 4 - 12;
    const wgt = [(1 - st.navX) * st.navY, st.navX * st.navY, st.navX * (1 - st.navY), (1 - st.navX) * (1 - st.navY)][k];
    g += t(0, y + hh / 2 + 4, 'ABCD'[k], wgt > .25 ? ICE : MUTE, 10, 'start', 800);
    for (let i = 0; i < 4; i++) {
      const o = b[i] || { harm: 0, level: 0 }, x = 16 + i * (w - 18) / 4, bw = (w - 18) / 4 - 6, v = o.level / 127;
      g += R(x, y, bw, hh, DARK, LINE) + R(x, y + hh * (1 - v), bw, hh * v, `rgba(205,230,240,${(.2 + .6 * wgt).toFixed(2)})`);
      if (o.level) g += t(x + bw / 2, y + hh + 9, `${o.harm}`, MUTE, 7, 'middle');
    }
  });
  put('voice', g);
  $('t-voice').textContent = st.algo < 8 ? ROUTE[st.algo] : '';
  $('c-voice').innerHTML = `XM <b>${pct(st.xm)}</b> · WARP <b>${pct(st.warp)}</b> · FOLD <b>${pct(st.fold)}</b>`;
}

function react() {
  const [w, h] = SZ.react, cx = w / 2, cy = h * .37, r = Math.min(w, h * .7) * .36, ns = Math.max(1, st.rNst || 6);
  const at = st.rPos / 256, cur = Math.floor(at) % ns;
  let g = C(cx, cy, r, 'none', LINE);
  for (let i = 0; i < 6; i++) {
    const a = -Math.PI / 2 + i * Math.PI / 3, x = cx + r * Math.cos(a), y = cy + r * Math.sin(a), used = i < ns;
    g += C(x, y, i === cur ? 7 : 5, i === cur ? RED : DARK, i === cur ? RED : used ? ICE : MUTE) +
         t(cx + (r + 15) * Math.cos(a), cy + (r + 15) * Math.sin(a) + 3, i + 1, i === cur ? ICE : MUTE, 9, 'middle');
  }
  const pa = -Math.PI / 2 + (at % ns) * Math.PI / 3;
  g += L(cx, cy, cx + (r - 9) * Math.cos(pa), cy + (r - 9) * Math.sin(pa), ICE, 1.5) + C(cx, cy, 3, ICE);
  const y1 = h - 4, hh = h * .22;                // the resonance: RES as the peaks' sharpness, MIX their height
  g += L(0, y1, w, y1, LINE);
  [.2, .36, .54, .72].forEach((f, i) => {
    const q = 2 + st.rRes / 12, pts = [];
    for (let k = 0; k <= 30; k++) { const u = k / 30; pts.push([(f - .1 + u * .2) * w, y1 - hh * (.3 + .7 * st.rMix / 127) * Math.exp(-Math.pow((u - .5) * q, 2)) * (1 - i * .15)]); }
    g += P(pts, i ? DIM : ICE, 1.2);
  });
  put('react', g);
  $('t-react').textContent = st.on.reactor ? RMODEL[st.rModel] || '' : 'OFF';
  $('c-react').innerHTML = `STAGE <b>${cur + 1}</b> · POS <b>${(at % ns + 1).toFixed(2)}</b> · RES <b>${pct(st.rRes)}</b>`;
  $('react').parentNode.classList.toggle('off', !st.on.reactor);
}

function dist() {
  const [w, h] = SZ.dist; let g = '';
  const lo = 40, hi = 124, xs = [0, ...st.dSplit.map((m) => w * Math.min(1, Math.max(0, (m - lo) / (hi - lo)))), w];
  for (let i = 0; i < 4; i++) {
    const x = xs[i] + 2, bw = Math.max(4, xs[i + 1] - xs[i] - 4), y = 16, hh = h - 46, lv = Math.min(1, Math.max(0, (st.dBand[i] + 60) / 60));
    const on = st.dType[i] > 0;
    g += R(x, y, bw, hh, DARK, LINE) + R(x, y + hh * (1 - lv), bw, hh * lv, fill(on || i < 3, on ? .6 : .35));
    if (on && lv > .1) g += R(x, y + hh * (1 - lv), bw, 3, RED);
    g += t(x + bw / 2, y + hh + 12, SHAPER[st.dType[i]] || '', on ? ICE : MUTE, 8, 'middle');
    g += t(x + bw / 2, 9, ['LOW', 'LO-MID', 'HI-MID', 'TOP'][i], MUTE, 7, 'middle');
  }
  st.dSplit.forEach((m, i) => g += L(xs[i + 1], 12, xs[i + 1], h - 20, RED, 1, '2 2') + t(xs[i + 1], h - 4, hz(m), DIM, 7, 'middle'));
  put('dist', g);
  $('t-dist').textContent = st.on.distroy ? (st.dXover ? 'LR4' : 'CMP') : 'OFF';
  $('c-dist').innerHTML = `LOOP <b>FB ${pct(st.dFb)}</b> · TAME <b>${pct(st.dTame)}</b> · MIX <b>${pct(st.dMix)}</b>`;
  $('dist').parentNode.classList.toggle('off', !st.on.distroy);
}

function time() {
  const [w, h] = SZ.time; let g = '';
  echoHist.push(st.echo); if (echoHist.length > 90) echoHist.shift();
  const ya = h * .36;                            // the echoes as they ring, newest right
  g += L(0, ya, w, ya, LINE);
  echoHist.forEach((v, i) => {                 // (ping-pong: left up, right down, in turn)
    const x = w - (echoHist.length - 1 - i) * w / 90, a = v / 127 * h * .3, pp = st.sMode & 2;
    if (a > .5) g += L(x, pp && i % 2 ? ya : ya - a, x, pp && !(i % 2) ? ya : ya + a, i === echoHist.length - 1 ? RED : ICE, 1.6);
  });
  g += t(0, 10, 'ECHOES', MUTE, 8);
  const cx = w / 2, cy = h * .76, r = Math.min(w * .3, h * .18);
  g += C(cx, cy, r, 'none', LINE, 6);
  if (st.loopPlays || st.loopArmed) {
    const a0 = -Math.PI / 2, a1 = a0 + Math.PI * 2 * st.lpAt, big = st.lpAt > .5 ? 1 : 0;
    if (st.loopPlays) g += `<path d="M${cx},${cy - r} A${r},${r} 0 ${big} 1 ${(cx + r * Math.cos(a1)).toFixed(1)},${(cy + r * Math.sin(a1)).toFixed(1)}" fill="none" stroke="rgba(205,230,240,.55)" stroke-width="6"/>` +
                           L(cx, cy, cx + (r + 6) * Math.cos(a1), cy + (r + 6) * Math.sin(a1), RED, 2);
    else g += C(cx, cy, r, 'none', 'rgba(235,28,28,.5)', 2);
  }
  g += t(cx, cy + 4, st.loopPlays ? 'LOOP' : st.loopArmed ? 'REC' : 'OFF', st.loopPlays ? ICE : st.loopArmed ? RED : MUTE, 8, 'middle');
  put('time', g);
  const sync = st.sMode & 1, tm = sync ? SPDIV[Math.min(11, (st.sTime * 12) >> 16)] : `${1 + Math.round(st.sTime * 999 / 65535)} MS`;
  $('t-time').textContent = st.on.time ? `${tm}${st.sMode & 2 ? ' PP' : ''}` : 'OFF';
  $('c-time').innerHTML = `FDBK <b>${pct(st.sFb)}</b> · LEVEL <b>${pct(st.sDlevel)}</b> · SPEED <b>${st.lpSpeed > 0 ? '+' : ''}${Math.round(st.lpSpeed * 64 / 256)}</b>`;
  $('time').parentNode.classList.toggle('off', !st.on.time && !st.loopPlays);
}

function space() {
  const [w, h] = SZ.space; let g = '';
  spaceHist.push(st.spPeak); if (spaceHist.length > 120) spaceHist.shift();
  const y0 = h * .64, pts = spaceHist.map((v, i) => [w - (spaceHist.length - 1 - i) * w / 120, y0 - (y0 - 10) * Math.min(1, v / 100)]);
  g += P([[pts[0][0], y0], ...pts, [w, y0]], 'none', 0, 'rgba(205,230,240,.12)') + P(pts, ICE, 1.3) + L(0, y0, w, y0, LINE);
  g += L(w - 1, 8, w - 1, y0, RED, 1.5) + t(w - 4, 16, 'NOW', RED, 8, 'end');
  const shim = st.sMode >> 2;
  [['FREEZE', st.freeze], ['SHIMMER', shim > 0]].forEach(([n, on], i) => {
    const x = 4 + i * w / 2, y = y0 + 24;
    g += C(x + 5, y - 3, 5, on ? ICE : '#18212a', on ? ICE : LINE) + t(x + 15, y, n, on ? ICE : MUTE, 8);
  });
  g += t(0, h - 4, `SIZE ${pct(st.sSize)} · DECAY ${pct(st.sDecay)}`, MUTE, 8);
  put('space', g);
  $('t-space').textContent = st.on.space ? 'PLATE' : 'OFF';
  $('c-space').innerHTML = `LEVEL <b>${pct(st.sRlevel)}</b> · FRZ PITCH <b>${st.frzPitch > 0 ? '+' : ''}${st.frzPitch}</b>`;
  $('space').parentNode.classList.toggle('off', !st.on.space);
}

function out() {
  const [w, h] = SZ.out; let g = '';
  st.out.forEach((v, i) => {
    const x = 6 + i * 22, hh = h - 30, db = v ? 20 * Math.log10(v / 16383) : -99, lv = Math.max(0, (db + 48) / 48);
    g += R(x, 8, 14, hh, DARK, LINE);
    for (let k = 0; k < 24; k++) { const y = 8 + hh - (k + 1) * hh / 24; if ((k + .5) / 24 < lv) g += R(x + 2, y + 1, 10, hh / 24 - 2, k > 20 ? RED : 'rgba(205,230,240,.7)'); }
    g += t(x + 7, h - 8, 'LR'[i], DIM, 8, 'middle');
  });
  put('out', g);
  const gr = st.lim >= 127 ? 0 : -20 * Math.log10(Math.max(1, st.lim) / 128);
  $('c-out').innerHTML = gr > .5 ? `LIM <b>-${gr.toFixed(0)}</b>` : `SUB <b>${st.on.sub ? 'ON' : 'OFF'}</b>`;
}

function macros() {
  const [w] = SZ.mac; let g = '';
  ['MUTATE', 'MOVE', 'BREAK', MAC4[st.mac4] || 'MACRO 4'].forEach((n, i) => {
    const x = 8 + i * (w - 16) / 4, bw = (w - 16) / 4 - 14, hh = 92, v = st.mac[i] / 127;
    g += R(x, 4, bw, hh, DARK, LINE) + R(x, 4 + hh * (1 - v), bw, hh * v, 'rgba(205,230,240,.55)') +
         L(x - 3, 4 + hh * (1 - v), x + bw + 3, 4 + hh * (1 - v), v ? ICE : MUTE, 2) + t(x + bw / 2, hh + 18, n, ICE, 8, 'middle');
  });
  const cx = w / 2, cy = 210, r = 66, n = st.turLen || 16;   // RADAR: its loop round the scope, its bits under it
  const lock = st.turChange === 0 || st.turChange >= 100;
  g += t(8, 136, '/// RADAR', RED, 10, 'start', 700) +
       t(w - 8, 136, st.radarOn ? `${lock ? 'LOCK' : 'SEARCH ' + (st.turChange > 50 ? 100 - st.turChange : st.turChange)} · ${n} · ${SDIV[st.turDiv] || ''}` : 'OFF', MUTE, 8, 'end');
  for (let a = 0; a < 36; a++) { const an = a * Math.PI / 18, r1 = a % 9 === 0 ? r - 8 : r - 4;
    g += L(cx + r1 * Math.cos(an), cy + r1 * Math.sin(an), cx + r * Math.cos(an), cy + r * Math.sin(an), a % 9 ? MUTE : DIM); }
  g += C(cx, cy, r * .55, 'none', GRID, 1);
  const bit = (k) => (st.radar >> k) & 1, place = st.radarPlace === 127 ? 0 : st.radarPlace;
  if (st.radarOn) {
    for (let i = 0; i < n; i++) {                // the loop's places: the newest bit is the place now
      const age = (place - i + n) % n, b = bit(age), a0 = -Math.PI / 2 + i * 2 * Math.PI / n, a1 = a0 + 2 * Math.PI / n, rr = r * (b ? .78 : .45);
      g += `<path d="M${(cx + rr * Math.cos(a0 + .04)).toFixed(1)},${(cy + rr * Math.sin(a0 + .04)).toFixed(1)} A${rr},${rr} 0 0 1 ${(cx + rr * Math.cos(a1 - .04)).toFixed(1)},${(cy + rr * Math.sin(a1 - .04)).toFixed(1)}" stroke="${i === place ? RED : b ? ICE : MUTE}" stroke-width="3" fill="none"/>`;
    }
    const sw = -Math.PI / 2 + (place + .5) * 2 * Math.PI / n;
    for (let k = 0; k < 14; k++) { const an = sw - k * .03; g += L(cx, cy, cx + r * Math.cos(an), cy + r * Math.sin(an), `rgba(235,28,28,${(.7 - k * .05).toFixed(2)})`); }
  }
  for (let i = 0; i < 16; i++) {                // (place i's bit: the register's, as many steps old as i is behind)
    const x = 10 + i * (w - 20) / 16, on = i < n && st.radarOn;
    g += t(x, SZ.mac[1] - 4, on ? bit((place - i + n) % n) : '·', i === place && on ? RED : on ? ICE : MUTE, 11, 'start', 700);
  }
  put('mac', g);
  $('n-mac').textContent = `MACRO 4: ${MAC4[st.mac4] || '—'}`;
}

function lane(id, k) {
  const [w, h] = SZ[id], tr = st.seq[k]; let g = t(0, 12, k ? 'SEQ B' : 'SEQ A', ICE, 11, 'start', 800);
  const x0 = 70, cw = (w - x0) / 16, steps = tr.steps || [];
  const on = k ? st.seqBOn : st.seqAOn, now = st.step[k] - (tr.bank || 0) * 16;
  const notes = steps.filter((s) => s.type <= 1).map((s) => s.note);
  const lo = notes.length ? Math.min(...notes) - 2 : 48, hi = notes.length ? Math.max(...notes) + 2 : 72;
  g += t(0, 28, `${tr.len || 16} · ${SDIV[tr.div] || ''}`, MUTE, 8) + t(0, 42, on ? 'ON' : 'OFF', on ? ICE : MUTE, 8) +
       t(0, 56, nn(st.note[k]), st.note[k] ? RED : MUTE, 9, 'start', 700);
  for (let i = 0; i < 16; i++) {
    const s = steps[i] || { type: 3 }, x = x0 + i * cw, hh = h - 22, here = i === now && st.play && on, past = (tr.bank || 0) * 16 + i >= (tr.len || 16);
    g += R(x + 2, 14, cw - 4, hh, here ? 'rgba(235,28,28,.16)' : DARK, here ? RED : LINE);
    if (!past && s.type <= 1) {
      const y = 14 + hh * (1 - (s.note - lo) / Math.max(1, hi - lo));
      g += R(x + 5, y, cw - 10, 5, here ? RED : ICE) + R(x + 5, y + 5, cw - 10, 14 + hh - y - 5, `rgba(205,230,240,${(.05 + s.vel / 900).toFixed(2)})`);
      if (s.type === 1) g += t(x + cw / 2, y - 3, '/', DIM, 8, 'middle');
    } else if (!past && s.type === 2) g += L(x - 2, 14 + hh / 2, x + cw - 2, 14 + hh / 2, DIM, 2);
    if (past) g += t(x + cw / 2, 14 + hh / 2 + 3, '·', MUTE, 10, 'middle');
    if (i % 4 === 0) g += t(x + 3, h, (tr.bank || 0) * 16 + i + 1, MUTE, 7);
  }
  (tr.split || []).slice(0, Math.max(0, (tr.segs || 1) - 1)).forEach((sp, j) => {
    const i = sp - (tr.bank || 0) * 16; if (i > 0 && i < 16) g += L(x0 + i * cw, 4, x0 + i * cw, h - 6, RED, 1, '3 3') + t(x0 + i * cw + 3, 10, 'BCDEF'[j], MUTE, 8);
  });
  put(id, g);
}
function players() {
  lane('seqa', 0); lane('seqb', 1);
  { const [w] = SZ.orbit, o = st.orbit; let g = t(0, 12, 'ORBIT', ICE, 11, 'start', 800) + t(0, 28, o.on ? 'ON' : 'OFF', o.on ? ICE : MUTE, 8);
    const x0 = 70, n = o.len || 4, steps = (o.steps || []).slice(0, n), tot = steps.reduce((a, s) => a + s.loops, 0) || 1;
    let x = x0;
    steps.forEach((s, i) => { const bw = (w - x0) * s.loops / tot, here = i === st.orbStep && st.orbitOn;
      g += R(x + 1, 6, bw - 2, 32, here ? 'rgba(235,28,28,.16)' : DARK, here ? RED : LINE) +
           t(x + 5, 19, s.a ? 'P' + String(s.a).padStart(2, '0') : '—', here ? ICE : DIM, 8) + t(x + 5, 32, `×${s.loops}`, MUTE, 7);
      if (here) g += L(x + (bw * Math.min(st.orbLoops, s.loops) / s.loops), 2, x + (bw * Math.min(st.orbLoops, s.loops) / s.loops), 42, RED, 2);
      x += bw; });
    put('orbit', g); }
  { const [w] = SZ.arp; let g = t(0, 12, 'ARP', ICE, 11, 'start', 800) + t(0, 26, st.arpOn ? `${AMODE[st.arp.mode] || ''} · ${st.arp.oct || 1} OCT` : 'OFF', st.arpOn ? DIM : MUTE, 8);
    const ns = (st.arp.notes || []).slice(0, st.arp.n || 0), x0 = 70;
    ns.forEach((n, i) => { const x = x0 + i * 52, y = 30 - (n - 48) * .6, cur = st.arpNote && n % 12 === st.arpNote % 12;
      g += R(x, y, 40, 5, cur ? RED : 'rgba(205,230,240,.6)') + t(x, y - 3, nn(n), cur ? ICE : MUTE, 7); });
    if (st.arpNote) g += t(w, 12, nn(st.arpNote), RED, 10, 'end', 700);
    put('arp', g); }
  const ma = st.masterB ? 'B' : 'A';
  $('n-seq').textContent = `MASTER ${ma}${st.rec ? ' · REC' : ''}`;
}

function mod() {
  const [w] = SZ.mod; let g = '';
  (st.mod.length ? st.mod : Array(8).fill({ src: 0, dst: 0, amt: 0 })).forEach((m, i) => {
    const y = 2 + i * 19;
    if (!m.src && !m.dst) { g += t(0, y + 11, '--', GRID, 9) + t(78, y + 11, '--', GRID, 9); return; }
    const own = m.dst >= 103, amt = Math.round(m.amt * 99 / 32767), v = st.slot[i] / 63;
    g += t(0, y + 11, SRC_NAME[m.src] || '?', m.src === 24 || m.src === 19 ? ICE : DIM, 9) + t(74, y + 11, '→ ' + dstName(m.dst), ICE, 9);
    const cx = 200, bw = 64;
    g += L(cx - bw / 2, y + 7, cx + bw / 2, y + 7, GRID) + L(cx, y + 3, cx, y + 11, MUTE) +
         R(v >= 0 ? cx : cx + v * bw / 2, y + 5, Math.abs(v) * bw / 2, 4, own ? RED : ICE);
    g += t(w, y + 11, own ? (amt < 0 ? 'ON 0' : 'ON 1') : `${amt > 0 ? '+' : ''}${amt}`, own ? RED : MUTE, 8, 'end');
  });
  const wave = (x, y, ww, hh, k) => {
    const L2 = st.lfoSet ? st.lfoSet[k] : { shape: 0, mode: 0 }, sh = L2.shape, pts = [];
    for (let j = 0; j <= 48; j++) { const u = j / 48;
      const v = sh === 0 ? Math.sin(u * 2 * Math.PI) : sh === 1 ? 1 - 4 * Math.abs(u - .25 - Math.floor(u + .25)) : sh === 2 ? 1 - 2 * u : sh === 3 ? 2 * u - 1 : sh === 4 ? (u < .5 ? 1 : -1) : [.4, -.7, .9, -.2][Math.floor(u * 4) % 4];
      pts.push([x + u * ww, y + hh / 2 - hh / 2 * v]); }
    const now = st.lfo[k] / 63;
    return R(x, y, ww, hh, 'none', GRID) + P(pts, DIM, 1.1) + L(x, y + hh / 2 - hh / 2 * now, x + ww, y + hh / 2 - hh / 2 * now, RED, 1, '2 2') +
           t(x, y - 4, `LFO ${k + 1} · ${LSHAPE[sh] || ''}${L2.mode & 1 ? ' SYNC' : ''}`, ICE, 8);
  };
  const y1 = 172; g += wave(0, y1, w / 2 - 8, 40, 0) + wave(w / 2 + 8, y1, w / 2 - 8, 40, 1);
  const env = (x, y, ww, hh, k) => {
    const e = st.envSet ? st.envSet[k] : { a: 20, d: 40, s: 100, r: 40 }, tot = e.a + e.d + 40 + e.r || 1;
    const xa = x + ww * e.a / tot, xd = xa + ww * e.d / tot, xs = xd + ww * 40 / tot, sy = y + hh * (1 - e.s / 127);
    const lv = st.env[k] / 127;
    return R(x, y, ww, hh, 'none', GRID) + P([[x, y + hh], [xa, y], [xd, sy], [xs, sy], [x + ww, y + hh]], DIM, 1.1) +
           R(x + ww - 4, y + hh * (1 - lv), 4, hh * lv, RED) + t(x, y - 4, `ENV ${k + 1}`, ICE, 8);
  };
  const y2 = 246, ew = (w - 16) / 3;
  g += env(0, y2, ew, 36, 0) + env(ew + 8, y2, ew, 36, 1) + env(2 * ew + 16, y2, ew, 36, 2);
  put('mod', g);
}

function here() {
  document.querySelectorAll('[data-pages]').forEach((e) => {
    e.classList.toggle('here', e.dataset.pages.split(',').map(Number).includes(st.page));
  });
}

function draw() {
  if (!dirty || !st.slow[0]) return;
  dirty = false;
  header(); nav(); voice(); react(); dist(); time(); space(); out(); macros(); players(); mod(); here();
}
function loop() {
  draw();
  const quiet = performance.now() - lastAt > 2000;
  stage.classList.toggle('wait', quiet);
  if (quiet && src) { src = ''; header(); say(usbOn ? 'Waiting for the FM-1 (is it on, with Star Distroyer, and on USB?)' : 'Waiting for the player.'); }
  requestAnimationFrame(loop);
}
requestAnimationFrame(loop);

const status = $('c-status');
const say = (s) => { status.textContent = s; };
function take(bytes, from) {
  if (decode(bytes, st)) {
    if (src !== from) { src = from; say(from === 'usb' ? 'The FM-1, over USB.' : 'The player below.'); }
    lastAt = performance.now(); dirty = true;
  }
}

// ---- the player below: its frames (player.js hands them on), asked for from the start
addEventListener('sd-console', (e) => take(e.detail, 'player'));
dispatchEvent(new CustomEvent('sd-console-want', { detail: true }));
window.sdConsoleWants = true;
$('c-player').addEventListener('click', () => { $('ship').scrollIntoView({ behavior: 'smooth' }); });

// ---- an FM-1 over USB MIDI (the installer's ports: never other gear)
const PORT = /fm-1|felucca|sloop|clipper|composite|sinco|usb-midi|star/i;
let midi = null, usbOn = false, ask = 0;
function ports() {
  const outs = [...midi.outputs.values()].filter((p) => PORT.test(p.name));
  [...midi.inputs.values()].filter((p) => PORT.test(p.name)).forEach((p) => {
    p.onmidimessage = (e) => { const d = e.data; if (isFrame(d)) take(d.subarray(5, d.length - 1), 'usb'); };
  });
  return outs;
}
function poll() {
  const outs = ports();
  if (!outs.length) { say(`No FM-1 among the MIDI ports${midi.outputs.size ? ` (${[...midi.outputs.values()].map((p) => p.name).join(', ')})` : ''}.`); return; }
  outs.forEach((o) => { try { o.send(ASK); } catch (_) { /* (gone) */ } });
}
$('c-usb').addEventListener('click', async () => {
  if (usbOn) {
    usbOn = false; clearInterval(ask); $('c-usb').classList.remove('on');
    if (midi) [...midi.outputs.values()].filter((p) => PORT.test(p.name)).forEach((o) => { try { o.send(STOP); } catch (_) { /* (gone) */ } });
    say('USB off.');
    return;
  }
  if (!navigator.requestMIDIAccess) { say('This browser has no Web MIDI: use Chrome or Edge on a computer.'); return; }
  try {
    midi = midi || await navigator.requestMIDIAccess({ sysex: true });
  } catch (_) { say('Web MIDI was not allowed (SysEx is needed): allow it in the address bar and try again.'); return; }
  usbOn = true; $('c-usb').classList.add('on');
  say('Looking for the FM-1 on USB…');
  poll(); ask = setInterval(poll, 1000);
  midi.onstatechange = () => ports();
});
addEventListener('pagehide', () => {
  if (midi && usbOn) [...midi.outputs.values()].filter((p) => PORT.test(p.name)).forEach((o) => { try { o.send(STOP); } catch (_) { /* */ } });
});
