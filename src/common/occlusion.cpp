// Oclusão de ambiente (SSAO / GTAO, shaders próprios) como extensão do renderer do runtime (gpu::Extension).
// Entra uma vez por quadro, quando o jogo limpa a profundidade depois da cena 3D (antes do HUD), ou, sem esse clear,
// no primeiro desenho com Z ALWAYS e sem escrever Z: fica sobre cenário e lutadores e por baixo do placar.
// A câmera vem da matriz de projeção do jogo, achada na RAM (formato de linhas do D3D com w = +-z: a do Def Jam é
// a de OpenGL, [xs 0 0 0][0 ys 0 0][0 0 -1.00002 -1][0 0 -1.00002 0]); o endereço é guardado e conferido a cada uso.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#include "common/occlusion.h"
#include "common/widescreen.h"
#include "core/log.h"
#include "gpu/ext.h"
#include "gpu/state.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

using Microsoft::WRL::ComPtr;

namespace defjam {

uint32_t g_ao_mode = 0;  // 0 desligada, 1 SSAO, 2 GTAO (settings.toml: ambient_occlusion)

namespace {

// ---- a câmera do jogo -------------------------------------------------------------------------------------------

std::atomic<uint32_t> g_proj_va{0};
std::atomic<bool> g_scanning{false};

bool LooksLikeProjection(const float m[16]) {
    return std::fabs(m[11]) == 1.0f && m[15] == 0 && m[1] == 0 && m[2] == 0 && m[3] == 0 && m[4] == 0 && m[6] == 0 &&
           m[7] == 0 && m[12] == 0 && m[13] == 0 && m[14] != 0 && m[0] > 0.1f && m[0] < 20 && m[5] > 0.1f &&
           m[5] < 20 && std::isfinite(m[10]) && std::isfinite(m[14]);
}

void ScanForProjection() {
    const uint8_t* ram = kernel::g_base + mem::kContiguousBase;
    for (uint32_t off = 0; off + 64 <= mem::kPhysicalSize; off += 4) {
        float m[16];
        std::memcpy(m, ram + off, 64);
        if (!LooksLikeProjection(m)) continue;
        g_proj_va = mem::kContiguousBase + off;
        LOGI("defjam: oclusao: projecao da camera em %08X (xs %.3f ys %.3f)", g_proj_va.load(), m[0], m[5]);
        // (diagnóstico) os floats em volta da matriz: procura a proporção e o campo de visão da câmera
        if (std::getenv("DEFJAM_CAM_DUMP"))
            for (int k = -64; k < 64; k += 4) {
                float f[4];
                std::memcpy(f, ram + off + k * 4, 16);
                LOGI("cam %+4d: %10.5f %10.5f %10.5f %10.5f", k * 4, f[0], f[1], f[2], f[3]);
            }
        break;
    }
    g_scanning = false;
}

// a matriz atual; se o endereço guardado não tem mais uma projeção, procura de novo numa thread (até 1 vez a cada 2 s)
bool CameraProjection(float m[16]) {
    if (uint32_t va = g_proj_va) {
        std::memcpy(m, kernel::g_base + va, 64);
        if (LooksLikeProjection(m)) return true;
        g_proj_va = 0;
    }
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - last > 2000 && !g_scanning.exchange(true)) {
        last = now;
        std::thread(ScanForProjection).detach();
    }
    return false;
}

// ---- shaders ----------------------------------------------------------------------------------------------------

// Shaders próprios deste projeto. SSAO: "Alchemy AO" (McGuire et al., HPG 2011) com amostras numa espiral de Vogel
// e ruído "interleaved gradient" (Jimenez, 2014). GTAO: Jimenez et al., "Practical Real-Time Strategies for Accurate
// Indirect Occlusion" (2016), como descrito também no XeGTAO da Intel (MIT). Borrão bilateral separável.
// b1: p0 = 1/xs, 1/ys, distância = p0.z / (profundidade - p0.w); p1 = raio (unidades do jogo), alcance máximo em
// pixels, direção do borrão (x, y). t0 profundidade, t1 oclusão anterior (oclusão, distância), t2 cor da cena.
const char* kAoHlsl = R"(
cbuffer P : register(b1) { float4 p0; float4 p1; };
Texture2D<float> depth : register(t0);
Texture2D<float2> ao : register(t1);
Texture2D<float4> scene : register(t2);

float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2(id == 1 ? 2 : 0, id == 2 ? 2 : 0);
    return float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
}

