#pragma once

#include "rtp/nalsource.h"
#include "ffmpeg/ffmpeg_avformat.h"
#include "ffmpeg/ffmpeg_avcodec.h"
#include "ffmpeg/ffmpeg_muxer.h"
#include <string>
#include <memory>

namespace rtp {

/// @brief 基于 FFmpeg 的 NAL 源实现
/// 
/// 特性：
/// 1. 支持所有 FFmpeg 支持的容器格式（MP4/MKV/AVI/TS/MOV 等）
/// 2. 支持 H.264 和 H.265 编码
/// 3. 自动检测编码格式
/// 4. 高效的 NAL 单元提取
class FFmpegNALSource : public NALSource {
public:
    FFmpegNALSource();
    ~FFmpegNALSource() override;

    // 实现 NALSource 接口
    bool Open(const std::string& source) override;
    void Close() override;
    FrameNAL ReadNextFrame() override;
    bool TryReadNextFrame(FrameNAL& frame_nals) override;
    bool HasMoreData() const override;
    MediaType GetMediaType() const override { return media_type_; }
    std::string GetCodecName() const override;
    uint32_t GetFrameRate() const override { return frame_rate_; }
    int GetWidth() const override { return width_; }
    int GetHeight() const override { return height_; }
    int64_t GetDuration() const override { return duration_ms_; }
    bool Seek(int64_t timestamp_ms) override;
    void Reset() override;

private:
    /// @brief 从 AVPacket 中提取所有 NAL 单元
    std::vector<std::vector<uint8_t>> ExtractNALUnits(FFmpeg::Packet* pkt);

    /// @brief 将原始数据转换为 NALUnit 对象
    NALUnit CreateNALUnit(const std::vector<uint8_t>& nal_data, uint64_t pts, uint64_t dts);

    /// @brief 检查是否为关键帧
    bool IsKeyFrame(const std::vector<std::vector<uint8_t>>& nals);

private:
    std::unique_ptr<FFmpeg::Demuxer> demuxer_;  ///< 解复用器
    std::string source_;                        ///< 媒体源路径
    int video_stream_idx_ = -1;                 ///< 视频流索引
    
    MediaType media_type_ = NONE;               ///< 媒体类型
    uint32_t frame_rate_ = 30;                  ///< 帧率
    int width_ = 0;                             ///< 视频宽度
    int height_ = 0;                            ///< 视频高度
    int64_t duration_ms_ = 0;                   ///< 时长（毫秒）
    
    bool is_opened_ = false;                    ///< 是否已打开
    bool eof_reached_ = false;                  ///< 是否到达文件末尾
};

} // namespace rtp
