#include "puller/pcap_puller.hpp"

#include "log/logger.h"
#include "media/simple_buffer.hpp"
#include "stream/stream_info.h"

#include <array>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr int    kPcapSnapLen  = 65536;   ///< 抓包最大长度
constexpr int    kPcapTimeout  = 1000;    ///< pcap_open_live 读超时（ms）
constexpr size_t kEthernetHeaderLen = 14; ///< 以太网头长度

struct PcapApi {
    using findalldevs_fn = int(__cdecl*)(pcap_if_t**, char*);
    using freealldevs_fn = void(__cdecl*)(pcap_if_t*);
    using lookupdev_fn = char*(__cdecl*)(char*);
    using open_live_fn = pcap_t*(__cdecl*)(const char*, int, int, int, char*);
    using compile_fn = int(__cdecl*)(pcap_t*, bpf_program*, const char*, int,
                                     bpf_u_int32);
    using setfilter_fn = int(__cdecl*)(pcap_t*, bpf_program*);
    using freecode_fn = void(__cdecl*)(bpf_program*);
    using loop_fn = int(__cdecl*)(pcap_t*, int, pcap_handler, u_char*);
    using breakloop_fn = void(__cdecl*)(pcap_t*);
    using close_fn = void(__cdecl*)(pcap_t*);
    using geterr_fn = char*(__cdecl*)(pcap_t*);

    bool EnsureLoaded();
    const std::string& LastError() const { return last_error_; }
    const std::string& RuntimePath() const { return runtime_path_; }

    findalldevs_fn findalldevs{nullptr};
    freealldevs_fn freealldevs{nullptr};
    lookupdev_fn lookupdev{nullptr};
    open_live_fn open_live{nullptr};
    compile_fn compile{nullptr};
    setfilter_fn setfilter{nullptr};
    freecode_fn freecode{nullptr};
    loop_fn loop{nullptr};
    breakloop_fn breakloop{nullptr};
    close_fn close{nullptr};
    geterr_fn geterr{nullptr};

private:
    /// @brief 解析所有函数符号    
    bool ResolveFunctions();

#ifdef _WIN32
    static std::vector<std::string> CandidateRuntimePaths();
    static std::string LastWindowsError(DWORD error);
    HMODULE module_{nullptr};
#endif
    bool tried_{false};
    bool loaded_{false};
    std::string runtime_path_;
    std::string last_error_;
};

PcapApi& GetPcapApi() {
    static PcapApi api;
    return api;
}

/// @brief 解析pcap函数指针
/// @tparam Fn 函数指针类型
/// @param module pcap模块句柄
/// @param name 函数名
/// @param[out] target 函数指针
/// @param[out] error 错误信息
/// @return 是否成功解析
template <typename Fn>
bool ResolvePcapFunction(
#ifdef _WIN32
    HMODULE module,
#endif
    const char* name,
    Fn& target,
    std::string& error) {
#ifdef _WIN32
    // 找到name对应的函数指针,并转换为正确的类型赋值给target引用
    target = reinterpret_cast<Fn>(GetProcAddress(module, name));    // 从dll中获取函数指针
    if (!target) {
        error = std::string("missing pcap symbol: ") + name;
        return false;
    }
    return true;
#else
    (void)name;
    (void)target;
    (void)error;
    return false;
#endif
}

bool PcapApi::ResolveFunctions() {
    return ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_findalldevs", findalldevs, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_freealldevs", freealldevs, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_lookupdev", lookupdev, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_open_live", open_live, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_compile", compile, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_setfilter", setfilter, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_freecode", freecode, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_loop", loop, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_breakloop", breakloop, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_close", close, last_error_) &&
           ResolvePcapFunction(
#ifdef _WIN32
               module_,
#endif
               "pcap_geterr", geterr, last_error_);
}

#ifdef _WIN32
std::string PcapApi::LastWindowsError(DWORD error) {
    if (error == 0) {
        return {};
    }
    LPSTR buffer = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr);
    std::string message =
        size != 0 && buffer ? std::string(buffer, size)
                            : std::string("Win32 error ") +
                                  std::to_string(error);
    if (buffer) {
        LocalFree(buffer);
    }
    while (!message.empty() &&
           (message.back() == '\r' || message.back() == '\n' ||
            message.back() == ' ')) {
        message.pop_back();
    }
    return message;
}

