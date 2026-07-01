#pragma once

#include <cstddef>
#include <cstdint>

namespace rtp {

/// @brief 解析后的RTP数据包结构体
/// @note 该结构体不拥有有效载荷数据，仅包含对应指针
struct ParsedRtpPacket {
    bool marker{false}; ///< 是否有标记位
    uint8_t payload_type{0}; ///< 有效载荷类型
    uint16_t sequence_number{0}; ///< 序列号
    uint32_t timestamp{0}; ///< 时间戳
    uint32_t ssrc{0}; ///< SSRC
    const uint8_t* payload{nullptr}; ///< 有效载荷指针
    std::size_t payload_size{0}; ///< 有效载荷大小
};

/// @brief 解析RTP数据包
class RtpPacketParser {
public:
    /// @brief 解析RTP数据包
    /// @param data 输入数据指针
    /// @param size 输入数据大小
    /// @param packet 输出解析结果
    /// @return 是否成功解析
    static bool Parse(const uint8_t* data, std::size_t size, ParsedRtpPacket& packet);
};

} // namespace rtp
