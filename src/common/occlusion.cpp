// Oclusão de ambiente (SSAO / GTAO, shaders do port do BLACK) como extensão do renderer do runtime (gpu::Extension).
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

// b1: p0 = 1/xs, 1/ys, distância = zp.x / (depth - zp.y); p1 = raio (unidades do jogo), maior alcance em pixels,
// passo do borrão (x, y). t0 profundidade, t1 oclusão do passe anterior (oclusão, distância), t2 a cor da cena.
const char* kAoHlsl = R"(
cbuffer P : register(b1) { float4 p0; float4 p1; };
Texture2D<float> depth : register(t0);
Texture2D<float2> ao : register(t1);
Texture2D<float4> scene : register(t2);
float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 t = float2((id << 1) & 2, id & 2);
    return float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
}
float2 Size() { uint w, h; depth.GetDimensions(w, h); return float2(w, h); }
float3 View(float2 px) {
    float2 sz = Size();
    int2 p = clamp(int2(px), int2(0, 0), int2(sz) - 1);
    float z = p0.z / (depth.Load(int3(p, 0)) - p0.w);
    float2 ndc = (p + 0.5) / sz * float2(2, -2) + float2(-1, 1);
    return float3(ndc * p0.xy * z, z);
}
float3 Normal(float2 pos, float3 c) {
    float3 l = View(pos - float2(1, 0));
    float3 r = View(pos + float2(1, 0));
    float3 u = View(pos - float2(0, 1));
    float3 d = View(pos + float2(0, 1));
    float3 dx = abs(r.z - c.z) < abs(c.z - l.z) ? r - c : c - l;
    float3 dy = abs(d.z - c.z) < abs(c.z - u.z) ? d - c : c - u;
    return normalize(cross(dx, dy));
}
static const float kDither[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
float2 occlusion(float4 pos : SV_Position) : SV_Target {
    float3 c = View(pos.xy);
    float2 sz = Size();
    float2 rad = p1.x * 0.5 * sz / (p0.xy * c.z);
    float span = max(rad.x, rad.y);
    if (span < 1 || c.z <= 0) return float2(1, c.z);
    float3 n = Normal(pos.xy, c);
    float k = min(1, p1.y / span);
    float R = p1.x * k;
    int2 cell = int2(pos.xy) & 3;
    float turn = kDither[cell.y * 4 + cell.x] * (6.2831853 / 16);
    float a = 0;
    for (int i = 0; i < 16; i++) {
        float f = (i + 0.5) / 16;
        float t = i * 2.3999632 + turn;
        float3 v = (View(pos.xy + float2(cos(t), sin(t)) * rad * (k * f)) - c) / R;
        float vv = dot(v, v);
        float w = max(1 - vv, 0);
        a += w * w * w * max((dot(v, n) - 0.05) / (vv + 0.01), 0);
    }
    return float2(max(1 - a * 0.25, 0), c.z);
}
static const float kHalfPi = 1.5707963;
float2 gtao(float4 pos : SV_Position) : SV_Target {
    float3 c = View(pos.xy);
    float2 sz = Size();
    float2 rad = p1.x * 0.5 * sz / (p0.xy * c.z);
    float span = max(rad.x, rad.y);
    if (span < 1 || c.z <= 0) return float2(1, c.z);
    float3 n = Normal(pos.xy, c);
    float3 v = normalize(-c);
    float k = min(1, p1.y / span);
    float R = p1.x * k;
    int2 cell = int2(pos.xy) & 3;
    float slice_noise = kDither[cell.y * 4 + cell.x] / 16;
    float step_noise = kDither[cell.x * 4 + cell.y] / 16;
    float vis = 0;
    for (int s = 0; s < 3; s++) {
        float phi = (s + slice_noise) * (kHalfPi * 2 / 3);
        float3 dir = float3(cos(phi), sin(phi), 0);
        float2 omega = float2(dir.x, -dir.y) * rad * k;
        float3 ortho = dir - dot(dir, v) * v;
        float3 axis = normalize(cross(ortho, v));
        float3 pn = n - axis * dot(n, axis);
        float pn_len = max(length(pn), 1e-4);
        float cos_n = saturate(dot(pn, v) / pn_len);
        float an = sign(dot(ortho, pn)) * acos(cos_n);
        float2 low = float2(cos(an + kHalfPi), cos(an - kHalfPi));
        float2 hc = low;
        for (int j = 0; j < 3; j++) {
            float f = (j + step_noise) / 3;
            float2 off = omega * (f * f + 1.3 / length(omega));
            float3 d0 = View(pos.xy + off) - c;
            float3 d1 = View(pos.xy - off) - c;
            float2 len = max(float2(length(d0), length(d1)), 1e-4);
            float2 w = saturate((R - len) / (R * 0.615));
            float2 sc = float2(dot(d0, v), dot(d1, v)) / len;
            hc = max(hc, lerp(low, sc, w));
        }
        float h0 = an + clamp(-acos(hc.y) - an, -kHalfPi, kHalfPi);
        float h1 = an + clamp(acos(hc.x) - an, -kHalfPi, kHalfPi);
        float sin_n = sin(an);
        float arc0 = (cos_n + 2 * h0 * sin_n - cos(2 * h0 - an)) / 4;
        float arc1 = (cos_n + 2 * h1 * sin_n - cos(2 * h1 - an)) / 4;
        vis += pn_len * (arc0 + arc1);
    }
    return float2(pow(saturate(vis / 3), 2.2), c.z);
}
float Smooth(float2 pos, out float z) {
    int2 p = int2(pos);
    uint w, h;
    ao.GetDimensions(w, h);
    int2 hi = int2(w, h) - 1;
    z = ao.Load(int3(p, 0)).y;
    float sum = 0, n = 0;
    for (int i = -2; i <= 2; i++) {
        float2 s = ao.Load(int3(clamp(p + i * int2(p1.zw), int2(0, 0), hi), 0));
        float wt = (abs(i) == 2 ? 0.5 : 1) * saturate(2 - abs(s.y - z) / (z * 0.02));
        sum += s.x * wt;
        n += wt;
    }
    return sum / n;
}
float2 blur(float4 pos : SV_Position) : SV_Target {
    float z;
    float a = Smooth(pos.xy, z);
    return float2(a, z);
}
// a cor da cena vezes a oclusão; o alfa fica (os blends do jogo leem)
float4 apply(float4 pos : SV_Position) : SV_Target {
    float z;
    float a = Smooth(pos.xy, z);
    float4 c = scene.Load(int3(int2(pos.xy), 0));
    return float4(c.rgb * a, c.a);
}
// depuração: a profundidade crua (realçada) e a distância (faixas a cada 100 unidades)
float4 showz(float4 pos : SV_Position) : SV_Target {
    float d = depth.Load(int3(int2(pos.xy), 0));
    float z = View(pos.xy).z;
    return float4(pow(saturate(d), 256), frac(z / 100), saturate(-z / 1000), 1);
}
// depuração: só a oclusão em cinza
float4 show(float4 pos : SV_Position) : SV_Target {
    float z;
    float a = Smooth(pos.xy, z);
    return float4(a.xxx, 1);
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
