#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace avtp {

constexpr uint16_t kEtherTypeAvtp = 0x22F0;
constexpr uint8_t kSubtypeCvf = 0x03;
constexpr uint8_t kCvfFormatRfc = 0x02;
constexpr uint8_t kCvfFormatSubtypeH264 = 0x01;
constexpr uint8_t kCvfFormatSubtypeCustom = 0x00;
constexpr std::size_t kCvfHeaderSize = 24;

// Custom payload format constants (我司CAM)
constexpr uint32_t kCustomPayloadMagic = 0x415DA05A;
constexpr std::size_t kCustomPayloadHeaderSize = 8; // 4-byte length + 4-byte magic

using MacAddress = std::array<uint8_t, 6>;

enum class ParseError {
    None,
    TooShort,
    UnsupportedEtherType,
    UnsupportedSubtype,
    UnsupportedVersion,
    UnsupportedFormat,
    StreamDataLengthTooLarge,
};

struct ParsedCvfPacket {
    bool has_ethernet_header{false};
    MacAddress destination_mac{};
    MacAddress source_mac{};
    uint16_t ether_type{0};
    std::size_t avtp_offset{0};

    uint8_t subtype{0};
    bool stream_id_valid{false};
    uint8_t version{0};
    bool media_clock_restart{false};
    bool timestamp_valid{false};
    uint8_t sequence_num{0};
    bool timestamp_uncertain{false};
    uint64_t stream_id{0};
    uint32_t avtp_timestamp{0};

    uint8_t format{0};
    uint8_t format_subtype{0};
    uint16_t stream_data_length{0};
    bool h264_payload_timestamp_valid{false};
    bool marker{false};
    uint8_t event{0};

    const uint8_t* payload{nullptr};
    std::size_t payload_size{0};

    // Custom payload format fields (我司CAM)
    bool is_custom_format{false};
    uint32_t custom_payload_length{0};
    uint32_t custom_magic{0};
    uint32_t custom_rtp_timestamp{0};
    uint32_t custom_ssrc{0};
    const uint8_t* custom_rtp_payload{nullptr};
    std::size_t custom_rtp_payload_size{0};
};

class AvtpPacketParser {
public:
    static bool Parse(const uint8_t* data, std::size_t size,
                      ParsedCvfPacket& packet,
                      ParseError* error = nullptr);

    static bool ParseCvfPdu(const uint8_t* data, std::size_t size,
                            ParsedCvfPacket& packet,
                            ParseError* error = nullptr);

    static const char* ErrorToString(ParseError error);

private:
    static bool ParseEthernetPrefix(const uint8_t* data, std::size_t size,
                                    ParsedCvfPacket& packet,
                                    std::size_t& avtp_offset,
                                    ParseError* error);
};

bool IsSameMac(const MacAddress& lhs, const MacAddress& rhs);
MacAddress ZeroMac();

} // namespace avtp
