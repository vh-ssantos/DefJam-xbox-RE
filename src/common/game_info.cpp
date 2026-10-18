#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <cstdlib>
#include <vector>

#include "app/app.h"
#include "core/input.h"
#include "gpu/backend.h"

namespace {

// DEFJAM_WINDOW_MONITOR=<n> (testes): abre a janela no monitor n (0 = principal) sem roubar o foco.
void PlaceWindow(void* hwnd) {
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

void PadScript(int port, input::Pad& pad) {
    static const std::vector<ScriptPress> script = LoadPadScript();
    static std::vector<double> captures = LoadCaptureTimes();
    if (port != 0 || (script.empty() && captures.empty())) return;
    static const auto start = std::chrono::steady_clock::now();
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (!captures.empty() && t >= captures.front()) {
        captures.erase(captures.begin());
        gpu::g_capture = true;
    }
    pad.connected = true;
    for (const ScriptPress& s : script)
        if (t >= s.at && t <= s.at + s.len) ApplyButtons(s.buttons, pad);
}

}  // namespace

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Def Jam: Fight for NY",
    .short_title = L"Def Jam FFNY",
    .name = L"DefJamFFNY",
    .log = "defjam",
    .settings = "settings.toml",
    .splash = L"splash",
    .missing = L"Coloque o jogo na pasta assets: a imagem do disco (.iso) ou o default.xbe com as pastas ao lado.",
    .images = {{"default.xbe", "DefJamFFNY"}},
    .options = {},
    .load = nullptr,
    .save = nullptr,
    .init = nullptr,
    .loaded = nullptr,
    .window = PlaceWindow,
    .message = nullptr,
    .ui = nullptr,
    .pad = PadScript,
    .rumble = nullptr,
};
