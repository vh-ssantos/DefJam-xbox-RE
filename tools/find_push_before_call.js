// Acha os "push imm32" logo antes das chamadas a uma função do XBE (ex.: o descritor de dispositivo passado a
// XInputOpen / XGetDevices). Uso: node find_push_before_call.js <default.xbe> <game.toml> <nome> [<nome>...]
const fs = require('fs');
const [, , xbePath, cfgPath, ...names] = process.argv;
const b = fs.readFileSync(xbePath);
const cfg = fs.readFileSync(cfgPath, 'utf8');
const sym = (n) => {
  const m = cfg.match(new RegExp('^' + n + '\\s*=\\s*(0x[0-9A-Fa-f]+)', 'm'));
  return m ? parseInt(m[1], 16) : 0;
};
const base = b.readUInt32LE(0x104), nsec = b.readUInt32LE(0x11C), sh = b.readUInt32LE(0x120) - base;
const secs = [];
for (let i = 0; i < nsec; i++) {
  const o = sh + i * 56;
  const va = b.readUInt32LE(o + 4), vs = b.readUInt32LE(o + 8), ra = b.readUInt32LE(o + 12), rs = b.readUInt32LE(o + 16);
  const na = b.readUInt32LE(o + 20) - base;
  let name = '';
  for (let k = na; b[k]; k++) name += String.fromCharCode(b[k]);
  secs.push({ name, va, vs, ra, rs });
}
for (const fn of names) {
  const t = sym(fn);
  if (!t) { console.log(fn, 'não está no config'); continue; }
  const hits = {};
  for (const s of secs) {
    for (let i = 0; i + 5 <= s.rs; i++) {
      const o = s.ra + i;
      if (b[o] !== 0xE8) continue;
      const tgt = (s.va + i + 5 + b.readInt32LE(o + 1)) >>> 0;
      if (tgt !== t) continue;
      const pushes = [];
      for (let k = 5; k <= 40; k++)
        if (b[o - k] === 0x68) pushes.push('0x' + b.readUInt32LE(o - k + 1).toString(16));
      hits['call@0x' + (s.va + i).toString(16)] = pushes.join(' ');
    }
  }
  console.log(fn, '0x' + t.toString(16), hits);
}