int2 TargetSize() { uint w, h; depth.GetDimensions(w, h); return int2(w, h); }

// posição no espaço da câmera do centro do pixel p (z = distância, positiva para a frente)
float3 ViewPos(float2 p) {
    int2 size = TargetSize();
    int2 q = clamp(int2(p), 0, size - 1);
    float dist = p0.z / (depth.Load(int3(q, 0)) - p0.w);
    float2 ndc = float2((q.x + 0.5) / size.x * 2 - 1, 1 - (q.y + 0.5) / size.y * 2);
    return float3(ndc * p0.xy * dist, dist);
}

// normal pela profundidade: de cada lado, a diferença com o vizinho mais parecido (não atravessa bordas)
float3 ViewNormal(float2 p, float3 c) {
    float3 dx0 = c - ViewPos(p - float2(1, 0)), dx1 = ViewPos(p + float2(1, 0)) - c;
    float3 dy0 = c - ViewPos(p - float2(0, 1)), dy1 = ViewPos(p + float2(0, 1)) - c;
    float3 dx = abs(dx0.z) < abs(dx1.z) ? dx0 : dx1;
    float3 dy = abs(dy0.z) < abs(dy1.z) ? dy0 : dy1;
    float3 nn = normalize(cross(dy, dx));
    return dot(nn, c) > 0 ? -nn : nn;  // sempre virada para a câmera
}

float Noise(float2 p) { return frac(52.9829189 * frac(dot(p, float2(0.06711056, 0.00583715)))); }

// o raio do mundo em pixels nesta distância, limitado a p1.y; devolve o fator de encolhimento
float RadiusPixels(float dist, out float shrink) {
    float r = p1.x / (dist * min(p0.x, p0.y)) * 0.5 * TargetSize().y;
    shrink = min(1.0, p1.y / max(r, 1e-3));
    return r * shrink;
}

float2 occlusion(float4 pos : SV_Position) : SV_Target {
    float3 c = ViewPos(pos.xy);
    if (!(c.z > 0)) return float2(1, c.z);
    float shrink;
    float rpx = RadiusPixels(c.z, shrink);
    if (rpx < 1.5) return float2(1, c.z);
    float3 n = ViewNormal(pos.xy, c);
    float radius = p1.x * shrink;
    float rot = Noise(pos.xy) * 6.2831853;
    const int kTaps = 12;
    float sum = 0;
    [unroll] for (int i = 0; i < kTaps; i++) {
        float t = (i + 0.5) / kTaps;
        float ang = i * 2.39996323 + rot;  // ângulo de ouro
        float2 off = float2(cos(ang), sin(ang)) * sqrt(t) * rpx;
        float3 v = ViewPos(pos.xy + off) - c;
        float vv = dot(v, v);
        float fall = saturate(1 - vv / (radius * radius));
        sum += fall * max(0, dot(v, n) - 0.03 * radius) / (vv + 0.01 * radius * radius);
    }
    // termo do Alchemy, sem unidade: vezes o raio (o sigma do artigo) e 2/amostras, com intensidade 0.5
    float a = saturate(1 - sum * radius * (2.0 / kTaps) * 0.5);
    return float2(a, c.z);
}

