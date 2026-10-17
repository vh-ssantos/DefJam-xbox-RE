// Extrai uma imagem de disco do Xbox original (XDVDFS / "xiso"). Uso: node xiso_extract.js <imagem.iso> <pasta de saída>
// Formato: cabeçalho "MICROSOFT*XBOX*MEDIA" no setor 32 (0x10000) da partição do jogo; diretórios são árvores binárias
// de entradas {esq (u16, em dwords), dir (u16), setor (u32), tamanho (u32), atributos (u8), tamanho do nome (u8), nome}.
const fs = require('fs');
const path = require('path');

const [, , isoPath, outDir] = process.argv;
if (!isoPath || !outDir) {
  console.error('uso: node xiso_extract.js <imagem.iso> <pasta>');
  process.exit(1);
}

const fd = fs.openSync(isoPath, 'r');
const SECTOR = 2048;
const MAGIC = 'MICROSOFT*XBOX*MEDIA';

function read(offset, length) {
  const buf = Buffer.alloc(length);
  fs.readSync(fd, buf, 0, length, offset);
  return buf;
}

// A imagem pode ser "xiso" (partição no início) ou cópia completa (partição do jogo em 0x18300000 nos discos XGD1).
let base = -1;
for (const candidate of [0, 0x18300000, 0x0FD90000, 0x02080000]) {
  try {
    if (read(candidate + 32 * SECTOR, 20).toString('latin1') === MAGIC) {
      base = candidate;
      break;
    }
  } catch (e) { /* fora do arquivo */ }
}
if (base < 0) {
  console.error('cabeçalho XDVDFS não encontrado');
  process.exit(1);
}
const header = read(base + 32 * SECTOR, 36);
const rootSector = header.readUInt32LE(20);
const rootSize = header.readUInt32LE(24);
console.log(`XDVDFS em 0x${base.toString(16)}: raiz no setor ${rootSector}, ${rootSize} bytes`);

let files = 0, bytes = 0;
function walkDir(sector, size, dest) {
  if (size === 0) return;
  const dir = read(base + sector * SECTOR, size);
  fs.mkdirSync(dest, { recursive: true });
  const seen = new Set();
  const visit = (offsetDwords) => {
    const off = offsetDwords * 4;
    if (off + 14 > dir.length || seen.has(off)) return;
    seen.add(off);
    const left = dir.readUInt16LE(off);
    if (left === 0xFFFF) return;
    const right = dir.readUInt16LE(off + 2);
    const start = dir.readUInt32LE(off + 4);
    const length = dir.readUInt32LE(off + 8);
    const attrs = dir[off + 12];
    const nameLen = dir[off + 13];
    const name = dir.toString('latin1', off + 14, off + 14 + nameLen);
    if (left) visit(left);
    const target = path.join(dest, name);
    if (attrs & 0x10) {
      walkDir(start, length, target);
    } else {
      const out = fs.openSync(target, 'w');
      let done = 0;
      const chunk = 8 * 1024 * 1024;
      while (done < length) {
        const n = Math.min(chunk, length - done);
        fs.writeSync(out, read(base + start * SECTOR + done, n));
        done += n;
      }
      fs.closeSync(out);
      ++files;
      bytes += length;
    }
    if (right) visit(right);
  };
  visit(0);
}
walkDir(rootSector, rootSize, outDir);
console.log(`${files} arquivos, ${(bytes / 1048576).toFixed(1)} MB em ${outDir}`);
