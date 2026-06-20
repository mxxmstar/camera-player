#include "rtp/nalsourceffmpeg.h"
#include <iostream>
#include <cstring>

using namespace rtp;

FFmpegNALSource::FFmpegNALSource() = default;

FFmpegNALSource::~FFmpegNALSource() {
    Close();
}

bool FFmpegNALSource::Open(const std::string& source) {
    try {
        source_ = source;
        
        // 1. 创建 Demuxer（使用新的 Demuxer 类）
        demuxer_ = std::make_unique<FFmpeg::Demuxer>(source);
        
        // 2. 打开输入源
        if (!demuxer_->Open()) {
            std::cerr << "无法打开 Demuxer: " << source << std::endl;
            return false;
        }
        
        // 3. 查找视频流
        video_stream_idx_ = demuxer_->GetVideoStreamIndex();
        if (video_stream_idx_ == -1) {
            std::cerr << "未找到视频流" << std::endl;
            return false;
        }
        
        // 4. 获取流信息
        auto codec_params = demuxer_->GetCodecParams(video_stream_idx_);
        if (!codec_params) {
            std::cerr << "无法获取编解码器参数" << std::endl;
            return false;
        }
        
        // 5. 检测编码格式
        AVCodecID codec_id = codec_params->codec_id;
        if (codec_id == AV_CODEC_ID_H264) {
            media_type_ = H264;
        } else if (codec_id == AV_CODEC_ID_HEVC) {
            media_type_ = H265;
        } else {
            std::cerr << "不支持的编码格式：" 
                      << avcodec_get_name(codec_id) << std::endl;
            return false;
        }
        
        // 6. 获取视频参数
        width_ = codec_params->width;
        height_ = codec_params->height;
        
        // 7. 获取帧率
        auto fmt_ctx = demuxer_->GetFormatContext();
        auto stream = fmt_ctx->stream(video_stream_idx_);
        if (stream) {
            AVRational avg_frame_rate = stream->avg_frame_rate;
            if (avg_frame_rate.num > 0 && avg_frame_rate.den > 0) {
                frame_rate_ = static_cast<uint32_t>(av_q2d(avg_frame_rate));
            } else {
                frame_rate_ = 30;
            }
        }
        
        // 8. 获取时长
        duration_ms_ = demuxer_->GetDurationUs() / 1000;
        
        is_opened_ = true;
        eof_reached_ = false;
        
        std::cout << "成功打开文件：" << source 
                  << ", 编码：" << GetCodecName()
                  << ", 分辨率：" << width_ << "x" << height_
                  << ", 帧率：" << frame_rate_
                  << ", 时长：" << duration_ms_ << "ms" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "打开文件失败：" << e.what() << std::endl;
        return false;
    }
}

void FFmpegNALSource::Close() {
    if (demuxer_) {
        demuxer_->Close();
        demuxer_.reset();
    }
    
    video_stream_idx_ = -1;
    media_type_ = NONE;
    width_ = 0;
    height_ = 0;
    frame_rate_ = 30;
    duration_ms_ = 0;
    is_opened_ = false;
    eof_reached_ = false;
}

NALSource::FrameNAL FFmpegNALSource::ReadNextFrame() {
    if (!is_opened_ || !demuxer_ || eof_reached_) {
        return std::nullopt;
    }

    FFmpeg::StackPacket pkt;
    
    // 使用 Demuxer::ReadPacket 接口读取数据包
    while (demuxer_->ReadPacket(pkt.raw())) {
        if (pkt.StreamIndex() == video_stream_idx_) {
            // 提取 NAL 单元
            auto nals_raw = ExtractNALUnits(&pkt);
            
            if (!nals_raw.empty()) {
                // 转换为 NALUnit 对象
                NALUnitList frame_nals;
                frame_nals.reserve(nals_raw.size());
                
                for (auto& nal_data : nals_raw) {
                    NALUnit nal = CreateNALUnit(nal_data, pkt.get()->pts, pkt.get()->dts);
                    
                    // 设置 NAL 类型
                    if (media_type_ == H264) {
                        nal.type = GetH264NALType(nal_data);
                    } else if (media_type_ == H265) {
                        nal.type = GetH265NALType(nal_data);
                    }
                    
                    frame_nals.push_back(std::move(nal));
                }
                
                // 设置关键帧标志
                bool is_keyframe = IsKeyFrame(nals_raw);
                for (auto& nal : frame_nals) {
                    nal.is_keyframe = is_keyframe;
                }
                
                return frame_nals;
            }
        }
        pkt.Unref();
    }

    // 到达文件末尾
    eof_reached_ = true;
    return std::nullopt;
}

