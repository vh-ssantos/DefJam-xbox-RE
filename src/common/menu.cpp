// Menu de opções do port: modo de tela, resolução, escala de renderização, filtro anisotrópico, V-Sync e contador de
// FPS. Tudo vale na hora (o renderer relê as opções a cada quadro, a janela pelo ApplyDisplay) e fica salvo no
// settings.toml. Abre com F10 ou BACK+START; enquanto aberto, o jogo não recebe o controle.
// A navegação é própria (linha selecionada), alimentada pelo mesmo controle que o jogo lê e pelo teclado: funciona com
// qualquer controle que o runtime reconheça, não só XInput.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "app/app.h"
#include "common/menu.h"
#include "common/occlusion.h"
#include "common/update.h"
#include "audio/audio.h"
#include "core/input.h"

namespace defjam {

bool g_skip_intros = true;
bool g_pause_unfocused = true;

namespace {

std::atomic<bool> g_open{false};
HWND g_hwnd = nullptr;
bool g_show_fps = false;
int g_row = 0;

// comandos de navegação vindos do controle (thread do jogo) e do teclado (thread da janela)
enum Nav { kUp, kDown, kLeft, kRight, kAccept, kBack, kNavCount };
std::mutex g_nav_lock;
int g_nav[kNavCount] = {};

void Push(Nav n) {
    std::lock_guard<std::mutex> lock(g_nav_lock);
    g_nav[n]++;
}

struct Res {
    uint32_t w, h;
};

// as resoluções que o monitor aceita, da menor para a maior (direita aumenta); 0x0 = a do desktop, no começo
const std::vector<Res>& Resolutions() {
    static std::vector<Res> list = [] {
        std::vector<Res> v{{0, 0}};
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++) {
            if (dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480) continue;
            bool seen = false;
            for (const Res& r : v) seen |= r.w == dm.dmPelsWidth && r.h == dm.dmPelsHeight;
            if (!seen) v.push_back({dm.dmPelsWidth, dm.dmPelsHeight});
        }
        std::sort(v.begin() + 1, v.end(), [](const Res& a, const Res& b) { return a.w * a.h < b.w * b.h; });
        return v;
    }();
    return list;
}

std::string ResText(const Res& r) {
    if (!r.w) {
        app::Mode d = app::Desktop();
        return "Do monitor (" + std::to_string(d.width) + "x" + std::to_string(d.height) + ")";
    }
    const char* aspect = r.w * 9 == r.h * 16 ? "  16:9" : r.w * 3 == r.h * 4 ? "  4:3" : r.w * 10 == r.h * 16 ? "  16:10" : "";
    return std::to_string(r.w) + "x" + std::to_string(r.h) + aspect;
}

template <class T>
int IndexOf(const std::vector<T>& v, const T& x) {
    for (size_t i = 0; i < v.size(); i++)
        if (v[i] == x) return int(i);
    return 0;
}
int Wrap(int i, int n) { return (i % n + n) % n; }

constexpr int kRows = 14;  // 13 opções + Fechar
char g_name[16] = {};  // nome do próximo User ID, digitado no teclado do PC
bool g_name_edit = false;

// os códigos da tela Extras -> Cheats (lista pública do jogo)
struct Cheat {
    const char* code;
    const char* what;
};
const Cheat kCheats[] = {
    {"GETSTUFF", "100 pontos de recompensa"},   {"DUCKETS", "100 pontos de recompensa"},
    {"CROOKLYN", "100 pontos de recompensa"},   {"THESOURCE", "100 pontos de recompensa"},
    {"NEWJACK", "100 pontos de recompensa"},    {"LOYALTY", "musica: Nene - Afterhours"},
    {"MILITAIN", "musica: CNN - Anything Goes"}, {"BIGBOI", "musica: Outkast - Bust"},
    {"CHOPPER", "musica: Baxter - Chopshop"},    {"CHOCOCITY", "musica: Comp - Comp"},
    {"AKIRA", "musica: Chiang - Dragon House"},  {"PLATINUMB", "musica: Bless - Get it Now"},
    {"GHOSTSHELL", "musica: Chiang - Koto"},     {"GONBETRUBL", "musica: Ricbyobiche - Lil Bro"},
    {"KIRKJONES", "musica: Sticky Fingaz - Man Up"}, {"RESPECT", "musica: Public Enemy - Move"},
    {"POWER", "musica: Ice T - Original Gangster"}, {"ULTRAMAG", "musica: Ultramagnetic MCs - Poppa Large"},
    {"SIEZE", "musica: Bless - Sieze the Day"},  {"CARTAGENA", "musica: Fat Joe - Take a Look at my Life"},
    {"PUMP", "musica: Joe Budden - Walk with Me"},
};
constexpr int kCheatCount = int(sizeof(kCheats) / sizeof(kCheats[0]));
int g_cheat = 0;
std::mutex g_cheat_lock;
std::string g_cheat_request, g_cheat_result;

// desenha uma linha; selecionada se index == g_row. Clique nas setas também muda. Devolve -1, 0 ou +1.
int Row(int index, const char* label, const std::string& value, int nav_step) {
    ImGui::PushID(index);
    const bool sel = index == g_row;
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    if (sel) {
        ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + w, start.y + ImGui::GetFrameHeight()),
                                                  IM_COL32(170, 40, 30, 200));
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(w * 0.45f);
    int step = sel ? nav_step : 0;
    if (ImGui::ArrowButton("##menos", ImGuiDir_Left)) step = -1, g_row = index;
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    const float vw = w * 0.45f, tw = ImGui::CalcTextSize(value.c_str()).x;
    const ImVec2 at = ImGui::GetCursorPos();
    ImGui::SetCursorPosX(at.x + std::max(0.0f, (vw - tw) * 0.5f));
    ImGui::TextUnformatted(value.c_str());
    ImGui::SameLine(at.x + vw);
    if (ImGui::ArrowButton("##mais", ImGuiDir_Right)) step = 1, g_row = index;
    ImGui::PopID();
    return step;
}

