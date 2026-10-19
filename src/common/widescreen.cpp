// Widescreen do 2D. O jogo desenha tudo numa tela de 640x480 que o renderer estica até 16:9. O 3D sai certo porque a
// câmera já foi aberta (Hor+); o 2D precisa ser apertado de volta para a proporção de 4:3. Cada desenho chega aqui com
// o lugar onde cai (calculado na CPU pelo renderer): os planos (w = 1 em todos os vértices) são 2D.
//
// Regras (as mesmas que o port do vyanhursky mediu no mesmo XBE, docs/widescreen/02-design.md de lá):
//   - 3D: fica como está, a não ser num retângulo menor que a tela (anda com o 2D, centralizado);
//   - 2D da largura da tela inteira sem imagem, ou com textura pequena (escurecer, tingir, vinheta): esticado;
//   - qualquer outro 2D: centralizado (apertado por k = (4/3) / proporção);
//   - durante a luta, as peças inteiras à esquerda de 300 (P1) e à direita de 340 (P2) vão para a borda delas.
#include "common/widescreen.h"

#include <atomic>
#include <chrono>
#include <cstdlib>

#include "app/app.h"

namespace defjam {

uint32_t g_hud_wide = 1;

namespace {

std::atomic<int64_t> g_match_ms{0};

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

constexpr float kHalfLeft = 300.0f / 640.0f;   // o placar do P1 acaba antes disto
constexpr float kHalfRight = 340.0f / 640.0f;  // e o do P2 começa depois
constexpr uint32_t kSmallTexture = 128;        // escurecer, tingir, vinheta: esticados
constexpr int64_t kMatchMs = 250;              // a luta "acabou" se o passo dela parou faz isso

}  // namespace

float ScreenAspect() { return app::Widescreen() ? 16.0f / 9.0f : 4.0f / 3.0f; }

void MatchTick() { g_match_ms.store(NowMs(), std::memory_order_relaxed); }

void PlaceDraw(const gpu::State& s, const gpu::Target* rt, uint32_t gw, uint32_t gh, const gpu::Extension::Footprint& f, float* place) {
    static const bool off = std::getenv("DEFJAM_NO_HUD_FIX") != nullptr;
    if (off || !g_hud_wide || !rt || !gw || !gh) return;
    const float k = (4.0f / 3.0f) / ScreenAspect();
    if (k >= 0.999f) return;
    // só a tela principal (alvos 4:3 de 640 ou mais; texturas que o jogo desenha, menores, ficam como estão)
    if (gw < 640 || gw * 3 != gh * 4) return;
    if (f.kind != 1) {
        // 3D num retângulo menor que a tela (o lutador num quadro do menu): a câmera dele ficou 4:3 (hooks.cpp),
        // então ele anda junto com o 2D, centralizado
        const auto& v = s.viewport;
        if (v.x > 0 || v.x + v.w < gw) place[0] = k, place[1] = 0;
        return;
    }
    const float x0 = f.x0 / float(gw), x1 = f.x1 / float(gw);
    const bool full = x0 <= 0.02f && x1 >= 0.98f;
    if (full && (!f.tex_w || (f.tex_w <= kSmallTexture && f.tex_h <= kSmallTexture))) return;
    place[0] = k;
    place[1] = 0;  // centro
    const bool match = NowMs() - g_match_ms.load(std::memory_order_relaxed) < kMatchMs;
    if (match && !full) {
        if (x1 <= kHalfLeft) place[1] = k - 1;        // presa à esquerda
        else if (x0 >= kHalfRight) place[1] = 1 - k;  // presa à direita
    }
}

}  // namespace defjam