std::vector<std::string> PcapApi::CandidateRuntimePaths() {
    std::vector<std::string> paths;

    std::array<char, MAX_PATH> windows_dir{};
    const UINT len = GetWindowsDirectoryA(
        windows_dir.data(), static_cast<UINT>(windows_dir.size()));
    if (len > 0 && len < windows_dir.size()) {
        std::string base = windows_dir.data();
        if constexpr (sizeof(void*) == 8) {
            paths.push_back(base + "\\System32\\Npcap\\wpcap.dll");
        } else {
            paths.push_back(base + "\\SysWOW64\\Npcap\\wpcap.dll");
        }
    }

    // Fallback for developer machines that put wpcap.dll on PATH or beside
    // the executable. The installed Npcap path above is preferred because
    // app-local WinPcap-compatible DLLs can enumerate zero adapters.
    paths.emplace_back("wpcap.dll");
    return paths;
}
#endif

bool PcapApi::EnsureLoaded() {
    if (loaded_) {
        return true;
    }
    if (tried_) {
        return false;
    }
    tried_ = true;

#ifdef _WIN32
    std::string errors;
    for (const std::string& path : CandidateRuntimePaths()) {
        HMODULE module = nullptr;
        if (path.find('\\') != std::string::npos ||
            path.find('/') != std::string::npos) {
            module = LoadLibraryExA(
                path.c_str(),
                nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                    LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        }
        if (!module) {
            module = LoadLibraryA(path.c_str());
        }

        if (!module) {
            errors += path + ": " + LastWindowsError(GetLastError()) + "; ";
            continue;
        }

        module_ = module;
        runtime_path_ = path;
        if (ResolveFunctions()) {
            loaded_ = true;
            LOG_INFO("Pcap runtime loaded from {}", runtime_path_);
            return true;
        }

        errors += path + ": " + last_error_ + "; ";
        FreeLibrary(module_);
        module_ = nullptr;
        runtime_path_.clear();
    }
    last_error_ = errors.empty() ? "failed to load wpcap.dll" : errors;
    return false;
#else
    last_error_ = "dynamic pcap loading is only implemented on Windows";
    return false;
#endif
}
} // namespace

// ── ctor / dtor ────────────────────────────────────────────────────

PcapPuller::PcapPuller() = default;

PcapPuller::~PcapPuller() {
    Close();
}

// ── IPuller ─────────────────────────────────────────────────────────

bool PcapPuller::Open(const std::string& url) {
    if (handler_) {
        LOG_WARN("PcapPuller::Open already opened, close first");
        Close();
    }

    PcapApi& api = GetPcapApi();
    if (!api.EnsureLoaded()) {
        LOG_ERROR("PcapPuller::Open failed to load pcap runtime: {}",
                  api.LastError());
        if (event_cb_) {
            event_cb_(std::string("load pcap runtime: ") + api.LastError());
        }
        return false;
    }

    const std::string device = ParseUrl(url);
    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    // 默认网卡：尝试 pcap_lookupdev
    std::string dev_name = device;
    if (dev_name.empty() || dev_name == "default") {
        const char* d = api.lookupdev(errbuf);
        if (!d) {
            LOG_ERROR("PcapPuller::Open pcap_lookupdev failed: {}", errbuf);
            if (event_cb_) event_cb_(std::string("pcap_lookupdev: ") + errbuf);
            return false;
        }
        dev_name = d;
    }

    handler_ = api.open_live(dev_name.c_str(),
                             kPcapSnapLen,
                             promisc_ ? 1 : 0,
                             kPcapTimeout,
                             errbuf);
    if (!handler_) {
        LOG_ERROR("PcapPuller::Open pcap_open_live({}) failed: {}", dev_name, errbuf);
        if (event_cb_) event_cb_(std::string("pcap_open_live: ") + errbuf);
        return false;
    }

    if (errbuf[0] != 0) {
        // pcap_open_live 在 errbuf 中写入警告但仍返回句柄
        LOG_WARN("PcapPuller::Open pcap_open_live warning: {}", errbuf);
    }

    if (!ApplyBpfFilter()) {
        // 应用过滤器失败：关闭句柄并返回
        api.close(handler_);
        handler_ = nullptr;
        return false;
    }

    // 填充 cached_info_（pcap 无明确编码格式，标 UNKNOWN）
    cached_info_.media_type   = MediaType::UNKNOWN;
    cached_info_.codec_type   = CodecType::UNKNOWN;
    cached_info_.stream_index = 0;

    // 启动 capture_thread_（pcap_loop 阻塞在专用线程）
    stopped_.store(false);
    running_.store(true);
    capture_thread_ = std::thread(&PcapPuller::CaptureLoop, this);

    LOG_INFO("PcapPuller::Open success on device: {} (filter=\"{}\")",
             dev_name, bpf_filter_);
    return true;
}

