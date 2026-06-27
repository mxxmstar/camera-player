#pragma once
#include <memory>
#include <cstdint>
#include <string>
#include <variant>
namespace rtp {
/// @brief RTP包头部大小
constexpr int RTP_HEADER_SIZE = 12;
constexpr int MAX_RTP_PAYLOAD_SIZE = 1420; //1460  1500-20-12-8
constexpr int RTP_VERSION = 2;
// 2.2.1.1 RTP Header
/// @brief RTP包TCP头部大小
constexpr int RTP_TCP_HEAD_SIZE = 4;
constexpr int RTP_VPX_HEAD_SIZE = 1;

enum TransportMode
{
    RTP_OVER_TCP = 1,   // 复用 RTSP socket
    RTP_OVER_UDP = 2,   // 独立的 UDP socket
    RTP_OVER_MULTICAST = 3, // 组播 UDP
};

struct RtpHeader {
    // 注意：位域布局依赖编译器，这里仅用于内存映射
    // 实际 RTP 头部字节序由 FillRtpHeader 手动控制
    unsigned char first_byte;   // version(2) | padding(1) | extension(1) | csrc_count(4)
    unsigned char second_byte;  // marker(1) | payload_type(7)
    unsigned short seq;
    unsigned int   ts;
    unsigned int   ssrc;
    
    // 存储 payload type（不直接参与网络传输，用于构建 second_byte）
    unsigned char payload = 0;
};


/// @brief RTP/RTCP over TCP 的传输信息
struct TcpTransportInfo {
    uint16_t rtp_channel = 0;     // RTP interleaved 通道号
    uint16_t rtcp_channel = 1;    // RTCP interleaved 通道号
};

/// @brief RTP/RTCP over UDP 的传输信息
struct UdpTransportInfo {
    uint16_t local_rtp_port = 0;      // 本地 RTP 端口
    uint16_t local_rtcp_port = 0;     // 本地 RTCP 端口
    uint16_t peer_rtp_port = 0;       // 对端 RTP 端口
    uint16_t peer_rtcp_port = 0;      // 对端 RTCP 端口    
};

/// @brief RTP/RTCP over Multicast 的传输信息
struct MulticastTransportInfo {
    std::string multicast_ip;
    uint16_t port = 0;    
};

/// @brief 媒体通道信息
struct MediaChannelInfo
{
    RtpHeader rtp_header;	// 该通道RTP包头部

    uint16_t packet_seq = 0;	// 该通道RTP包序号
    uint32_t clock_rate = 0;	// 该通道RTP包时钟率

    // rtcp
    uint64_t packet_count = 0;	// 该通道已发送RTP包数量
    uint64_t octet_count = 0;	// 该通道已发送RTP包字节数, 不包含RTP包头部
    uint64_t last_rtcp_ntp_time = 0;	// 该通道上次发送RTCP包时间, 单位为秒

    bool is_setup = false;	// 该通道是否已设置
    bool is_playing = false;	// 该通道是否处于播放状态
    bool is_recording = false;	// 该通道是否处于录制状态

    std::variant<TcpTransportInfo, UdpTransportInfo, MulticastTransportInfo> transport_info;
    TransportMode transport_mode = TransportMode::RTP_OVER_TCP;

    // 辅助函数：获取当前模式
    TransportMode GetMode() const { return transport_mode; }
    
    // 辅助函数：设置 TCP 模式
    void SetTcpMode(uint16_t rtp_ch, uint16_t rtcp_ch) {
        transport_info.emplace<TcpTransportInfo>(TcpTransportInfo{rtp_ch, rtcp_ch});
        transport_mode = TransportMode::RTP_OVER_TCP;
        is_setup = true;
    }
    
    // 辅助函数：设置 UDP 模式
    void SetUdpMode(uint16_t peer_rtp, uint16_t peer_rtcp,
                   uint16_t local_rtp, uint16_t local_rtcp) {
        UdpTransportInfo info;
        info.peer_rtp_port = peer_rtp;
        info.peer_rtcp_port = peer_rtcp;
        info.local_rtp_port = local_rtp;
        info.local_rtcp_port = local_rtcp;
        transport_info.emplace<UdpTransportInfo>(info);
        transport_mode = TransportMode::RTP_OVER_UDP;
        is_setup = true;
    }
    
    // 辅助函数：设置组播模式
    void SetMulticastMode(const std::string& ip, uint16_t port) {
        MulticastTransportInfo info;
        info.multicast_ip = ip;
        info.port = port;        
        transport_info.emplace<MulticastTransportInfo>(info);
        transport_mode = TransportMode::RTP_OVER_MULTICAST;
        is_setup = true;
    }
    
    // 辅助函数：获取对应模式的信息
    TcpTransportInfo* GetTcpInfo() {
        if (transport_mode == TransportMode::RTP_OVER_TCP) {
            return &std::get<TcpTransportInfo>(transport_info);
        }
        return nullptr;
    }
    
    UdpTransportInfo* GetUdpInfo() {
        if (transport_mode == TransportMode::RTP_OVER_UDP) {
            return &std::get<UdpTransportInfo>(transport_info);
        }
        return nullptr;
    }
    
    MulticastTransportInfo* GetMulticastInfo() {
        if (transport_mode == TransportMode::RTP_OVER_MULTICAST) {
            return &std::get<MulticastTransportInfo>(transport_info);
        }
        return nullptr;
    }
    
};

struct RtpPacket
{
    RtpPacket(): data(new uint8_t[1600], std::default_delete<uint8_t[]>())
    {
        type = 0;
        size = 0;
        timestamp = 0;
        last = 0;
    }

    std::shared_ptr<uint8_t> data;  // 包数据
    std::size_t size;   // 包大小
    std::size_t timestamp;  // 时间戳
    uint8_t type;   // 帧类型
    uint8_t last;   // 是否为最后一帧
};
}