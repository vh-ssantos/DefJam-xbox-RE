// Teclado do PC como controle do jogador 1 e troca dos botões do controle (veja controls.h).
#include "common/controls.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>

#include <toml++/toml.hpp>

#include "common/lang.h"
#include "core/input.h"

namespace defjam {

bool g_keyboard = true;

namespace {

// nomes no settings.toml, na ordem de Action
const char* const kIds[kActions] = {
    "a", "b", "x", "y", "preto", "branco", "gatilho_l", "gatilho_r", "start", "back",
    "dpad_cima", "dpad_baixo", "dpad_esquerda", "dpad_direita", "l3", "r3",
    "esq_cima", "esq_baixo", "esq_esquerda", "esq_direita", "dir_cima", "dir_baixo", "dir_esquerda", "dir_direita"};

// mão esquerda anda (WASD), mão direita bate (J soco, K chute, L agarra; U/O os gatilhos)
const uint32_t kDefaultKeys[kActions] = {
    'L', 'I', 'K', 'J', 'M', 'N', 'U', 'O', VK_RETURN, VK_BACK,
    VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, 'Q', 'E',
    'W', 'S', 'A', 'D', 'T', 'G', 'F', 'H'};

std::atomic<uint32_t> g_key[kActions];
std::atomic<uint8_t> g_from[kPadButtons];  // a ação i do jogo vem do botão g_from[i] do controle
std::atomic<bool> g_down[256];             // teclas apertadas agora (só chegam com a janela em foco)
std::atomic<bool> g_test[256];             // teclas dos testes (DEFJAM_PAD_CMD), só na thread do jogo
bool g_test_before[256];

std::atomic<int> g_capture{0};  // Capture
std::atomic<int> g_capture_action{0};
std::atomic<bool> g_capture_released{false};  // botão: espera soltar tudo antes de pegar o próximo
std::atomic<bool> g_changed{false};

// os botões do controle do Xbox: 0-7 analógicos (A B X Y preto branco L R), o resto bits de buttons
constexpr uint16_t kBits[kPadButtons] = {0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0x20, 0x1, 0x2, 0x4, 0x8, 0x40, 0x80};
uint8_t Get(const input::Pad& p, int i) { return i < 8 ? p.analog[i] : (p.buttons & kBits[i]) ? 0xFF : 0; }
void Set(input::Pad& p, int i, uint8_t v) {
    if (i < 8) p.analog[i] = v;
    else if (v > 0x7F) p.buttons |= kBits[i];
    else p.buttons &= uint16_t(~kBits[i]);
}

// o nome da tecla no settings.toml: estável, em inglês, sem depender do idioma do Windows
std::string KeyName(uint32_t vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, char(vk));
    if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return "Num" + std::to_string(vk - VK_NUMPAD0);
    static const struct {
        uint32_t vk;
        const char* name;
    } kNames[] = {{VK_RETURN, "Enter"},   {VK_BACK, "Backspace"}, {VK_TAB, "Tab"},       {VK_SPACE, "Space"},
                  {VK_SHIFT, "Shift"},    {VK_CONTROL, "Ctrl"},   {VK_MENU, "Alt"},      {VK_UP, "Up"},
                  {VK_DOWN, "Down"},      {VK_LEFT, "Left"},      {VK_RIGHT, "Right"},   {VK_INSERT, "Insert"},
                  {VK_DELETE, "Delete"},  {VK_HOME, "Home"},      {VK_END, "End"},       {VK_PRIOR, "PageUp"},
                  {VK_NEXT, "PageDown"},  {VK_CAPITAL, "CapsLock"}, {VK_MULTIPLY, "Num*"}, {VK_ADD, "Num+"},
                  {VK_SUBTRACT, "Num-"},  {VK_DECIMAL, "Num."},   {VK_DIVIDE, "Num/"}};
    for (const auto& n : kNames)
        if (n.vk == vk) return n.name;
    char hex[8];
    std::snprintf(hex, sizeof hex, "0x%02X", vk);
    return hex;
}

uint32_t KeyFromName(const std::string& name) {
    for (uint32_t vk = 1; vk < 256; vk++)
        if (KeyName(vk) == name) return vk;
    return 0;
}

const char* PadName(int i) {
    switch (i) {
        case kActBlack: return T("Preto", "Black");
        case kActWhite: return T("Branco", "White");
        case kActLT: return T("Gatilho L", "L trigger");
        case kActRT: return T("Gatilho R", "R trigger");
        case kActUp: return T("Direcional cima", "D-pad up");
        case kActDown: return T("Direcional baixo", "D-pad down");
        case kActLeft: return T("Direcional esquerda", "D-pad left");
        case kActRight: return T("Direcional direita", "D-pad right");
        case kActL3: return T("L3 (apertar analogico)", "L3 (stick click)");
        case kActR3: return T("R3 (apertar analogico)", "R3 (stick click)");
        default: {
            static const char* const kPlain[] = {"A", "B", "X", "Y", "", "", "", "", "START", "BACK"};
            return i >= 0 && i < 10 ? kPlain[i] : "-";
        }
    }
}

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
std::atomic<int64_t> g_capture_end{0};

void EndCapture() {
    g_capture = int(Capture::None);
    g_capture_end = NowMs();
}

// uma tecla (ou botão) faz uma ação só: quem a tinha fica com a antiga da ação nova (troca)
void AssignKey(int action, uint32_t vk) {
    const uint32_t old = g_key[action];
    for (int i = 0; i < kActions; i++)
        if (i != action && g_key[i] == vk) g_key[i] = old;
    g_key[action] = vk;
    g_changed = true;
}
void AssignPad(int action, uint8_t button) {
    const uint8_t old = g_from[action];
    for (int i = 0; i < kPadButtons; i++)
        if (i != action && g_from[i] == button) g_from[i] = old;
    g_from[action] = button;
    g_changed = true;
}

[[maybe_unused]] const bool kInit = [] {
    ResetControls();
    return true;
}();

}  // namespace

