// Desmonta um trecho do default.xbe (x86 32 bits) com llvm-mc.
// Uso: node xbe_disasm.js <default.xbe> <va_hex> [<bytes>=256]
const fs = require('fs');
const { execFileSync } = require('child_process');
const [, , xbePath, vaHex, lenArg] = process.argv;
const b = fs.readFileSync(xbePath);
const va = parseInt(vaHex, 16), len = parseInt(lenArg || '256', 10);
const base = b.readUInt32LE(0x104), nsec = b.readUInt32LE(0x11C), sh = b.readUInt32LE(0x120) - base;
let off = -1;
for (let i = 0; i < nsec; i++) {
  const o = sh + i * 56;
  const sva = b.readUInt32LE(o + 4), ra = b.readUInt32LE(o + 12), rs = b.readUInt32LE(o + 16);
  if (va >= sva && va < sva + rs) off = ra + (va - sva);
}
if (off < 0) { console.log('endereço fora das seções'); process.exit(1); }
const bytes = [...b.subarray(off, off + len)].map((x) => '0x' + x.toString(16)).join(' ');
const out = execFileSync('E:/Tools/llvm/bin/llvm-mc.exe',
  ['--disassemble', '-triple=i386', '--output-asm-variant=1', '--show-encoding'], { input: bytes }).toString();
// numera cada instrução com o endereço do guest
let pc = va;
for (const line of out.split('\n')) {
  const m = line.match(/^\s+(.*?)\s*# encoding: \[(.*)\]/);
  if (!m) continue;
  const n = m[2].split(',').length;
  console.log(pc.toString(16).padStart(8, '0') + '  ' + m[1]);
  pc += n;
}
