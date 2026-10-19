// Idioma dos textos do port (menu do PC, mensagens): português do Brasil ou inglês. O jogo em si é em inglês.
#pragma once
#include <cstdint>

namespace defjam {

extern uint32_t g_language;  // settings.toml language: 0 automático (idioma do Windows), 1 português, 2 inglês
bool Portuguese();           // o idioma em uso é português?
// o texto no idioma em uso
inline const char* T(const char* pt, const char* en) { return Portuguese() ? pt : en; }

}  // namespace defjam