const char* ActionLabel(int a) {
    switch (a) {
        case kActA: return T("A (agarrar)", "A (grab)");
        case kActX: return T("X (chute)", "X (kick)");
        case kActY: return T("Y (soco)", "Y (punch)");
        case kActLT: return T("Gatilho L (golpes fortes)", "L trigger (strong attacks)");
        case kActLsUp: return T("Analogico esq.: cima", "Left stick: up");
        case kActLsDown: return T("Analogico esq.: baixo", "Left stick: down");
        case kActLsLeft: return T("Analogico esq.: esquerda", "Left stick: left");
        case kActLsRight: return T("Analogico esq.: direita", "Left stick: right");
        case kActRsUp: return T("Analogico dir.: cima", "Right stick: up");
        case kActRsDown: return T("Analogico dir.: baixo", "Right stick: down");
        case kActRsLeft: return T("Analogico dir.: esquerda", "Right stick: left");
        case kActRsRight: return T("Analogico dir.: direita", "Right stick: right");
        default: return PadName(a);
    }
}

std::string KeyLabel(int a) {
    const uint32_t vk = g_key[a];
    switch (vk) {
        case 0: return "-";
        case VK_UP: return T("Seta cima", "Up arrow");
        case VK_DOWN: return T("Seta baixo", "Down arrow");
        case VK_LEFT: return T("Seta esquerda", "Left arrow");
        case VK_RIGHT: return T("Seta direita", "Right arrow");
        case VK_SPACE: return T("Espaco", "Space");
        default: break;
    }
    // pontuação: o nome que o Windows dá no teclado do jogador (ABNT2, US...), se couber na fonte do menu
    if (vk >= 0xBA && vk <= 0xE2) {
        wchar_t w[32];
        const LONG scan = LONG(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)) << 16;
        if (GetKeyNameTextW(scan, w, 32) == 1 && w[0] > 0x20 && w[0] < 0x7F) return std::string(1, char(w[0]));
        return std::string(T("Tecla ", "Key ")) + KeyName(vk);
    }
    return KeyName(vk);
}

const char* PadLabel(int a) { return a < kPadButtons ? PadName(g_from[a]) : "-"; }

void ResetControls() {
    for (int i = 0; i < kActions; i++) g_key[i] = kDefaultKeys[i];
    for (int i = 0; i < kPadButtons; i++) g_from[i] = uint8_t(i);
    g_changed = true;
}

void StartCapture(Capture what, int action) {
    if (what == Capture::Pad && action >= kPadButtons) return;
    g_capture_action = action;
    g_capture_released = false;
    g_capture = int(what);
}
void CancelCapture() { EndCapture(); }

Capture Capturing() { return Capture(g_capture.load()); }

bool CaptureBusy() {
    // logo depois de pegar, o mesmo aperto ainda chega ao menu como navegação: ignorado por um instante
    return g_capture != int(Capture::None) || NowMs() - g_capture_end < 300;
}

bool TakeChanged() { return g_changed.exchange(false); }

