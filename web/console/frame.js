// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 joakimskurk
// CONSOLE: a frame of the FM-1's state (firmware/src/sd_console.h's sd_con_frame, field for field in its order) into
// one state object that keeps the slow parts between their turns. Over USB MIDI the frame comes as
// F0 7D 53 44 08 <frame> F7; from the page's player as the frame alone. tests/console_test.mjs reads the player's.
export const VER = 2;                     // (2: twelve MOD slots, RADAR's own DESTs at 110)
// the names the FM-1's screen uses (stardistroyer.c's lists; tests/console_test.mjs checks them against it)
export const SRC_NAME = ["OFF", "ENV1", "ENV2", "LFO1", "LFO2", "NAV X", "NAV Y", "VEL", "KEY", "WHEEL", "AFTER", "RAND",
  "STEP", "SEGMENT", "D LOW", "D TOP", "R STAGE", "ECHO", "ENV3", "RADAR", "KICK", "SNARE", "SEQ A", "SEQ B", "RADAR 1"];
export const DST_NAME = ["OFF", "PITCH", "XM", "WARP", "FOLD", "DETUNE", "NAV X", "NAV Y", "WIDTH", "R POS",
  "R FREQ", "R RES", "R DRIVE", "R MIX", "DRV ALL", "BIAS", "LP FDBK", "LP TAME", "LP MIX", "D-FDBK", "D-LEVEL",
  "R-LEVEL", "R-DECAY", "DRV TOP", "SPL HM", "CUTOFF", "F RES", "E1 ATK", "E1 DEC", "E1 SUS", "E1 REL", "E2 ATK",
  "E2 DEC", "E2 SUS", "E2 REL", "E3 ATK", "E3 DEC", "E3 SUS", "E3 REL", "F ENV", "R SLEW", "LP TIME", "LP SPRD",
  "D-TIME", "D-TONE", "R-SIZE", "R-TONE", "L1 RATE", "L2 RATE", "L-SPEED", "SPL LOW", "SPL LM", "DRV LOW", "DRV LM",
  "DRV HM", "BOOM", "SIZE", "AIR", "P MACRO", "DRUMS", "VOLUME", "P TUNE", "FRZ PCH", "FREEZE", "FRZ LVL", "SHIMMER",
  "L-LEVEL", "L FILT", "L RSEND", "L DSEND", "R LOW", "LVL LOW", "LVL LM", "LVL HM", "LVL TOP", "PAN LOW", "PAN LM",
  "PAN HM", "PAN TOP", "ALT LOW", "ALT LM", "ALT HM", "ALT TOP", "DYN ALL", "BDY ALL", "F KEY", "SUB LVL", "SUB SHP",
  "SEARCH", "RDR OFS", "MUTATE", "MOVE", "BREAK", "M BODY", "M AIR", "DROWN", "GATE", "AMT 1", "AMT 2", "AMT 3",
  "AMT 4", "AMT 5", "AMT 6", "AMT 7", "AMT 8", "AMT 9", "AMT 10", "AMT 11", "AMT 12"];
export const OWN_DST = ["NOTES", "ACCENTS", "ROLLS", "RESTART", "R NEXT", "SEQ A", "SEQ B", "ARP", "REPEAT"];   // 0x80 + (110 + in a frame)
export const LSHAPE = ["SINE", "TRI", "SAW", "RAMP", "SQUARE", "S&H"];
export const SCALE = ["MINOR", "MAJOR", "DORIAN", "PHRYG", "PENTA-", "PENTA+", "CHROM", "LYDIAN", "MIXO", "HMINOR",
  "PHRDOM", "DBLHRM", "WHOLE", "HIRA", "BLUES"];
export const ROUTE = ["1>2>3>4", "2>3>4 1>4", "1+2+3>4", "1>2 3>4", "1+2>3 4", "1 2>3>4", "1>2 3 4", "1 2 3 4"];
export const SHAPER = ["NONE", "SOFT", "HARD", "FOLD", "CRUSH"];
export const RMODEL = ["BP6", "BP12", "VBP", "KP", "AP1", "AP2"];
export const MODE = ["MONO", "DUO", "POLY"];
export const KEY = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
export const SPDIV = ["1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2"];
export const SDIV = ["1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/64", "1/32T", "1/16D", "1/4T", "1/8D", "1/2"];
export const STYPE = ["NOTE", "SLIDE", "TIE", "REST", "SKIP"];
export const AMODE = ["UP", "DOWN", "UP-DN", "RND", "ORDER", "IN", "OUT", "PEDAL", "UPx2"];
// the FM-1's pages (stardistroyer.c's page): where the console lights a panel
export const PAGE = ["NAV", "REACTOR", "DISTROY", "SPACE", "MOD", "MACROS", "SEQ", "ENV", "FILTER", "ARP", "ORBIT", "TIME",
  "LOOPER", "SETUP", "DRUM", "SUB"];
export const MODS = 12;
export const dstName = (d) => d >= 110 ? OWN_DST[d - 110] || "?" : DST_NAME[d] || "?";

export function newState() {
  return { ok: false, frames: 0, slow: [false, false, false, false, false, false], seq: [{ steps: [] }, { steps: [] }],
           orbit: { steps: [] }, mod: [], banks: [], arp: { notes: [] } };
}

