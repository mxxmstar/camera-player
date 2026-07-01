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

} // namespace

AvtpPuller::AvtpPuller() {
    stream_info_.media_type = MediaType::VIDEO;
    stream_info_.codec_type = CodecType::H264;
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
    stream_info_.codec_type = CodecType::H264;
    stream_info_.stream_index = 0;
    stream_info_.width = config_.width;
    stream_info_.height = config_.height;
    stream_info_.fps = config_.fps;

    assembler_.Reset();
    raw_packets_ = 0;
    parsed_video_packets_ = 0;
    filtered_packets_ = 0;
    parse_errors_ = 0;
    access_units_ = 0;

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

    LOG_INFO("AVTP/CVF/H264 listening on {} src={} stream={}",
             config_.device,
             config_.source_mac ? FormatMacAddress(*config_.source_mac) : "*",
             config_.stream_id ? std::to_string(*config_.stream_id) : "*");
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
    stats.assembler = assembler_.GetStats();
    return stats;
}
