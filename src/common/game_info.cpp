#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <string>
#include <cstdlib>
#include <vector>

#include "app/app.h"
#include "core/input.h"
#include "common/menu.h"
#include "common/controls.h"
#include "common/discord.h"
#include "common/icon.h"
#include "common/lang.h"
#include "common/widescreen.h"
#include "common/update.h"
#include "common/occlusion.h"
#include "gpu/backend.h"

namespace {

// DEFJAM_WINDOW_MONITOR=<n> (testes): abre a janela no monitor n (0 = principal) sem roubar o foco.
void PlaceWindow(void* hwnd) {
    defjam::SetWindow(hwnd);
    const char* env = std::getenv("DEFJAM_WINDOW_MONITOR");
    if (!env) return;
    const int want = std::atoi(env);
    std::vector<RECT> monitors;
    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR, HDC, LPRECT rect, LPARAM data) -> BOOL {
            reinterpret_cast<std::vector<RECT>*>(data)->push_back(*rect);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&monitors));
    if (want < 0 || want >= int(monitors.size())) return;
    RECT window{};
    GetWindowRect(static_cast<HWND>(hwnd), &window);
    const RECT& m = monitors[size_t(want)];
    const int w = window.right - window.left, h = window.bottom - window.top;
    const int x = m.left + std::max(0, int((m.right - m.left - w) / 2));
    const int y = m.top + std::max(0, int((m.bottom - m.top - h) / 2));
    SetWindowPos(static_cast<HWND>(hwnd), nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// DEFJAM_PAD_SCRIPT=<arquivo> (testes): aperta botões no controle 0 por dentro do processo, sem tocar no teclado.
// Linhas "segundos botões [duração em segundos]" (padrão 0.2), contadas da primeira leitura do controle; # comenta.
// Botões juntos por +: UP DOWN LEFT RIGHT START BACK A B X Y LT RT LS LSD LSL LSR.
struct ScriptPress {
    double at, len;
    std::string buttons;
};

std::vector<ScriptPress> LoadPadScript() {
    std::vector<ScriptPress> v;
    const char* path = std::getenv("DEFJAM_PAD_SCRIPT");
    if (FILE* f = path ? std::fopen(path, "r") : nullptr) {
        char line[160];
        while (std::fgets(line, sizeof line, f)) {
            char b[64];
            double t, len = 0.2;
            if (line[0] != '#' && std::sscanf(line, "%lf %63s %lf", &t, b, &len) >= 2) v.push_back({t, len, b});
        }
        std::fclose(f);
    }
    return v;
}

void ApplyButtons(const std::string& s, input::Pad& p) {
    size_t at = 0;
    while (at <= s.size()) {
        size_t e = s.find('+', at);
        if (e == std::string::npos) e = s.size();
        const std::string b = s.substr(at, e - at);
        at = e + 1;
        if (b == "UP") p.buttons |= 1;
        else if (b == "DOWN") p.buttons |= 2;
        else if (b == "LEFT") p.buttons |= 4;
        else if (b == "RIGHT") p.buttons |= 8;
        else if (b == "START") p.buttons |= 16;
        else if (b == "BACK") p.buttons |= 32;
        else if (b == "A") p.analog[0] = 255;
        else if (b == "B") p.analog[1] = 255;
        else if (b == "X") p.analog[2] = 255;
        else if (b == "Y") p.analog[3] = 255;
        else if (b == "LT") p.analog[6] = 255;
        else if (b == "RT") p.analog[7] = 255;
        else if (b == "LS") p.ly = 32767;
        else if (b == "LSD") p.ly = -32767;
        else if (b == "LSL") p.lx = -32767;
        else if (b == "LSR") p.lx = 32767;
        else if (b.rfind("KEY_", 0) == 0) defjam::PressTestKey(b.substr(4));  // tecla simulada no processo
    }
}

// DEFJAM_CAPTURE_AT=<s>[,<s>...] (testes): captura de quadro do runtime (a do F3, em XBE_CAPTURE_DIR) nesses segundos.
std::vector<double> LoadCaptureTimes() {
    std::vector<double> v;
    if (const char* e = std::getenv("DEFJAM_CAPTURE_AT"))
        for (const char* p = e; *p;) {
            char* end = nullptr;
            const double s = std::strtod(p, &end);
            if (end == p) break;
            v.push_back(s);
            p = *end == ',' ? end + 1 : end;
        }
    return v;
}

// DEFJAM_PAD_CMD=<arquivo> (testes ao vivo): o arquivo tem linhas "botões [duração]" (ou "CAPTURE") que são
// executadas em sequência, uma depois da outra, a partir de quando aparecem; o arquivo é apagado ao ser lido.
struct LivePress {
    std::string buttons;
    double len;
};
std::vector<LivePress> ReadLiveCommands(const char* path) {
    std::vector<LivePress> v;
    if (FILE* f = std::fopen(path, "r")) {
        char line[160];
        while (std::fgets(line, sizeof line, f)) {
            char b[64];
            double len = 0.4;  // toques de 0.2 s às vezes se perdem (telas com vídeo, diálogos)
            if (line[0] != '#' && std::sscanf(line, "%63s %lf", b, &len) >= 1) v.push_back({b, len});
        }
        std::fclose(f);
        std::remove(path);
    }
    return v;
}

// o comando ao vivo em curso; "P2_A", "P2_RIGHT"... são do controle 2 (que aparece conectado no primeiro P2_)
std::mutex g_live_lock;
std::string g_live_holding;  // com g_live_lock
std::atomic<bool> g_live_p2{false};

// controle 2 dos testes: só os botões P2_ do comando em curso, sem o prefixo
void PadScriptP2(input::Pad& pad) {
    if (!g_live_p2) return;
    pad.connected = true;
    std::string p2;
    size_t at = 0;
    std::string holding;
    {
        std::lock_guard lock(g_live_lock);
        holding = g_live_holding;
    }
    while (at <= holding.size()) {
        size_t e = holding.find('+', at);
        if (e == std::string::npos) e = holding.size();
        if (holding.compare(at, 3, "P2_") == 0) p2 += (p2.empty() ? "" : "+") + holding.substr(at + 3, e - at - 3);
        at = e + 1;
    }
    if (!p2.empty()) ApplyButtons(p2, pad);
}

void PadScript(int port, input::Pad& pad) {
    static const std::vector<ScriptPress> script = LoadPadScript();
    static std::vector<double> captures = LoadCaptureTimes();
    static const char* live_path = std::getenv("DEFJAM_PAD_CMD");
    if (port == 1 && live_path) PadScriptP2(pad);
    if (port != 0 || (script.empty() && captures.empty() && !live_path)) return;
    static const auto start = std::chrono::steady_clock::now();
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (!captures.empty() && t >= captures.front()) {
        captures.erase(captures.begin());
        gpu::g_capture = true;
    }
    pad.connected = true;
    defjam::BeginTestKeys();
    for (const ScriptPress& s : script)
        if (t >= s.at && t <= s.at + s.len) ApplyButtons(s.buttons, pad);
    if (live_path) {
        std::lock_guard lock(g_live_lock);
        // fila: cada comando segura os botões por len segundos e solta por 0.15 s antes do próximo
        static std::vector<LivePress> queue;
        std::string& holding = g_live_holding;
        static double hold_end = 0, next_start = 0, next_read = 0;
        if (t >= next_read) {
            next_read = t + 0.1;
            for (LivePress& c : ReadLiveCommands(live_path)) queue.push_back(c);
        }
        if (!holding.empty() && t >= hold_end) holding.clear();
        if (holding.empty() && t >= next_start && !queue.empty()) {
            const LivePress c = queue.front();
            queue.erase(queue.begin());
            if (c.buttons == "CAPTURE") {
                gpu::g_capture = true;
            } else if (c.buttons == "WAIT") {
                next_start = t + c.len;
            } else {
                holding = c.buttons;
                if (holding.find("P2_") != std::string::npos) g_live_p2 = true;
                hold_end = t + c.len;
                next_start = hold_end + 0.15;
            }
        }
        if (!holding.empty()) ApplyButtons(holding, pad);
    }
}

// controle 0: script de teste, depois BACK+START abre/fecha o menu de opções; com o menu aberto o jogo não vê o controle
void PadHook(int port, input::Pad& pad) {
    PadScript(port, pad);
    defjam::RemapPad(port, pad);  // botões trocados no F10 (todos os controles)
    // teclado como jogador 2 (o jogador 1 num controle, ou o amigo pelo Parsec): soma ao controle 2
    if (port == 1 && defjam::g_keyboard_player == 2 && !defjam::MenuOpen()) defjam::ApplyKeyboard(pad);
    if (port != 0) return;
    defjam::WaitWhileUnfocused();
    defjam::ApplyPendingCheat();
    if (defjam::SkipIntroPress()) pad.buttons |= 0x10;  // START
    static bool combo = false, hold_off = false;
    const bool both = (pad.buttons & 0x30) == 0x30;  // START e BACK
    if (both && !combo) defjam::ToggleMenu();
    combo = both;
    defjam::FeedPad(pad);
    // o teclado do PC soma ao jogador 1 (com o menu aberto as teclas são do menu); entra no "tudo solto" abaixo
    if (!defjam::MenuOpen() && defjam::g_keyboard_player == 1) defjam::ApplyKeyboard(pad);
    const bool any = pad.buttons || pad.analog[0] || pad.analog[1] || pad.analog[2] || pad.analog[3] ||
                     pad.analog[6] || pad.analog[7];
    // menu aberto: o jogo não vê o controle; ao fechar, só volta a ver depois de tudo solto (o B que fechou não vira B
    // no jogo)
    if (defjam::MenuOpen() || combo) hold_off = true;
    else if (!any) hold_off = false;
    if (hold_off) {
        const bool connected = pad.connected;
        pad = input::Pad{};
        pad.connected = connected;
    }
}

}  // namespace

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Def Jam: Fight for NY",
    .short_title = L"Def Jam FFNY",
    .name = L"DefJamFFNY",
    .log = "defjam",
    // os testes automáticos (DEFJAM_WINDOW_MONITOR) usam um arquivo próprio, em janela, sem mexer no do jogador
    .settings = std::getenv("DEFJAM_WINDOW_MONITOR") ? "settings-teste.toml" : "settings.toml",
    .splash = L"splash",
    .missing = L"Coloque o jogo na pasta assets: a imagem do disco (.iso) ou o default.xbe com as pastas ao lado.\n\n"
               L"Put the game in the assets folder: the disc image (.iso), or default.xbe with its folders.",
    .images = {{"default.xbe", "DefJamFFNY"}},
    .options = {{"ambient_occlusion", nullptr, &defjam::g_ao_mode},
                {"language", nullptr, &defjam::g_language},
                {"aspect_4x3", nullptr, &app::g_force_4x3},
                {"skip_videos", nullptr, &defjam::g_skip_videos},
                {"hud_widescreen", nullptr, &defjam::g_hud_wide},
                {"hd_textures", nullptr, &app::g_hd_textures},
                {"pause_on_focus_loss", &defjam::g_pause_unfocused, nullptr},
                {"discord", &defjam::g_discord, nullptr},
                {"check_updates", &defjam::g_check_updates, nullptr},
                {"keyboard_player", nullptr, &defjam::g_keyboard_player}},
    .load = defjam::LoadControls,
    .save = defjam::SaveControls,
    .init = [] {
        defjam::InstallOcclusion();
        defjam::StartDiscord();
        defjam::StartUpdateCheck();
    },
    .loaded = [] { defjam::ApplyGameIcon(app::Window()); },
    .window = PlaceWindow,
    .message = defjam::Message,
    .ui = defjam::DrawUi,
    .pad = PadHook,
    .rumble = nullptr,
};
