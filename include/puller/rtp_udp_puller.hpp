#pragma once

#include "puller/i_puller.hpp"
#include "rtp/h264_rtp_depacketizer.h"

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

/// Receives an RTP/H.264 push stream through a bound UDP socket.
///
/// URL example:
/// rtp://0.0.0.0:60000?remote=192.168.66.166&pt=96&recv_buffer=4194304
class RtpUdpPuller : public IPuller {
public:
    struct Stats {
        uint64_t udp_packets{0};
        uint64_t filtered_packets{0};
        uint64_t invalid_rtp_packets{0};
        uint64_t access_units{0};
        uint64_t queue_drops{0};
        rtp::H264RtpDepacketizer::Stats depacketizer;
    };

    RtpUdpPuller();
    ~RtpUdpPuller() override;

    bool Open(const std::string& url) override;
    void Close() override;
    bool ReadPacket(std::shared_ptr<MediaPacket>& packet) override;
    StreamInfo GetStreamInfo() const override;
    void SetEventCallback(EventCallback cb) override;
    void SetReadTimeoutMs(int ms) override;

    Stats GetStats() const;

private:
    struct Config {
        std::string local_address{"0.0.0.0"};
        uint16_t local_port{60000};
        std::string remote_address;
        uint16_t remote_port{0};
        uint8_t payload_type{96};
        int receive_buffer_size{4 * 1024 * 1024};
        std::size_t max_queue_size{64};
    };

    static bool ParseUrl(const std::string& url, Config& config,
                         std::string& error);
    void ReceiveLoop();
    void PushAccessUnit(rtp::H264AccessUnit access_unit);
    int64_t ExtendTimestamp(uint32_t timestamp, uint32_t ssrc);
    void ReportEvent(const std::string& message);

    Config config_;
    StreamInfo stream_info_;

    asio::io_context io_context_;
    std::unique_ptr<asio::ip::udp::socket> socket_;
    std::thread receive_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{true};

    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<std::shared_ptr<MediaPacket>> packet_queue_;
    int read_timeout_ms_{100};

    rtp::H264RtpDepacketizer depacketizer_;
    bool has_extended_timestamp_{false};
    uint32_t timestamp_ssrc_{0};
    uint32_t last_timestamp_{0};
    uint64_t extended_timestamp_{0};

    mutable std::mutex callback_mutex_;
    EventCallback event_callback_;

    mutable std::mutex depacketizer_mutex_;
    std::atomic<uint64_t> udp_packets_{0};
    std::atomic<uint64_t> filtered_packets_{0};
    std::atomic<uint64_t> invalid_rtp_packets_{0};
    std::atomic<uint64_t> access_units_{0};
    std::atomic<uint64_t> queue_drops_{0};
};
