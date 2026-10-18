// Menu de opções do port (F10 ou BACK+START no controle), desenhado por cima do jogo com ImGui.
#pragma once
#include <cstdint>

namespace input {
struct Pad;
}

namespace defjam {

bool MenuOpen();
void ToggleMenu();
// o controle 0 como o jogador o segura (antes de o menu tirá-lo do jogo): navega no menu
void FeedPad(const input::Pad& pad);
// GameInfo::ui: cada quadro, dentro do quadro do ImGui
void DrawUi(double fps);
// GameInfo::message: F10 abre e fecha, setas/Enter/Esc navegam
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);

}  // namespace defjam