void PcapPuller::Close() {
    stopped_.store(true);
    running_.store(false);

    if (handler_) {
        PcapApi& api = GetPcapApi();
        if (api.EnsureLoaded()) {
            api.breakloop(handler_);
        }
    }

    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        queue_cv_.notify_all();
    }

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    if (handler_) {
        PcapApi& api = GetPcapApi();
        if (api.EnsureLoaded()) {
            api.close(handler_);
        }
        handler_ = nullptr;
    }

    std::queue<std::shared_ptr<MediaPacket>> empty;
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        std::swap(packet_queue_, empty);
    }

    LOG_INFO("PcapPuller::Close done (dropped={})", dropped_count_.load());
}

bool PcapPuller::ReadPacket(std::shared_ptr<MediaPacket>& packet) {
    packet.reset();

    if (!handler_ && !running_.load()) {
        // 已关闭
        return false;
    }

    std::unique_lock<std::mutex> lk(queue_mutex_);

    if (read_timeout_ms_ < 0) {
        // 阻塞直到有包或停止
        queue_cv_.wait(lk, [this]() {
            return !packet_queue_.empty() || stopped_.load();
        });
    } else if (read_timeout_ms_ == 0) {
        // 非阻塞
        if (packet_queue_.empty()) {
            // 无包：返回 ok=true, packet=nullptr（StreamSession 视为跳过本轮）
            return true;
        }
    } else {
        // 带超时等待
        bool has_data = queue_cv_.wait_for(lk,
            std::chrono::milliseconds(read_timeout_ms_),
            [this]() { return !packet_queue_.empty() || stopped_.load(); });
        if (!has_data || packet_queue_.empty()) {
            // 超时无包：返回 ok=true, packet=nullptr
            return true;
        }
    }

    if (packet_queue_.empty()) {
        // 被 stopped_ 唤醒且无包
        return false;
    }

    packet = std::move(packet_queue_.front());
    packet_queue_.pop();
    return true;
}

StreamInfo PcapPuller::GetStreamInfo() const {
    return cached_info_;
}

void PcapPuller::SetEventCallback(EventCallback cb) {
    event_cb_ = std::move(cb);
}

// ── PcapPuller 扩展 ────────────────────────────────────────────────

std::vector<PcapPuller::DeviceInfo> PcapPuller::ListDevices() {
    std::vector<DeviceInfo> result;
    PcapApi& api = GetPcapApi();
    if (!api.EnsureLoaded()) {
        LOG_ERROR("PcapPuller::ListDevices failed to load pcap runtime: {}",
                  api.LastError());
        return result;
    }

    pcap_if_t* alldevs = nullptr;
    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    if (api.findalldevs(&alldevs, errbuf) == -1) {
        LOG_ERROR("PcapPuller::ListDevices pcap_findalldevs failed: {}", errbuf);
        return result;
    }

    for (pcap_if_t* d = alldevs; d != nullptr; d = d->next) {
        DeviceInfo info;
        info.name        = d->name ? d->name : "";
        info.description = d->description ? d->description : "";
        result.push_back(std::move(info));
    }

    api.freealldevs(alldevs);
    return result;
}

// ── 内部 ────────────────────────────────────────────────────────────

std::string PcapPuller::ParseUrl(const std::string& url) {
    const std::string kPrefix = "pcap://";
    if (url.compare(0, kPrefix.size(), kPrefix) == 0) {
        std::string rest = url.substr(kPrefix.size());
        if (rest == "default" || rest.empty()) {
            return "";
        }
        return rest;
    }
    // 不带前缀直接当网卡名
    return url;
}

