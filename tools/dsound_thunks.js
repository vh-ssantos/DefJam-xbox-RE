// Lista os thunks de interface do DSOUND (mov eax,[esp+4] ... add eax,-N ... call alvo ... ret K) com o nome do
// config quando já conhecido. Uso: node dsound_thunks.js <default.xbe> <game.toml>
const fs = require('fs');
const [, , xbePath, cfgPath] = process.argv;
const b = fs.readFileSync(xbePath);
const base = b.readUInt32LE(0x104), nsec = b.readUInt32LE(0x11C), sh = b.readUInt32LE(0x120) - base;
const byVa = {};
for (const line of fs.readFileSync(cfgPath, 'utf8').split('\n')) {
  const m = line.match(/^(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*$/);
  if (m) byVa[parseInt(m[2], 16)] = m[1];
}
for (let i = 0; i < nsec; i++) {
  const o = sh + i * 56;
  const va = b.readUInt32LE(o + 4), ra = b.readUInt32LE(o + 12), rs = b.readUInt32LE(o + 16);
  const na = b.readUInt32LE(o + 20) - base;
  let name = '';
  for (let k = na; b[k]; k++) name += String.fromCharCode(b[k]);
  if (name !== 'DSOUND') continue;
  for (let k = 0; k + 4 < rs; k++) {
    const p = ra + k;
    // 8B 44 24 04 (mov eax,[esp+4]) ou 55 8B EC (push ebp; mov ebp,esp) seguido de "83 C0 xx" em até 20 bytes
    const a = b[p] === 0x8B && b[p + 1] === 0x44 && b[p + 2] === 0x24 && b[p + 3] === 0x04;
    const c = b[p] === 0x55 && b[p + 1] === 0x8B && b[p + 2] === 0xEC;
    if (!a && !c) continue;
    let adj = -1;
    for (let j = 3; j < 24; j++) if (b[p + j] === 0x83 && b[p + j + 1] === 0xC0) { adj = b.readInt8(p + j + 2); break; }
    if (adj >= 0 || adj === -1) continue;
    let call = 0, ret = -1, end = 0;
    for (let j = 4; j < 60; j++) {
      if (!call && b[p + j] === 0xE8) call = (va + k + j + 5 + b.readInt32LE(p + j + 1)) >>> 0;
      if (b[p + j] === 0xC2 && b[p + j + 2] === 0) { ret = b.readUInt16LE(p + j + 1); end = j + 3; break; }
      if (b[p + j] === 0xC3) { ret = 0; end = j + 1; break; }
    }
    if (!call || ret < 0) continue;
    const at = va + k;
    console.log('0x' + at.toString(16).toUpperCase(), 'adj', adj, 'ret', ret, '-> 0x' + call.toString(16).toUpperCase(),
      byVa[at] || '');
    k += end - 1;
  }
}
