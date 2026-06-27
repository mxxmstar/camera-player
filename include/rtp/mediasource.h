#pragma once
#include "rtp/media.h"
#include "rtp/rtp.h"

#include <functional>
#include <memory>
#include <string>
#include <optional>
namespace rtp {
    /// @brief 媒体源基类, 定义了媒体源的基本接口
class RTPSource :public std::enable_shared_from_this<RTPSource> {
public:
    using Ptr = std::shared_ptr<RTPSource>;
    using SendFrameCallback = std::function<bool (MediaChannelId channel_id, RtpPacket pkt)>;
    
    /// @brief 构造函数
    RTPSource() = default;

    // 禁止拷贝和移动
    RTPSource(const RTPSource&) = delete;
    RTPSource& operator=(const RTPSource&) = delete;
    RTPSource(RTPSource&&) = delete;
    RTPSource& operator=(RTPSource&&) = delete;

    virtual ~RTPSource() = default;

    virtual MediaType GetMediaType() const { 
        return media_type_; 
    }

    virtual std::string GetMediaDescription(uint16_t port=0) = 0;

    virtual std::string GetAttribute()  = 0;

    virtual bool HandleFrame(MediaChannelId channelId, NALFrame frame) = 0;
    
    virtual void SetSendFrameCallback(SendFrameCallback callback) { 
        send_frame_callback_ = std::move(callback); 
    }

    virtual uint32_t GetPayloadType() const { 
        return payload_; 
    }

    virtual uint32_t GetClockRate() const { 
        return clock_rate_; 
    }

protected:
    
    MediaType media_type_ = NONE;
    // 负载类型
    uint32_t  payload_ = 0;
    // 时钟频率
    uint32_t  clock_rate_ = 0;
    SendFrameCallback send_frame_callback_;

    // 允许派生类访问 weak_from_this
    std::weak_ptr<RTPSource> weak_from_this() const {
        try {
            return std::const_pointer_cast<RTPSource>(shared_from_this());
        } catch (const std::bad_weak_ptr&) {
            return std::weak_ptr<RTPSource>();
        }
    }
};

} // namespace rtp