void DrawPanel() {
    int nav[kNavCount];
    {
        std::lock_guard<std::mutex> lock(g_nav_lock);
        std::copy(std::begin(g_nav), std::end(g_nav), nav);
        std::fill(std::begin(g_nav), std::end(g_nav), 0);
    }
    if (g_name_edit) {  // o controle não navega enquanto o teclado do PC digita o nome
        nav[kUp] = nav[kDown] = nav[kLeft] = nav[kRight] = nav[kBack] = nav[kAccept] = 0;
    }
    g_row = Wrap(g_row + nav[kDown] - nav[kUp], kRows);
    const int step = nav[kRight] - nav[kLeft];
    if (nav[kBack]) {
        g_open = false;
        return;
    }

    app::Settings& s = app::Get();
    bool display = false, save = false;

    ImGuiIO& io = ImGui::GetIO();
    const float em = ImGui::GetFontSize();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(em * 36, 0), ImGuiCond_Always);
    ImGui::Begin("Def Jam: Fight for NY - Opcoes do PC", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);

    // modo de tela
    static const char* kModes[] = {"Tela cheia", "Tela cheia sem bordas", "Janela"};
    static const app::DisplayMode kModeValues[] = {app::DisplayMode::Fullscreen, app::DisplayMode::Borderless,
                                                   app::DisplayMode::Windowed};
    int mode = 0;
    for (int i = 0; i < 3; i++)
        if (kModeValues[i] == s.display_mode) mode = i;
    if (int st = Row(0, "Modo de tela", kModes[mode], step)) {
        s.display_mode = kModeValues[Wrap(mode + st, 3)];
        display = save = true;
    }

    // resolução
    const auto& res = Resolutions();
    int ri = 0;
    for (size_t i = 0; i < res.size(); i++)
        if (res[i].w == s.width && res[i].h == s.height) ri = int(i);
    if (int st = Row(1, "Resolucao", ResText(res[size_t(ri)]), step)) {
        const Res& r = res[size_t(Wrap(ri + st, int(res.size())))];
        s.width = r.w;
        s.height = r.h;
        display = save = true;
    }

    // escala de renderização: % da altura da saída
    static const std::vector<uint32_t> kScales = {50, 67, 75, 100, 125, 150, 175, 200, 250, 300};
    char scale[64];
    std::snprintf(scale, sizeof scale, "%u%%%s", s.render_percent,
                  s.render_percent > 100 ? " (supersampling)" : s.render_percent < 100 ? " (mais leve)" : "");
    if (int st = Row(2, "Escala de renderizacao", scale, step)) {
        int i = IndexOf(kScales, s.render_percent);
        s.render_percent = kScales[size_t(std::clamp(i + st, 0, int(kScales.size()) - 1))];
        save = true;
    }

    // filtro anisotrópico
    static const std::vector<uint32_t> kAniso = {0, 2, 4, 8, 16};
    std::string aniso = s.aniso ? std::to_string(s.aniso) + "x" : std::string("Como o jogo pede");
    if (int st = Row(3, "Filtro anisotropico", aniso, step)) {
        s.aniso = kAniso[size_t(Wrap(IndexOf(kAniso, s.aniso) + st, int(kAniso.size())))];
        save = true;
    }

    if (Row(4, "V-Sync", s.vsync ? "Ligado" : "Desligado", step)) {
        s.vsync = !s.vsync;
        save = true;
    }
    static const char* kAo[] = {"Desligada", "SSAO", "GTAO (melhor)"};
    if (int st = Row(5, "Oclusao de ambiente", kAo[g_ao_mode % 3], step)) {
        g_ao_mode = uint32_t(Wrap(int(g_ao_mode) + st, 3));
        save = true;
    }
    if (Row(6, "Mostrar FPS", g_show_fps ? "Sim" : "Nao", step)) g_show_fps = !g_show_fps;
    if (Row(7, "Pular videos de abertura", g_skip_intros ? "Sim" : "Nao", step)) {
        g_skip_intros = !g_skip_intros;
        save = true;
    }
    if (Row(8, "Pausar sem foco", g_pause_unfocused ? "Sim" : "Nao", step)) {
        g_pause_unfocused = !g_pause_unfocused;
        save = true;
    }

    // cheat: esquerda/direita escolhem, A aplica (na thread do jogo)
    {
        std::string label = std::string(kCheats[g_cheat].code) + "  (" + kCheats[g_cheat].what + ")";
        if (int st = Row(9, "Cheat (A aplica)", label, step)) g_cheat = Wrap(g_cheat + st, kCheatCount);
        if (g_row == 9 && nav[kAccept]) {
            std::lock_guard<std::mutex> lock(g_cheat_lock);
            g_cheat_request = kCheats[g_cheat].code;
            g_cheat_result = "enviando...";
        }
        std::lock_guard<std::mutex> lock(g_cheat_lock);
        if (!g_cheat_result.empty()) ImGui::TextWrapped("Resposta do jogo: %s", g_cheat_result.c_str());
    }
    // nome do próximo User ID: A (ou Enter) neste item abre o campo; Enter fecha
    {
        ImGui::PushID(10);
        const bool sel = g_row == 10;
        const float w = ImGui::GetContentRegionAvail().x;
        if (sel) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w, p.y + ImGui::GetFrameHeight()), IM_COL32(170, 40, 30, 200));
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Nome do proximo User ID");
        ImGui::SameLine(w * 0.45f);
        ImGui::SetNextItemWidth(w * 0.55f);
        if (sel && nav[kAccept] && !g_name_edit) {
            g_name_edit = true;
            ImGui::SetKeyboardFocusHere();
        }
        auto filter = [](ImGuiInputTextCallbackData* d) -> int {
            if (d->EventChar >= 'a' && d->EventChar <= 'z') d->EventChar -= 32;
            return (d->EventChar >= 'A' && d->EventChar <= 'Z') || (d->EventChar >= '0' && d->EventChar <= '9') ? 0 : 1;
        };
        if (ImGui::InputText("##nome", g_name, 9, ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue, filter))
            g_name_edit = false;
        if (ImGui::IsItemActivated()) g_name_edit = true;
        if (ImGui::IsItemDeactivated()) g_name_edit = false;
        ImGui::PopID();
        if (g_name[0]) ImGui::TextDisabled("Na tela do jogo, digite qualquer letra e DONE: o nome vira %s.", g_name);
    }
    // abre a pasta dos saves (o HD do port; DEFJAM_HDD nos testes) no Explorer
    if (Row(11, "Pasta de saves", "A abre no Explorer", 0) || (g_row == 11 && nav[kAccept])) {
        const wchar_t* hdd = _wgetenv(L"DEFJAM_HDD");
        std::filesystem::path dir = app::ExeDir() / (hdd ? hdd : L"hdd") / L"E" / L"UDATA";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    // versão e atualização (GitHub Releases)
    {
        const std::string latest = UpdateAvailable(), status = UpdateStatus();
        const std::string value = !status.empty() ? status
                                  : latest.empty() ? std::string("v") + Version() + " (em dia)"
                                                   : "Nova: " + latest + " (A atualiza)";
        Row(12, "Versao", value, 0);
        if (g_row == 12 && nav[kAccept] && !latest.empty()) InstallUpdate();
    }
    ImGui::Separator();
    // Fechar: selecionado vira vermelho; A/Enter ou clique fecha
    const bool sel = g_row == kRows - 1;
    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(170, 40, 30, 230));
    if (ImGui::Button("Fechar", ImVec2(-1, 0)) || (sel && nav[kAccept])) g_open = false;
    if (sel) ImGui::PopStyleColor();
    ImGui::TextDisabled("Esquerda/direita mudam o valor. B, Esc ou F10 fecham.");
    ImGui::End();

    if (save) app::Save();
    if (display) app::ApplyDisplay();
}

}  // namespace

