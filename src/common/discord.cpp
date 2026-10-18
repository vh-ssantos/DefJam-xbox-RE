// Discord Rich Presence pelo IPC local do app do Discord (\\.\pipe\discord-ipc-N), sem SDK: handshake e SET_ACTIVITY em
// quadros {op int32, tamanho int32, JSON}. Precisa do Application ID de um app criado no portal de desenvolvedor do
// Discord (kAppId, ou discord_app_id no settings.toml). Sem Discord aberto, tenta de novo a cada 15 s, em silêncio.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>

#include "common/discord.h"
#include "core/log.h"

namespace defjam {

bool g_discord = true;  // settings.toml: discord

namespace {

// Application ID do app "Def Jam: Fight for NY" no portal do Discord (0 = ainda não criado; desliga a presença)
constexpr uint64_t kAppId = 0;

std::mutex g_lock;
std::string g_details = "Jogando";
std::atomic<bool> g_dirty{true};

bool Send(HANDLE pipe, uint32_t op, const std::string& json) {
    std::string frame(8 + json.size(), '\0');
    const uint32_t len = uint32_t(json.size());
    std::memcpy(frame.data(), &op, 4);
    std::memcpy(frame.data() + 4, &len, 4);
    std::memcpy(frame.data() + 8, json.data(), json.size());
    DWORD done = 0;
    return WriteFile(pipe, frame.data(), DWORD(frame.size()), &done, nullptr) && done == frame.size();
}

// lê e descarta uma resposta (o Discord responde a cada quadro)
bool Drain(HANDLE pipe) {
    uint32_t head[2];
    DWORD got = 0;
    if (!ReadFile(pipe, head, 8, &got, nullptr) || got != 8) return false;
    std::string body(head[1], '\0');
    return head[1] == 0 || (ReadFile(pipe, body.data(), head[1], &got, nullptr) && got == head[1]);
}

std::string Escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

void Run(uint64_t app_id) {
    const long long start = static_cast<long long>(std::time(nullptr));
    for (;;) {
        HANDLE pipe = INVALID_HANDLE_VALUE;
        for (int i = 0; i < 10 && pipe == INVALID_HANDLE_VALUE; i++) {
            const std::wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(i);
            pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        }
        if (pipe == INVALID_HANDLE_VALUE) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            continue;
        }
        bool ok = Send(pipe, 0, "{\"v\":1,\"client_id\":\"" + std::to_string(app_id) + "\"}") && Drain(pipe);
        if (ok) LOGI("discord: conectado");
        g_dirty = true;
        int nonce = 0;
        while (ok) {
            if (g_dirty.exchange(false)) {
                std::string details;
                {
                    std::lock_guard<std::mutex> lock(g_lock);
                    details = g_details;
                }
                const std::string json = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(GetCurrentProcessId()) +
                                         ",\"activity\":{\"details\":\"" + Escape(details) +
                                         "\",\"state\":\"Port nativo para PC\",\"timestamps\":{\"start\":" +
                                         std::to_string(start) +
                                         "},\"assets\":{\"large_image\":\"logo\",\"large_text\":\"Def Jam: Fight for NY\"}}},"
                                         "\"nonce\":\"" + std::to_string(++nonce) + "\"}";
                ok = Send(pipe, 1, json) && Drain(pipe);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        CloseHandle(pipe);
        std::this_thread::sleep_for(std::chrono::seconds(15));
    }
}

}  // namespace

void StartDiscord() {
    uint64_t id = kAppId;
    if (const char* e = std::getenv("DEFJAM_DISCORD_APP_ID")) id = std::strtoull(e, nullptr, 10);
    if (!g_discord || !id) return;
    std::thread(Run, id).detach();
}

void SetDiscordDetails(const char* details) {
    std::lock_guard<std::mutex> lock(g_lock);
    if (g_details == details) return;
    g_details = details;
    g_dirty = true;
}

}  // namespace defjam
