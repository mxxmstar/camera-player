#include "rtp/rtpsender.h"
#include "log/logger.h"
#include <cstring>
#include <asio.hpp>
namespace rtp {
AsioTcpRtpSender::AsioTcpRtpSender(asio::ip::tcp::socket& socket)
    : socket_(socket) 
{
    
}

int AsioTcpRtpSender::Send(MediaChannelId channel, const RtpPacket& pkt) {
    if (pkt.data == nullptr || pkt.size <= RTP_TCP_HEAD_SIZE) {
        return -1;
    }

    size_t payload_size = pkt.size - RTP_TCP_HEAD_SIZE;
    std::vector<uint8_t> buffer(payload_size + 4);
    uint8_t* ptr = buffer.data();
    
    ptr[0] = '$';
    ptr[1] = static_cast<uint8_t>(channel);
    ptr[2] = static_cast<uint8_t>((payload_size & 0xFF00) >> 8);
    ptr[3] = static_cast<uint8_t>(payload_size & 0xFF);
    
    std::memcpy(ptr + 4, pkt.data.get() + RTP_TCP_HEAD_SIZE, payload_size);

    asio::error_code ec;
    asio::write(socket_, asio::buffer(ptr, payload_size + 4), ec);
    if (ec) {
        return -1;
    }

    return 0;
}

std::string AsioTcpRtpSender::GetPeerIp() const {
    asio::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return "";
    }
    
    return endpoint.address().to_string();
}

uint16_t AsioTcpRtpSender::GetPeerPort() const {
    asio::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return 0;
    }
    
    return endpoint.port();
}

uint16_t AsioTcpRtpSender::GetLocalPort() const {
    asio::error_code ec;
    auto local_endpoint = socket_.local_endpoint(ec);
    if (ec) {
        LOG_ERROR("AsioTcpRtpSender::GetLocalPort, error: {}", ec.message());
        return 0;
    }
    
    return local_endpoint.port();
}

AsioUdpRtpSender::AsioUdpRtpSender(asio::io_context& io_context, const std::string& peer_ip,
                                    uint16_t peer_rtp_port, uint16_t peer_rtcp_port)
    : io_context_(io_context), peer_ip_(peer_ip), peer_rtp_port_(peer_rtp_port), peer_rtcp_port_(peer_rtcp_port)
{
    socket_ = std::make_unique<asio::ip::udp::socket>(io_context_);
    socket_->open(asio::ip::udp::v4());
    socket_->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
}

int AsioUdpRtpSender::Send(MediaChannelId channel, const RtpPacket& pkt) {
    if (pkt.data == nullptr || pkt.size <= 4) {
        return -1;
    }

    auto peer_endpoint = asio::ip::udp::endpoint(asio::ip::make_address(peer_ip_), peer_rtp_port_);
    asio::error_code ec;
    socket_->send_to(asio::buffer(pkt.data.get() + 4, pkt.size - 4), peer_endpoint, 0, ec);
    if (ec) {
        return -1;
    }

    return 0;
}

std::string AsioUdpRtpSender::GetPeerIp() const {
    return peer_ip_;
}

uint16_t AsioUdpRtpSender::GetPeerPort() const {
    return peer_rtp_port_;
}

uint16_t AsioUdpRtpSender::GetLocalPort() const {
    asio::error_code ec;
    auto local_endpoint = socket_->local_endpoint(ec);
    if (ec) {
        LOG_ERROR("AsioUdpRtpSender::GetLocalPort, error: {}", ec.message());
        return 0;
    }
    
    return local_endpoint.port();
}

AsioMulticastRtpSender::AsioMulticastRtpSender(asio::io_context& io_context,
                                                const std::string& multicast_ip,
                                                uint16_t port)
    : io_context_(io_context), multicast_ip_(multicast_ip), multicast_port_(port)
{
    auto multicast_address = asio::ip::make_address(multicast_ip);
    auto multicast_endpoint = asio::ip::udp::endpoint(multicast_address, multicast_port_);
    
    socket_ = std::make_unique<asio::ip::udp::socket>(io_context_);
    socket_->open(asio::ip::udp::v4());
    socket_->set_option(asio::ip::multicast::join_group(multicast_address));
}

int AsioMulticastRtpSender::Send(MediaChannelId channel, const RtpPacket& pkt) {
    if (pkt.data == nullptr || pkt.size == 0) {
        return -1;
    }

    uint8_t* ptr = pkt.data.get();
    auto size = pkt.size - 4;

    auto multicast_endpoint = asio::ip::udp::endpoint(asio::ip::make_address(multicast_ip_), multicast_port_);
    asio::error_code ec;
    socket_->send_to(asio::buffer(ptr, size), multicast_endpoint, 0, ec);
    if (ec) {
        return -1;
    }

    return 0;
}

std::string AsioMulticastRtpSender::GetPeerIp() const {
    return multicast_ip_;
}

uint16_t AsioMulticastRtpSender::GetPeerPort() const {
    return multicast_port_;
}

uint16_t AsioMulticastRtpSender::GetLocalPort() const {
    asio::error_code ec;
    auto local_endpoint = socket_->local_endpoint(ec);
    if (ec) {
        LOG_ERROR("AsioMulticastRtpSender::GetLocalPort, error: {}", ec.message());
        return 0;
    }
    
    return local_endpoint.port();
}
}