bool KeyMessage(uint32_t msg, uintptr_t wp, intptr_t lp) {
    switch (msg) {
        case WM_KILLFOCUS:
            for (auto& d : g_down) d = false;  // sem foco não chegam os "soltou": nada fica preso
            return false;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wp < 256) g_down[wp] = true;
            if (g_capture == int(Capture::Key)) {
                if (lp & (1 << 30)) return true;  // repetição da tecla segurada (o Enter que abriu a captura)
                if (wp == VK_ESCAPE) EndCapture();
                else if (wp > 0 && wp < 256) {
                    AssignKey(g_capture_action, uint32_t(wp));
                    EndCapture();
                }
                return true;
            }
            return false;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wp < 256) g_down[wp] = false;
            return false;
        default: return false;
    }
}

void RemapPad(int port, input::Pad& pad) {
    uint8_t raw[kPadButtons];
    for (int i = 0; i < kPadButtons; i++) raw[i] = Get(pad, i);
    // só o controle do jogador 1: os outros, vazios, diriam "tudo solto" com o A que abriu a captura ainda apertado
    if (port == 0 && g_capture == int(Capture::Pad)) {
        int pressed = -1;
        for (int i = 0; i < kPadButtons && pressed < 0; i++)
            if (raw[i] > 0x7F) pressed = i;
        if (!g_capture_released) {
            if (pressed < 0) g_capture_released = true;
        } else if (pressed >= 0) {
            AssignPad(g_capture_action, uint8_t(pressed));
            EndCapture();
        }
    }
    for (int i = 0; i < kPadButtons; i++) Set(pad, i, raw[g_from[i]]);
}

void ApplyKeyboard(input::Pad& pad) {
    if (!g_keyboard) return;
    pad.connected = true;  // só no teclado também é o jogador 1
    auto down = [](int a) {
        const uint32_t vk = g_key[a];
        return vk > 0 && vk < 256 && (g_down[vk].load(std::memory_order_relaxed) || g_test[vk].load());
    };
    for (int a = 0; a < kPadButtons; a++)
        if (down(a)) Set(pad, a, 0xFF);
    // tecla de direção = analógico no fim do curso (y para cima, como no controle do Xbox)
    auto stick = [&](int16_t& x, int16_t& y, int first) {
        const int dx = int(down(first + 3)) - int(down(first + 2)), dy = int(down(first)) - int(down(first + 1));
        if (dx) x = dx > 0 ? 32767 : -32768;
        if (dy) y = dy > 0 ? 32767 : -32768;
    };
    stick(pad.lx, pad.ly, kActLsUp);
    stick(pad.rx, pad.ry, kActRsUp);
}

void BeginTestKeys() {
    for (int i = 0; i < 256; i++) g_test_before[i] = g_test[i].exchange(false);
}

void PressTestKey(const std::string& name) {
    const uint32_t vk = KeyFromName(name);
    if (!vk) return;
    g_test[vk] = true;
    if (g_test_before[vk] || g_capture != int(Capture::Key)) return;
    if (vk == VK_ESCAPE) EndCapture();
    else {
        AssignKey(g_capture_action, vk);
        EndCapture();
    }
}

void LoadControls(const void* table) {
    const auto& t = *static_cast<const toml::table*>(table);
    if (const toml::table* keys = t["teclado"].as_table()) {
        for (int a = 0; a < kActions; a++)
            if (auto name = (*keys)[kIds[a]].value<std::string>())
                if (const uint32_t vk = KeyFromName(*name)) g_key[a] = vk;
    }
    if (const toml::table* pad = t["controle"].as_table()) {
        for (int a = 0; a < kPadButtons; a++)
            if (auto name = (*pad)[kIds[a]].value<std::string>())
                for (int b = 0; b < kPadButtons; b++)
                    if (*name == kIds[b]) g_from[a] = uint8_t(b);
        // um botão que ficou em duas ações (arquivo editado à mão): o controle volta ao padrão
        bool used[kPadButtons] = {};
        for (int a = 0; a < kPadButtons; a++) used[g_from[a]] = true;
        for (bool u : used)
            if (!u) {
                for (int a = 0; a < kPadButtons; a++) g_from[a] = uint8_t(a);
                break;
            }
    }
    g_changed = false;
}

void SaveControls(void* table) {
    auto& t = *static_cast<toml::table*>(table);
    toml::table keys, pad;
    for (int a = 0; a < kActions; a++) keys.insert(kIds[a], KeyName(g_key[a]));
    for (int a = 0; a < kPadButtons; a++) pad.insert(kIds[a], std::string(kIds[g_from[a]]));
    t.insert("teclado", std::move(keys));
    t.insert("controle", std::move(pad));
}

}  // namespace defjam
