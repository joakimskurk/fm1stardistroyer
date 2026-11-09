// SPDX-License-Identifier: GPL-3.0-only
// Star Distroyer on its own page: the firmware runs in the audio thread (worklet.js, stardistroyer.wasm); here its
// screen, its lights and the FM-1's controls, laid out as the FM-1 has them (the knobs and OCT on the left, the screen
// in the middle, K1-K4 and the twelve buttons on the right, the 27 keys across the bottom), drawn as a ship's console.
// Nothing is loaded before POWER ON (a browser plays sound only after a click). What SAVE writes stays in this browser.
const $ = (id) => document.getElementById(id);
const ship = $('ship');
if (ship) {
  const BTN = { OCT_DOWN: 0, OCT_UP: 1, FX: 2, SCL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, SAVE: 9, ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };
  // the buttons: the FM-1's name, its id, what it does here (the note under it)
  const ROW1 = [['FX', 2, 'FX CHAIN'], ['SCL', 3, '2ND LAYER'], ['ENV', 4, 'ENV · FILTER'], ['LFO', 5, 'LFO · TURING'],
                ['EDIT', 6, 'ON · OFF'], ['GLO', 7, 'VOICES']];
  const ROW2 = [['HOME', 8, 'NAV · MACROS'], ['SAVE', 9, 'STORE'], ['ARP', 10, 'ARPEGGIO'], ['SEQ', 11, 'SEQ A · B'],
                ['PLAY', 12, 'RUN', 'red'], ['REC', 13, 'RECORD', 'red']];
  const ABOUT = { 0: 'OCT−: an octave down (on SEQ: the step bank)', 1: 'OCT+: an octave up (on SEQ: the step bank)',
    2: 'FX: REACTOR, DESTROY, SPACE', 3: 'SCL held: the second layer. A click locks it, another lets it go (or hold Shift)',
    4: 'ENV: the envelopes, the filter', 5: 'LFO: LFO 1, LFO 2, TURING, the matrix', 6: 'EDIT: what the page shows, on or off',
    7: 'GLO: MONO, DUO, POLY', 8: 'HOME: NAV, the macros', 9: 'SAVE: the patch (on SEQ: the pattern) to a place of your own',
    10: 'ARP: the arpeggiator', 11: 'SEQ: SEQ A, SEQ B', 12: 'PLAY: the sequencer (Space)', 13: 'REC: record steps' };
  // the encoders: name, id, note
  const ENCS = { SELECT: [0, 'TEMPO'], ALGORITHM: [1, 'CHOICE'], PRESETS: [6, 'PATCH'],
                 'KNOB 1': [2, 'K1'], 'KNOB 2': [3, 'K2'], 'KNOB 3': [4, 'K3'], 'KNOB 4': [5, 'K4'] };
  const isBlack = (k) => (0x54A >> ((53 + k) % 12)) & 1;           // key 0 = F3 (MIDI 53)
  const NOTE = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  // the computer's keys (by place, any layout): A row the white keys from C4, the row above the black ones
  const KEYMAP = { KeyA: 7, KeyW: 8, KeyS: 9, KeyE: 10, KeyD: 11, KeyF: 12, KeyT: 13, KeyG: 14, KeyY: 15, KeyH: 16,
                   KeyU: 17, KeyJ: 18, KeyK: 19, KeyO: 20, KeyL: 21, KeyP: 22, Semicolon: 23, Quote: 24 };
  const KEYBTN = { KeyZ: BTN.OCT_DOWN, KeyX: BTN.OCT_UP, Space: BTN.PLAY, ShiftLeft: BTN.SCL, ShiftRight: BTN.SCL };

  const knob = (name, pot) => {
    const [id, note] = pot ? [-1, 'VOLUME'] : ENCS[name];
    return `<div class="kc"><div class="knob${pot ? ' pot' : ''}" tabindex="0" role="${pot ? 'slider' : 'spinbutton'}"
      ${pot ? 'aria-valuemin="0" aria-valuemax="100"' : ''} data-enc="${id}"
      aria-label="${name}: ${pot ? 'the volume' : note.toLowerCase()} (drag, scroll or the arrow keys)">${pot ? '<s class="arc"></s>' : ''}
      <s class="sk"></s><s class="fc"></s><s class="pt"></s></div><b>${name}</b><small>${note}</small></div>`;
  };
  const btn = ([name, id, note, cls], wide) =>
    `<div class="bc${wide ? ' wide' : ''}"><button class="btn${cls ? ' ' + cls : ''}" data-btn="${id}" aria-label="${ABOUT[id]}"
      title="${ABOUT[id]}"><i></i><span>${name}</span></button><small>${note}</small></div>`;
  ship.innerHTML = `
    <div class="head">
      <img class="logo" src="screens/logo.png" width="1263" height="64" alt="">
      <span class="unit">FM-1 · UNIT 962</span>
      <span class="lights">
        <span class="lamp red" id="l-pwr"><i></i>PWR</span>
        <span class="lamp meter" aria-hidden="true"><span class="vu" id="vu"><b></b><b></b></span>OUT</span>
        <span class="lamp" id="l-midi"><i></i>MIDI</span>
        <span class="lamp" id="l-mem"><i></i>MEM</span>
      </span>
    </div>
    <div class="deck">
      <div class="zl"><span class="ztag">HELM</span>
        <div class="kg">${knob('MASTER', true)}${knob('SELECT')}${knob('PRESETS')}${knob('ALGORITHM')}</div>
        <div class="octs">${btn(['OCT−', 0, 'OCTAVE'], true)}${btn(['OCT+', 1, 'OCTAVE'], true)}</div>
      </div>
      <div class="zs"><div class="bezel"><s class="screw"></s><s class="screw"></s><s class="screw"></s><s class="screw"></s>
        <canvas id="lcd" width="480" height="480" aria-label="The FM-1's screen"></canvas><s class="glass"></s>
        <span class="plate">DISPLAY · 240 × 240</span>
        <button class="power" id="power"><span>▶ POWER ON</span><small>the firmware, in this browser</small></button>
      </div></div>
      <div class="zr"><span class="ztag">SYSTEMS · K1–K4</span>
        <div class="kr">${knob('KNOB 1')}${knob('KNOB 2')}${knob('KNOB 3')}${knob('KNOB 4')}</div>
        <div class="br">${ROW1.map((b) => btn(b)).join('')}</div>
        <div class="br">${ROW2.map((b) => btn(b)).join('')}</div>
      </div>
    </div>
    <div class="bed" id="bed" role="group" aria-label="The 27 keys, F3 to G5"></div>
    <div class="foot"><s class="haz"></s><span>KEYS · F3–G5</span><s class="haz"></s></div>`;

  const lcd = $('lcd'), g = lcd.getContext('2d');
  const small = document.createElement('canvas');
  small.width = small.height = 240;
  const gs = small.getContext('2d');
  const img = gs.createImageData(240, 240);
  const px32 = new Uint32Array(img.data.buffer);
  const status = $('appstatus'), power = $('power');
  const say = (t) => { if (status) status.textContent = t; };
  let ac = null, node = null, gain = null, booting = false, running = false, wasmBytes = null, storeSize = 0;
  const send = (m) => node && node.port.postMessage(m);
  const capture = (el, e) => { try { el.setPointerCapture(e.pointerId); } catch (_) { /* (a pointer that is gone already) */ } };

  // ---- the keys: whites in a row along the bottom, the blacks in a row over the gaps, as on the FM-1
  const bed = $('bed');
  const keyEls = [];
  const whites = [...Array(27).keys()].filter((k) => !isBlack(k));
  const W = 100 / (16 + 15 * 0.18);                // a white key's width (% of the bed), its gap 0.18 of that
  for (let k = 0; k < 27; k++) {
    const el = document.createElement('span');
    const name = NOTE[(53 + k) % 12] + (Math.floor((53 + k) / 12) - 1);
    el.dataset.k = k;
    el.setAttribute('aria-hidden', 'true');
    if (isBlack(k)) {
      const after = whites.filter((w) => w < k).length - 1;   // the white key before it: it sits over the gap after
      el.className = 'key b';
      el.style.left = `${(after + 1) * W * 1.18 - W * 0.09 - W / 2}%`;
    } else {
      const i = whites.indexOf(k);
      el.className = 'key w';
      el.style.left = `${i * W * 1.18}%`;
      if (k === 0 || k === 7 || k === 19 || k === 26) el.dataset.note = name;
    }
    el.style.width = `${W}%`;
    el.innerHTML = '<i></i><em></em>';
    bed.appendChild(el);
    keyEls.push(el);
  }
  (async () => {                                   // the computer key on each key it plays (its letter on this layout)
    let map = null;
    try { if (navigator.keyboard && navigator.keyboard.getLayoutMap) map = await navigator.keyboard.getLayoutMap(); } catch (_) { /* (none) */ }
    for (const [code, k] of Object.entries(KEYMAP)) {
      const ch = (map && map.get(code)) || { Semicolon: ';', Quote: "'" }[code] || code.slice(3);
      keyEls[k].querySelector('em').textContent = ch.toUpperCase();
    }
  })();

  // ---- what holds a key or a button (a pointer, the computer's keyboard): down at the first, up at the last
  const keyHold = new Array(27).fill(0), btnHold = new Array(14).fill(0);
  function keyDown(k) { if (k < 0 || k > 26) return; if (keyHold[k]++ === 0) { keyEls[k].classList.add('down'); send({ type: 'key', k, down: 1 }); } }
  function keyUp(k) { if (k < 0 || k > 26 || keyHold[k] === 0) return; if (--keyHold[k] === 0) { keyEls[k].classList.remove('down'); send({ type: 'key', k, down: 0 }); } }
  const btnEls = [...ship.querySelectorAll('[data-btn]')];
  const btnEl = (id) => btnEls.find((b) => +b.dataset.btn === id);
  function btnDown(id) { if (btnHold[id]++ === 0) { btnEl(id).classList.add('down'); send({ type: 'btn', id, down: 1 }); } }
  function btnUp(id) { if (btnHold[id] === 0) return; if (--btnHold[id] === 0) { btnEl(id).classList.remove('down'); send({ type: 'btn', id, down: 0 }); } }

  const pointerKey = new Map();
  const keyAt = (x, y) => { const el = document.elementFromPoint(x, y); const kk = el && el.closest && el.closest('.key'); return kk ? +kk.dataset.k : -1; };
  bed.addEventListener('pointerdown', (e) => {
    const kk = e.target.closest('.key');
    if (!kk || !running) return;
    e.preventDefault();
    capture(bed, e);
    pointerKey.set(e.pointerId, +kk.dataset.k);
    keyDown(+kk.dataset.k);
  });
  bed.addEventListener('pointermove', (e) => {     // a glide across the keys: the one under it now
    if (!pointerKey.has(e.pointerId)) return;
    const was = pointerKey.get(e.pointerId), k = keyAt(e.clientX, e.clientY);
    if (k !== was && k >= 0) { keyUp(was); keyDown(k); pointerKey.set(e.pointerId, k); }
  });
  const keyLift = (e) => { if (pointerKey.has(e.pointerId)) { keyUp(pointerKey.get(e.pointerId)); pointerKey.delete(e.pointerId); } };
  bed.addEventListener('pointerup', keyLift);
  bed.addEventListener('pointercancel', keyLift);

  // ---- the buttons: down while pressed (SCL: a click locks it, another lets it go; held, it is held)
  let sclLock = false, sclAt = 0;
  for (const b of btnEls) {
    const id = +b.dataset.btn;
    b.addEventListener('pointerdown', (e) => {
      if (!running) return;
      e.preventDefault();
      capture(b, e);
      if (id === BTN.SCL) {
        if (sclLock) { sclLock = false; b.classList.remove('locked'); btnUp(id); return; }
        sclAt = performance.now();
      }
      btnDown(id);
    });
    const lift = () => {
      if (id === BTN.SCL) {
        if (sclLock || !btnHold[id]) return;
        if (performance.now() - sclAt < 350) { sclLock = true; b.classList.add('locked'); return; }   // a click: locked
      }
      btnUp(id);
    };
    b.addEventListener('pointerup', lift);
    b.addEventListener('pointercancel', lift);
    b.addEventListener('click', (e) => {           // the keyboard (Enter on a focused button): a press and a release
      if (e.detail !== 0 || !running) return;
      btnDown(id); btnUp(id);
    });
  }

  // ---- the knobs: drag up (or right), scroll, or the arrow keys; a detent each 4 px. MASTER is the volume
  let vol = 0.7;
  try { const v = parseFloat(localStorage.getItem('sd-vol')); if (v >= 0 && v <= 1) vol = v; } catch (_) { /* (none) */ }
  const knobs = [...ship.querySelectorAll('.knob')];
  const showVol = (el) => { el.style.setProperty('--v', vol); el.style.setProperty('--a', `${-135 + 270 * vol}deg`); el.setAttribute('aria-valuenow', Math.round(vol * 100)); };
  const setVol = (v, el) => {
    vol = Math.min(1, Math.max(0, v));
    showVol(el);
    if (gain) gain.gain.setTargetAtTime(vol * vol, ac.currentTime, 0.02);
    try { localStorage.setItem('sd-vol', String(vol)); } catch (_) { /* (none) */ }
  };
  for (const el of knobs) {
    const id = +el.dataset.enc, pot = id < 0;
    let angle = 0, acc = 0, wheel = 0, drag = null, hotT = 0;
    if (pot) showVol(el);
    const hot = () => { el.classList.add('hot'); clearTimeout(hotT); hotT = setTimeout(() => el.classList.remove('hot'), 260); };
    const turn = (n) => {
      if (!n) return;
      hot();
      if (pot) { setVol(vol + n / 100, el); return; }
      if (!running) return;
      angle += 15 * n;
      el.style.setProperty('--a', `${angle}deg`);
      send({ type: 'enc', id, n });
    };
    el.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      el.focus({ preventScroll: true });
      capture(el, e);
      drag = { x: e.clientX, y: e.clientY };
      acc = 0;
    });
    el.addEventListener('pointermove', (e) => {
      if (!drag) return;
      acc += (drag.y - e.clientY) + (e.clientX - drag.x);
      drag = { x: e.clientX, y: e.clientY };
      const n = Math.trunc(acc / 4);
      if (n) { acc -= n * 4; turn(n); }
    });
    const end = () => { drag = null; };
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
    el.addEventListener('wheel', (e) => {
      e.preventDefault();
      wheel += -(e.deltaMode === 1 ? e.deltaY * 33 : e.deltaY) + (e.deltaMode === 1 ? e.deltaX * 33 : e.deltaX);
      const n = Math.max(-10, Math.min(10, Math.trunc(wheel / 30)));
      if (n) { wheel -= n * 30; turn(n); }
    }, { passive: false });
    el.addEventListener('keydown', (e) => {
      const n = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: 10, PageDown: -10 }[e.key];
      if (n) { e.preventDefault(); turn(n); }
    });
  }

  // ---- the computer's keyboard, while the panel is on screen and the player runs
  let inView = false;
  new IntersectionObserver((es) => { inView = es[0].isIntersecting; }, { threshold: 0.25 }).observe(ship);
  const heldCodes = new Set();
  const typing = (t) => t && (t.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(t.tagName));
  document.addEventListener('keydown', (e) => {
    if (!running || !inView || e.metaKey || e.ctrlKey || e.altKey || typing(e.target)) return;
    const k = KEYMAP[e.code], b = KEYBTN[e.code];
    if (k === undefined && b === undefined) return;
    if (e.code === 'Space' && e.target.closest && e.target.closest('button, .knob, a')) return;   // (its own press)
    e.preventDefault();
    if (e.repeat || heldCodes.has(e.code)) return;
    heldCodes.add(e.code);
    if (k !== undefined) keyDown(k); else btnDown(b);
  });
  document.addEventListener('keyup', (e) => {
    if (!heldCodes.has(e.code)) return;
    heldCodes.delete(e.code);
    const k = KEYMAP[e.code], b = KEYBTN[e.code];
    if (k !== undefined) keyUp(k); else btnUp(b);
  });
  window.addEventListener('blur', () => {          // (the window left with keys held: let them go)
    for (const c of heldCodes) { const k = KEYMAP[c], b = KEYBTN[c]; if (k !== undefined) keyUp(k); else btnUp(b); }
    heldCodes.clear();
  });

  // ---- the lights, the screen, the level
  const lamp = (id, on) => $(id).classList.toggle('on', !!on);
  const flash = (id) => { const el = $(id); el.classList.add('on'); clearTimeout(el._t); el._t = setTimeout(() => el.classList.remove('on'), 140); };
  const vu = [...$('vu').children];
  const level = [0, 0];
  function draw(p) {                               // RGB565 -> the canvas, twice its size, sharp
    for (let i = 0; i < 57600; i++) {
      const c = p[i];
      const r = ((c >> 11) * 527 + 23) >> 6, gg = (((c >> 5) & 63) * 259 + 33) >> 6, b = ((c & 31) * 527 + 23) >> 6;
      px32[i] = 0xFF000000 | (b << 16) | (gg << 8) | r;
    }
    gs.putImageData(img, 0, 0);
    g.imageSmoothingEnabled = false;
    g.drawImage(small, 0, 0, lcd.width, lcd.height);
  }
  const bit = (a, id) => (id < 32 ? a[0] >>> id : a[1] >>> (id - 32)) & 1;
  function show(m) {
    for (const b of btnEls) {
      const id = +b.dataset.btn;
      b.classList.toggle('lit', !!bit(m.leds, id));
      b.classList.toggle('glow', !bit(m.leds, id) && !!bit(m.glow, id));
    }
    for (let k = 0; k < 27; k++) {
      keyEls[k].classList.toggle('lit', !!bit(m.leds, 14 + k));
      keyEls[k].classList.toggle('glow', !bit(m.leds, 14 + k) && !!bit(m.glow, 14 + k));
    }
    for (let c = 0; c < 2; c++) {                  // the level: 0 dB full, -48 dB nothing; it falls back gently
      const db = 20 * Math.log10(Math.max(m.peak[c], 1e-6));
      const v = Math.max(0, Math.min(1, (db + 48) / 48));
      level[c] = Math.max(v, level[c] - 0.06);
      vu[c].style.setProperty('--l', level[c].toFixed(3));
    }
  }

  // ---- what SAVE wrote: kept in this browser (gzip, base64), put back at the next POWER ON
  const KEEP = 'sd-flash-1';
  async function pack(u8) {
    const s = new Blob([u8]).stream().pipeThrough(new CompressionStream('gzip'));
    const z = new Uint8Array(await new Response(s).arrayBuffer());
    let t = '';
    for (let i = 0; i < z.length; i += 0x8000) t += String.fromCharCode.apply(null, z.subarray(i, i + 0x8000));
    return btoa(t);
  }
  async function unpack(t) {
    const z = Uint8Array.from(atob(t), (c) => c.charCodeAt(0));
    const s = new Blob([z]).stream().pipeThrough(new DecompressionStream('gzip'));
    return new Uint8Array(await new Response(s).arrayBuffer());
  }
  async function kept() {
    try { const t = localStorage.getItem(KEEP); return t && typeof DecompressionStream === 'function' ? await unpack(t) : null; } catch (_) { return null; }
  }
  async function keep(u8) {
    try { if (typeof CompressionStream === 'function') localStorage.setItem(KEEP, await pack(u8)); } catch (_) { /* (full, or blocked: kept for now only) */ }
  }

  // ---- POWER ON: the firmware, its worklet, the sound
  async function boot(store) {
    node = new AudioWorkletNode(ac, 'star-distroyer', { numberOfInputs: 0, outputChannelCount: [2] });
    node.connect(gain);
    node.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === 'frame') {
        if (m.lcd) draw(m.lcd);
        show(m);
        if (m.store) { keep(m.store); flash('l-mem'); }
      } else if (m.type === 'ready') {
        storeSize = m.storeSize;
        running = true;
        ship.classList.add('on');
        lamp('l-pwr', 1);
        say('Running. Play the keys (or A–\' on the computer\'s keyboard, W E T Y U O P the black ones; Z X the octave, Space PLAY, Shift SCL).');
      } else if (m.type === 'error') say(`It did not start: ${m.message}`);
    };
    const wasm = wasmBytes.slice(0);
    node.port.postMessage({ type: 'boot', wasm, store }, store ? [wasm, store.buffer] : [wasm]);
  }
  async function start() {
    if (booting) return;
    if (ac) {                                      // POWER again: off (suspended) or on
      if (ac.state === 'running') { await ac.suspend(); running = false; ship.classList.remove('on'); lamp('l-pwr', 0); power.hidden = false; say('Off. POWER ON to go on where it was.'); }
      else { await ac.resume(); running = true; ship.classList.add('on'); lamp('l-pwr', 1); power.hidden = true; say('Running.'); }
      return;
    }
    booting = true;
    power.classList.add('busy');
    say('Loading the firmware…');
    try {
      ac = new AudioContext({ sampleRate: 44100, latencyHint: 'interactive' });
      const [bytes] = await Promise.all([
        fetch('app/stardistroyer.wasm').then((r) => { if (!r.ok) throw new Error(`stardistroyer.wasm: ${r.status}`); return r.arrayBuffer(); }),
        ac.audioWorklet.addModule('app/worklet.js'),
      ]);
      wasmBytes = bytes;
      gain = ac.createGain();
      gain.gain.value = vol * vol;
      gain.connect(ac.destination);
      await boot(await kept());
      await ac.resume();
      power.hidden = true;
    } catch (err) {
      say(`It did not start here: ${err.message || err}. It needs a browser with Web Audio worklets (any recent one).`);
      ac = null;
    }
    power.classList.remove('busy');
    booting = false;
  }
  power.addEventListener('click', start);
  $('l-pwr').addEventListener('click', () => { if (ac) start(); });

  const off = $('p-off'), reset = $('p-reset'), midiB = $('p-midi');
  if (off) off.addEventListener('click', () => { if (ac) start(); });
  if (reset) reset.addEventListener('click', async () => {   // back to the factory: what this browser kept, gone
    let had = false;
    try { had = !!localStorage.getItem(KEEP); } catch (_) { /* (none) */ }
    if (had && !confirm('Forget the patches, patterns and settings SAVE kept in this browser, and start as from the factory?')) return;
    try { localStorage.removeItem(KEEP); } catch (_) { /* (none) */ }
    if (!ac) { say('Nothing kept here now: it starts as from the factory.'); return; }
    running = false;
    for (let k = 0; k < 27; k++) keyHold[k] = 0;
    for (let i = 0; i < 14; i++) btnHold[i] = 0;
    sclLock = false;
    for (const el of ship.querySelectorAll('.down, .locked')) el.classList.remove('down', 'locked');
    node.disconnect();
    node.port.close();
    if (ac.state !== 'running') await ac.resume();
    power.hidden = true;
    await boot(null);
  });
  if (midiB) midiB.addEventListener('click', async () => {   // a MIDI keyboard: its notes, its controllers, its clock
    if (!navigator.requestMIDIAccess) { say('This browser has no Web MIDI (Chrome or Edge do).'); return; }
    try {
      const access = await navigator.requestMIDIAccess();
      const hook = () => {
        let n = 0;
        for (const input of access.inputs.values()) {
          n++;
          input.onmidimessage = (e) => {
            const d = e.data, st = d[0];
            if (st < 0x80 || (st >= 0xF0 && ![0xF8, 0xFA, 0xFB, 0xFC].includes(st))) return;
            send({ type: 'midi', d: [st, d[1] || 0, d[2] || 0] });
            flash('l-midi');
          };
        }
        midiB.classList.toggle('on', n > 0);
        say(n ? `MIDI: ${[...access.inputs.values()].map((i) => i.name).join(', ')} plays it.` : 'MIDI: no keyboard found. Plug one in.');
      };
      access.onstatechange = hook;
      hook();
    } catch (err) {
      say(`MIDI was not allowed: ${err.message || err}`);
    }
  });
}