// one frame (its bytes between F0 7D 53 44 08 and F7) into st; false if it is not one this page reads
export function decode(b, st) {
  let i = 0;
  const u = () => b[i++] & 127, w = () => { const h = u(); return (h << 7) | u(); }, s = () => u() - 64;
  if (b.length < 8 || u() !== VER) return false;
  st.count = u(); const slow = u();
  const fa = u(), fb = u(), fc = u();
  st.play = !!(fa & 1); st.rec = !!(fa & 2); st.arpOn = !!(fa & 4); st.orbitOn = !!(fa & 8); st.radarOn = !!(fa & 16);
  st.freeze = !!(fa & 32); st.loopPlays = !!(fa & 64);
  st.mode = fb & 3; st.clock = !!(fb & 4); st.masterB = !!(fb & 8); st.seqAOn = !!(fb & 16); st.seqBOn = !!(fb & 32);
  st.loopArmed = !!(fb & 64);
  st.on = { reactor: !!(fc & 1), distroy: !!(fc & 2), time: !!(fc & 4), space: !!(fc & 8), filter: !!(fc & 16),
            sub: !!(fc & 32), course: !!(fc & 64) };
  st.tempo = w() / 10; st.cpu = w(); st.cpuMax = w(); st.wait = w();
  st.page = u();
  st.voices = [u(), u(), u(), u()];
  st.voiceLv = [u(), u(), u(), u()];
  st.navX = w() / 16383; st.navY = w() / 16383;
  st.xm = u(); st.warp = u(); st.fold = u(); st.dt = u();
  st.rPos = w(); st.rStage = u();
  st.dBand = [u(), u(), u(), u()].map((v) => v - 64);
  st.dSplit = [u(), u(), u()];
  st.echo = u(); st.lpAt = w() / 4096; st.spPeak = u();
  st.out = [w(), w()]; st.lim = u();
  st.mac = [u(), u(), u(), u()];
  st.radar = u() | (u() << 7) | (u() << 14); st.radarPlace = u(); st.radarOut = u();
  st.lfo = [s(), s()];
  st.env = [u(), u(), u()];
  st.slot = []; for (let k = 0; k < MODS; k++) st.slot.push(s());
  st.step = [u(), 0]; st.note = [u(), 0]; st.step[1] = u(); st.note[1] = u();
  st.orbStep = u(); st.orbLoops = u(); st.arpNote = u(); st.wheel = u(); st.after = u();
  if (u() !== slow) return false;
  if (slow === 0) {
    st.patch = u(); st.name = String.fromCharCode(...Array.from({ length: 8 }, u)).trim();
    st.algo = u(); st.dark = u(); st.course = u(); st.fxOff = u();
    st.rModel = u(); st.rRes = u(); st.rMix = u(); st.rNst = u();
    st.dType = [u(), u(), u(), u()]; st.dXover = u(); st.dFb = u(); st.dTame = u(); st.dMix = u();
    st.sTime = w() << 2; st.sMode = u(); st.sFb = u(); st.sDlevel = u(); st.sSize = u(); st.sDecay = u(); st.sRlevel = u();
    st.frzPitch = s(); st.lpLen = w(); st.lpSpeed = s(); st.lpLvl = u();
    st.lfoSet = [0, 1].map(() => ({ shape: u(), mode: u(), rate: w() << 2 }));
    st.envSet = [0, 1, 2].map(() => ({ a: u(), d: u(), s: u(), r: u() }));
    st.turLen = u(); st.turDiv = u(); st.turChange = u(); st.turOff = u() - 100;
    st.fType = u(); st.fCut = u(); st.fRes = u(); st.subLvl = u(); st.subSrc = u(); st.width = u();
    st.key = u(); st.scale = u();
  } else if (slow === 1) {
    st.mod = [];
    for (let k = 0; k < MODS; k++) st.mod.push({ src: u(), dst: u(), amt: (w() - 8192) * 4, off: w() - 100 });
  } else if (slow === 2 || slow === 3) {
    const t = st.seq[slow - 2];
    t.on = u(); t.in = u(); t.len = u(); t.div = u(); t.bank = u(); t.segs = u();
    t.split = [u(), u(), u(), u(), u()];
    t.steps = [];
    for (let k = 0; k < 16; k++) { const note = u(), tv = u(); t.steps.push({ note, type: tv & 7, vel: (tv >> 3) * 32 + 16 }); }
  } else if (slow === 4) {
    const o = st.orbit;
    o.on = u(); o.len = u(); o.end = u(); o.steps = [];
    for (let k = 0; k < 16; k++) o.steps.push({ loops: u() || 1, a: u(), b: u() });
  } else if (slow === 5) {
    st.arp = { mode: u(), oct: u(), div: u(), n: u(), notes: [] };
    for (let k = 0; k < 8; k++) st.arp.notes.push(u());
    st.banks = [];
    for (let k = 0; k < 4; k++) { const osc = []; for (let j = 0; j < 4; j++) osc.push({ harm: u(), level: u() }); st.banks.push(osc); }
  } else return false;
  if (i > b.length) return false;
  st.slow[slow] = true; st.frames++;
  st.ok = st.slow.every(Boolean);
  st.len = i;
  return true;
}

// the USB requests (sd_console.h): frames for 3 s more; none
export const ASK = [0xF0, 0x7D, 0x53, 0x44, 0x07, 0x01, 0xF7];
export const STOP = [0xF0, 0x7D, 0x53, 0x44, 0x07, 0x00, 0xF7];
export const isFrame = (d) => d.length > 6 && d[0] === 0xF0 && d[1] === 0x7D && d[2] === 0x53 && d[3] === 0x44 && d[4] === 0x08;
