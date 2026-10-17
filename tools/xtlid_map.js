// Lista os exports da seção .XTLID do XBE (id -> endereço) com o nome do signatures.toml do SDK, e compara com
// o [symbols] do config. Uso: node xtlid_map.js <default.xbe> <signatures.toml> <game.toml>
const fs = require('fs');
const [, , xbePath, sigPath, cfgPath] = process.argv;
const b = fs.readFileSync(xbePath);
const base = b.readUInt32LE(0x104), nsec = b.readUInt32LE(0x11C), sh = b.readUInt32LE(0x120) - base;
const names = {};
for (const line of fs.readFileSync(sigPath, 'utf8').split('\n')) {
  const m = line.match(/^(\w+)\s*=.*xtlid = (0x[0-9A-Fa-f]+)/);
  if (m) names[parseInt(m[2], 16)] = m[1];
}
const cfg = {};
for (const line of fs.readFileSync(cfgPath, 'utf8').split('\n')) {
  const m = line.match(/^(\w+)\s*=\s*(0x[0-9A-Fa-f]+|0)\s*$/);
  if (m) cfg[m[1]] = parseInt(m[2], 16);
}
for (let i = 0; i < nsec; i++) {
  const o = sh + i * 56;
  const vs = b.readUInt32LE(o + 8), ra = b.readUInt32LE(o + 12), rs = b.readUInt32LE(o + 16);
  const na = b.readUInt32LE(o + 20) - base;
  let name = '';
  for (let k = na; b[k]; k++) name += String.fromCharCode(b[k]);
  if (name !== '.XTLID') continue;
  for (let k = 0; k + 8 <= Math.min(vs, rs); k += 8) {
    const id = b.readUInt32LE(ra + k), va = b.readUInt32LE(ra + k + 4);
    if (!id && !va) break;
    const n = names[id] || '?';
    const c = cfg[n];
    const tag = c === undefined ? '' : c === va ? 'ok' : c === 0 ? 'FALTA no config' : 'DIFERE (config 0x' + c.toString(16) + ')';
    console.log('0x' + id.toString(16).padStart(5, '0'), '0x' + va.toString(16).toUpperCase(), n, tag);
  }
}
