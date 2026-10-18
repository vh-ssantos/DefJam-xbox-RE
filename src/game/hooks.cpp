// Hooks for default.xbe. A strong definition replaces the recompiled function at that address, for example:
//
// #include "xdk/hle.h"
//
// X86_EXTERN_FUNC(__imp__sub_00012345);
// X86_FUNC(sub_00012345) {
//     __imp__sub_00012345(ctx, base);  // the original
// }

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/memory.h"
#include "xdk/hle.h"
#include "common/discord.h"
#include "common/menu.h"

#include <string>

namespace {
constexpr uint32_t kDevice = SYM(D3D_Device);
constexpr uint32_t kDevBackBufferScale = 0x96C;  // sdk/xdk/5849/layout.toml CDevice_BackBufferScale
}  // namespace

// D3DDevice_SetBackBufferScale(float x, float y), stdcall. Fora das assinaturas do SDK: o corpo original refaz o
// viewport lendo o front buffer do device (+0x1A18) e grava métodos NV2A crus, e caía em ponteiro nulo. O renderer
// do host não usa essa escala, então só guardamos os valores onde o código inline do XDK os lê.
X86_FUNC(sub_00219B90) {
    (void)base;
    ::kernel::Args a{ctx};
    const uint32_t x = a[0], y = a[1];
    ::kernel::W32(kDevice + kDevBackBufferScale, x);
    ::kernel::W32(kDevice + kDevBackBufferScale + 4, y);
    static int logged = 0;
    if (logged++ < 4) {
        float fx, fy;
        std::memcpy(&fx, &x, 4);
        std::memcpy(&fy, &y, 4);
        LOGI("defjam: SetBackBufferScale %.3f %.3f", fx, fy);
    }
    ::kernel::Return(ctx, 0, 2);
}

// IDirectSoundBuffer_SetPitch / SetLFO / SetEG (this, valor), stdcall: o runtime não tem versão do host e o corpo
// original mexe nos campos internos da voz da DSOUND, que não existem nos objetos do host. Achados pela ordem da
// vtable do XDK (SetVolume 0x25925F, Pitch, LFO, EG, SetFilter 0x2592CF, SetHeadroom 0x2592EB).
#define DEFJAM_DSOUND_NOOP(addr)       \
    X86_FUNC(sub_##addr) {             \
        (void)base;                    \
        ::kernel::Return(ctx, 0, 2);   \
    }
DEFJAM_DSOUND_NOOP(0025927B)  // SetPitch
DEFJAM_DSOUND_NOOP(00259297)  // SetLFO
DEFJAM_DSOUND_NOOP(002592B3)  // SetEG

// IDirectSoundBuffer_PlayEx (0x259347) e Pause / PauseEx (0x2593A7 / 0x2593C3): fora do runtime; o corpo original
// roda sobre os objetos do host sem tocar nada (as falas das cutscenes não andavam e a cena congelava). Montados com
// as versões do host de Play (0x259323), Stop (0x25936B) e GetStatus (0x259407): Play com flags 0 retoma da posição.
namespace {
constexpr uint32_t kBufferPlay = 0x259323, kBufferStop = 0x25936B, kBufferGetStatus = 0x259407;
std::mutex g_pauseLock;
std::unordered_map<uint32_t, uint32_t> g_pausedFlags;  // buffer pausado -> flags de Play para retomar (1 = loop)

void LogFirst(const char* what, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    static std::atomic<int> n{0};
    if (n++ < 40) LOGI("defjam: %s(%08X, %08X, %08X, %08X)", what, a0, a1, a2, a3);
}

// dwPause: 0 retoma, 1 pausa (2, sincronizar, tratado como retomar)
void PauseBuffer(X86Context& ctx, uint32_t buffer, uint32_t pause) {
    if (pause == 1) {
        const uint32_t status = ::mem::AllocSystem(4);
        ::kernel::CallGuest(ctx, kBufferGetStatus, {buffer, status});
        const uint32_t s = ::kernel::R32(status);
        ::mem::FreeSystem(status);
        if (!(s & 1)) return;  // não estava tocando
        {
            std::lock_guard<std::mutex> lock(g_pauseLock);
            g_pausedFlags[buffer] = (s & 4) ? 1u : 0u;
        }
        ::kernel::CallGuest(ctx, kBufferStop, {buffer});
    } else {
        uint32_t flags;
        {
            std::lock_guard<std::mutex> lock(g_pauseLock);
            auto it = g_pausedFlags.find(buffer);
            if (it == g_pausedFlags.end()) return;
            flags = it->second;
            g_pausedFlags.erase(it);
        }
        ::kernel::CallGuest(ctx, kBufferPlay, {buffer, 0, 0, flags});
    }
}
}  // namespace

