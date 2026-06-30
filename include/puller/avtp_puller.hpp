#pragma once

#include "avtp/avtp_h264_assembler.h"
#include "puller/i_puller.hpp"
#include "puller/pcap_puller.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

/// Receives AVTP/CVF/H.264 packets from a local Npcap interface and emits
/// Annex-B H.264 access units as MediaPacket objects.
///
/// URL examples:
///   avtp://default?src=aa:87:26:53:bb:6c
///   avtp://\\Device\\NPF_{GUID}?src=aa:87:26:53:bb:6c&queue=1024
///
/// This puller is intentionally passive: it listens for EtherType 0x22f0 and
/// does not send any camera start/stop control messages.
class AvtpPuller : public IPuller {
public:
    struct Stats {
        uint64_t raw_packets{0};
        uint64_t parsed_video_packets{0};
        uint64_t filtered_packets{0};
        uint64_t parse_errors{0};
        uint64_t access_units{0};
        avtp::AvtpH264Assembler::Stats assembler;
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
    struct Config {
        std::string device{"default"};
        std::optional<avtp::MacAddress> source_mac;
        std::optional<uint64_t> stream_id;
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
    std::shared_ptr<MediaPacket> MakeMediaPacket(
        avtp::H264AccessUnit access_unit) const;
    void ReportEvent(const std::string& message);

    Config config_;
    StreamInfo stream_info_;
    PcapPuller pcap_puller_;
    avtp::AvtpH264Assembler assembler_;

    mutable std::mutex callback_mutex_;
    EventCallback event_callback_;

    std::atomic<uint64_t> raw_packets_{0};
    std::atomic<uint64_t> parsed_video_packets_{0};
    std::atomic<uint64_t> filtered_packets_{0};
    std::atomic<uint64_t> parse_errors_{0};
    std::atomic<uint64_t> access_units_{0};
};
