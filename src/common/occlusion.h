// Oclusão de ambiente (SSAO / GTAO) como extensão do renderer.
#pragma once
#include <cstdint>

namespace defjam {

extern uint32_t g_ao_mode;  // 0 desligada, 1 SSAO, 2 GTAO
void InstallOcclusion();    // GameInfo::init: antes de o renderer ser criado

}  // namespace defjam
