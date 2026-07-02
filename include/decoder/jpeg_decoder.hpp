#pragma once

#include "decoder/i_decoder.hpp"

#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
}

/// @brief JPEG/MJPEG 解码器
///
/// 实现 IDecoder 接口，使用 FFmpeg libavcodec 完成 JPEG/MJPEG 格式解码。
/// 专门用于卡特CAM的JPEG数据流。
///
/// 数据流：
///   MediaPacket (raw JPEG data)
///     → avcodec_send_packet()
///     → loop avcodec_receive_frame()
///     → FFmpegFrameBuffer + MediaFrame
///     → FrameCallback
class JpegDecoder : public IDecoder {
public:
    JpegDecoder() = default;
    ~JpegDecoder() override;

    // 禁止拷贝
    JpegDecoder(const JpegDecoder&) = delete;
    JpegDecoder& operator=(const JpegDecoder&) = delete;

    // ==================== IDecoder ====================

    bool Open(const StreamInfo& info) override;
    void Close() override;
    bool Decode(std::shared_ptr<MediaPacket> packet) override;
    void SetFrameCallback(FrameCallback cb) override;

    // ==================== 工具 ====================

    /// @brief AVPixelFormat → PixelFormat 映射
    static PixelFormat MapAVPixelFormat(AVPixelFormat fmt);

private:
    /// @brief 接收所有已解码帧并回调
    /// @return true 成功（至少零帧已回调），false 解码器错误
    bool ReceiveFrames();

    AVCodecContext* codec_ctx_{nullptr};  ///< FFmpeg 解码器上下文
    StreamInfo      stream_info_;          ///< 解码器打开的流信息
    FrameCallback   frame_cb_;             ///< 解码帧回调
    std::mutex      cb_mutex_;             ///< 保护 frame_cb_ setter
};
