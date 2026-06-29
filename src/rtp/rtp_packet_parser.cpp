#include "rtp/rtp_packet_parser.h"

namespace rtp {
namespace {

uint16_t ReadBigEndian16(const uint8_t* data) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[0]) << 8) |
        static_cast<uint16_t>(data[1]));
}

uint32_t ReadBigEndian32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

} // namespace

bool RtpPacketParser::Parse(const uint8_t* data, std::size_t size,
                            ParsedRtpPacket& packet) {
    packet = {};
    if (!data || size < 12) {
        return false;
    }

    const uint8_t first = data[0];
    const uint8_t version = first >> 6;
    if (version != 2) {
        return false;
    }

    const bool has_padding = (first & 0x20U) != 0;
    const bool has_extension = (first & 0x10U) != 0;
    const std::size_t csrc_count = first & 0x0FU;

    std::size_t header_size = 12 + csrc_count * 4;
    if (header_size > size) {
        return false;
    }

    if (has_extension) {
        if (header_size + 4 > size) {
            return false;
        }
        const std::size_t extension_words =
            ReadBigEndian16(data + header_size + 2);
        const std::size_t extension_size = 4 + extension_words * 4;
        if (extension_size > size - header_size) {
            return false;
        }
        header_size += extension_size;
    }

    std::size_t payload_end = size;
    if (has_padding) {
        const std::size_t padding_size = data[size - 1];
        if (padding_size == 0 || padding_size > size - header_size) {
            return false;
        }
        payload_end -= padding_size;
    }

    if (payload_end <= header_size) {
        return false;
    }

    packet.marker = (data[1] & 0x80U) != 0;
    packet.payload_type = data[1] & 0x7FU;
    packet.sequence_number = ReadBigEndian16(data + 2);
    packet.timestamp = ReadBigEndian32(data + 4);
    packet.ssrc = ReadBigEndian32(data + 8);
    packet.payload = data + header_size;
    packet.payload_size = payload_end - header_size;
    return true;
}

} // namespace rtp