bool MenuOpen() { return g_open.load(std::memory_order_relaxed); }

bool TakeCheatRequest(std::string& code) {
    std::lock_guard<std::mutex> lock(g_cheat_lock);
    if (g_cheat_request.empty()) return false;
    code.swap(g_cheat_request);
    g_cheat_request.clear();
    return true;
}

bool TakeTypedName(std::string& name) {
    std::lock_guard<std::mutex> lock(g_cheat_lock);
    // teste automático: DEFJAM_TEST_NAME faz o papel do que seria digitado (uma vez)
    static bool test_used = false;
    if (const char* t = std::getenv("DEFJAM_TEST_NAME"); t && !test_used && !g_name[0]) {
        test_used = true;
        std::snprintf(g_name, sizeof g_name, "%.8s", t);
    }
    if (!g_name[0]) return false;
    name = g_name;
    g_name[0] = 0;
    return true;
}

void CheatResult(const std::string& text) {
    std::lock_guard<std::mutex> lock(g_cheat_lock);
    g_cheat_result = text;
}

namespace {
std::atomic<int64_t> g_skip_at{0};  // ms do relógio estável: quando apertar o START automático (0 = nenhum)
int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
}  // namespace

void IntroMovieStarted() {
    if (g_skip_intros) g_skip_at = NowMs() + 500;
}