// PlayEx(this, REFERENCE_TIME rtTimeStamp, DWORD dwFlags): toca agora
X86_FUNC(sub_00259347) {
    (void)base;
    ::kernel::Args a{ctx};
    LogFirst("PlayEx", a[0], a[1], a[2], a[3]);
    {
        std::lock_guard<std::mutex> lock(g_pauseLock);
        g_pausedFlags.erase(a[0]);
    }
    const uint32_t r = ::kernel::CallGuest(ctx, kBufferPlay, {a[0], 0, 0, a[3]});
    ::kernel::Return(ctx, r, 4);
}

// Pause(this, DWORD dwPause)
X86_FUNC(sub_002593A7) {
    (void)base;
    ::kernel::Args a{ctx};
    LogFirst("Pause", a[0], a[1], 0, 0);
    PauseBuffer(ctx, a[0], a[1]);
    ::kernel::Return(ctx, 0, 2);
}

// PauseEx(this, REFERENCE_TIME rtTimeStamp, DWORD dwPause)
X86_FUNC(sub_002593C3) {
    (void)base;
    ::kernel::Args a{ctx};
    LogFirst("PauseEx", a[0], a[1], a[2], a[3]);
    PauseBuffer(ctx, a[0], a[3]);
    ::kernel::Return(ctx, 0, 4);
}

// D3DDevice_SetTimerCallback(ULONGLONG time, D3DCALLBACK callback, DWORD context), stdcall, 4 dwords. Fora das
// assinaturas do SDK; o original arma o alarme do PTIMER da NV2A pelo miniport (+0x1C28), que o host não tem. O jogo
// usa como relógio periódico: 0xD7E8A arma "agora + período" com o callback 0xD7EA0, que se rearma. Aqui uma thread do
// host espera o contador de 733 MHz (x86_rdtsc) chegar no tempo e chama o callback.
namespace {
std::mutex g_timerLock;
std::condition_variable g_timerCv;
uint64_t g_timerTime = 0;
uint32_t g_timerCallback = 0, g_timerContext = 0;
constexpr uint32_t kD3dErrTimeInPast = 0x8876082A;  // o que o original devolve quando o tempo já passou

void TimerThread(X86Context& ctx) {
    std::unique_lock<std::mutex> lock(g_timerLock);
    for (;;) {
        if (!g_timerCallback) {
            g_timerCv.wait(lock);
            continue;
        }
        const uint64_t now = x86_rdtsc();
        if (now < g_timerTime) {
            const uint64_t us = (g_timerTime - now) * 1000000ull / 733333333ull;
            g_timerCv.wait_for(lock, std::chrono::microseconds(std::max<uint64_t>(us, 1)));
            continue;
        }
        const uint32_t callback = g_timerCallback, context = g_timerContext;
        g_timerCallback = 0;  // um disparo; o callback se rearma
        lock.unlock();
        ::kernel::CallGuest(ctx, callback, {context});
        lock.lock();
    }
}
}  // namespace

X86_FUNC(sub_00216410) {
    (void)base;
    ::kernel::Args a{ctx};
    const uint64_t time = uint64_t(a[0]) | uint64_t(a[1]) << 32;
    const uint32_t callback = a[2], context = a[3];
    uint32_t result = 0;
    {
        std::lock_guard<std::mutex> lock(g_timerLock);
        static bool started = false;
        if (!started) {
            started = true;
            ::kernel::StartHostThread("timer callback", TimerThread);
        }
        // O original recusa um tempo já passado (D3DERR_TIMEINPAST). O jogo arma o próximo em "anterior + período";
        // num PC uma thread pode atrasar mais de um período e aí a recusa matava o relógio para sempre (cutscene e
        // diálogo congelavam com a tela ainda sendo apresentada). Um tempo passado dispara já; o jogo alcança a grade.
        (void)kD3dErrTimeInPast;
        const uint64_t now = x86_rdtsc();
        if (callback && time <= now + 40) {
            static int late = 0;
            if (++late <= 20 || late % 100 == 0)
                LOGI("defjam: timer pedido %.2f ms no passado (%d vezes)", double(now - time) / 733333.0, late);
        }
        g_timerTime = time;
        g_timerCallback = callback;
        g_timerContext = context;
    }
    g_timerCv.notify_all();
    ::kernel::Return(ctx, result, 4);
}

