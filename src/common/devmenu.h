// Menu de desenvolvedor da EA (o módulo CRunModuleAki, desligado da versão de loja): montado por fora e entregue ao
// gerenciador de módulos do jogo (src/game/devmenu.cpp). Pesquisa e endereços: vyanhursky/DefJamFFNY-recomp (MIT),
// docs/research/dev-module-revival.md.
#pragma once
#include <string>

namespace defjam {

// pedidos do menu F10 (qualquer thread); a thread do jogo atende no próximo passo do gerenciador
void OpenDevMenu();   // só a partir do front end (título, menu principal)
void LeaveDevMenu();  // volta ao menu principal
bool DevMenuActive();  // o módulo de dev é o que está rodando
std::string DevMenuMessage();  // o resultado do último pedido, para o F10 mostrar

}  // namespace defjam