bool SkipIntroPress() {
    const int64_t at = g_skip_at;
    if (!at) return false;
    const int64_t now = NowMs();
    if (now < at) return false;
    if (now > at + 150) {
        g_skip_at = 0;
        return false;
    }
    return true;
}

void WaitWhileUnfocused() {
    // os testes automáticos rodam em segundo plano de propósito
    static const bool tests = std::getenv("DEFJAM_WINDOW_MONITOR") != nullptr;
    auto unfocused = [] {
        return g_hwnd && IsWindowVisible(g_hwnd) && (GetForegroundWindow() != g_hwnd || IsIconic(g_hwnd));
    };
    if (tests || !g_pause_unfocused || !unfocused()) return;
    audio::g_paused = true;
    while (g_pause_unfocused && unfocused()) Sleep(20);
    audio::g_paused = false;
}

void ToggleMenu() {
    const bool open = !MenuOpen();
    if (open) {
        std::lock_guard<std::mutex> lock(g_nav_lock);
        std::fill(std::begin(g_nav), std::end(g_nav), 0);
    }
    g_open.store(open, std::memory_order_relaxed);
}

void FeedPad(const input::Pad& pad) {
    // bordas com repetição para direções seguradas (0.4 s, depois a cada 0.12 s)
    using clock = std::chrono::steady_clock;
    static bool held[kNavCount] = {};
    static clock::time_point next[kNavCount];
    const bool down[kNavCount] = {
        (pad.buttons & 1) || pad.ly > 16000,  (pad.buttons & 2) || pad.ly < -16000,
        (pad.buttons & 4) || pad.lx < -16000, (pad.buttons & 8) || pad.lx > 16000,
        pad.analog[0] > 30,                   pad.analog[1] > 30,
    };
    const auto now = clock::now();
    for (int n = 0; n < kNavCount; n++) {
        if (down[n] && !held[n]) {
            if (MenuOpen()) Push(Nav(n));
            next[n] = now + std::chrono::milliseconds(400);
        } else if (down[n] && n < kAccept && now >= next[n]) {
            if (MenuOpen()) Push(Nav(n));
            next[n] = now + std::chrono::milliseconds(120);
        }
        held[n] = down[n];
    }
}

