#pragma once
// (DefJam) Optional local PNG pack. Disk access, PNG decode and mip generation never run on the render thread.
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace gpu::hd {
struct Image {
    uint32_t width = 0, height = 0;
    std::vector<std::vector<uint8_t>> levels; // tight, straight-alpha BGRA8
    size_t Bytes() const;
};
class Loader {
public:
    ~Loader();
    void Start(std::filesystem::path root);
    // Empty until the index/image is ready. Bounded queue; misses always use the original texture.
    std::shared_ptr<const Image> Get(const std::string& id, const std::string& decoded, std::string& canonical);
    void Consumed(const std::string& canonical);
private:
    enum class Status { Queued, Ready, Bad };
    struct Entry { Status status = Status::Queued; std::shared_ptr<Image> image; uint64_t used = 0; };
    void Work();
    std::filesystem::path root_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_ = false, indexed_ = false;
    uint64_t clock_ = 0;
    size_t bytes_ = 0;
    std::unordered_map<std::string, std::string> index_;
    std::unordered_map<std::string, Entry> entries_;
    std::deque<std::string> queue_;
};
} // namespace gpu::hd
