#pragma once

#include "avtp/avtp_packet_parser.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace avtp {

struct H264AccessUnit {
    std::vector<uint8_t> data;
    uint64_t stream_id{0};
    MacAddress source_mac{};
    uint8_t sequence_num{0};
    uint32_t avtp_timestamp{0};
    bool timestamp_valid{false};
    int64_t capture_timestamp_us{0};
    bool keyframe{false};
};

class AvtpH264Assembler {
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
        uint64_t waiting_for_start_packets{0};
        uint64_t payload_bytes{0};
    };

    explicit AvtpH264Assembler(
        std::size_t max_access_unit_size = 8 * 1024 * 1024);

    Result Push(const ParsedCvfPacket& packet,
                int64_t capture_timestamp_us,
                H264AccessUnit& output);

    void Reset();
    const Stats& GetStats() const { return stats_; }

private:
    bool Append(const uint8_t* data, std::size_t size);
    void ResetAccessUnit();
    void CacheParameterSetsFromAccessUnit(const std::vector<uint8_t>& data);
    bool SameStream(const ParsedCvfPacket& packet) const;
    void SwitchStream(const ParsedCvfPacket& packet);
    Result DropCurrentAccessUnit(bool malformed);
    H264AccessUnit FinishAccessUnit(const ParsedCvfPacket& packet,
                                    int64_t capture_timestamp_us);

    std::size_t max_access_unit_size_;
    std::vector<uint8_t> access_unit_;
    bool has_stream_{false};
    uint64_t current_stream_id_{0};
    MacAddress current_source_mac_{};
    bool has_expected_sequence_{false};
    uint8_t expected_sequence_{0};
    bool dropping_until_marker_{false};

    std::vector<uint8_t> cached_sps_;
    std::vector<uint8_t> cached_pps_;
    Stats stats_;
};

bool StartsWithAnnexBStartCode(const uint8_t* data, std::size_t size);
bool ContainsNalType(const std::vector<uint8_t>& data, uint8_t nal_type);

} // namespace avtp
