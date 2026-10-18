// Menu de opções do port (F10 ou BACK+START no controle), desenhado por cima do jogo com ImGui.
#pragma once
#include <cstdint>

namespace input {
struct Pad;
}

namespace defjam {

// opções do port salvas no settings.toml (skip_intros, pause_on_focus_loss)
extern bool g_skip_intros;
extern bool g_pause_unfocused;

bool MenuOpen();
// um vídeo da abertura começou: com g_skip_intros, PadHook aperta START logo depois
void IntroMovieStarted();
bool SkipIntroPress();  // START automático agora?
// thread do jogo: com g_pause_unfocused, espera aqui (som parado) enquanto a janela está em segundo plano
void WaitWhileUnfocused();
// a janela do jogo (GameInfo::window), para esconder o cursor do mouse parado
void SetWindow(void* hwnd);
void ToggleMenu();
// o controle 0 como o jogador o segura (antes de o menu tirá-lo do jogo): navega no menu
void FeedPad(const input::Pad& pad);
// GameInfo::ui: cada quadro, dentro do quadro do ImGui
void DrawUi(double fps);
// GameInfo::message: F10 abre e fecha, setas/Enter/Esc navegam
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);

}  // namespace defjam
