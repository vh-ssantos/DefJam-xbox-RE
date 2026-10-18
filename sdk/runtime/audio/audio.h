#pragma once
#include <cstdint>
#include <atomic>
#include <functional>

namespace audio {

constexpr uint32_t kRate = 48000;

// the platform output pulls interleaved stereo float frames from render
void StartOutput(std::function<void(float* out, uint32_t frames)> render);

// (DefJam) true: o mixer entrega silêncio e não avança as vozes (jogo pausado sem foco)
inline std::atomic<bool> g_paused{false};

}  // namespace audio
