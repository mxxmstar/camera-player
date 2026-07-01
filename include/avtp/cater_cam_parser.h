#pragma once

#include "avtp/avtp_packet_parser.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace avtp {

/// Cater CAM (卡特CAM) custom AVTP payload format.
///
/// This format uses format_subtype=0x00 and contains raw encrypted H.264 data
/// without standard NAL unit structure. The payload is passed through as-is.
///
/// Characteristics:
///   - Stream ID: typically 0x0123456789abcdef
///   - Format: 0x02 (RFC), Format Subtype: 0x00 (custom)
///   - Payload: raw encrypted data, no Annex-B start codes
///   - Marker bit indicates end of access unit
struct CaterCamPacket {
    uint64_t stream_id{0};
    uint8_t sequence_num{0};
    uint32_t avtp_timestamp{0};
    bool timestamp_valid{false};
    bool marker{false};
    uint8_t event{0};
    MacAddress source_mac{};

    const uint8_t* payload{nullptr};
    std::size_t payload_size{0};
};

class CaterCamParser {
public:
    /// Parse a Cater CAM packet from an already-parsed AVTP packet.
    /// Returns true if the packet matches Cater CAM format characteristics.
    static bool Parse(const ParsedCvfPacket& cvf, CaterCamPacket& output);

    /// Check if a parsed CVF packet matches Cater CAM format.
    static bool IsCaterCamFormat(const ParsedCvfPacket& cvf);
};

} // namespace avtp
