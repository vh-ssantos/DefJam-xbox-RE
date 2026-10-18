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