float2 gtao(float4 pos : SV_Position) : SV_Target {
    const float kPi = 3.14159265, kHalfPi = 1.57079633;
    float3 c = ViewPos(pos.xy);
    if (!(c.z > 0)) return float2(1, c.z);
    float shrink;
    float rpx = RadiusPixels(c.z, shrink);
    if (rpx < 1.5) return float2(1, c.z);
    float3 n = ViewNormal(pos.xy, c);
    float3 viewDir = normalize(-c);
    float radius = p1.x * shrink;
    float noiseSlice = Noise(pos.xy);
    float noiseStep = Noise(pos.yx + 17.0);
    const int kSlices = 3, kSteps = 4;
    float visibility = 0;
    [unroll] for (int s = 0; s < kSlices; s++) {
        float phi = (s + noiseSlice) / kSlices * kPi;
        float3 dir = float3(cos(phi), sin(phi), 0);
        float2 omega = float2(dir.x, -dir.y);  // na tela o y desce
        // plano da fatia (direção e vetor da câmera) e a normal projetada nele
        float3 orthoDir = dir - dot(dir, viewDir) * viewDir;
        float3 axis = normalize(cross(orthoDir, viewDir));
        float3 projN = n - axis * dot(n, axis);
        float projLen = length(projN);
        if (projLen < 1e-4) continue;
        float cosN = saturate(dot(projN, viewDir) / projLen);
        float nAngle = sign(dot(orthoDir, projN)) * acos(cosN);
        // cosseno do horizonte mais alto de cada lado; começa no plano tangente da superfície
        float low0 = cos(nAngle + kHalfPi), low1 = cos(nAngle - kHalfPi);
        float hc0 = low0, hc1 = low1;
        [unroll] for (int j = 0; j < kSteps; j++) {
            float t = (j + noiseStep) / kSteps;
            float2 off = omega * (t * t * rpx + 1.0);
            float3 d0 = ViewPos(pos.xy + off) - c;
            float3 d1 = ViewPos(pos.xy - off) - c;
            float l0 = length(d0), l1 = length(d1);
            float w0 = saturate((radius - l0) / (0.6 * radius));
            float w1 = saturate((radius - l1) / (0.6 * radius));
            hc0 = max(hc0, lerp(low0, dot(d0, viewDir) / max(l0, 1e-4), w0));
            hc1 = max(hc1, lerp(low1, dot(d1, viewDir) / max(l1, 1e-4), w1));
        }
        // os horizontes em ângulo, presos ao hemisfério da normal, e o arco visível ponderado pelo cosseno
        float h0 = nAngle + clamp(-acos(hc1) - nAngle, -kHalfPi, kHalfPi);
        float h1 = nAngle + clamp(acos(hc0) - nAngle, -kHalfPi, kHalfPi);
        float sinN = sin(nAngle);
        float arc0 = (cosN + 2 * h0 * sinN - cos(2 * h0 - nAngle)) * 0.25;
        float arc1 = (cosN + 2 * h1 * sinN - cos(2 * h1 - nAngle)) * 0.25;
        visibility += projLen * (arc0 + arc1);
    }
    float a = saturate(visibility / kSlices);
    return float2(a * a, c.z);  // um pouco mais de contraste, como o expoente do XeGTAO
}

// borrão bilateral de 7 toques na direção p1.zw, sem misturar distâncias diferentes
float Bilateral(float2 pos, out float dist) {
    static const float kW[4] = {0.20, 0.17, 0.12, 0.07};
    int2 size = TargetSize();
    float2 center = ao.Load(int3(int2(pos), 0));
    dist = center.y;
    float sum = center.x * kW[0], wsum = kW[0];
    [unroll] for (int i = 1; i <= 3; i++) {
        [unroll] for (int sgn = -1; sgn <= 1; sgn += 2) {
            int2 q = clamp(int2(pos) + sgn * i * int2(p1.zw), 0, size - 1);
            float2 s = ao.Load(int3(q, 0));
            float w = kW[i] * saturate(1 - abs(s.y - dist) / max(dist * 0.03, 1e-3));
            sum += s.x * w;
            wsum += w;
        }
    }
    return sum / wsum;
}

float2 blur(float4 pos : SV_Position) : SV_Target {
    float dist;
    float a = Bilateral(pos.xy, dist);
    return float2(a, dist);
}

// cena vezes oclusão; o alfa fica como estava (os blends do jogo leem)
float4 apply(float4 pos : SV_Position) : SV_Target {
    float dist;
    float a = Bilateral(pos.xy, dist);
    float4 c = scene.Load(int3(int2(pos.xy), 0));
    return float4(c.rgb * a, c.a);
}

