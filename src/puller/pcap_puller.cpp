#include "puller/pcap_puller.hpp"

#include "log/logger.h"
#include "media/simple_buffer.hpp"
#include "stream/stream_info.h"

#include <chrono>
#include <utility>

namespace {
constexpr int    kPcapSnapLen  = 65536;   ///< 抓包最大长度
constexpr int    kPcapTimeout  = 1000;    ///< pcap_open_live 读超时（ms）
constexpr size_t kEthernetHeaderLen = 14; ///< 以太网头长度
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

    const std::string device = ParseUrl(url);
    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    // 默认网卡：尝试 pcap_lookupdev
    std::string dev_name = device;
    if (dev_name.empty() || dev_name == "default") {
        const char* d = pcap_lookupdev(errbuf);
        if (!d) {
            LOG_ERROR("PcapPuller::Open pcap_lookupdev failed: {}", errbuf);
            if (event_cb_) event_cb_(std::string("pcap_lookupdev: ") + errbuf);
            return false;
        }
        dev_name = d;
    }

    handler_ = pcap_open_live(dev_name.c_str(),
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
        pcap_close(handler_);
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
        pcap_breakloop(handler_);
    }

    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        queue_cv_.notify_all();
    }

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    if (handler_) {
        pcap_close(handler_);
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
    pcap_if_t* alldevs = nullptr;
    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        LOG_ERROR("PcapPuller::ListDevices pcap_findalldevs failed: {}", errbuf);
        return result;
    }

    for (pcap_if_t* d = alldevs; d != nullptr; d = d->next) {
        DeviceInfo info;
        info.name        = d->name ? d->name : "";
        info.description = d->description ? d->description : "";
        result.push_back(std::move(info));
    }

    pcap_freealldevs(alldevs);
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

    bpf_program prog;
    if (pcap_compile(handler_, &prog, bpf_filter_.c_str(), 1,
                     PCAP_NETMASK_UNKNOWN) == -1) {
        LOG_ERROR("PcapPuller::ApplyBpfFilter pcap_compile(\"{}\") failed: {}",
                  bpf_filter_, pcap_geterr(handler_));
        return false;
    }

    if (pcap_setfilter(handler_, &prog) == -1) {
        LOG_ERROR("PcapPuller::ApplyBpfFilter pcap_setfilter failed: {}",
                  pcap_geterr(handler_));
        pcap_freecode(&prog);
        return false;
    }

    pcap_freecode(&prog);
    LOG_INFO("PcapPuller::ApplyBpfFilter applied: {}", bpf_filter_);
    return true;
}

void PcapPuller::CaptureLoop() {
    // pcap_loop 阻塞直到 pcap_breakloop 或出错
    const int ret = pcap_loop(handler_, 0, &PcapPuller::Dispatch,
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
        event_cb_(std::string("pcap_loop error: ") + pcap_geterr(handler_));
    }
}

void PcapPuller::Dispatch(u_char* self, const struct pcap_pkthdr* h,
                          const u_char* pkt) {
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
    packet->type         = MediaType::UNKNOWN;
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
