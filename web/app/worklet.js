// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joakim (joakimskurk)
// Star Distroyer in a browser: the firmware (stardistroyer.wasm, web/app/build.sh) in the audio thread. Its sound is
// its time base, as the audio interrupt is on the FM-1: each quantum asks it for its frames, and it runs its main
// loop for the milliseconds they are worth. The screen, the lights and the output's level go to the page about 30
// times a second; the flash when SAVE (or the settings) wrote it. For the lab (web/lab/): the patch's field table, the
// patch itself (sent when it changed, set from the page, a factory one loaded), and a recording of the frames exactly
// as the firmware made them.
const FS = 44100;                                // the firmware's rate; another context rate is resampled (linear)

class StarDistroyer extends AudioWorkletProcessor {
  constructor() {
    super();
    this.x = null;
    this.rev = -1;
    this.storeRev = 0;
    this.since = 0;
    this.peak = [0, 0];
    this.step = FS / sampleRate;                 // firmware frames per output frame
    this.pos = 0;                                // (resampling) where we are in buf, in its frames
    this.buf = new Float32Array(2 * 4096);       // its frames not yet behind us, L R L R
    this.have = 0;
    this.port.onmessage = (e) => this.message(e.data);
    this.patSent = null;                         // (the lab) the patch as last sent to the page
    this.rec = null;                             // (the lab) recording: the firmware's frames, Int16, in chunks
    this.seqSent = null;                         // (the lab) the sequencer as last sent to the page
  }

  str(p) {                                       // a C string in the firmware's memory
    const m = new Uint8Array(this.x.memory.buffer);
    let s = '';
    while (m[p]) s += String.fromCharCode(m[p++]);
    return s;
  }
  pat() { const x = this.x; return new Uint8Array(x.memory.buffer, x.web_pat(), x.web_pat_size()); }
  labInit() {                                    // the fields, the lists' names, the patches, the patch now
    const x = this.x, dv = new DataView(x.memory.buffer), n = x.web_nfields(), f0 = x.web_fields(), sz = x.web_field_size();
    const fields = [];
    for (let i = 0; i < n; i++) {
      const b = f0 + i * sz;
      fields.push({ off: dv.getUint16(b, true), type: dv.getUint8(b + 2), show: dv.getUint8(b + 3), list: dv.getUint8(b + 4),
                    group: dv.getUint8(b + 5), lo: dv.getInt32(b + 8, true), hi: dv.getInt32(b + 12, true),
                    name: this.str(dv.getUint32(b + 16, true)), path: this.str(dv.getUint32(b + 20, true)) });
    }
    const enums = {};
    for (let l = 1; l < 32; l++) {
      const names = [];
      for (let i = 0; i < 256; i++) { const p = x.web_enum(l, i); if (!p) break; names.push(this.str(p)); }
      if (names.length) enums[l] = names;
    }
    const patches = [];
    for (let i = 0; i < x.web_pat_count(); i++) if (x.web_pat_ok(i)) patches.push({ i, name: this.str(x.web_pat_name(i)) });
    this.patSent = this.pat().slice();
    const layout = [];
    for (let i = 0; i < 64; i++) { const v = x.web_seq_layout(i); if (v === 0xFFFF) break; layout.push(v); }
    this.seqSent = this.seqNow();
    this.port.postMessage({ type: 'lab-init', fields, enums, patches, now: x.web_pat_now(), bytes: this.patSent.slice(),
                            layout, seq: this.seqNow() });
  }
  seqNow() {                                     // (the lab) the sequencer: both patterns, ORBIT, the settings
    const x = this.x, n = x.web_seq_size(), m = x.memory.buffer, set = [];
    for (let i = 0; i < x.web_lset_n(); i++) set.push(x.web_lset(i));
    return { a: new Uint8Array(m, x.web_seq(0), n).slice(), b: new Uint8Array(m, x.web_seq(1), n).slice(),
             orb: new Uint8Array(m, x.web_orb(), x.web_orb_size()).slice(), set };
  }
  turNow() {                                     // (the lab) TURING: its register, and the values of its steps since
    const x = this.x, c = x.web_tur(1) >>> 0, reg = x.web_tur(0), vals = [];   // the last frame (8 at most: the
    if (this.turSeen === undefined) this.turSeen = c;                          // register holds them)
    const n = Math.min(8, (c - this.turSeen) >>> 0);
    for (let i = n; i > 0; i--) vals.push(x.web_tur_dac(reg >>> (i - 1)));
    this.turSeen = c;
    return { reg, vals };
  }
  lpNow() {                                      // (the lab) the looper: its state each frame, its loop's outline
    const x = this.x, g = (i) => x.web_lp(i) >>> 0, on = g(0), total = g(6);   // (240 peaks) a few times a second
    const o = { on, park: g(1), ln: g(2), k: g(3), rev: g(8), lev: g(9), rate: g(10), len: g(7), total, wait: g(15) };
    this.lpN = (this.lpN || 0) + 1;
    if (total > 0 && this.lpN % 4 === 0) {
      const m = new Uint8Array(x.memory.buffer), A = g(11), na = g(12), B = g(13), nb = g(14), C = 240;
      const n = Math.max(1, Math.min(total, Math.floor((on ? o.ln : o.len) / 256)));
      const end = on ? g(5) : g(4), start = ((end - n) % total + total) % total, env = new Uint8Array(C);
      const at = (i) => { i %= total; return i < na ? m[A + i] : B && i - na < nb ? m[B + i - na] : 0; };
      for (let c = 0; c < C; c++) {
        const a = Math.floor((c * n) / C), b = Math.max(a + 1, Math.floor(((c + 1) * n) / C)), st = Math.max(1, Math.floor((b - a) / 24));
        let pk = 0;
        for (let j = a; j < b; j += st) { const v = at(start + j) & 127; if (v > pk) pk = v; }   // (mu-law: the magnitude
        env[c] = pk;                                                                           //  its low 7 bits, rising)
      }
      o.env = env;
    }
    return o;
  }
  seqSame(p, q) {
    const eq = (u, v) => { if (u.length !== v.length) return false; for (let i = 0; i < u.length; i++) if (u[i] !== v[i]) return false; return true; };
    return eq(p.a, q.a) && eq(p.b, q.b) && eq(p.orb, q.orb) && p.set.every((v, i) => v === q.set[i]);
  }

