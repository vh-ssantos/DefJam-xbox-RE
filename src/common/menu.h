// Menu de opções do port (F10 ou BACK+START no controle), desenhado por cima do jogo com ImGui.
#pragma once
#include <cstdint>
#include <string>

namespace input {
struct Pad;
}

namespace defjam {

// opções do port salvas no settings.toml (skip_intros, pause_on_focus_loss)
extern uint32_t g_skip_videos;  // 0 nenhum, 1 só os logos (EA, THX), 2 logos e o filme de abertura
extern bool g_pause_unfocused;

bool MenuOpen();
// um vídeo da abertura começou (logo = EA ou THX): conforme g_skip_videos, PadHook aperta START logo depois
void IntroMovieStarted(bool logo);
bool SkipIntroPress();  // START automático agora?
// cheats do jogo: o menu enfileira um código; a thread do jogo o aplica (game/hooks.cpp) e devolve a resposta
bool TakeCheatRequest(std::string& code);
void CheatResult(const std::string& text);
void ApplyPendingCheat();
// nome digitado no teclado do PC para o próximo User ID (A-Z, 0-9, até 8): a thread do jogo o pega uma vez
bool TakeTypedName(std::string& name);
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
