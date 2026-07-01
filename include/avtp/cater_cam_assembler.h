#pragma once

#include "avtp/cater_cam_parser.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace avtp {

/// Cater CAM access unit assembled from multiple packets.
struct CaterCamAccessUnit {
    std::vector<uint8_t> data;
    uint64_t stream_id{0};
    MacAddress source_mac{};
    uint8_t sequence_num{0};
    uint32_t avtp_timestamp{0};
    bool timestamp_valid{false};
    int64_t capture_timestamp_us{0};
};

/// Assembles Cater CAM packets into access units.
///
/// Cater CAM packets contain raw encrypted H.264 data. The assembler
/// collects packets until the marker bit is set, indicating the end
/// of an access unit.
class CaterCamAssembler {
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
        uint64_t payload_bytes{0};
    };

    explicit CaterCamAssembler(
        std::size_t max_access_unit_size = 8 * 1024 * 1024);

    Result Push(const CaterCamPacket& packet,
                int64_t capture_timestamp_us,
                CaterCamAccessUnit& output);

    void Reset();
    const Stats& GetStats() const { return stats_; }

private:
    bool Append(const uint8_t* data, std::size_t size);
    void ResetAccessUnit();
    bool SameStream(const CaterCamPacket& packet) const;
    void SwitchStream(const CaterCamPacket& packet);
    Result FinishAccessUnit(const CaterCamPacket& packet,
                            int64_t capture_timestamp_us,
                            CaterCamAccessUnit& output);
    Result DropCurrentAccessUnit(bool malformed);

    std::size_t max_access_unit_size_;
    std::vector<uint8_t> access_unit_;
    bool has_stream_{false};
    uint64_t current_stream_id_{0};
    MacAddress current_source_mac_{};
    bool has_expected_sequence_{false};
    uint8_t expected_sequence_{0};
    Stats stats_;
};

} // namespace avtp