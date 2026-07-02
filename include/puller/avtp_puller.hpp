#pragma once

#include "avtp/avtp_h264_assembler.h"
#include "avtp/avtp_payload_assembler.h"
#include "puller/i_puller.hpp"
#include "puller/pcap_puller.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

/// @brief AVTP 拉流器
/// 从本地 Npcap 接口接收 AVTP 数据包，并输出已组帧的 MediaPacket。
///
/// 支持的格式：
///   - subtype=0x03 的私有 CAM/JPEG 分片（卡特、我司）
///   - subtype=0x02 的 CVF H.264/H.265/JPEG
///   - format_subtype 仅作为 hint，payload/NAL 特征优先
///
/// URL 示例：
///   avtp://default?src=aa:87:26:53:bb:6c
///   avtp://\\Device\\NPF_{GUID}?src=aa:87:26:53:bb:6c&queue=1024
///   avtp://default?src=02:aa:bb:cc:dd:ee&format=auto
///
/// 该数据获取器设计为被动模式：它仅监听 EtherType 0x22f0 流量，
/// 而不发送任何摄像机启动/停止控制消息。
class AvtpPuller : public IPuller {
public:
    /// @brief 拉流器统计信息
    struct Stats {
        uint64_t raw_packets{0};     ///< 从 Npcap 接口接收的原始数据包数
        uint64_t parsed_video_packets{0};   ///< 解析后的视频数据包数
        uint64_t filtered_packets{0};   ///< 过滤后的数据包数
        uint64_t parse_errors{0};   ///< 解析错误包数
        uint64_t custom_subtype_packets{0};
        uint64_t custom_subtype_access_units{0};
        uint64_t h265_access_units{0};
        uint64_t jpeg_access_units{0};
        uint64_t access_units{0};   ///< 总访问单元数量
        avtp::AvtpH264Assembler::Stats assembler;   ///< H.264 访问单元组装器统计信息
        avtp::AvtpPayloadAssembler::Stats payload_assembler;
    };

    AvtpPuller();
    ~AvtpPuller() override;

    bool Open(const std::string& url) override;
    void Close() override;
    bool ReadPacket(std::shared_ptr<MediaPacket>& packet) override;
    StreamInfo GetStreamInfo() const override;
    void SetEventCallback(EventCallback cb) override;
    void SetReadTimeoutMs(int ms) override;

    Stats GetStats() const;

private:
    enum class PayloadFormat {
        Auto,
        H264,
        H265,
        Jpeg,
    };

    struct Config {
        std::string device{"default"};
        std::optional<avtp::MacAddress> source_mac;
        std::optional<uint64_t> stream_id;
        PayloadFormat format{PayloadFormat::Auto};
        std::size_t pcap_queue_size{1024};
        int read_timeout_ms{100};
        bool promiscuous{true};
        int width{0};
        int height{0};
        float fps{25.0F};
    };

    static bool ParseUrl(const std::string& url, Config& config,
                         std::string& error);
    static std::string BuildBpfFilter(const Config& config);
    static bool ParseMacAddress(const std::string& text,
                                avtp::MacAddress& mac);
    static std::string FormatMacAddress(const avtp::MacAddress& mac);

    bool PassesConfiguredFilters(const avtp::ParsedCvfPacket& packet);
    bool PassesFormatFilter(CodecType codec) const;
    CodecType SelectCodec(const avtp::ParsedCvfPacket& packet,
                          const uint8_t* payload,
                          std::size_t payload_size);
    void RememberCodec(const avtp::ParsedCvfPacket& packet, CodecType codec);
    bool SameCodecStream(const avtp::ParsedCvfPacket& packet) const;
    std::shared_ptr<MediaPacket> MakeMediaPacket(
        avtp::H264AccessUnit access_unit) const;
    std::shared_ptr<MediaPacket> MakeMediaPacketFromAccessUnit(
        avtp::AvtpAccessUnit access_unit,
        CodecType codec,
        bool keyframe) const;
    void ReportEvent(const std::string& message);

    Config config_;
    StreamInfo stream_info_;
    PcapPuller pcap_puller_;
    avtp::AvtpH264Assembler assembler_;
    avtp::AvtpPayloadAssembler payload_assembler_;
    bool has_codec_stream_{false};
    uint64_t codec_stream_id_{0};
    avtp::MacAddress codec_source_mac_{};
    CodecType current_codec_{CodecType::UNKNOWN};

    mutable std::mutex callback_mutex_;
    EventCallback event_callback_;

    std::atomic<uint64_t> raw_packets_{0};
    std::atomic<uint64_t> parsed_video_packets_{0};
    std::atomic<uint64_t> filtered_packets_{0};
    std::atomic<uint64_t> parse_errors_{0};
    std::atomic<uint64_t> access_units_{0};
    std::atomic<uint64_t> custom_subtype_packets_{0};
    std::atomic<uint64_t> custom_subtype_access_units_{0};
    std::atomic<uint64_t> h265_access_units_{0};
    std::atomic<uint64_t> jpeg_access_units_{0};
};
