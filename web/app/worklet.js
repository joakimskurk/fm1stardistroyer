// SPDX-License-Identifier: GPL-3.0-only
// Star Distroyer in a browser: the firmware (stardistroyer.wasm, web/app/build.sh) in the audio thread. Its sound is
// its time base, as the audio interrupt is on the FM-1: each quantum asks it for its frames, and it runs its main
// loop for the milliseconds they are worth. The screen, the lights and the output's level go to the page about 30
// times a second; the flash when SAVE (or the settings) wrote it.
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
    if (m.type === 'key') x.web_key(m.k, m.down ? 1 : 0);
    else if (m.type === 'btn') x.web_btn(m.id, m.down ? 1 : 0);
    else if (m.type === 'enc') x.web_enc(m.id, m.n);
    else if (m.type === 'midi') x.web_midi(m.d[0], m.d[1] || 0, m.d[2] || 0);
  }

  process(_inputs, outputs) {
    const x = this.x;
    const out = outputs[0];
    if (!x) return true;
    const L = out[0], R = out[1] || out[0], n = L.length;
    if (this.step === 1) {                       // the usual case: the context runs at 44.1 kHz
      x.web_run(n);
      const o = new Int32Array(x.memory.buffer, x.web_out_ptr(), 2 * n);
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

registerProcessor('star-distroyer', StarDistroyer);
