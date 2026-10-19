// Atualização automática pelo GitHub Releases: ao abrir, consulta a versão mais nova do repositório; se for maior que
// a deste exe, o menu F10 oferece atualizar. Atualizar baixa o .zip da versão (só código, nada do jogo), põe o código
// novo em source e roda o Instalar.cmd -Update, que recompila no PC do jogador. Saves, settings e assets ficam.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include "app/app.h"
#include "common/lang.h"
#include "common/update.h"
#include "core/log.h"

#ifndef DEFJAM_VERSION
#define DEFJAM_VERSION "0.0.0-dev"
#endif

namespace defjam {

bool g_check_updates = true;  // settings.toml: check_updates

namespace {

constexpr const wchar_t* kHost = L"api.github.com";
constexpr const wchar_t* kLatest = L"/repos/vh-ssantos/DefJam-xbox-RE/releases/latest";

std::mutex g_lock;
std::string g_latest, g_zip_url, g_status;
std::atomic<bool> g_busy{false};

// GET por HTTPS (segue redirecionamentos, como os do download de assets do GitHub)
bool HttpGet(const std::wstring& host, const std::wstring& path, std::string& body) {
    HINTERNET s = WinHttpOpen(L"DefJamFFNY-PC", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!s) return false;
    bool ok = false;
    if (HINTERNET c = WinHttpConnect(s, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0)) {
        if (HINTERNET r = WinHttpOpenRequest(c, L"GET", path.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE)) {
            const wchar_t* headers = L"Accept: application/vnd.github+json, application/octet-stream\r\n";
            if (WinHttpSendRequest(r, headers, DWORD(-1), nullptr, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
                DWORD code = 0, len = sizeof(code);
                WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &len, nullptr);
                for (DWORD avail = 0; WinHttpQueryDataAvailable(r, &avail) && avail;) {
                    std::string chunk(avail, '\0');
                    DWORD got = 0;
                    if (!WinHttpReadData(r, chunk.data(), avail, &got)) break;
                    body.append(chunk.data(), got);
                }
                ok = code == 200;
            }
            WinHttpCloseHandle(r);
        }
        WinHttpCloseHandle(c);
    }
    WinHttpCloseHandle(s);
    return ok;
}

// o primeiro valor "chave": "texto" depois de from
std::string JsonString(const std::string& j, const std::string& key, size_t from = 0, size_t* at = nullptr) {
    const size_t k = j.find("\"" + key + "\"", from);
    if (k == std::string::npos) return {};
    const size_t q1 = j.find('"', j.find(':', k) + 1);
    const size_t q2 = j.find('"', q1 + 1);
    if (q1 == std::string::npos || q2 == std::string::npos) return {};
    if (at) *at = q2;
    return j.substr(q1 + 1, q2 - q1 - 1);
}

// "v1.2.3" > "1.2.0"?
bool Newer(std::string a, std::string b) {
    auto parts = [](std::string v, int out[3]) {
        if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(0, 1);
        out[0] = out[1] = out[2] = 0;
        std::sscanf(v.c_str(), "%d.%d.%d", &out[0], &out[1], &out[2]);
    };
    int x[3], y[3];
    parts(a, x);
    parts(b, y);
    for (int i = 0; i < 3; i++)
        if (x[i] != y[i]) return x[i] > y[i];
    return false;
}

void Check() {
    std::string body;
    // DEFJAM_UPDATE_REPO=dono/repo (teste): consulta outro repositório
    std::wstring latest = kLatest;
    if (const char* r = std::getenv("DEFJAM_UPDATE_REPO")) latest = L"/repos/" + std::wstring(r, r + std::strlen(r)) + L"/releases/latest";
    if (!HttpGet(kHost, latest, body)) return;  // sem internet ou sem versões publicadas: silêncio
    const std::string tag = JsonString(body, "tag_name");
    std::string zip;
    for (size_t at = 0;;) {
        const std::string url = JsonString(body, "browser_download_url", at, &at);
        if (url.empty()) break;
        if (url.size() > 4 && url.compare(url.size() - 4, 4, ".zip") == 0) {
            zip = url;
            break;
        }
    }
    if (tag.empty() || zip.empty() || !Newer(tag, DEFJAM_VERSION)) return;
    std::lock_guard<std::mutex> lock(g_lock);
    g_latest = tag;
    g_zip_url = zip;
    LOGI("update: versao %s disponivel (esta e %s)", tag.c_str(), DEFJAM_VERSION);
}

void SetStatus(const std::string& s) {
    std::lock_guard<std::mutex> lock(g_lock);
    g_status = s;
}

void Install(std::string url) {
    namespace fs = std::filesystem;
    SetStatus(T("Baixando...", "Downloading..."));
    // https://github.com/... -> host e caminho
    const size_t p = url.find("://");
    const size_t slash = url.find('/', p + 3);
    const std::wstring host(url.begin() + long(p + 3), url.begin() + long(slash));
    const std::wstring path(url.begin() + long(slash), url.end());
    std::string zip;
    // o GitHub redireciona para objects.githubusercontent.com; WinHTTP segue sozinho
    if (!HttpGet(host, path, zip) || zip.size() < 1024) {
        SetStatus(T("Falhou o download. Tente de novo mais tarde.", "Download failed. Try again later."));
        g_busy = false;
        return;
    }
    // o zip traz o código novo (source); o Instalar.cmd -Update recompila só o que mudou e troca o exe. O jogo fecha
    // para isso (o exe em uso não pode ser trocado), e o instalador mostra o progresso
    const fs::path dir = app::ExeDir();
    const fs::path tmp = dir / L"update.zip", unpacked = dir / L"update-tmp";
    std::error_code ec;
    fs::remove_all(unpacked, ec);
    fs::create_directories(unpacked, ec);
    std::ofstream(tmp, std::ios::binary).write(zip.data(), std::streamsize(zip.size()));
    auto run = [](std::wstring cmd, DWORD& code) {
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
        WaitForSingleObject(pi.hProcess, 600000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    };
    DWORD code = 1;
    bool ok = run(L"tar.exe -xf \"" + tmp.wstring() + L"\" -C \"" + unpacked.wstring() + L"\"", code) && code == 0 &&
              fs::exists(unpacked / L"source" / L"setup" / L"instalar.ps1", ec) && fs::exists(dir / L"source", ec);
    // robocopy: 0 a 7 é sucesso; o que já existe e não veio no zip (assetsdefault.xbe, out) fica
    if (ok) ok = run(L"robocopy \"" + (unpacked / L"source").wstring() + L"\" \"" + (dir / L"source").wstring() + L"\" /E /NFL /NDL /NJH /NJS /NP", code) && code < 8;
    for (const wchar_t* top : {L"Instalar.cmd", L"README.md", L"LICENSE.txt", L"THIRD_PARTY.txt"})
        if (ok) fs::copy_file(unpacked / top, dir / top, fs::copy_options::overwrite_existing, ec);
    fs::remove(tmp, ec);
    fs::remove_all(unpacked, ec);
    if (!ok) {
        SetStatus(T("Falhou ao instalar. O jogo continua na versao atual.", "Install failed. The game stays on this version."));
        g_busy = false;
        return;
    }
    SetStatus(T("Fechando o jogo para recompilar...", "Closing the game to rebuild..."));
    std::wstring setup = L"cmd.exe /c \"\"" + (dir / L"Instalar.cmd").wstring() + L"\" -Update\"";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, setup.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Sleep(1500);
        ExitProcess(0);
    }
    SetStatus(T("Codigo novo baixado: rode o Instalar.cmd para terminar.", "New code downloaded: run Instalar.cmd to finish."));
    g_busy = false;
}

}  // namespace

void StartUpdateCheck() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::remove(app::ExeDir() / L"DefJamFFNY.old.exe", ec);  // sobra da última atualização
    if (g_check_updates) std::thread(Check).detach();
}

const char* Version() { return DEFJAM_VERSION; }

std::string UpdateAvailable() {
    std::lock_guard<std::mutex> lock(g_lock);
    return g_latest;
}

std::string UpdateStatus() {
    std::lock_guard<std::mutex> lock(g_lock);
    return g_status;
}

void InstallUpdate() {
    std::string url;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        url = g_zip_url;
    }
    if (url.empty() || g_busy.exchange(true)) return;
    std::thread(Install, url).detach();
}

}  // namespace defjam