  async message(m) {
    if (m.type === 'boot') {
      try {
        const { instance } = await WebAssembly.instantiate(m.wasm, {});
        const x = instance.exports;
        if (m.store && m.store.length === x.web_store_size())   // the flash the page kept, before the app reads it
          new Uint8Array(x.memory.buffer, x.web_store(), m.store.length).set(m.store);
        x.web_init();
        this.storeRev = x.web_store_rev();
        this.x = x;
        this.port.postMessage({ type: 'ready', storeSize: x.web_store_size() });
      } catch (err) {
        this.port.postMessage({ type: 'error', message: String(err) });
      }
      return;
    }
    const x = this.x;
    if (!x) return;
    if (m.type === 'lab-init') this.labInit();
    else if (m.type === 'lab-set') {             // the page's patch: into the firmware's, to the engine as a knob does
      this.pat().set(m.bytes);
      x.web_pat_apply();
      this.patSent = this.pat().slice();
    } else if (m.type === 'lab-seq-set') {     // a track's pattern from the page, to the engine as SEQ hands it
      new Uint8Array(x.memory.buffer, x.web_seq(m.k), x.web_seq_size()).set(m.bytes);
      x.web_seq_apply(m.k);
      this.seqSent = this.seqNow();
    } else if (m.type === 'lab-seq-op') {      // LENGTH, a cut, EVEN, a segment's length, RND, CLR: SEQ's own
      x.web_seq_op(m.k, m.op, m.a || 0, m.b || 0);
      this.seqSent = this.seqNow();
      this.port.postMessage({ type: 'lab-seq', seq: this.seqSent });
    } else if (m.type === 'lab-orb-set') {
      new Uint8Array(x.memory.buffer, x.web_orb(), x.web_orb_size()).set(m.bytes);
      x.web_orb_apply();
      this.seqSent = this.seqNow();
    } else if (m.type === 'lab-lset') {        // a setting, as its button sets it (ARP only with room: sent back)
      x.web_lset_put(m.i, m.v);
      this.seqSent = this.seqNow();
      if (this.seqSent.set[m.i] !== m.v) this.port.postMessage({ type: 'lab-seq', seq: this.seqSent });
    } else if (m.type === 'lab-load') {          // a patch of the bank, as PRESETS + EDIT
      x.web_pat_load(m.i);
      this.patSent = this.pat().slice();
      this.port.postMessage({ type: 'lab-loaded', now: x.web_pat_now(), bytes: this.patSent.slice() });
    } else if (m.type === 'lab-rec') {           // recording on, or off and sent
      if (m.on) this.rec = [];
      else if (this.rec) {
        let n = 0;
        for (const c of this.rec) n += c.length;
        const all = new Int16Array(n);
        n = 0;
        for (const c of this.rec) { all.set(c, n); n += c.length; }
        this.rec = null;
        this.port.postMessage({ type: 'lab-rec', frames: all }, [all.buffer]);
      }
    } else if (m.type === 'key') x.web_key(m.k, m.down ? 1 : 0);
    else if (m.type === 'btn') x.web_btn(m.id, m.down ? 1 : 0);
    else if (m.type === 'enc') x.web_enc(m.id, m.n);
    else if (m.type === 'midi') x.web_midi(m.d[0], m.d[1] || 0, m.d[2] || 0, m.src || 1);
  }

