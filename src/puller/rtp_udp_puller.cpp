#include "puller/rtp_udp_puller.hpp"

#include "log/logger.h"
#include "media/simple_buffer.hpp"
#include "rtp/rtp_packet_parser.h"

#include <array>
#include <chrono>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace {

bool ParseUnsigned(const std::string& text, uint64_t maximum,
                   uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t parsed = 0;
        const unsigned long long result =
            std::stoull(text, &parsed, 10);
        if (parsed != text.size() || result > maximum) {
            return false;
        }
        value = static_cast<uint64_t>(result);
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

} // namespace

RtpUdpPuller::RtpUdpPuller() {
    stream_info_.media_type = MediaType::VIDEO;
    stream_info_.codec_type = CodecType::H264;
    stream_info_.stream_index = 0;
}

RtpUdpPuller::~RtpUdpPuller() {
    Close();
}

bool RtpUdpPuller::ParseUrl(const std::string& url, Config& config,
                            std::string& error) {
    constexpr const char* kScheme = "rtp://";
    if (url.rfind(kScheme, 0) != 0) {
        error = "RTP URL must start with rtp://";
        return false;
    }

    const std::string rest = url.substr(6);
    const std::size_t query_pos = rest.find('?');
    const std::string authority = rest.substr(0, query_pos);
    const std::string query = query_pos == std::string::npos
        ? std::string()
        : rest.substr(query_pos + 1);

    const std::size_t colon = authority.rfind(':');
    if (colon == std::string::npos) {
        error = "RTP URL must include a local UDP port";
        return false;
    }

    config.local_address = authority.substr(0, colon);
    if (config.local_address.empty() || config.local_address == "*") {
        config.local_address = "0.0.0.0";
    }

    uint64_t number = 0;
    if (!ParseUnsigned(authority.substr(colon + 1), 65535, number) ||
        number == 0) {
        error = "invalid local UDP port";
        return false;
    }
    config.local_port = static_cast<uint16_t>(number);

    const auto values = ParseQuery(query);
    if (const auto it = values.find("remote"); it != values.end()) {
        config.remote_address = it->second;
    }
    if (const auto it = values.find("remote_port"); it != values.end()) {
        if (!ParseUnsigned(it->second, 65535, number)) {
            error = "invalid remote_port";
            return false;
        }
        config.remote_port = static_cast<uint16_t>(number);
    }
    if (const auto it = values.find("pt"); it != values.end()) {
        if (!ParseUnsigned(it->second, 127, number)) {
            error = "invalid RTP payload type";
            return false;
        }
        config.payload_type = static_cast<uint8_t>(number);
    }
    if (const auto it = values.find("recv_buffer"); it != values.end()) {
        if (!ParseUnsigned(
                it->second,
                static_cast<uint64_t>(std::numeric_limits<int>::max()),
                number) ||
            number < 64 * 1024) {
            error = "recv_buffer must be at least 65536 bytes";
            return false;
        }
        config.receive_buffer_size = static_cast<int>(number);
    }
    if (const auto it = values.find("queue"); it != values.end()) {
        if (!ParseUnsigned(it->second, 4096, number) || number == 0) {
            error = "invalid queue size";
            return false;
        }
        config.max_queue_size = static_cast<std::size_t>(number);
    }

    asio::error_code ec;
    (void)asio::ip::make_address(config.local_address, ec);
    if (ec) {
        error = "invalid local address: " + config.local_address;
        return false;
    }
    if (!config.remote_address.empty()) {
        (void)asio::ip::make_address(config.remote_address, ec);
        if (ec) {
            error = "invalid remote address: " + config.remote_address;
            return false;
        }
    }
    return true;
}

bool RtpUdpPuller::Open(const std::string& url) {
    Close();

    Config parsed;
    std::string error;
    if (!ParseUrl(url, parsed, error)) {
        LOG_ERROR("RtpUdpPuller::Open: {}", error);
        ReportEvent(error);
        return false;
    }
    config_ = std::move(parsed);

    asio::error_code ec;
    const asio::ip::address local_address =
        asio::ip::make_address(config_.local_address, ec);
    if (ec) {
        ReportEvent("invalid local address: " + ec.message());
        return false;
    }

    socket_ = std::make_unique<asio::ip::udp::socket>(io_context_);
    socket_->open(local_address.is_v6()
                      ? asio::ip::udp::v6()
                      : asio::ip::udp::v4(),
                  ec);
    if (ec) {
        ReportEvent("UDP socket open failed: " + ec.message());
        socket_.reset();
        return false;
    }

    socket_->set_option(
        asio::socket_base::receive_buffer_size(config_.receive_buffer_size),
        ec);
    if (ec) {
        LOG_WARN("RtpUdpPuller receive buffer request failed: {}",
                 ec.message());
        ec.clear();
    }

    socket_->bind(
        asio::ip::udp::endpoint(local_address, config_.local_port), ec);
    if (ec) {
        ReportEvent("UDP bind failed: " + ec.message());
        socket_->close();
        socket_.reset();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::queue<std::shared_ptr<MediaPacket>> empty;
        packet_queue_.swap(empty);
    }
    {
        std::lock_guard<std::mutex> lock(depacketizer_mutex_);
        depacketizer_.Reset();
    }
    has_extended_timestamp_ = false;
    udp_packets_ = 0;
    filtered_packets_ = 0;
    invalid_rtp_packets_ = 0;
    access_units_ = 0;
    queue_drops_ = 0;

    stopped_ = false;
    running_ = true;
    receive_thread_ = std::thread(&RtpUdpPuller::ReceiveLoop, this);

    LOG_INFO(
        "RTP/H264 listening on {}:{}, remote={}, PT={}, SO_RCVBUF={}",
        config_.local_address, config_.local_port,
        config_.remote_address.empty() ? "*" : config_.remote_address,
        static_cast<unsigned>(config_.payload_type),
        config_.receive_buffer_size);
    return true;
}

