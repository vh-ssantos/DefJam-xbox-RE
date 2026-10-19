// (DefJam) PNG is lossless (including alpha), portable and smaller on disk than uncompressed DDS.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_set>
#include "gpu/hd_textures.h"
#include "core/log.h"

namespace gpu::hd {
namespace {
constexpr size_t kCpuBudget = 128ull << 20;
constexpr size_t kQueueLimit = 64;
bool HexId(const std::string& s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
std::shared_ptr<Image> Read(IWICImagingFactory* factory, const std::filesystem::path& path, bool opacity) {
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> convert;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &decoder)) || FAILED(decoder->GetFrame(0, &frame))) return {};
    auto image = std::make_shared<Image>();
    if (FAILED(frame->GetSize(&image->width, &image->height))) return {};
    uint32_t w = image->width, h = image->height;
    // Bound allocation before invoking WIC. Our supported originals are at most 1024 square (4x = 4096).
    if (!w || !h || w > 4096 || h > 4096 || uint64_t(w) * h * 4 > (64ull << 20)) return {};
    if (FAILED(factory->CreateFormatConverter(&convert)) || FAILED(convert->Initialize(frame.Get(),
        GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return {};
    image->levels.emplace_back(size_t(w) * h * 4);
    if (FAILED(convert->CopyPixels(nullptr, w * 4, UINT(image->levels[0].size()), image->levels[0].data()))) return {};
    // The generator records an explicit policy: material alpha is data, opacity alpha weights RGB.
    // Missing policy preserves every channel; no material semantics are inferred from pixel values.
    while (w > 1 || h > 1) {
        uint32_t nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
        const auto& source = image->levels.back();
        std::vector<uint8_t> next(size_t(nw) * nh * 4);
        for (uint32_t y = 0; y < nh; ++y) for (uint32_t x = 0; x < nw; ++x) {
            uint32_t sums[3] = {}, alpha = 0, count = 0;
            for (uint32_t sy = y * h / nh; sy < (y + 1) * h / nh; ++sy)
                for (uint32_t sx = x * w / nw; sx < (x + 1) * w / nw; ++sx) {
                    const auto* p = &source[(size_t(sy) * w + sx) * 4];
                    alpha += p[3]; ++count;
                    for (int c = 0; c < 3; ++c) sums[c] += uint32_t(p[c]) * (opacity ? p[3] : 1u);
                }
            auto* out = &next[(size_t(y) * nw + x) * 4];
            uint32_t divisor = opacity ? alpha : count;
            for (int c = 0; c < 3; ++c) out[c] = divisor ? uint8_t((sums[c] + divisor / 2) / divisor) : 0;
            out[3] = uint8_t((alpha + count / 2) / count);
        }
        image->levels.push_back(std::move(next)); w = nw; h = nh;
    }
    return image;
}
} // namespace

size_t Image::Bytes() const { size_t n = 0; for (const auto& level : levels) n += level.size(); return n; }
Loader::~Loader() {
    { std::lock_guard lock(mutex_); stop_ = true; }
    wake_.notify_one();
    if (worker_.joinable()) worker_.join();
}
void Loader::Start(std::filesystem::path root) {
    root_ = std::move(root);
    worker_ = std::thread([this] { Work(); });
}
std::shared_ptr<const Image> Loader::Get(const std::string& id, const std::string& decoded, std::string& canonical) {
    std::lock_guard lock(mutex_);
    if (!indexed_) return {};
    auto it = index_.find(id);
    if (it == index_.end() && !decoded.empty()) it = index_.find(decoded);
    if (it == index_.end()) return {};
    canonical = it->second;
    auto found = entries_.find(canonical);
    if (found != entries_.end()) { found->second.used = ++clock_; return found->second.image; }
    if (queue_.size() >= kQueueLimit) return {};
    entries_.emplace(canonical, Entry{Status::Queued, {}, ++clock_});
    queue_.push_back(canonical); wake_.notify_one();
    return {};
}
void Loader::Consumed(const std::string& canonical) {
    std::lock_guard lock(mutex_);
    auto it = entries_.find(canonical);
    if (it != entries_.end() && it->second.status == Status::Ready) {
        bytes_ -= it->second.image->Bytes(); entries_.erase(it);
    }
}
void Loader::Work() {
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(com) || FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)))) { if (SUCCEEDED(com)) CoUninitialize(); return; }
    std::unordered_map<std::string, std::string> index;
    std::unordered_set<std::string> opacity;
    std::ifstream policies(root_ / "policies.tsv");
    std::string policy;
    while (std::getline(policies, policy)) {
        if (!policy.empty() && policy.back() == '\r') policy.pop_back();
        if (policy.size() == 72 && policy.substr(64) == "\topacity" && HexId(policy.substr(0, 64)))
            opacity.insert(policy.substr(0, 64));
    }
    std::ifstream file(root_ / "index.tsv");
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string id = line.substr(0, tab), path = line.substr(tab + 1);
        // Generated names only: index entries cannot escape the pack or select arbitrary files.
        const std::string p2 = "textures/lanczos3-material-v3-2x/", p4 = "textures/lanczos3-material-v3-4x/";
        size_t prefix = path.starts_with(p2) ? p2.size() : path.starts_with(p4) ? p4.size() : 0;
        if (HexId(id) && prefix && path.size() == prefix + 68 &&
            HexId(path.substr(prefix, 64)) && path.substr(prefix + 64) == ".png") index.emplace(id, path);
    }
    { std::lock_guard lock(mutex_); index_ = std::move(index); indexed_ = true; }
    LOGI("hd: index ready, %zu texture identities", index_.size());
    for (;;) {
        std::string path;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_) break;
            path = std::move(queue_.front()); queue_.pop_front();
        }
        auto image = Read(factory.Get(), root_ / path, opacity.contains(std::filesystem::path(path).stem().string()));
        std::lock_guard lock(mutex_);
        auto& entry = entries_[path];
        entry.status = image ? Status::Ready : Status::Bad;
        if (!image) { LOG_FIRST(8, "hd: invalid or missing PNG %s", path.c_str()); continue; }
        while (bytes_ + image->Bytes() > kCpuBudget) {
            auto victim = entries_.end();
            for (auto it = entries_.begin(); it != entries_.end(); ++it)
                if (it->first != path && it->second.status == Status::Ready &&
                    (victim == entries_.end() || it->second.used < victim->second.used)) victim = it;
            if (victim == entries_.end()) break;
            bytes_ -= victim->second.image->Bytes(); entries_.erase(victim);
        }
        // Unordered-map erase doesn't invalidate the surviving entry reference.
        bytes_ += image->Bytes(); entry.image = std::move(image);
    }
    factory.Reset(); CoUninitialize();
}
} // namespace gpu::hd
