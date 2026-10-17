// Lê a base do DefJamFFNY.exe num minidump e traduz endereços do host para função/linha via llvm-symbolizer.
// Uso: node dump_symbolize.js <arquivo.dmp> <exe> <addr_hex> [<addr_hex>...]
const fs = require('fs');
const { execFileSync } = require('child_process');
const [, , dmpPath, exePath, ...addrs] = process.argv;
const d = fs.readFileSync(dmpPath);
const nStreams = d.readUInt32LE(8), dirRva = d.readUInt32LE(12);
let base = 0n;
for (let i = 0; i < nStreams; i++) {
  const o = dirRva + i * 12;
  if (d.readUInt32LE(o) !== 4) continue; // ModuleListStream
  const rva = d.readUInt32LE(o + 8);
  const n = d.readUInt32LE(rva);
  for (let k = 0; k < n; k++) {
    const m = rva + 4 + k * 108;
    const b = d.readBigUInt64LE(m), nameRva = d.readUInt32LE(m + 20);
    const len = d.readUInt32LE(nameRva);
    const name = d.toString('utf16le', nameRva + 4, nameRva + 4 + len);
    if (/DefJamFFNY\.exe$/i.test(name)) base = b;
  }
}
console.log('base', '0x' + base.toString(16));
const offs = addrs.map((a) => '0x' + (BigInt('0x' + a.replace(/^0x/i, '')) - base + 0x140000000n).toString(16));
console.log(execFileSync('E:/Tools/llvm/bin/llvm-symbolizer.exe', ['--obj=' + exePath, '--inlining', ...offs]).toString());
