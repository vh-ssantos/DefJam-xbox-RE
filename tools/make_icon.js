// Gera o ícone do Windows a partir do ícone oficial do jogo no dashboard do Xbox: a seção $$XTIMAGE do default.xbe
// (XPR0 com uma textura DXT1 de 128x128). Assim o ícone sai da cópia do jogo do usuário e não vai para o git.
// Uso: node make_icon.js <default.xbe> <saida.ico>
const fs = require('fs');
const zlib = require('zlib');

const [, , xbePath, outPath] = process.argv;
const b = fs.readFileSync(xbePath);
const base = b.readUInt32LE(0x104), nsec = b.readUInt32LE(0x11C), sh = b.readUInt32LE(0x120) - base;
let xpr = null;
for (let i = 0; i < nsec; i++) {
  const o = sh + i * 56;
  const na = b.readUInt32LE(o + 20) - base;
  let name = '';
  for (let k = na; b[k]; k++) name += String.fromCharCode(b[k]);
  if (name === '$$XTIMAGE') xpr = b.subarray(b.readUInt32LE(o + 12), b.readUInt32LE(o + 12) + b.readUInt32LE(o + 16));
}
if (!xpr || xpr.toString('latin1', 0, 4) !== 'XPR0') {
  console.error('sem $$XTIMAGE (XPR0) no XBE');
  process.exit(1);
}
const headerSize = xpr.readUInt32LE(8);
const format = xpr.readUInt32LE(12 + 12);  // recurso de textura: common, data, lock, format
const fmt = (format >> 8) & 0xFF, w = 1 << ((format >> 20) & 15), h = 1 << ((format >> 24) & 15);
if (fmt !== 0x0C) {
  console.error('formato ' + fmt.toString(16) + ' não suportado (esperado DXT1)');
  process.exit(1);
}
const data = xpr.subarray(headerSize);

// DXT1 -> RGBA
const rgba = Buffer.alloc(w * h * 4);
const c565 = (v) => [((v >> 11) & 31) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31];
let p = 0;
for (let by = 0; by < h; by += 4)
  for (let bx = 0; bx < w; bx += 4, p += 8) {
    const a = data.readUInt16LE(p), c = data.readUInt16LE(p + 2), bits = data.readUInt32LE(p + 4);
    const A = c565(a), C = c565(c);
    const pal = a > c
      ? [[...A, 255], [...C, 255], [...A.map((x, i) => (2 * x + C[i]) / 3), 255], [...A.map((x, i) => (x + 2 * C[i]) / 3), 255]]
      : [[...A, 255], [...C, 255], [...A.map((x, i) => (x + C[i]) / 2), 255], [0, 0, 0, 0]];
    for (let i = 0; i < 16; i++) {
      const col = pal[(bits >>> (2 * i)) & 3];
      const o = ((by + (i >> 2)) * w + bx + (i & 3)) * 4;
      for (let k = 0; k < 4; k++) rgba[o + k] = Math.round(col[k]);
    }
  }

// reamostragem (bilinear para aumentar, média de caixa para reduzir)
function resize(src, sw, sh2, dw, dh) {
  const out = Buffer.alloc(dw * dh * 4);
  for (let y = 0; y < dh; y++)
    for (let x = 0; x < dw; x++) {
      const acc = [0, 0, 0, 0];
      if (dw >= sw) {
        const fx = Math.min((x + 0.5) * sw / dw - 0.5, sw - 1), fy = Math.min((y + 0.5) * sh2 / dh - 0.5, sh2 - 1);
        const x0 = Math.max(Math.floor(fx), 0), y0 = Math.max(Math.floor(fy), 0);
        const x1 = Math.min(x0 + 1, sw - 1), y1 = Math.min(y0 + 1, sh2 - 1), tx = Math.max(fx - x0, 0), ty = Math.max(fy - y0, 0);
        for (let k = 0; k < 4; k++) {
          const v = (s, t) => src[(t * sw + s) * 4 + k];
          acc[k] = (v(x0, y0) * (1 - tx) + v(x1, y0) * tx) * (1 - ty) + (v(x0, y1) * (1 - tx) + v(x1, y1) * tx) * ty;
        }
      } else {
        const r = sw / dw;
        let n = 0;
        for (let yy = Math.floor(y * r); yy < Math.floor((y + 1) * r); yy++)
          for (let xx = Math.floor(x * r); xx < Math.floor((x + 1) * r); xx++, n++)
            for (let k = 0; k < 4; k++) acc[k] += src[(yy * sw + xx) * 4 + k];
        for (let k = 0; k < 4; k++) acc[k] /= n;
      }
      for (let k = 0; k < 4; k++) out[(y * dw + x) * 4 + k] = Math.round(acc[k]);
    }
  return out;
}

// PNG RGBA
const crcTable = Array.from({ length: 256 }, (_, n) => {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});
const crc = (buf) => {
  let c = 0xFFFFFFFF;
  for (const x of buf) c = crcTable[(c ^ x) & 255] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
};
function png(px, pw, ph) {
  const chunk = (type, body) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(body.length);
    const tb = Buffer.concat([Buffer.from(type, 'latin1'), body]);
    const c = Buffer.alloc(4);
    c.writeUInt32BE(crc(tb));
    return Buffer.concat([len, tb, c]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(pw, 0);
  ihdr.writeUInt32BE(ph, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  const raw = Buffer.alloc((pw * 4 + 1) * ph);
  for (let y = 0; y < ph; y++) px.copy(raw, y * (pw * 4 + 1) + 1, y * pw * 4, (y + 1) * pw * 4);
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]), chunk('IHDR', ihdr),
    chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

// ICO com imagens PNG (Windows Vista em diante)
const sizes = [256, 128, 64, 48, 32, 16];
const images = sizes.map((s) => png(s === w ? rgba : resize(rgba, w, h, s, s), s, s));
const dir = Buffer.alloc(6 + 16 * sizes.length);
dir.writeUInt16LE(1, 2);
dir.writeUInt16LE(sizes.length, 4);
let offset = dir.length;
sizes.forEach((s, i) => {
  const e = 6 + 16 * i;
  dir[e] = s === 256 ? 0 : s;
  dir[e + 1] = s === 256 ? 0 : s;
  dir.writeUInt16LE(1, e + 4);
  dir.writeUInt16LE(32, e + 6);
  dir.writeUInt32LE(images[i].length, e + 8);
  dir.writeUInt32LE(offset, e + 12);
  offset += images[i].length;
});
fs.mkdirSync(require('path').dirname(outPath), { recursive: true });
fs.writeFileSync(outPath, Buffer.concat([dir, ...images]));
fs.writeFileSync(outPath.replace(/\.ico$/i, '.png'), images[0]);
console.log(`ícone ${w}x${h} -> ${outPath}`);
