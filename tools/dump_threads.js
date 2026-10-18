// Lista as threads de um minidump com o RIP de cada uma traduzido para função (sub_XXXXXXXX = código do jogo) e os
// endereços de retorno que aparecem na pilha do host. Uso: node dump_threads.js <arquivo.dmp> <exe> [palavras=400]
const fs = require('fs');
const { execFileSync } = require('child_process');
const [, , dmpPath, exePath, wordsArg] = process.argv;
const words = parseInt(wordsArg || '400', 10);
const d = fs.readFileSync(dmpPath);
const nStreams = d.readUInt32LE(8), dirRva = d.readUInt32LE(12);
const streams = {};
for (let i = 0; i < nStreams; i++) {
  const o = dirRva + i * 12;
  streams[d.readUInt32LE(o)] = { size: d.readUInt32LE(o + 4), rva: d.readUInt32LE(o + 8) };
}
// módulos
let base = 0n, size = 0;
{
  const rva = streams[4].rva, n = d.readUInt32LE(rva);
  for (let k = 0; k < n; k++) {
    const m = rva + 4 + k * 108;
    const nameRva = d.readUInt32LE(m + 20), len = d.readUInt32LE(nameRva);
    const name = d.toString('utf16le', nameRva + 4, nameRva + 4 + len);
    if (/DefJamFFNY\.exe$/i.test(name)) { base = d.readBigUInt64LE(m); size = d.readUInt32LE(m + 8); }
  }
}
// memória (lista de faixas) para ler as pilhas
const ranges = [];
if (streams[5]) { // MemoryListStream
  const rva = streams[5].rva, n = d.readUInt32LE(rva);
  for (let k = 0; k < n; k++) {
    const o = rva + 4 + k * 16;
    ranges.push({ start: d.readBigUInt64LE(o), size: d.readUInt32LE(o + 8), rva: d.readUInt32LE(o + 12) });
  }
}
function read64(addr) {
  for (const r of ranges)
    if (addr >= r.start && addr + 8n <= r.start + BigInt(r.size)) return d.readBigUInt64LE(r.rva + Number(addr - r.start));
  return null;
}
const inExe = (a) => a >= base && a < base + BigInt(size);
const symCache = {};
function sym(addrs) {
  const need = addrs.filter((a) => !(a in symCache));
  if (need.length) {
    const offs = need.map((a) => '0x' + (a - base + 0x140000000n).toString(16));
    const out = execFileSync('E:/Tools/llvm/bin/llvm-symbolizer.exe', ['--obj=' + exePath, ...offs]).toString()
      .split(/\r?\n\r?\n/).filter((s) => s.trim());
    need.forEach((a, i) => { symCache[a] = (out[i] || '?').split(/\r?\n/)[0]; });
  }
  return addrs.map((a) => symCache[a]);
}
const tl = streams[3].rva, nt = d.readUInt32LE(tl);
for (let k = 0; k < nt; k++) {
  const o = tl + 4 + k * 48;
  const tid = d.readUInt32LE(o), stackStart = d.readBigUInt64LE(o + 24);
  const ctxRva = d.readUInt32LE(o + 44);
  const rip = d.readBigUInt64LE(ctxRva + 0xF8), rsp = d.readBigUInt64LE(ctxRva + 0x98);
  const chain = [];
  if (inExe(rip)) chain.push(rip);
  for (let w = 0; w < words && chain.length < 14; w++) {
    const v = read64(rsp + BigInt(w * 8));
    if (v !== null && inExe(v) && !chain.includes(v)) chain.push(v);
  }
  if (!chain.length) continue;
  const names = sym(chain).filter((n, i, a) => a.indexOf(n) === i);
  console.log('thread', tid, '| rip no exe:', inExe(rip), '|', names.join(' <- '));
}
