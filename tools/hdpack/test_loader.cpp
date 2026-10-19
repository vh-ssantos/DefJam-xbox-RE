// Synthetic integration test for the runtime loader; no game files or generated game textures.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "core/log.h"
#include "gpu/hd_textures.h"

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;
using namespace std::chrono_literals;
static std::atomic<unsigned> bad_png_logs{0};

// The test links the production loader, with a small logger instead of the rest of the runtime.
namespace logx {
void Write(Level, const char* format, ...) {
    if (std::strstr(format, "invalid or missing PNG")) bad_png_logs.fetch_add(1);
}
}

static void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void Hr(HRESULT result, const char* message) { Check(SUCCEEDED(result), message); }
template<class F> static void Until(F&& ready, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error(message);
        std::this_thread::sleep_for(1ms);
    }
}
struct TempPack {
    fs::path root = fs::temp_directory_path() /
        (L"defjam-hd-loader-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    TempPack() { Check(fs::create_directory(root), "Cannot create unique synthetic pack directory"); }
    ~TempPack() { std::error_code error; fs::remove_all(root, error); }
};
struct Com {
    Com() { Hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "COM initialization failed"); }
    ~Com() { CoUninitialize(); }
};
static void Png(IWICImagingFactory* factory, const fs::path& path, uint32_t width, uint32_t height,
                const std::vector<uint8_t>& bgra) {
    fs::create_directories(path.parent_path());
    ComPtr<IWICStream> stream;
    Hr(factory->CreateStream(&stream), "Cannot create PNG stream");
    Hr(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "Cannot create synthetic PNG");
    ComPtr<IWICBitmapEncoder> encoder;
    Hr(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "Cannot create PNG encoder");
    Hr(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "Cannot initialize PNG encoder");
    ComPtr<IWICBitmapFrameEncode> frame;
    Hr(encoder->CreateNewFrame(&frame, nullptr), "Cannot create PNG frame");
    Hr(frame->Initialize(nullptr), "Cannot initialize PNG frame");
    Hr(frame->SetSize(width, height), "Cannot set PNG size");
    auto format = GUID_WICPixelFormat32bppBGRA;
    Hr(frame->SetPixelFormat(&format), "Cannot select BGRA PNG");
    Check(IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA), "PNG encoder changed channel format");
    Hr(frame->WritePixels(height, width * 4, UINT(bgra.size()), const_cast<BYTE*>(bgra.data())), "PNG encoding failed");
    Hr(frame->Commit(), "PNG frame commit failed");
    Hr(encoder->Commit(), "PNG commit failed");
}

