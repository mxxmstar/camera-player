#pragma once

#include "puller/i_puller.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// pcap 头文件（由 third_party/pcap/Include 提供）
#include <pcap.h>

/// @brief 基于 libpcap/Npcap 的网卡捕获拉流器
///
/// 实现 IPuller 接口，将指定网卡的数据包捕获后封装为 MediaPacket。
///
/// 工作模型：
///   - Open(url)     打开网卡（url 格式见 ParseUrl）
///   - 内部专用线程跑 pcap_loop，将包放入有界队列
///   - ReadPacket()  从队列取包返回（带超时，与 StreamSession.ReadLoop 兼容）
///   - Close()       pcap_breakloop + 线程 join
///
/// 线程模型：
///   - capture_thread_ 生产 packet_queue_
///   - ReadPacket 在 asio io_context 线程上消费（StreamSession 通过 asio::post 串行调度）
///   - 队列保护：queue_mutex_ + queue_cv_
///
/// URL 格式（参考 StreamSession.SetUrl 的 url 参数）：
///   - "pcap://<device_name>"   指定网卡，如 "pcap://\\Device\\NPF_{GUID}"
///   - "<device_name>"          直接网卡名（不带前缀）
///   - "pcap://default"         使用 pcap_lookupdev 默认网卡
///   - ""                       空字符串，使用默认网卡
class PcapPuller : public IPuller {
public:
    /// @brief 网卡设备信息
    struct DeviceInfo {
        std::string name;         ///< 设备名（pcap 接口名，传给 Open）
        std::string description;  ///< 可读描述
    };

    PcapPuller();
    ~PcapPuller() override;

    // ==================== IPuller ====================

    bool Open(const std::string& url) override;
    void Close() override;
    bool ReadPacket(std::shared_ptr<MediaPacket>& packet) override;
    StreamInfo GetStreamInfo() const override;
    void SetEventCallback(EventCallback cb) override;

    // ==================== PcapPuller 扩展 ====================

    /// @brief 枚举所有网卡设备（静态工具）
    /// @return 设备列表（失败返回空，并写日志）
    static std::vector<DeviceInfo> ListDevices();

    /// @brief 设置 BPF 过滤表达式（Open 前调用）
    /// @param filter BPF 表达式，如 "udp port 5004"、"ether proto 0x22"、"ip"
    void SetBpfFilter(const std::string& filter) { bpf_filter_ = filter; }

    /// @brief 设置是否剥离以太网头（14 字节，默认 true）
    void SetStripEthernetHeader(bool strip) { strip_ethernet_ = strip; }

    /// @brief 设置 ReadPacket 等待超时
    /// @param ms 超时毫秒；0=非阻塞返回；-1=阻塞直到有包或停止
    /// @note 超时返回时 ok=true 但 packet=nullptr，StreamSession 视为"跳过本轮"
    void SetReadTimeoutMs(int ms) { read_timeout_ms_ = ms; }

    /// @brief 设置内部队列最大长度（默认 500，超出后丢弃最旧包）
    void SetMaxQueueSize(size_t size) { max_queue_size_ = size; }

    /// @brief 设置混杂模式（默认 true）
    void SetPromiscuous(bool enable) { promisc_ = enable; }

private:
    // ==================== 内部 ====================

    /// @brief 解析 URL，提取网卡名
    /// @return 网卡名；若为默认网卡则返回空
    static std::string ParseUrl(const std::string& url);

    /// @brief 应用 BPF 过滤器
    bool ApplyBpfFilter();

    /// @brief capture_thread_ 入口
    void CaptureLoop();

    /// @brief pcap_loop 回调入口（C 风格）
    static void Dispatch(u_char* self, const struct pcap_pkthdr* h,
                         const u_char* pkt);

    /// @brief pcap_loop 回调处理（实例方法）
    void HandlePacket(const struct pcap_pkthdr* h, const u_char* pkt);

    // ── pcap 资源 ──
    pcap_t* handler_{nullptr};
    std::thread capture_thread_;
    std::atomic<bool> running_{false};

    // ── 配置 ──
    std::string bpf_filter_;          ///< BPF 过滤表达式
    bool        strip_ethernet_{true};///< 是否剥离以太网头
    int         read_timeout_ms_{100};///< ReadPacket 等待超时
    size_t      max_queue_size_{500}; ///< 队列上限
    bool        promisc_{true};       ///< 混杂模式

    // ── 队列（capture_thread → ReadPacket） ──
    std::mutex              queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<std::shared_ptr<MediaPacket>> packet_queue_;
    std::atomic<bool>       stopped_{false};   ///< Close 后置位，用于唤醒等待
    std::atomic<uint64_t>   dropped_count_{0}; ///< 队列满丢弃计数

    // ── 缓存与回调 ──
    StreamInfo    cached_info_;
    EventCallback event_cb_;
};
