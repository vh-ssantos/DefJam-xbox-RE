// Widescreen do 2D: menus e placar com a proporção do jogo numa tela 16:9 (o 3D é aberto pela câmera, hooks.cpp).
#pragma once
#include <cstdint>

#include "gpu/ext.h"

namespace defjam {

extern uint32_t g_hud_wide;  // 1 (padrão): 2D na proporção certa; 0: esticado como antes
float ScreenAspect();        // a proporção da imagem que o renderer apresenta: 16:9 ou 4:3
void MatchTick();            // o passo da luta rodou (só roda dentro de uma luta)
// a colocação de um desenho (gpu::Extension::Place)
void PlaceDraw(const gpu::State& s, const gpu::Target* rt, uint32_t gw, uint32_t gh, const gpu::Extension::Footprint& f, float* place);

}  // namespace defjam