bool FFmpegNALSource::TryReadNextFrame(FrameNAL& frame_nals) {
    if (!is_opened_ || eof_reached_) {
        return false;
    }
    
    frame_nals = ReadNextFrame();
    return frame_nals.has_value();
}

std::vector<std::vector<uint8_t>> FFmpegNALSource::ExtractNALUnits(FFmpeg::Packet* pkt) {
    std::vector<std::vector<uint8_t>> nals;
    
    uint8_t* data = pkt->get()->data;
    uint8_t* data_end = pkt->get()->data + pkt->get()->size;
    
    while (data < data_end) {
        // 查找起始码
        uint8_t* nal_start = nullptr;
        size_t start_code_size = 0;
        
        // 检查 4 字节起始码 00 00 00 01
        if (data + 4 <= data_end && 
            data[0] == 0 && data[1] == 0 && 
            data[2] == 0 && data[3] == 1) {
            nal_start = data;
            start_code_size = 4;
            data += 4;
        }
        // 检查 3 字节起始码 00 00 01
        else if (data + 3 <= data_end && 
                 data[0] == 0 && data[1] == 0 && data[2] == 1) {
            nal_start = data;
            start_code_size = 3;
            data += 3;
        } else {
            data++;
            continue;
        }
        
        // 查找下一个起始码
        uint8_t* next_nal = data;
        while (next_nal < data_end) {
            if (next_nal + 4 <= data_end && 
                next_nal[0] == 0 && next_nal[1] == 0 && 
                next_nal[2] == 0 && next_nal[3] == 1) {
                break;
            }
            if (next_nal + 3 <= data_end && 
                next_nal[0] == 0 && next_nal[1] == 0 && next_nal[2] == 1) {
                break;
            }
            next_nal++;
        }
        
        // 提取 NAL 单元（包含起始码）
        std::vector<uint8_t> nal(nal_start, next_nal);
        nals.push_back(std::move(nal));
        
        data = next_nal;
    }
    
    return nals;
}

NALUnit FFmpegNALSource::CreateNALUnit(const std::vector<uint8_t>& nal_data, 
                                        uint64_t pts, uint64_t dts) {
    NALUnit nal(static_cast<uint32_t>(nal_data.size()));
    std::memcpy(nal.data.get(), nal_data.data(), nal_data.size());
    nal.pts = pts;
    nal.dts = dts;
    return nal;
}

bool FFmpegNALSource::IsKeyFrame(const std::vector<std::vector<uint8_t>>& nals) {
    for (const auto& nal : nals) {
        if (nal.empty()) continue;
        
        uint8_t nal_type = 0;
        if (media_type_ == H264) {
            nal_type = GetH264NALType(nal);
            if (IsH264IDR(nal_type)) {
                return true;
            }
        } else if (media_type_ == H265) {
            nal_type = GetH265NALType(nal);
            if (IsH265IDR(nal_type)) {
                return true;
            }
        }
    }
    
    return false;
}

std::string FFmpegNALSource::GetCodecName() const {
    if (media_type_ == H264) {
        return "h264";
    } else if (media_type_ == H265) {
        return "hevc";
    }
    return "unknown";
}

bool FFmpegNALSource::Seek(int64_t timestamp_ms) {
    if (!is_opened_ || !demuxer_ || video_stream_idx_ == -1) {
        return false;
    }
    
    // 使用 Demuxer::SeekToKeyframe 接口
    int64_t timestamp_us = timestamp_ms * 1000;
    bool ret = demuxer_->SeekToKeyframe(video_stream_idx_, timestamp_us);
    
    if (!ret) {
        std::cerr << "Seek 失败" << std::endl;
        return false;
    }
    
    // 重置 EOF 标志
    eof_reached_ = false;
    
    return true;
}

void FFmpegNALSource::Reset() {
    Seek(0);
}