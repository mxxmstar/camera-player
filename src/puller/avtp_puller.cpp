#include "puller/avtp_puller.hpp"

#include "avtp/avtp_packet_parser.h"
#include "log/logger.h"
#include "media/simple_buffer.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {

bool ParseUnsigned(const std::string& text, uint64_t maximum,
                   uint64_t& value) {
    if (text.empty()) {
        return false;
    }

    int base = 10;
    std::string_view view(text);
    if (view.size() > 2 && view[0] == '0' &&
        (view[1] == 'x' || view[1] == 'X')) {
        base = 16;
        view.remove_prefix(2);
    }
    if (view.empty()) {
        return false;
    }

    uint64_t parsed = 0;
    const char* first = view.data();
    const char* last = view.data() + view.size();
    const auto [ptr, ec] = std::from_chars(first, last, parsed, base);
    if (ec != std::errc{} || ptr != last || parsed > maximum) {
        return false;
    }
    value = parsed;
    return true;
}

bool ParseFloat(const std::string& text, float& value) {
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t parsed = 0;
        const float result = std::stof(text, &parsed);
        if (parsed != text.size() || result <= 0.0F) {
            return false;
        }
        value = result;
        return true;
    } catch (...) {
        return false;
    }
}

std::unordered_map<std::string, std::string> ParseQuery(
    const std::string& query) {
    std::unordered_map<std::string, std::string> values;
    std::size_t start = 0;
    while (start <= query.size()) {
        const std::size_t end = query.find('&', start);
        const std::string item = query.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (!item.empty()) {
            const std::size_t equals = item.find('=');
            if (equals == std::string::npos) {
                values[item] = "";
            } else {
                values[item.substr(0, equals)] = item.substr(equals + 1);
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return values;
}

std::string MakePcapUrl(const std::string& device) {
    if (device.empty() || device == "default") {
        return "pcap://default";
    }
    return "pcap://" + device;
}

bool IsJpegStart(const uint8_t* data, std::size_t size) {
    return data && size >= 3 &&
           data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

std::size_t FindStartCode(const uint8_t* data,
                          std::size_t size,
                          std::size_t offset,
                          std::size_t& start_code_size) {
    if (!data) {
        start_code_size = 0;
        return size;
    }
    for (std::size_t i = offset; i + 3 <= size; ++i) {
        if (i + 4 <= size && data[i] == 0 && data[i + 1] == 0 &&
            data[i + 2] == 0 && data[i + 3] == 1) {
            start_code_size = 4;
            return i;
        }
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            start_code_size = 3;
            return i;
        }
    }
    start_code_size = 0;
    return size;
}

CodecType CodecFromFormatSubtype(uint8_t format_subtype) {
    switch (format_subtype) {
        case avtp::kCvfFormatSubtypeMjpeg:
            return CodecType::JPEG;
        case avtp::kCvfFormatSubtypeH264:
            return CodecType::H264;
        case avtp::kCvfFormatSubtypeH265:
            return CodecType::H265;
        default:
            return CodecType::UNKNOWN;
    }
}

bool LooksLikeH265NalHeader(const uint8_t* data,
                            std::size_t size,
                            std::size_t nal_start) {
    if (!data || nal_start + 1 >= size) {
        return false;
    }
    if ((data[nal_start] & 0x80U) != 0) {
        return false;
    }
    if ((data[nal_start] & 0x01U) != 0) {
        return false;
    }
    return (data[nal_start + 1] & 0x07U) != 0;
}

CodecType DetectAnnexBCodec(const uint8_t* data, std::size_t size) {
    if (!avtp::StartsWithAnnexBStartCode(data, size)) {
        return CodecType::UNKNOWN;
    }

    int h264_score = 0;
    int h265_score = 0;
    std::size_t offset = 0;
    int nal_count = 0;
    while (offset < size && nal_count < 8) {
        std::size_t start_code_size = 0;
        const std::size_t start =
            FindStartCode(data, size, offset, start_code_size);
        if (start == size) {
            break;
        }

        const std::size_t nal_start = start + start_code_size;
        if (nal_start >= size) {
            break;
        }

        const uint8_t first = data[nal_start];
        const uint8_t h264_type = static_cast<uint8_t>(first & 0x1FU);
        const uint8_t h265_type = static_cast<uint8_t>((first >> 1) & 0x3FU);
        const bool h265_header =
            LooksLikeH265NalHeader(data, size, nal_start);

        if (h264_type == 7 || h264_type == 8 || h264_type == 5) {
            h264_score += 4;
        } else if (h264_type == 1 || h264_type == 6 || h264_type == 9) {
            h264_score += 1;
        }

        if (h265_header &&
            (h265_type == 32 || h265_type == 33 || h265_type == 34)) {
            h265_score += 5;
        } else if (h265_header &&
                   (h265_type == 19 || h265_type == 20 ||
                    h265_type == 21)) {
            h265_score += 4;
        } else if (h265_header &&
                   (h265_type == 0 || h265_type == 1 ||
                    h265_type == 39 || h265_type == 40)) {
            h265_score += 1;
        }

        ++nal_count;
        offset = nal_start + 1;
    }

    if (h265_score > h264_score) {
        return CodecType::H265;
    }
    if (h264_score > h265_score) {
        return CodecType::H264;
    }
    return CodecType::UNKNOWN;
}

bool ContainsH265KeyNal(const std::vector<uint8_t>& data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        std::size_t start_code_size = 0;
        const std::size_t start =
            FindStartCode(data.data(), data.size(), offset, start_code_size);
        if (start == data.size()) {
            return false;
        }

        const std::size_t nal_start = start + start_code_size;
        if (nal_start >= data.size()) {
            return false;
        }

        const uint8_t nal_type =
            static_cast<uint8_t>((data[nal_start] >> 1) & 0x3FU);
        if (LooksLikeH265NalHeader(data.data(), data.size(), nal_start) &&
            (nal_type == 19 || nal_type == 20 || nal_type == 21)) {
            return true;
        }
        offset = nal_start + 1;
    }
    return false;
}

const char* CodecName(CodecType codec) {
    switch (codec) {
        case CodecType::H264:
            return "H264";
        case CodecType::H265:
            return "H265";
        case CodecType::JPEG:
            return "JPEG";
        default:
            return "UNKNOWN";
    }
}

} // namespace

AvtpPuller::AvtpPuller() {
    stream_info_.media_type = MediaType::VIDEO;
    stream_info_.codec_type = CodecType::UNKNOWN;
    stream_info_.stream_index = 0;
    stream_info_.fps = 25.0F;
}

AvtpPuller::~AvtpPuller() {
    Close();
}

bool AvtpPuller::ParseUrl(const std::string& url, Config& config,
                          std::string& error) {
    Config parsed;
    std::string rest = url;

    constexpr const char* kAvtpScheme = "avtp://";
    constexpr const char* kPcapScheme = "pcap://";
    if (rest.rfind(kAvtpScheme, 0) == 0) {
        rest = rest.substr(std::char_traits<char>::length(kAvtpScheme));
    } else if (rest.rfind(kPcapScheme, 0) == 0) {
        rest = rest.substr(std::char_traits<char>::length(kPcapScheme));
    }

    const std::size_t query_pos = rest.find('?');
    parsed.device = rest.substr(0, query_pos);
    if (parsed.device.empty()) {
        parsed.device = "default";
    }
    const std::string query = query_pos == std::string::npos
                                  ? std::string()
                                  : rest.substr(query_pos + 1);
    const auto values = ParseQuery(query);

    if (const auto it = values.find("src"); it != values.end()) {
        avtp::MacAddress mac{};
        if (!ParseMacAddress(it->second, mac)) {
            error = "invalid AVTP source MAC";
            return false;
        }
        parsed.source_mac = mac;
    }
    if (const auto it = values.find("source"); it != values.end()) {
        avtp::MacAddress mac{};
        if (!ParseMacAddress(it->second, mac)) {
            error = "invalid AVTP source MAC";
            return false;
        }
        parsed.source_mac = mac;
    }
    if (const auto it = values.find("stream"); it != values.end()) {
        uint64_t stream = 0;
        if (!ParseUnsigned(it->second,
                           std::numeric_limits<uint64_t>::max(), stream)) {
            error = "invalid AVTP stream id";
            return false;
        }
        parsed.stream_id = stream;
    }
    if (const auto it = values.find("queue"); it != values.end()) {
        uint64_t queue = 0;
        if (!ParseUnsigned(it->second, 65536, queue) || queue == 0) {
            error = "invalid pcap queue size";
            return false;
        }
        parsed.pcap_queue_size = static_cast<std::size_t>(queue);
    }
    if (const auto it = values.find("read_timeout"); it != values.end()) {
        uint64_t timeout = 0;
        if (!ParseUnsigned(it->second, 60000, timeout)) {
            error = "invalid read timeout";
            return false;
        }
        parsed.read_timeout_ms = static_cast<int>(timeout);
    }
    if (const auto it = values.find("promisc"); it != values.end()) {
        parsed.promiscuous =
            it->second != "0" && it->second != "false" &&
            it->second != "False";
    }
    if (const auto it = values.find("width"); it != values.end()) {
        uint64_t width = 0;
        if (!ParseUnsigned(it->second, 32768, width)) {
            error = "invalid width";
            return false;
        }
        parsed.width = static_cast<int>(width);
    }
    if (const auto it = values.find("height"); it != values.end()) {
        uint64_t height = 0;
        if (!ParseUnsigned(it->second, 32768, height)) {
            error = "invalid height";
            return false;
        }
        parsed.height = static_cast<int>(height);
    }
    if (const auto it = values.find("fps"); it != values.end()) {
        if (!ParseFloat(it->second, parsed.fps)) {
            error = "invalid fps";
            return false;
        }
    }
    if (const auto it = values.find("format"); it != values.end()) {
        if (it->second == "h264" || it->second == "standard" ||
            it->second == "1") {
            parsed.format = PayloadFormat::H264;
        } else if (it->second == "h265" || it->second == "hevc" ||
                   it->second == "3") {
            parsed.format = PayloadFormat::H265;
        } else if (it->second == "jpeg" || it->second == "mjpeg" ||
                   it->second == "custom" || it->second == "vendor" ||
                   it->second == "cater" || it->second == "0") {
            parsed.format = PayloadFormat::Jpeg;
        } else if (it->second == "auto") {
            parsed.format = PayloadFormat::Auto;
        } else {
            error = "invalid format (use auto, h264, h265, jpeg, mjpeg, custom, 0, 1, or 3)";
            return false;
        }
    }

    config = std::move(parsed);
    return true;
}

bool AvtpPuller::ParseMacAddress(const std::string& text,
                                 avtp::MacAddress& mac) {
    std::string hex;
    hex.reserve(12);
    for (char ch : text) {
        if (ch == ':' || ch == '-' || ch == '.') {
            continue;
        }
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        hex.push_back(ch);
    }
    if (hex.size() != 12) {
        return false;
    }
    for (std::size_t i = 0; i < mac.size(); ++i) {
        uint8_t byte = 0;
        const char* first = hex.data() + i * 2;
        const char* last = first + 2;
        unsigned parsed = 0;
        const auto [ptr, ec] = std::from_chars(first, last, parsed, 16);
        if (ec != std::errc{} || ptr != last || parsed > 0xFFU) {
            return false;
        }
        byte = static_cast<uint8_t>(parsed);
        mac[i] = byte;
    }
    return true;
}

std::string AvtpPuller::FormatMacAddress(const avtp::MacAddress& mac) {
    std::ostringstream os;
    os << std::hex;
    for (std::size_t i = 0; i < mac.size(); ++i) {
        if (i != 0) {
            os << ':';
        }
        os.width(2);
        os.fill('0');
        os << static_cast<unsigned>(mac[i]);
    }
    return os.str();
}

std::string AvtpPuller::BuildBpfFilter(const Config& config) {
    std::string filter = "(ether proto 0x22f0 or (vlan and ether proto 0x22f0))";
    if (config.source_mac) {
        filter += " and ether src " + FormatMacAddress(*config.source_mac);
    }
    return filter;
}

bool AvtpPuller::Open(const std::string& url) {
    Close();

    Config parsed;
    std::string error;
    if (!ParseUrl(url, parsed, error)) {
        ReportEvent(error);
        return false;
    }
    config_ = std::move(parsed);

    stream_info_.media_type = MediaType::VIDEO;
    switch (config_.format) {
        case PayloadFormat::H264:
            stream_info_.codec_type = CodecType::H264;
            break;
        case PayloadFormat::H265:
            stream_info_.codec_type = CodecType::H265;
            break;
        case PayloadFormat::Jpeg:
            stream_info_.codec_type = CodecType::JPEG;
            break;
        case PayloadFormat::Auto:
        default:
            stream_info_.codec_type = CodecType::UNKNOWN;
            break;
    }
    stream_info_.stream_index = 0;
    stream_info_.width = config_.width;
    stream_info_.height = config_.height;
    stream_info_.fps = config_.fps;

    assembler_.Reset();
    payload_assembler_.Reset();
    has_codec_stream_ = false;
    codec_stream_id_ = 0;
    codec_source_mac_ = {};
    current_codec_ = CodecType::UNKNOWN;
    raw_packets_ = 0;
    parsed_video_packets_ = 0;
    filtered_packets_ = 0;
    parse_errors_ = 0;
    access_units_ = 0;
    custom_subtype_packets_ = 0;
    custom_subtype_access_units_ = 0;
    h265_access_units_ = 0;
    jpeg_access_units_ = 0;

    pcap_puller_.SetStripEthernetHeader(false);
    pcap_puller_.SetBpfFilter(BuildBpfFilter(config_));
    pcap_puller_.SetMaxQueueSize(config_.pcap_queue_size);
    pcap_puller_.SetReadTimeoutMs(config_.read_timeout_ms);
    pcap_puller_.SetPromiscuous(config_.promiscuous);
    pcap_puller_.SetEventCallback([this](const std::string& message) {
        ReportEvent(message);
    });

    const std::string pcap_url = MakePcapUrl(config_.device);
    if (!pcap_puller_.Open(pcap_url)) {
        return false;
    }

    const char* format_name = "auto";
    switch (config_.format) {
        case PayloadFormat::H264:
            format_name = "h264";
            break;
        case PayloadFormat::H265:
            format_name = "h265";
            break;
        case PayloadFormat::Jpeg:
            format_name = "jpeg";
            break;
        case PayloadFormat::Auto:
        default:
            break;
    }

    LOG_INFO("AVTP listening on {} src={} stream={} format={}",
             config_.device,
             config_.source_mac ? FormatMacAddress(*config_.source_mac) : "*",
             config_.stream_id ? std::to_string(*config_.stream_id) : "*",
             format_name);
    return true;
}

void AvtpPuller::Close() {
    pcap_puller_.Close();
}

bool AvtpPuller::PassesConfiguredFilters(
    const avtp::ParsedCvfPacket& packet) {
    if (config_.source_mac && packet.has_ethernet_header &&
        !avtp::IsSameMac(packet.source_mac, *config_.source_mac)) {
        return false;
    }
    if (config_.stream_id && packet.stream_id != *config_.stream_id) {
        return false;
    }
    return true;
}

bool AvtpPuller::PassesFormatFilter(CodecType codec) const {
    switch (config_.format) {
        case PayloadFormat::H264:
            return codec == CodecType::H264;
        case PayloadFormat::H265:
            return codec == CodecType::H265;
        case PayloadFormat::Jpeg:
            return codec == CodecType::JPEG;
        case PayloadFormat::Auto:
        default:
            return true;
    }
}

bool AvtpPuller::SameCodecStream(const avtp::ParsedCvfPacket& packet) const {
    if (!has_codec_stream_) {
        return false;
    }
    if (packet.stream_id != codec_stream_id_) {
        return false;
    }
    if (packet.has_ethernet_header &&
        !avtp::IsSameMac(packet.source_mac, codec_source_mac_)) {
        return false;
    }
    return true;
}

void AvtpPuller::RememberCodec(const avtp::ParsedCvfPacket& packet,
                               CodecType codec) {
    if (codec == CodecType::UNKNOWN) {
        return;
    }
    has_codec_stream_ = true;
    codec_stream_id_ = packet.stream_id;
    codec_source_mac_ = packet.has_ethernet_header
                            ? packet.source_mac
                            : avtp::ZeroMac();
    if (current_codec_ != CodecType::UNKNOWN && current_codec_ != codec) {
        assembler_.Reset();
        payload_assembler_.Reset();
    }
    if (current_codec_ != codec) {
        LOG_INFO("AVTP stream codec detected: {} subtype=0x{:02x} "
                 "format_subtype=0x{:02x}",
                 CodecName(codec),
                 static_cast<unsigned>(packet.subtype),
                 static_cast<unsigned>(packet.format_subtype));
    }
    current_codec_ = codec;
}

CodecType AvtpPuller::SelectCodec(const avtp::ParsedCvfPacket& packet,
                                  const uint8_t* payload,
                                  std::size_t payload_size) {
    if (IsJpegStart(payload, payload_size)) {
        return CodecType::JPEG;
    }

    const CodecType payload_codec = DetectAnnexBCodec(payload, payload_size);
    if (payload_codec != CodecType::UNKNOWN) {
        return payload_codec;
    }

    if (SameCodecStream(packet) && current_codec_ != CodecType::UNKNOWN) {
        return current_codec_;
    }

    const CodecType format_codec =
        CodecFromFormatSubtype(packet.format_subtype);
    if (format_codec == CodecType::JPEG &&
        !IsJpegStart(payload, payload_size)) {
        return packet.subtype == avtp::kSubtypeCustom
                   ? CodecType::JPEG
                   : CodecType::UNKNOWN;
    }
    if (format_codec != CodecType::UNKNOWN) {
        return format_codec;
    }

    if (packet.subtype == avtp::kSubtypeCustom) {
        return CodecType::JPEG;
    }
    return CodecType::UNKNOWN;
}

std::shared_ptr<MediaPacket> AvtpPuller::MakeMediaPacket(
    avtp::H264AccessUnit access_unit) const {
    auto packet = std::make_shared<MediaPacket>();
    packet->type = MediaType::VIDEO;
    packet->codec = CodecType::H264;
    packet->stream_index = 0;
    packet->pts = access_unit.capture_timestamp_us;
    packet->dts = packet->pts;
    packet->duration = 0;
    packet->time_base = Rational{1, 1000000};
    packet->keyframe = access_unit.keyframe;
    packet->buffer =
        std::make_shared<SimpleBuffer>(std::move(access_unit.data));
    packet->backend = {};
    return packet;
}

std::shared_ptr<MediaPacket> AvtpPuller::MakeMediaPacketFromAccessUnit(
    avtp::AvtpAccessUnit access_unit,
    CodecType codec,
    bool keyframe) const {
    auto packet = std::make_shared<MediaPacket>();
    packet->type = MediaType::VIDEO;
    packet->codec = codec;
    packet->stream_index = 0;
    packet->pts = access_unit.capture_timestamp_us;
    packet->dts = packet->pts;
    packet->duration = 0;
    packet->time_base = Rational{1, 1000000};
    packet->keyframe = keyframe;
    packet->buffer =
        std::make_shared<SimpleBuffer>(std::move(access_unit.data));
    packet->backend = {};
    return packet;
}

bool AvtpPuller::ReadPacket(std::shared_ptr<MediaPacket>& packet) {
    packet.reset();

    while (true) {
        std::shared_ptr<MediaPacket> raw_packet;
        if (!pcap_puller_.ReadPacket(raw_packet)) {
            return false;
        }
        if (!raw_packet) {
            return true;
        }
        if (!raw_packet->buffer || raw_packet->buffer->Size() == 0) {
            ++parse_errors_;
            continue;
        }

        ++raw_packets_;
        avtp::ParsedCvfPacket cvf_packet;
        avtp::ParseError parse_error = avtp::ParseError::None;
        if (!avtp::AvtpPacketParser::Parse(
                raw_packet->buffer->Data(),
                raw_packet->buffer->Size(),
                cvf_packet,
                &parse_error)) {
            if (parse_error == avtp::ParseError::UnsupportedSubtype ||
                parse_error == avtp::ParseError::UnsupportedFormat) {
                ++filtered_packets_;
            } else {
                ++parse_errors_;
            }
            continue;
        }

        if (!PassesConfiguredFilters(cvf_packet)) {
            ++filtered_packets_;
            continue;
        }

        ++parsed_video_packets_;

        const uint8_t* payload = cvf_packet.payload;
        std::size_t payload_size = cvf_packet.payload_size;
        if (cvf_packet.subtype == avtp::kSubtypeCustom) {
            ++custom_subtype_packets_;
            if (cvf_packet.is_custom_format &&
                payload_size > avtp::kCustomPayloadHeaderSize) {
                payload += avtp::kCustomPayloadHeaderSize;
                payload_size -= avtp::kCustomPayloadHeaderSize;
            }
        }

        const CodecType codec = SelectCodec(cvf_packet, payload, payload_size);
        if (codec == CodecType::UNKNOWN) {
            ++filtered_packets_;
            continue;
        }

        if (!PassesFormatFilter(codec)) {
            ++filtered_packets_;
            continue;
        }

        RememberCodec(cvf_packet, codec);

        if (codec != CodecType::H264) {
            avtp::AvtpAccessUnit access_unit;
            const auto result = payload_assembler_.Push(
                cvf_packet, payload, payload_size, raw_packet->pts,
                access_unit);
            if (result ==
                avtp::AvtpPayloadAssembler::Result::AccessUnitReady) {
                const bool keyframe =
                    codec == CodecType::JPEG ||
                    (codec == CodecType::H265 &&
                     ContainsH265KeyNal(access_unit.data));
                if (codec == CodecType::JPEG) {
                    ++jpeg_access_units_;
                } else if (codec == CodecType::H265) {
                    ++h265_access_units_;
                }
                if (cvf_packet.subtype == avtp::kSubtypeCustom) {
                    ++custom_subtype_access_units_;
                }
                ++access_units_;
                packet = MakeMediaPacketFromAccessUnit(
                    std::move(access_unit), codec, keyframe);
                return true;
            }
            continue;
        }

        avtp::H264AccessUnit access_unit;
        const auto result = assembler_.Push(
            cvf_packet, raw_packet->pts, access_unit);
        if (result == avtp::AvtpH264Assembler::Result::AccessUnitReady) {
            ++access_units_;
            packet = MakeMediaPacket(std::move(access_unit));
            return true;
        }
    }
}

StreamInfo AvtpPuller::GetStreamInfo() const {
    return stream_info_;
}

void AvtpPuller::SetEventCallback(EventCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    event_callback_ = std::move(cb);
}

void AvtpPuller::SetReadTimeoutMs(int ms) {
    config_.read_timeout_ms = ms;
    pcap_puller_.SetReadTimeoutMs(ms);
}

void AvtpPuller::ReportEvent(const std::string& message) {
    LOG_ERROR("AvtpPuller: {}", message);
    EventCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = event_callback_;
    }
    if (callback) {
        callback(message);
    }
}

AvtpPuller::Stats AvtpPuller::GetStats() const {
    Stats stats;
    stats.raw_packets = raw_packets_.load();
    stats.parsed_video_packets = parsed_video_packets_.load();
    stats.filtered_packets = filtered_packets_.load();
    stats.parse_errors = parse_errors_.load();
    stats.access_units = access_units_.load();
    stats.custom_subtype_packets = custom_subtype_packets_.load();
    stats.custom_subtype_access_units = custom_subtype_access_units_.load();
    stats.h265_access_units = h265_access_units_.load();
    stats.jpeg_access_units = jpeg_access_units_.load();
    stats.assembler = assembler_.GetStats();
    stats.payload_assembler = payload_assembler_.GetStats();
    return stats;
}
