// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joakim (joakimskurk)
// The lab's patch, as the firmware has it: its bytes (SdPatch), read and written through the field table the
// WebAssembly builds from the struct itself (web/app/web_lab.h: web_fields). Shared by the lab's page (lab.js) and
// its test (tests/lab_test.mjs), so what the page exports is what the test compiles.
export const TYPES = { 1: [1, false], 2: [1, true], 3: [2, false], 4: [2, true] };   // LAB_U8 LAB_I8 LAB_U16 LAB_I16
export const SHOW = { RAW: 0, P99: 1, PCT: 2, PITCH: 3, SEMI: 4, ENUM: 5, HEX: 6, MS: 7, CENTS: 8, POS: 9, PAN: 10, LEVEL: 11 };
export const GROUPS = ['THE Q PAGE', 'NAV · COURSE', 'BANK A', 'BANK B', 'BANK C', 'BANK D', 'ENV 1 · LEVEL', 'ENV 2 · FM',
  'FILTER', 'ENV 3 · FILTER', 'REACTOR', 'REACTOR · STAGES', 'DISTROY', 'DISTROY · BANDS', 'DISTROY · LOOP', 'TIME',
  'SPACE', 'MOD MATRIX', 'LFO', 'MACROS', 'TURING'];

// a field's value from the patch's bytes, and back (clamped to its range)
export function get(bytes, f) {
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  switch (f.type) {
    case 1: return dv.getUint8(f.off);
    case 2: return dv.getInt8(f.off);
    case 3: return dv.getUint16(f.off, true);
    default: return dv.getInt16(f.off, true);
  }
}
export function set(bytes, f, v) {
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  v = Math.round(Math.max(f.lo, Math.min(f.hi, Number(v) || 0)));
  switch (f.type) {
    case 1: dv.setUint8(f.off, v); break;
    case 2: dv.setInt8(f.off, v); break;
    case 3: dv.setUint16(f.off, v, true); break;
    default: dv.setInt16(f.off, v, true);
  }
  return v;
}

const NOTE = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
// a value as the firmware means it (the raw number stays beside it on the page)
export function show(f, v, enums) {
  switch (f.show) {
    case SHOW.P99: return String(Math.round((v * 99) / 65535));
    case SHOW.PCT: return `${v >= 0 ? '+' : ''}${((v * 100) / 32767).toFixed(1)} %`;
    case SHOW.PITCH: {
      const m = v / 256, n = Math.round(m), hz = 440 * 2 ** ((m - 69) / 12);
      return `${NOTE[((n % 12) + 12) % 12]}${Math.floor(n / 12) - 1}${Math.abs(m - n) > 0.02 ? (m > n ? '+' : '−') : ''} · ${hz < 1000 ? hz.toFixed(0) : (hz / 1000).toFixed(2) + 'k'} Hz`;
    }
    case SHOW.SEMI: return `${v >= 0 ? '+' : ''}${(v / 256).toFixed(2)} st`;
    case SHOW.ENUM: return (enums[f.list] && enums[f.list][v]) || '?';
    case SHOW.HEX: return '0x' + v.toString(16).toUpperCase().padStart(2, '0');
    case SHOW.MS: return `${v} ms`;
    case SHOW.CENTS: return `${v >= 0 ? '+' : ''}${v} ct`;
    case SHOW.POS: return `${(1 + v / 256).toFixed(2)}`;
    case SHOW.PAN: return v === 0 ? 'C' : `${v < 0 ? 'L' : 'R'} ${Math.round((Math.abs(v) * 100) / 32767)}`;
    case SHOW.LEVEL: return `${(v / 32768).toFixed(2)} x`;
    default: return String(v);
  }
}

// the patch as C: a designated initializer, every field that is not 0 (the struct's zero is the rest), in the
// table's order; its name and the patch it came from in a comment. It compiles into sd_patches.h's SD_PATCH[] as it is
export function toC(bytes, fields, name, from) {
  const parts = [];
  for (const f of fields) {
    const v = get(bytes, f);
    if (v) parts.push(`${f.path} = ${v}`);
  }
  const lines = [];
  let line = '        ';
  for (const p of parts) {
    if (line.length + p.length + 2 > 118) { lines.push(line.trimEnd()); line = '        '; }
    line += p + ', ';
  }
  if (line.trim()) lines.push(line.trimEnd().replace(/,$/, ''));
  return `    {   /* ${name}${from && from !== name ? ` (from ${from})` : ''}: made in the lab */\n${lines.join('\n')}},\n`;
}
// the patch as JSON: the fields by group and name (to read), and its bytes (to load back exactly)
export function toJSON(bytes, fields, name, from) {
  const values = {};
  for (const f of fields) values[`${GROUPS[f.group] || f.group} / ${f.name}`] = get(bytes, f);
  let b64 = '';
  for (let i = 0; i < bytes.length; i++) b64 += String.fromCharCode(bytes[i]);
  return JSON.stringify({ format: 'star-distroyer-patch', version: 1, name, from, size: bytes.length, values,
                          bytes: btoa(b64) }, null, 2);
}
export function fromJSON(text, size) {
  const j = JSON.parse(text);
  if (j.format !== 'star-distroyer-patch' || j.size !== size) throw new Error('not a patch of this firmware (its size differs)');
  const s = atob(j.bytes), b = new Uint8Array(size);
  for (let i = 0; i < size; i++) b[i] = s.charCodeAt(i);
  return { bytes: b, name: j.name || 'IMPORTED', from: j.from };
}