// depuração: só a oclusão (DEFJAM_AO_SHOW=1) e a profundidade (DEFJAM_AO_SHOW=z)
float4 show(float4 pos : SV_Position) : SV_Target {
    float dist;
    return float4(Bilateral(pos.xy, dist).xxx, 1);
}
float4 showz(float4 pos : SV_Position) : SV_Target {
    float d = depth.Load(int3(int2(pos.xy), 0));
    float z = ViewPos(pos.xy).z;
    return float4(pow(saturate(d), 256), frac(z / 100), saturate(-z / 1000), 1);
}
)";

constexpr DXGI_FORMAT kAoFormat = DXGI_FORMAT_R16G16_FLOAT;
constexpr uint32_t kZAlways = 0x207;  // NV2A: função de Z ALWAYS

class AoExtension : public gpu::Extension {
public:
    void Reset(gpu::Context&) override {
        for (auto& t : ao_) t.Reset();
        w_ = h_ = 0;
    }

    bool Bound(gpu::Context& ctx, const gpu::State& s, gpu::Target* rt, gpu::Target* ds, bool) override {
        const uint32_t mode = g_ao_mode;
        if (!mode || !rt || !ds || rt->depth) return false;
        NewFrame(ctx);
        if (done_) return false;
        uint32_t gw, gh;
        ctx.GuestSize(gw, gh);
        if (rt->gw != gw || rt->gh != gh || ds->w != rt->w || ds->h != rt->h) return false;
        const bool zen = s.rs[gpu::kRsZEnable] != 0;
        const bool zwrite = zen && s.simple[gpu::kMZWriteEnable >> 2] != 0;
        const uint32_t zfunc = s.simple[gpu::kMZFunc >> 2];
        if (zwrite) {
            scene_rt_ = rt;  // a cena 3D está sendo desenhada neste alvo
            return false;
        }
        if (scene_rt_ != rt || (zen && zfunc != kZAlways)) return false;
        done_ = true;
        return Run(ctx, s, *rt, *ds, mode);
    }

    // 2D na proporção certa em 16:9 (widescreen.cpp)
    bool Placing() const override { return true; }
    void Place(const gpu::State& s, const gpu::Target* rt, uint32_t gw, uint32_t gh, const Footprint& f, float* place) override {
        PlaceDraw(s, rt, gw, gh, f, place);
    }

    // o Def Jam limpa a profundidade (flag 0x1 do Xbox) antes do HUD: é aí que a oclusão entra, com a cena pronta
    void BeforeClear(gpu::Context& ctx, const gpu::State& s, gpu::Target* rt, gpu::Target* ds, uint32_t flags) override {
        const uint32_t mode = g_ao_mode;
        if (!mode || !(flags & 0x01) || !rt || !ds) return;
        NewFrame(ctx);
        if (done_ || scene_rt_ != rt || ds->w != rt->w || ds->h != rt->h) return;
        done_ = true;
        Run(ctx, s, *rt, *ds, mode);
    }

private:
    void NewFrame(gpu::Context& ctx) {
        const uint64_t frame = ctx.Presents();
        if (frame != frame_) {
            frame_ = frame;
            done_ = false;
            scene_rt_ = nullptr;
        }
    }

