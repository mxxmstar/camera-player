#pragma once

#include <cstddef>
#include <cstdint>

namespace rtp {

/// A non-owning view over one RTP datagram.
struct ParsedRtpPacket {
    bool marker{false};
    uint8_t payload_type{0};
    uint16_t sequence_number{0};
    uint32_t timestamp{0};
    uint32_t ssrc{0};
    const uint8_t* payload{nullptr};
    std::size_t payload_size{0};
};

/// Parses the fixed and optional RTP header fields defined by RFC 3550.
class RtpPacketParser {
public:
    static bool Parse(const uint8_t* data, std::size_t size,
                      ParsedRtpPacket& packet);
};

} // namespace rtp