  process(_inputs, outputs) {
    const x = this.x;
    const out = outputs[0];
    if (!x) return true;
    const L = out[0], R = out[1] || out[0], n = L.length;
    if (this.step === 1) {                       // the usual case: the context runs at 44.1 kHz
      x.web_run(n);
      const o = new Int32Array(x.memory.buffer, x.web_out_ptr(), 2 * n);
      if (this.rec) this.keep(o);
      for (let i = 0; i < n; i++) {
        L[i] = o[2 * i] / 32768;
        R[i] = o[2 * i + 1] / 32768;
      }
    } else {                                     // another rate: a line between each two of its frames
      const st = this.step, b = this.buf;
      const top = Math.floor(this.pos + (n - 1) * st) + 2;   // the frames this quantum reads, from buf's start
      while (this.have < top) {
        x.web_run(128);
        const o = new Int32Array(x.memory.buffer, x.web_out_ptr(), 256);
        if (this.rec) this.keep(o);
        for (let i = 0; i < 256; i++) b[2 * this.have + i] = o[i] / 32768;
        this.have += 128;
      }
      for (let i = 0; i < n; i++) {
        const p = this.pos + i * st, j = p | 0, f = p - j;
        L[i] = b[2 * j] + (b[2 * j + 2] - b[2 * j]) * f;
        R[i] = b[2 * j + 1] + (b[2 * j + 3] - b[2 * j + 1]) * f;
      }
      this.pos += n * st;
      const done = this.pos | 0;                 // the frames behind us
      b.copyWithin(0, 2 * done, 2 * this.have);
      this.have -= done;
      this.pos -= done;
    }
    for (let i = 0; i < n; i++) {                // the level: each side's peak since the last frame
      const a = Math.abs(L[i]), c = Math.abs(R[i]);
      if (a > this.peak[0]) this.peak[0] = a;
      if (c > this.peak[1]) this.peak[1] = c;
    }
    this.since += n;
    if (this.since >= sampleRate / 30) {         // the screen (when it changed), the lights, the level
      this.since = 0;
      const rev = x.web_lcd_rev();
      const msg = { type: 'frame', leds: [x.web_leds(0), x.web_leds(1)], glow: [x.web_glow(0), x.web_glow(1)],
                    peak: this.peak, ms: x.web_ms() };
      this.peak = [0, 0];
      const transfer = [];
      if (rev !== this.rev) {
        this.rev = rev;
        msg.lcd = new Uint16Array(x.memory.buffer, x.web_lcd(), 240 * 240).slice();
        transfer.push(msg.lcd.buffer);
      }
      if (this.seqSent) {                        // (the lab) the sequencer, when the panel changed it; the transport
        const q = this.seqNow();
        if (!this.seqSame(q, this.seqSent)) { this.seqSent = q; msg.seq = q; }
        msg.play = [x.web_play(0), x.web_play(1), x.web_play(2)];
        msg.tur = this.turNow();
        msg.lp = this.lpNow();
      }
      if (this.patSent) {                        // (the lab) the patch, when the panel's knobs changed it
        const p = this.pat();
        let same = p.length === this.patSent.length;
        for (let i = 0; same && i < p.length; i++) same = p[i] === this.patSent[i];
        if (!same) { this.patSent = p.slice(); msg.pat = this.patSent.slice(); msg.patNow = x.web_pat_now(); }
      }
      const sr = x.web_store_rev();
      if (sr !== this.storeRev) {                // SAVE wrote, or the settings: the page keeps a copy
        this.storeRev = sr;
        msg.store = new Uint8Array(x.memory.buffer, x.web_store(), x.web_store_size()).slice();
        transfer.push(msg.store.buffer);
      }
      this.port.postMessage(msg, transfer);
    }
    return true;
  }
}

StarDistroyer.prototype.keep = function (o) {   // (the lab) the frames as the firmware made them, as its 16 bits
  const c = new Int16Array(o.length);
  for (let i = 0; i < o.length; i++) c[i] = o[i] > 32767 ? 32767 : o[i] < -32767 ? -32767 : o[i];
  this.rec.push(c);
};

registerProcessor('star-distroyer', StarDistroyer);
