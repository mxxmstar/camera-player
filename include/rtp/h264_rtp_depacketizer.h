#pragma once

#include <cstdint>
#include <vector>

#include "rtp/rtp_packet_parser.h"

namespace rtp {

struct H264AccessUnit {
    std::vector<uint8_t> data;
    uint32_t timestamp{0};
    uint32_t ssrc{0};
    bool keyframe{false};
};

/// @brief H.264 RTP 重组器
///   - Single NAL units (types 1..23)
///   - STAP-A (type 24)
///   - FU-A (type 28)
///   - IDR (type 5)
///   - SPS (type 7)
///   - PPS (type 8)
class H264RtpDepacketizer {
public:
    enum class Result {
        NeedMore, ///< 需要更多数据
        AccessUnitReady, ///< 重组后的访问单元已准备就绪
        Dropped, ///< 丢弃了当前访问单元
        Malformed, ///< 格式错误的包
    };

    struct Stats {
        uint64_t packets{0}; ///< 收到的包数量
        uint64_t access_units{0}; ///< 重组后的访问单元数量
        uint64_t dropped_access_units{0}; ///< 丢弃的访问单元数量
        uint64_t lost_packets{0}; ///< 丢失的包数量
        uint64_t out_of_order_packets{0}; ///< 顺序错误的包数量
        uint64_t malformed_packets{0}; ///< 格式错误的包数量
    };

    explicit H264RtpDepacketizer(std::size_t max_access_unit_size = 8 * 1024 * 1024);

    Result Push(const ParsedRtpPacket& packet, H264AccessUnit& output);
    void Reset();
    const Stats& GetStats() const { return stats_; }

private:
    /// @brief 追加数据到当前访问单元
    /// @param data 数据指针
    /// @param size 数据大小
    /// @return 是否成功追加数据
    bool Append(const uint8_t* data, std::size_t size);

    /// @brief 追加 NALU 开头码
    /// @return 是否成功追加 NALU 开头码
    bool AppendStartCode();

    /// @brief 重置当前访问单元
    /// @param keyframe 是否为关键帧
    /// @param timestamp 时间戳
    /// @param ssrc SSRC
    void ResetAccessUnit();

    /// @brief 丢弃当前时间戳的访问单元
    /// @param packet 当前包
    /// @param malformed 是否为格式错误的包
    /// @return 丢弃结果
    Result DropCurrentTimestamp(const ParsedRtpPacket& packet, bool malformed);
    bool PacketCanStartUnit(const ParsedRtpPacket& packet) const;
    
    /// @brief 处理单个NAL单元
    /// @param packet RTP数据包
    /// @return 处理结果
    Result HandleSingleNal(const ParsedRtpPacket& packet);

    /// @brief 处理 STAP-A 单元
    /// @param packet RTP数据包
    /// @return 处理结果
    Result HandleStapA(const ParsedRtpPacket& packet);
        
    /// @brief 处理 FU-A 单元
    /// @param packet RTP数据包
    /// @return 处理结果
    Result HandleFuA(const ParsedRtpPacket& packet);
    Result FinishIfMarked(const ParsedRtpPacket& packet, H264AccessUnit& output);
    
    /// @brief 从当前已经组装好的访问单元中缓存参数集
    /// @note SPS/PPS 通常只在视频流开始时发送一次，防止后续关键帧丢失，无法初始化解码器
    void CacheParameterSetsFromAccessUnit();

    std::size_t max_access_unit_size_; ///< 最大访问单元大小
    std::vector<uint8_t> access_unit_; ///< 当前访问单元
    bool keyframe_{false};

    bool fu_active_;           ///< 是否正在接收 FU-A 分片
    bool has_timestamp_;       ///< 是否已记录当前时间戳
    uint32_t current_timestamp_; ///< 当前处理的时间戳
    bool dropping_timestamp_;  ///< 是否正在丢弃某个时间戳的包
    uint32_t dropped_timestamp_; ///< 被丢弃的时间戳值

    std::vector<uint8_t> cached_sps_; ///< 缓存的 SPS
    std::vector<uint8_t> cached_pps_; ///< 缓存的 PPS

    bool has_ssrc_{false}; ///< 是否已记录当前 SSRC
    uint32_t current_ssrc_{0}; ///< 当前处理的 SSRC
    bool has_expected_sequence_{false}; ///< 是否已记录期望的序列号
    uint16_t expected_sequence_{0}; ///< 期望的序列号
    Stats stats_; ///< 统计信息
};

} // namespace rtp