bool PcapPuller::ApplyBpfFilter() {
    if (bpf_filter_.empty()) {
        return true;
    }
    if (!handler_) {
        return false;
    }
    PcapApi& api = GetPcapApi();
    if (!api.EnsureLoaded()) {
        LOG_ERROR("PcapPuller::ApplyBpfFilter pcap runtime unavailable: {}",
                  api.LastError());
        return false;
    }

    bpf_program prog;
    if (api.compile(handler_, &prog, bpf_filter_.c_str(), 1,
                    PCAP_NETMASK_UNKNOWN) == -1) {
        LOG_ERROR("PcapPuller::ApplyBpfFilter pcap_compile(\"{}\") failed: {}",
                  bpf_filter_, api.geterr(handler_));
        return false;
    }

    if (api.setfilter(handler_, &prog) == -1) {
        LOG_ERROR("PcapPuller::ApplyBpfFilter pcap_setfilter failed: {}",
                  api.geterr(handler_));
        api.freecode(&prog);
        return false;
    }

    api.freecode(&prog);
    LOG_INFO("PcapPuller::ApplyBpfFilter applied: {}", bpf_filter_);
    return true;
}

void PcapPuller::CaptureLoop() {
    // pcap_loop 阻塞直到 pcap_breakloop 或出错
    PcapApi& api = GetPcapApi();
    if (!api.EnsureLoaded()) {
        LOG_ERROR("PcapPuller::CaptureLoop pcap runtime unavailable: {}",
                  api.LastError());
        stopped_.store(true);
        queue_cv_.notify_all();
        return;
    }

    const int ret = api.loop(handler_, 0, &PcapPuller::Dispatch,
                             reinterpret_cast<u_char*>(this));
    LOG_INFO("PcapPuller::CaptureLoop exit, ret={} (dropped={})",
             ret, dropped_count_.load());

    // 通知 ReadPacket 唤醒
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        stopped_.store(true);
        queue_cv_.notify_all();
    }

    if (ret == -1 && event_cb_) {
        event_cb_(std::string("pcap_loop error: ") + api.geterr(handler_));
    }
}

void PcapPuller::Dispatch(u_char* self, const struct pcap_pkthdr* h, const u_char* pkt) {
    auto* p = reinterpret_cast<PcapPuller*>(self);
    p->HandlePacket(h, pkt);
}

void PcapPuller::HandlePacket(const struct pcap_pkthdr* h, const u_char* pkt) {
    if (!running_.load() || h->caplen == 0) {
        return;
    }

    // 计算载荷范围
    const u_char* payload = pkt;
    size_t payload_len    = h->caplen;

    if (strip_ethernet_ && h->caplen >= kEthernetHeaderLen) {
        payload     = pkt + kEthernetHeaderLen;
        payload_len = h->caplen - kEthernetHeaderLen;
    }

    if (payload_len == 0) {
        return;
    }

    // 构造 MediaPacket（pcap 无明确编码，标 UNKNOWN；上层做协议解析）
    auto packet = std::make_shared<MediaPacket>();
    packet->type         = MediaType::PCAP;
    packet->codec        = CodecType::UNKNOWN;
    packet->stream_index = 0;
    // 使用 pcap 时间戳（微秒）
    packet->pts      = static_cast<int64_t>(h->ts.tv_sec) * 1000000LL
                     + static_cast<int64_t>(h->ts.tv_usec);
    packet->dts      = packet->pts;
    packet->duration = 0;
    packet->time_base = Rational{1, 1000000};
    packet->keyframe  = false;

    packet->buffer = std::make_shared<SimpleBuffer>(payload, payload_len);
    packet->backend.type = BackendHandle::NONE;
    packet->backend.ptr  = nullptr;

    // 入队（有界队列，满则丢弃最旧）
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        if (packet_queue_.size() >= max_queue_size_) {
            packet_queue_.pop();
            dropped_count_.fetch_add(1);
        }
        packet_queue_.push(std::move(packet));
    }
    queue_cv_.notify_one();
}