int main() {
    try {
        Com com;
        ComPtr<IWICImagingFactory> factory;
        Hr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)),
            "WIC factory initialization failed");
        TempPack fixture;
        const std::string raw(64, '1'), decoded(64, '2'), bad(64, '3'), escape(64, '4'), absolute(64, '5');
        const std::string backwards(64, '6'), legacy(64, '7'), opacity(64, '8'), missing(64, '9');
        const std::string prefix = "textures/lanczos3-material-v3-4x/";
        const std::string path = prefix + std::string(64, 'a') + ".png";
        const std::string bad_path = prefix + std::string(64, 'b') + ".png";
        const std::string opacity_path = prefix + std::string(64, 'c') + ".png";
        // Unequal color channels and zero alpha ensure conversion/mips cannot hide channel swaps
        // or accidentally discard color data by premultiplying the material's alpha channel.
        const std::vector<uint8_t> pixels = {
            8, 16, 24, 0,  40, 48, 56, 0,
            72, 80, 88, 0,  104, 112, 120, 255};
        Png(factory.Get(), fixture.root / path, 2, 2, pixels);
        Png(factory.Get(), fixture.root / opacity_path, 2, 2, pixels);
        // A missing policy defaults to independent channels; declared opacity uses alpha-weighted RGB.
        std::ofstream(fixture.root / "policies.tsv", std::ios::binary) << std::string(64, 'c') << "\topacity\r\n";
        std::ofstream(fixture.root / bad_path, std::ios::binary) << "this is not a PNG";
        std::ofstream index(fixture.root / "index.tsv", std::ios::binary);
        index << raw << '\t' << path << "\r\n" << decoded << '\t' << path << '\n'
              << bad << '\t' << bad_path << '\n'
              << opacity << '\t' << opacity_path << '\n'
              << escape << '\t' << prefix << "../" << std::string(64, 'a') << ".png\n"
              << absolute << '\t' << "C:/outside.png\n"
              << legacy << '\t' << "textures/lanczos3-premult-v1-4x/" << std::string(64, 'a') << ".png\n"
              << backwards << '\t' << "textures\\lanczos3-material-v3-4x\\" << std::string(64, 'a') << ".png\n";
        index.close();

        gpu::hd::Loader loader;
        std::string canonical;
        Check(!loader.Get(raw, decoded, canonical), "Unstarted loader must keep the original texture");
        loader.Start(fixture.root);
        // The first matching Get queues work and must immediately return the original texture.
        Until([&] {
            auto image = loader.Get(missing, decoded, canonical);
            Check(!image, "First indexed request unexpectedly blocked for the PNG");
            return !canonical.empty();
        }, "Pack index did not become ready");
        Check(canonical == path, "Decoded fallback did not resolve the canonical pack path");
        std::shared_ptr<const gpu::hd::Image> image;
        Until([&] { image = loader.Get(missing, decoded, canonical); return bool(image); }, "Async PNG never became ready");
        Check(image->width == 2 && image->height == 2 && image->levels.size() == 2, "PNG size or mip count is wrong");
        Check(image->levels[0] == pixels, "BGRA/straight-alpha PNG round trip changed pixel data");
        Check(image->levels[1] == std::vector<uint8_t>({56, 64, 72, 64}), "Mips must average all four channels independently");
        Check(image->Bytes() == 20, "Image byte accounting omitted a mip level");
        auto raw_image = loader.Get(raw, "", canonical);
        Check(raw_image == image && canonical == path, "Raw and decoded aliases did not share the same image");
        std::puts("PASS async original fallback, decoded/raw aliases, BGRA round trip and independent-channel mips");

        std::shared_ptr<const gpu::hd::Image> opacity_image;
        Until([&] { opacity_image = loader.Get(opacity, "", canonical); return bool(opacity_image); },
            "Opacity PNG never became ready");
        Check(canonical == opacity_path && opacity_image->levels[0] == pixels, "Opacity policy changed base pixels");
        Check(opacity_image->levels[1] == std::vector<uint8_t>({104, 112, 120, 64}),
            "Opacity mips must weight RGB by alpha while averaging alpha independently");
        std::puts("PASS declared opacity uses alpha-weighted mips; missing policy keeps independent channels");

        for (const auto& rejected : {escape, absolute, backwards, legacy, missing}) {
            canonical.clear();
            Check(!loader.Get(rejected, "", canonical) && canonical.empty(), "Unsafe/absent index entry was accepted");
        }
        std::puts("PASS traversal, absolute, obsolete recipe and noncanonical paths rejected");

        loader.Consumed(path);
        Check(!loader.Get(raw, "", canonical), "Consumed image should reload asynchronously on request");
        std::shared_ptr<const gpu::hd::Image> reloaded;
        Until([&] { reloaded = loader.Get(raw, "", canonical); return bool(reloaded); }, "Consumed PNG did not reload");
        Check(reloaded != image && reloaded->levels[0] == pixels, "Consumed image was not released and reconstructed");
        Check(image->levels[0] == pixels, "Previously returned shared image was invalidated by consumption");
        std::puts("PASS consumed image reloads while existing shared image stays valid");

        canonical.clear();
        Check(!loader.Get(bad, "", canonical) && canonical == bad_path, "Invalid PNG must queue and keep the original texture");
        Until([&] { return bad_png_logs.load() != 0; }, "Invalid PNG was not processed by the worker");
        Png(factory.Get(), fixture.root / bad_path, 2, 2, pixels);
        for (int i = 0; i < 10; ++i)
            Check(!loader.Get(bad, "", canonical), "Invalid PNG failure should remain cached for this session");
        Check(loader.Get(raw, "", canonical) == reloaded, "Invalid PNG stopped the worker's other requests");
        std::puts("PASS invalid PNG falls back safely and caches failure");
        std::puts("HD loader integration tests passed (synthetic temporary PNGs only).");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "HD loader test FAILED: %s\n", error.what());
        return 1;
    }
}