// Nome do próximo vídeo (this = ecx, nome em [esp+4]): copia para this+841. Com "Pular vídeos de abertura" ligado, os
// três da abertura (EA, THX e o filme de introdução) marcam um START automático meio segundo depois, que o jogo trata
// como o jogador pulando o vídeo (src/common/game_info.cpp, PadHook).
X86_EXTERN_FUNC(__imp__sub_0001E460);
X86_FUNC(sub_0001E460) {
    const std::string name = ::kernel::ReadCString(::kernel::R32(ctx.esp + 4), 64);
    if (name == "movies/eagames" || name == "movies/thx" || name == "movies/intro") defjam::IntroMovieStarted();
    __imp__sub_0001E460(ctx, base);
}

// Subsistema de dispositivos de armazenamento (0x130F00-0x133D00: HD e memory units, usado pela camada de memory card
// do jogo em 0x1CB00-0x1D700 e pelo módulo de saves em 0x134000-0x135800). Guarda o dispositivo "atual" em globais
// (0x3AD928, 0x3AD934, 0x3AD938) e as zera ao sair. No Xbox (um núcleo) duas threads quase nunca se cruzavam aí; no PC
// rodam em paralelo de verdade e uma lia [0x3AD928] já zerado pela outra (crash em 0x1328C6, início de luta com
// autosave). Uma trava recursiva única em todas as entradas devolve o "um por vez".
namespace {
std::recursive_mutex g_storage_lock;
}  // namespace
#define DEFJAM_STORAGE_SERIAL(addr)                                  \
    X86_EXTERN_FUNC(__imp__sub_##addr);                              \
    X86_FUNC(sub_##addr) {                                           \
        std::lock_guard<std::recursive_mutex> lock(g_storage_lock);  \
        __imp__sub_##addr(ctx, base);                                \
    }
DEFJAM_STORAGE_SERIAL(00130F40)
DEFJAM_STORAGE_SERIAL(00131060)
DEFJAM_STORAGE_SERIAL(00131080)
DEFJAM_STORAGE_SERIAL(00131310)
DEFJAM_STORAGE_SERIAL(00131620)
DEFJAM_STORAGE_SERIAL(00131760)
DEFJAM_STORAGE_SERIAL(00131780)
DEFJAM_STORAGE_SERIAL(001317C0)
DEFJAM_STORAGE_SERIAL(00131C00)
DEFJAM_STORAGE_SERIAL(00131D20)
DEFJAM_STORAGE_SERIAL(00131E90)
DEFJAM_STORAGE_SERIAL(00132060)
DEFJAM_STORAGE_SERIAL(001320A0)
DEFJAM_STORAGE_SERIAL(00132260)
DEFJAM_STORAGE_SERIAL(00132440)
DEFJAM_STORAGE_SERIAL(00132670)
DEFJAM_STORAGE_SERIAL(00132770)
DEFJAM_STORAGE_SERIAL(001329A0)
DEFJAM_STORAGE_SERIAL(00132BA0)
DEFJAM_STORAGE_SERIAL(00132C40)
DEFJAM_STORAGE_SERIAL(00132C70)
DEFJAM_STORAGE_SERIAL(00132CA0)
DEFJAM_STORAGE_SERIAL(00133240)
DEFJAM_STORAGE_SERIAL(00133570)
DEFJAM_STORAGE_SERIAL(00133580)
DEFJAM_STORAGE_SERIAL(001335B0)
DEFJAM_STORAGE_SERIAL(00133CD0)
// 0x133C00(flag), cdecl: chamada pelos callbacks 0x133CB0/0x133CC0 que o sistema de streaming (0xBF2B0) guarda e
// chama da thread dele. Ela mesma desiste se o subsistema está ocupado ([0x3AD938] != 0), mas essa conferência não é
// atômica. Aqui: se outra thread está dentro do subsistema, desiste também (try_lock), em vez de esperar; esperar
// travava o jogo, porque a thread principal pode estar esperando o streaming com a trava na mão.
X86_EXTERN_FUNC(__imp__sub_00133C00);
X86_FUNC(sub_00133C00) {
    std::unique_lock<std::recursive_mutex> lock(g_storage_lock, std::try_to_lock);
    if (!lock.owns_lock()) {
        ctx.esp += 4;  // ret: o chamador tira o argumento
        return;
    }
    __imp__sub_00133C00(ctx, base);
}

// Cheats do próprio jogo (Extras -> Cheats). O objeto da tela é criado por 0x49F40 (thiscall), que registra o
// comando de script "SubmitCheatCode" com o tratador 0x49D00(this, args, n): args[0] = o texto do código; devolve o
// texto da resposta (em this+3084). O menu do PC (common/menu.cpp) enfileira um código; PadHook, na thread do jogo,
// chama ApplyPendingCheat, que usa o mesmo tratador.
namespace {
std::atomic<uint32_t> g_cheat_obj{0};
}  // namespace

X86_EXTERN_FUNC(__imp__sub_00049F40);
X86_FUNC(sub_00049F40) {
    g_cheat_obj = ctx.ecx;
    __imp__sub_00049F40(ctx, base);
}

namespace defjam {
void ApplyPendingCheat() {
    std::string code;
    if (!TakeCheatRequest(code)) return;
    const uint32_t obj = g_cheat_obj;
    if (!obj) {
        CheatResult("O jogo ainda nao criou a tela de cheats: entre em EXTRAS uma vez e tente de novo.");
        return;
    }
    static uint32_t buf = 0;
    if (!buf) buf = ::mem::AllocSystem(64);
    std::memset(::kernel::HostPtr(buf), 0, 64);
    std::memcpy(::kernel::HostPtr(buf + 16), code.data(), std::min<size_t>(code.size(), 40));
    ::kernel::W32(buf, buf + 16);  // args[0] = o texto
    X86Context& ctx = ::kernel::CurrentContext();
    const X86Context saved = ctx;
    ctx.ecx = obj;
    const uint32_t msg = ::kernel::CallGuest(ctx, 0x49D00, {buf, 1});
    std::string text = msg ? ::kernel::ReadCString(msg, 200) : std::string();
    ctx = saved;
    LOGI("defjam: cheat %s -> %s", code.c_str(), text.c_str());
    // respostas do jogo: TYPE=1 música liberada (TRACKNAME/ARTIST), TYPE=2 pontos (VALUE), TYPE=4 código já usado
    auto field = [&](const char* key) {
        const std::string k = std::string(key) + "=";
        const size_t at = text.find(k);
        if (at == std::string::npos) return std::string();
        return text.substr(at + k.size(), text.find('&', at) == std::string::npos ? std::string::npos
                                                                                  : text.find('&', at) - at - k.size());
    };
    const std::string type = field("TYPE");
    std::string shown;
    if (type == "1") shown = "Musica liberada: " + field("ARTIST") + " - " + field("TRACKNAME") + " (toca no jogo e no Beat Box)";
    else if (type == "2") shown = "+" + field("VALUE") + " pontos de recompensa (use em Unlock Rewards)";
    else if (type == "4") shown = "Esse codigo ja foi usado neste save (cada um vale uma vez)";
    else if (text.empty()) shown = "(sem resposta)";
    else shown = "Resposta do jogo: " + text;
    CheatResult(shown);
}
}  // namespace defjam

// CreateNewUser(args, n) do script (DONE no teclado do User ID): args[0] = o nome. Com um nome
// digitado no teclado do PC (menu F10), ele entra no lugar do que foi escolhido na tela.
X86_EXTERN_FUNC(__imp__sub_00052820);
X86_FUNC(sub_00052820) {
    std::string name;
    if (defjam::TakeTypedName(name)) {
        static uint32_t buf = 0;
        if (!buf) buf = ::mem::AllocSystem(32);
        std::memset(::kernel::HostPtr(buf), 0, 32);
        std::memcpy(::kernel::HostPtr(buf), name.data(), std::min<size_t>(name.size(), 8));
        const uint32_t args = ::kernel::R32(ctx.esp + 4);
        LOGI("defjam: User ID digitado no PC: %s (no lugar de %s)", name.c_str(),
             ::kernel::ReadCString(::kernel::R32(args), 32).c_str());
        ::kernel::W32(args, buf);
    }
    __imp__sub_00052820(ctx, base);
}

// Comandos de script de troca de modo (handlers registrados com push handler; push nome): avisam a presença no
// Discord do que o jogador está fazendo e seguem para o original.
#define DEFJAM_PRESENCE(addr, text)          \
    X86_EXTERN_FUNC(__imp__sub_##addr);      \
    X86_FUNC(sub_##addr) {                   \
        defjam::SetDiscordDetails(text);     \
        __imp__sub_##addr(ctx, base);        \
    }
DEFJAM_PRESENCE(00046610, "No menu principal")  // StartMainMenu
DEFJAM_PRESENCE(00046410, "Modo Battle")        // StartBattleMode
DEFJAM_PRESENCE(00046450, "Modo historia")      // StartStoryMode
DEFJAM_PRESENCE(00046E40, "Lutando")            // StartGame (a luta começa)
