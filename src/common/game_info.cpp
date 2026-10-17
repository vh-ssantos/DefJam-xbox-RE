#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "app/app.h"

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
    .pad = nullptr,
    .rumble = nullptr,
};