void SetWindow(void* hwnd) { g_hwnd = static_cast<HWND>(hwnd); }

void DrawUi(double fps) {
    ImGuiIO& io = ImGui::GetIO();
    // o cursor do mouse some depois de 2 s parado em cima do jogo (volta ao mexer e fica sempre com o menu aberto)
    {
        using clock = std::chrono::steady_clock;
        static ImVec2 last{-1, -1};
        static clock::time_point moved = clock::now();
        static bool hidden = false;
        if (io.MousePos.x != last.x || io.MousePos.y != last.y) {
            last = io.MousePos;
            moved = clock::now();
        }
        const bool hide = !MenuOpen() && clock::now() - moved > std::chrono::seconds(2);
        if (hide) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        // o backend do ImGui só aplica o cursor no WM_SETCURSOR da janela: pede um quando o estado muda e o mouse
        // está sobre a área do jogo
        if (hide != hidden && g_hwnd) {
            POINT p;
            RECT r;
            if (GetCursorPos(&p) && ScreenToClient(g_hwnd, &p) && GetClientRect(g_hwnd, &r) && PtInRect(&r, p))
                PostMessageW(g_hwnd, WM_SETCURSOR, WPARAM(g_hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        }
        hidden = hide;
    }
    io.FontGlobalScale = std::max(1.0f, io.DisplaySize.y / 540.0f);
    if (g_show_fps) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        char text[32];
        std::snprintf(text, sizeof text, "%.0f FPS", fps);
        const ImVec2 at(ImGui::GetFontSize() * 0.6f, ImGui::GetFontSize() * 0.4f);
        dl->AddText(ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 220), text);
        dl->AddText(at, IM_COL32(255, 230, 80, 255), text);
    }
    if (MenuOpen()) DrawPanel();
}

bool Message(void*, uint32_t msg, uintptr_t wp, intptr_t, intptr_t* result) {
    // F10 é a tecla de menu do Windows (chega como WM_SYSKEYDOWN); sem tratar, a janela entra no modo de menu
    if ((msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_KEYDOWN) && wp == VK_F10) {
        if (msg != WM_SYSKEYUP) ToggleMenu();
        *result = 0;
        return true;
    }
    // digitando o nome: as teclas são do campo de texto
    if (msg == WM_KEYDOWN && MenuOpen() && g_name_edit) return false;
    if (msg == WM_KEYDOWN && MenuOpen()) {
        switch (wp) {
            case VK_UP: Push(kUp); break;
            case VK_DOWN: Push(kDown); break;
            case VK_LEFT: Push(kLeft); break;
            case VK_RIGHT: Push(kRight); break;
            case VK_RETURN: case VK_SPACE: Push(kAccept); break;
            case VK_ESCAPE: case VK_BACK: Push(kBack); break;
            default: return false;
        }
        *result = 0;
        return true;
    }
    return false;
}

}  // namespace defjam