void RtpUdpPuller::Close() {
    running_ = false;
    stopped_ = true;

    if (socket_) {
        asio::error_code ignored;
        socket_->cancel(ignored);
        socket_->close(ignored);
    }
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    socket_.reset();

    queue_cv_.notify_all();
}

bool RtpUdpPuller::ReadPacket(std::shared_ptr<MediaPacket>& packet) {
    packet.reset();
    std::unique_lock<std::mutex> lock(queue_mutex_);

    const auto ready = [this]() {
        return !packet_queue_.empty() || stopped_.load();
    };
    if (read_timeout_ms_ < 0) {
        queue_cv_.wait(lock, ready);
    } else if (read_timeout_ms_ > 0) {
        (void)queue_cv_.wait_for(
            lock, std::chrono::milliseconds(read_timeout_ms_), ready);
    }

    if (!packet_queue_.empty()) {
        packet = std::move(packet_queue_.front());
        packet_queue_.pop();
        return true;
    }
    return !stopped_.load();
}

StreamInfo RtpUdpPuller::GetStreamInfo() const {
    return stream_info_;
}

void RtpUdpPuller::SetEventCallback(EventCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    event_callback_ = std::move(cb);
}

void RtpUdpPuller::SetReadTimeoutMs(int ms) {
    read_timeout_ms_ = ms;
}

void RtpUdpPuller::ReportEvent(const std::string& message) {
    LOG_ERROR("RtpUdpPuller: {}", message);
    EventCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = event_callback_;
    }
    if (callback) {
        callback(message);
    }
}

int64_t RtpUdpPuller::ExtendTimestamp(uint32_t timestamp, uint32_t ssrc) {
    if (!has_extended_timestamp_ || timestamp_ssrc_ != ssrc) {
        has_extended_timestamp_ = true;
        timestamp_ssrc_ = ssrc;
        last_timestamp_ = timestamp;
        extended_timestamp_ = timestamp;
        return static_cast<int64_t>(extended_timestamp_);
    }

    const uint32_t delta = timestamp - last_timestamp_;
    extended_timestamp_ += delta;
    last_timestamp_ = timestamp;
    return static_cast<int64_t>(extended_timestamp_);
}

void RtpUdpPuller::PushAccessUnit(rtp::H264AccessUnit access_unit) {
    auto packet = std::make_shared<MediaPacket>();
    packet->type = MediaType::VIDEO;
    packet->codec = CodecType::H264;
    packet->stream_index = 0;
    packet->pts = ExtendTimestamp(
        access_unit.timestamp, access_unit.ssrc);
    packet->dts = packet->pts;
    packet->duration = 0;
    packet->time_base = Rational{1, 90000};
    packet->keyframe = access_unit.keyframe;
    packet->buffer =
        std::make_shared<SimpleBuffer>(std::move(access_unit.data));
    packet->backend = {};

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (packet_queue_.size() >= config_.max_queue_size) {
            packet_queue_.pop();
            ++queue_drops_;
        }
        packet_queue_.push(std::move(packet));
    }
    ++access_units_;
    queue_cv_.notify_one();
}

void RtpUdpPuller::ReceiveLoop() {
    std::array<uint8_t, 65536> datagram{};
    while (running_) {
        asio::ip::udp::endpoint sender;
        asio::error_code ec;
        const std::size_t size = socket_->receive_from(
            asio::buffer(datagram), sender, 0, ec);
        if (ec) {
            if (running_ && ec != asio::error::operation_aborted) {
                ReportEvent("UDP receive failed: " + ec.message());
            }
            break;
        }
        ++udp_packets_;

        if (!config_.remote_address.empty() &&
            sender.address().to_string() != config_.remote_address) {
            ++filtered_packets_;
            continue;
        }
        if (config_.remote_port != 0 &&
            sender.port() != config_.remote_port) {
            ++filtered_packets_;
            continue;
        }

        rtp::ParsedRtpPacket parsed;
        if (!rtp::RtpPacketParser::Parse(
                datagram.data(), size, parsed)) {
            ++invalid_rtp_packets_;
            continue;
        }
        if (parsed.payload_type != config_.payload_type) {
            ++filtered_packets_;
            continue;
        }

        rtp::H264AccessUnit access_unit;
        rtp::H264RtpDepacketizer::Result result;
        {
            std::lock_guard<std::mutex> lock(depacketizer_mutex_);
            result = depacketizer_.Push(parsed, access_unit);
        }
        if (result ==
            rtp::H264RtpDepacketizer::Result::AccessUnitReady) {
            PushAccessUnit(std::move(access_unit));
        }
    }

    stopped_ = true;
    queue_cv_.notify_all();
}

RtpUdpPuller::Stats RtpUdpPuller::GetStats() const {
    Stats stats;
    stats.udp_packets = udp_packets_.load();
    stats.filtered_packets = filtered_packets_.load();
    stats.invalid_rtp_packets = invalid_rtp_packets_.load();
    stats.access_units = access_units_.load();
    stats.queue_drops = queue_drops_.load();
    {
        std::lock_guard<std::mutex> lock(depacketizer_mutex_);
        stats.depacketizer = depacketizer_.GetStats();
    }
    return stats;
}
