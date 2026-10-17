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
#include <mutex>

#include "core/log.h"
#include "cpu/x86_ops.h"
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
        if (callback && time <= x86_rdtsc() + 40) {
            result = kD3dErrTimeInPast;
        } else {
            g_timerTime = time;
            g_timerCallback = callback;
            g_timerContext = context;
        }
    }
    g_timerCv.notify_all();
    ::kernel::Return(ctx, result, 4);
}
