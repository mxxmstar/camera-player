#pragma once

#include <cstdint>
#include <vector>

#include "rtp/rtp_packet_parser.h"

namespace rtp {

struct H264AccessUnit {
    std::vector<uint8_t> data;
    uint32_t timestamp{0};
    uint32_t ssrc{0};
    bool keyframe{false};
};

/// Reassembles H.264 RTP payloads into Annex-B access units.
///
/// Supported packetization modes:
///   - Single NAL units (types 1..23)
///   - STAP-A (type 24)
///   - FU-A (type 28)
class H264RtpDepacketizer {
public:
    enum class Result {
        NeedMore,
        AccessUnitReady,
        Dropped,
        Malformed,
    };

    struct Stats {
        uint64_t packets{0};
        uint64_t access_units{0};
        uint64_t dropped_access_units{0};
        uint64_t lost_packets{0};
        uint64_t out_of_order_packets{0};
        uint64_t malformed_packets{0};
    };

    explicit H264RtpDepacketizer(
        std::size_t max_access_unit_size = 8 * 1024 * 1024);

    Result Push(const ParsedRtpPacket& packet, H264AccessUnit& output);
    void Reset();
    const Stats& GetStats() const { return stats_; }

private:
    bool Append(const uint8_t* data, std::size_t size);
    bool AppendStartCode();
    void ResetAccessUnit();
    Result DropCurrentTimestamp(const ParsedRtpPacket& packet,
                                bool malformed);
    bool PacketCanStartUnit(const ParsedRtpPacket& packet) const;
    Result HandleSingleNal(const ParsedRtpPacket& packet);
    Result HandleStapA(const ParsedRtpPacket& packet);
    Result HandleFuA(const ParsedRtpPacket& packet);
    Result FinishIfMarked(const ParsedRtpPacket& packet,
                          H264AccessUnit& output);
    void CacheParameterSetsFromAccessUnit();

    std::size_t max_access_unit_size_;
    std::vector<uint8_t> access_unit_;
    bool keyframe_{false};
    bool fu_active_{false};
    bool has_timestamp_{false};
    uint32_t current_timestamp_{0};
    bool dropping_timestamp_{false};
    uint32_t dropped_timestamp_{0};

    std::vector<uint8_t> cached_sps_;
    std::vector<uint8_t> cached_pps_;

    bool has_ssrc_{false};
    uint32_t current_ssrc_{0};
    bool has_expected_sequence_{false};
    uint16_t expected_sequence_{0};
    Stats stats_;
};

} // namespace rtp
