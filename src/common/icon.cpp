// Ícone do jogo tirado da cópia do jogador: a seção $$XTIMAGE do default.xbe (ícone do dashboard do Xbox, XPR0 com uma
// textura DXT1 de 128x128). Vira o ícone da janela e um DefJamFFNY.ico ao lado do exe (para atalhos), assim o exe
// distribuído não leva arte do jogo dentro. Mesma decodificação de tools/make_icon.js.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "app/app.h"
#include "common/icon.h"
#include "core/log.h"
#include "xbe/xbe.h"

namespace kernel {
extern const xbe::Image* g_image;
}

namespace defjam {

namespace {

// RGBA do ícone, ou vazio
std::vector<uint8_t> DecodeIcon(uint32_t& w, uint32_t& h) {
    const xbe::Image* img = kernel::g_image;
    if (!img) return {};
    for (const xbe::Section& s : img->sections) {
        if (s.name != "$$XTIMAGE" || s.raw + s.rsize > img->file.size() || s.rsize < 64) continue;
        const uint8_t* x = img->file.data() + s.raw;
        if (std::memcmp(x, "XPR0", 4)) return {};
        uint32_t header, format;
        std::memcpy(&header, x + 8, 4);
        std::memcpy(&format, x + 24, 4);  // recurso de textura: common, data, lock, format
        if (((format >> 8) & 0xFF) != 0x0C) return {};  // DXT1
        w = 1u << ((format >> 20) & 15);
        h = 1u << ((format >> 24) & 15);
        if (header + w * h / 2 > s.rsize) return {};
        const uint8_t* d = x + header;
        std::vector<uint8_t> rgba(size_t(w) * h * 4);
        auto c565 = [](uint16_t v, float o[3]) {
            o[0] = ((v >> 11) & 31) * 255.0f / 31;
            o[1] = ((v >> 5) & 63) * 255.0f / 63;
            o[2] = (v & 31) * 255.0f / 31;
        };
        for (uint32_t by = 0; by < h; by += 4)
            for (uint32_t bx = 0; bx < w; bx += 4, d += 8) {
                uint16_t a, c;
                uint32_t bits;
                std::memcpy(&a, d, 2);
                std::memcpy(&c, d + 2, 2);
                std::memcpy(&bits, d + 4, 4);
                float A[3], C[3], pal[4][4];
                c565(a, A);
                c565(c, C);
                for (int k = 0; k < 3; k++) {
                    pal[0][k] = A[k];
                    pal[1][k] = C[k];
                    pal[2][k] = a > c ? (2 * A[k] + C[k]) / 3 : (A[k] + C[k]) / 2;
                    pal[3][k] = a > c ? (A[k] + 2 * C[k]) / 3 : 0;
                }
                pal[0][3] = pal[1][3] = pal[2][3] = 255;
                pal[3][3] = a > c ? 255.0f : 0.0f;
                for (int i = 0; i < 16; i++) {
                    const float* p = pal[(bits >> (2 * i)) & 3];
                    uint8_t* o = &rgba[(size_t(by + i / 4) * w + bx + i % 4) * 4];
                    for (int k = 0; k < 4; k++) o[k] = uint8_t(p[k] + 0.5f);
                }
            }
        return rgba;
    }
    return {};
}

// redução por média de caixa (128 -> 64, 48, 32, 16)
std::vector<uint8_t> Shrink(const std::vector<uint8_t>& src, uint32_t sw, uint32_t dw) {
    std::vector<uint8_t> out(size_t(dw) * dw * 4);
    const float r = float(sw) / dw;
    for (uint32_t y = 0; y < dw; y++)
        for (uint32_t x = 0; x < dw; x++) {
            float acc[4] = {};
            int n = 0;
            for (uint32_t yy = uint32_t(y * r); yy < uint32_t((y + 1) * r); yy++)
                for (uint32_t xx = uint32_t(x * r); xx < uint32_t((x + 1) * r); xx++, n++)
                    for (int k = 0; k < 4; k++) acc[k] += src[(size_t(yy) * sw + xx) * 4 + k];
            for (int k = 0; k < 4; k++) out[(size_t(y) * dw + x) * 4 + k] = uint8_t(acc[k] / std::max(n, 1));
        }
    return out;
}

// imagem de ícone: BITMAPINFOHEADER (altura dobrada), BGRA de baixo para cima e a máscara AND (zeros)
std::vector<uint8_t> IconDib(const std::vector<uint8_t>& rgba, uint32_t s) {
    const uint32_t mask_row = ((s + 31) / 32) * 4;
    std::vector<uint8_t> dib(40 + size_t(s) * s * 4 + size_t(mask_row) * s, 0);
    BITMAPINFOHEADER bh{};
    bh.biSize = 40;
    bh.biWidth = LONG(s);
    bh.biHeight = LONG(s * 2);
    bh.biPlanes = 1;
    bh.biBitCount = 32;
    std::memcpy(dib.data(), &bh, 40);
    for (uint32_t y = 0; y < s; y++)
        for (uint32_t x = 0; x < s; x++) {
            const uint8_t* p = &rgba[(size_t(s - 1 - y) * s + x) * 4];
            uint8_t* o = &dib[40 + (size_t(y) * s + x) * 4];
            o[0] = p[2];
            o[1] = p[1];
            o[2] = p[0];
            o[3] = p[3];
        }
    return dib;
}

HICON MakeIcon(const std::vector<uint8_t>& rgba, uint32_t s) {
    std::vector<uint8_t> dib = IconDib(rgba, s);
    return CreateIconFromResourceEx(dib.data(), DWORD(dib.size()), TRUE, 0x00030000, int(s), int(s), LR_DEFAULTCOLOR);
}

}  // namespace

void ApplyGameIcon(void* hwnd) {
    uint32_t w = 0, h = 0;
    const std::vector<uint8_t> rgba = DecodeIcon(w, h);
    if (rgba.empty() || w != h) return;
    const std::vector<uint8_t> i32 = Shrink(rgba, w, 32), i16 = Shrink(rgba, w, 16);
    if (hwnd) {
        if (HICON big = MakeIcon(w > 64 ? Shrink(rgba, w, 64) : rgba, w > 64 ? 64 : w))
            PostMessageW(static_cast<HWND>(hwnd), WM_SETICON, ICON_BIG, LPARAM(big));
        if (HICON small = MakeIcon(i16, 16)) PostMessageW(static_cast<HWND>(hwnd), WM_SETICON, ICON_SMALL, LPARAM(small));
    }
    // DefJamFFNY.ico ao lado do exe, para atalhos (uma vez)
    const auto path = app::ExeDir() / L"DefJamFFNY.ico";
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) return;
    const uint32_t sizes[] = {128, 64, 48, 32, 16};
    std::vector<std::vector<uint8_t>> dibs;
    for (uint32_t s : sizes) dibs.push_back(IconDib(s == w ? rgba : Shrink(rgba, w, s), s));
    std::ofstream f(path, std::ios::binary);
    const uint16_t head[3] = {0, 1, uint16_t(dibs.size())};
    f.write(reinterpret_cast<const char*>(head), 6);
    uint32_t offset = 6 + 16 * uint32_t(dibs.size());
    for (size_t i = 0; i < dibs.size(); i++) {
        uint8_t e[16] = {};
        e[0] = uint8_t(sizes[i] >= 256 ? 0 : sizes[i]);
        e[1] = e[0];
        e[4] = 1;
        e[6] = 32;
        const uint32_t len = uint32_t(dibs[i].size());
        std::memcpy(e + 8, &len, 4);
        std::memcpy(e + 12, &offset, 4);
        f.write(reinterpret_cast<const char*>(e), 16);
        offset += len;
    }
    for (const auto& d : dibs) f.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
    LOGI("defjam: icone salvo em %s", path.string().c_str());
}

}  // namespace defjam