    bool Ensure(gpu::Context& ctx, uint32_t w, uint32_t h) {
        if (ao_[1] && w_ == w && h_ == h) return true;
        ID3D12Device* dev = ctx.Device();
        if (!heap_) {
            D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
            if (FAILED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)))) return false;
            inc_ = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }
        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC td{};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = w;
        td.Height = h;
        td.DepthOrArraySize = 1;
        td.MipLevels = 1;
        td.Format = kAoFormat;
        td.SampleDesc.Count = 1;
        td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        for (int i = 0; i < 2; i++) {
            if (ao_[i]) ctx.Release(std::move(ao_[i]));
            state_[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
            if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, state_[i], nullptr,
                                                   IID_PPV_ARGS(&ao_[i])))) {
                LOGE("defjam: oclusao: sem alvo %ux%u", w, h);
                return false;
            }
            dev->CreateRenderTargetView(ao_[i].Get(), nullptr, Rtv(i));
        }
        w_ = w;
        h_ = h;
        return true;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Rtv(int i) const {
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap_->GetCPUDescriptorHandleForHeapStart();
        h.ptr += size_t(i) * inc_;
        return h;
    }

    bool Run(gpu::Context& ctx, const gpu::State&, gpu::Target& rt, gpu::Target& ds, uint32_t mode) {
        float m[16];
        if (!CameraProjection(m)) return false;
        if (!Ensure(ctx, rt.w, rt.h)) return false;
        static const float radius = [] {
            const char* e = std::getenv("DEFJAM_AO_RADIUS");
            return e ? float(std::atof(e)) : 40.0f;
        }();
        const float s = m[11];  // +1 (D3D) ou -1 (OpenGL): distância = zp.x / (profundidade - zp.y)
        float p[8] = {1 / m[0], 1 / m[5], m[14], s * m[10], radius, 0.15f * float(rt.h), 0, 0};
        LOG_FIRST(1, "defjam: oclusao %s %ux%u, xs %.3f ys %.3f, distancia = %.5f / (z - %.5f), raio %.1f",
                  mode == 2 ? "GTAO" : "SSAO", rt.w, rt.h, m[0], m[5], p[2], p[3], radius);

        ID3D12Resource* none[3] = {nullptr, nullptr, nullptr};
        const DXGI_FORMAT fmts[3] = {DXGI_FORMAT_R24_UNORM_X8_TYPELESS, kAoFormat, rt.fmt};

        // 1: oclusão crua, lendo a profundidade
        ctx.Transition(ds.res.Get(), ds.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        ctx.Transition(ao_[0].Get(), state_[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
        ID3D12Resource* src1[3] = {ds.res.Get(), nullptr, nullptr};
        ctx.Pass(kAoHlsl, mode == 2 ? "gtao" : "occlusion", kAoFormat, Rtv(0), rt.w, rt.h, src1, fmts, p, -1);
        ctx.Transition(ds.res.Get(), ds.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        // 2: borrão na horizontal
        p[6] = 1;
        p[7] = 0;
        ctx.Transition(ao_[0].Get(), state_[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        ctx.Transition(ao_[1].Get(), state_[1], D3D12_RESOURCE_STATE_RENDER_TARGET);
        ID3D12Resource* src2[3] = {ds.res.Get(), ao_[0].Get(), nullptr};
        ctx.Transition(ds.res.Get(), ds.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        ctx.Pass(kAoHlsl, "blur", kAoFormat, Rtv(1), rt.w, rt.h, src2, fmts, p, -1);

        // 3: borrão na vertical, multiplicado na cor da cena (lida de uma cópia)
        p[6] = 0;
        p[7] = 1;
        ctx.Transition(ao_[1].Get(), state_[1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        ID3D12Resource* scene = ctx.Feedback(rt);
        if (!scene) {
            ctx.Transition(ds.res.Get(), ds.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            return true;
        }
        ctx.Transition(rt.res.Get(), rt.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
        static const char* show_env = std::getenv("DEFJAM_AO_SHOW");
        const char* entry = !show_env ? "apply" : show_env[0] == 'z' ? "showz" : "show";
        ID3D12Resource* src3[3] = {ds.res.Get(), ao_[1].Get(), scene};
        ctx.Pass(kAoHlsl, entry, rt.fmt, ctx.View(rt), rt.w, rt.h, src3, fmts, p, -1);
        ctx.Transition(ds.res.Get(), ds.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        rt.drawn = true;
        (void)none;
        return true;  // o backend liga de novo os alvos do desenho
    }

    ComPtr<ID3D12DescriptorHeap> heap_;
    UINT inc_ = 0;
    ComPtr<ID3D12Resource> ao_[2];
    D3D12_RESOURCE_STATES state_[2] = {};
    uint32_t w_ = 0, h_ = 0;
    uint64_t frame_ = ~0ull;
    bool done_ = false;
    gpu::Target* scene_rt_ = nullptr;
};

}  // namespace

void InstallOcclusion() { gpu::SetExtension(std::make_unique<AoExtension>()); }

}  // namespace defjam
