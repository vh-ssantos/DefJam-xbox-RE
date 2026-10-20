// Teclado (e mouse) do PC como controle do jogador 1 e troca dos botões do controle, configuráveis no menu F10 e
// guardados no settings.toml ([teclado] e [controle], nomes legíveis). Padrão igual ao do port do vyanhursky: o
// losango I/J/K/L como os botões do controle, WASD anda, setas no analógico direito.
#pragma once
#include <cstdint>
#include <string>

namespace input {
struct Pad;
}

namespace defjam {

// os botões do controle do Xbox (0-15) e as direções dos dois analógicos (16-23, só no teclado)
enum Action {
    kActA, kActB, kActX, kActY, kActBlack, kActWhite, kActLT, kActRT, kActStart, kActBack,
    kActUp, kActDown, kActLeft, kActRight, kActL3, kActR3,
    kActLsUp, kActLsDown, kActLsLeft, kActLsRight, kActRsUp, kActRsDown, kActRsLeft, kActRsRight,
    kActions
};
constexpr int kPadButtons = kActLsUp;  // as ações que o controle pode trocar
constexpr int kKeySlots = 3;           // teclas (ou botões do mouse) por ação

extern bool g_keyboard;  // teclado como controle do jogador 1 (settings: keyboard)

const char* ActionLabel(int action);         // no idioma do port, com o que faz na luta
std::string KeyLabel(int action, int slot);  // a tecla nessa posição, "-" se vazia
const char* PadLabel(int action);            // o botão do controle que faz essa ação
void ResetControls();                        // teclas e botões padrão

// captura: a próxima tecla/botão do mouse (janela) ou o próximo botão do controle (thread do jogo) vira o da ação.
// Na tecla: Esc cancela, Delete esvazia a posição.
enum class Capture { None, Key, Pad };
void StartCapture(Capture what, int action, int slot = 0);
void CancelCapture();
Capture Capturing();
bool CaptureBusy();  // capturando, ou acabou de pegar (o menu ignora a navegação e os cliques)
bool TakeChanged();  // houve troca desde a última chamada (o menu salva)

// GameInfo::message: guarda as teclas e botões do mouse apertados; na captura, pega o novo. true = mensagem usada
bool KeyMessage(uint32_t msg, uintptr_t wp, intptr_t lp);
// thread do jogo: a troca de botões (todos os controles; a captura só no 0); depois, fora do menu, o teclado no 0
void RemapPad(int port, input::Pad& pad);
void ApplyKeyboard(input::Pad& pad);

// testes (DEFJAM_PAD_CMD "KEY_J"): tecla apertada só dentro do processo, nunca no teclado do PC. Begin a cada
// leitura do controle 0, Press para cada tecla do comando em curso (pega a captura como uma tecla de verdade).
void BeginTestKeys();
void PressTestKey(const std::string& name);

// GameInfo::load/save: as tabelas [teclado] e [controle] do settings.toml
void LoadControls(const void* table);
void SaveControls(void* table);

}  // namespace defjam
