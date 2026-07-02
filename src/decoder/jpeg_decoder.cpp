#include "decoder/jpeg_decoder.hpp"

#include "log/logger.h"
#include "media/ffmpeg_frame_buffer.hpp"

extern "C" {
#include <libavutil/imgutils.h>
}

JpegDecoder::~JpegDecoder() {
    Close();
}

bool JpegDecoder::Open(const StreamInfo& info) {
    if (info.codec_type != CodecType::JPEG) {
        LOG_ERROR("JpegDecoder:Open rejected: not JPEG codec");
        return false;
    }

    // 查找 MJPEG 解码器
    const AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    if (!decoder) {
        LOG_ERROR("JpegDecoder:Open: avcodec_find_decoder failed for MJPEG");
        return false;
    }

    // 分配解码器上下文
    codec_ctx_ = avcodec_alloc_context3(decoder);
    if (!codec_ctx_) {
        LOG_ERROR("JpegDecoder:Open: avcodec_alloc_context3 failed");
        return false;
    }

    // 设置解码参数
    if (info.media_type == MediaType::VIDEO) {
        codec_ctx_->width = info.width;
        codec_ctx_->height = info.height;
        codec_ctx_->pix_fmt = AV_PIX_FMT_NONE;  // 由解码器自动检测
        codec_ctx_->thread_count = 1;
    }

    // 打开解码器
    int ret = avcodec_open2(codec_ctx_, decoder, nullptr);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_ERROR("JpegDecoder:Open: avcodec_open2 failed: {}", buf);
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
        return false;
    }

    stream_info_ = info;
    LOG_INFO("JpegDecoder:Open success: {}x{}", info.width, info.height);
    return true;
}

void JpegDecoder::Close() {
    if (codec_ctx_) {
        avcodec_send_packet(codec_ctx_, nullptr);
        (void)ReceiveFrames();

        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    stream_info_ = {};
}

bool JpegDecoder::Decode(std::shared_ptr<MediaPacket> packet) {
    if (!codec_ctx_) {
        LOG_ERROR("JpegDecoder:Decode: codec_ctx_ is null");
        return false;
    }
    if (!packet || !packet->buffer) {
        LOG_ERROR("JpegDecoder:Decode: invalid packet");
        return false;
    }

    const std::size_t packet_size = packet->buffer->Size();
    if (!packet->buffer->Data() || packet_size == 0 ||
        packet_size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        LOG_ERROR("JpegDecoder:Decode: invalid raw packet");
        return false;
    }

    AVPacket* avpkt = av_packet_alloc();
    if (!avpkt || av_new_packet(avpkt, static_cast<int>(packet_size)) < 0) {
        av_packet_free(&avpkt);
        LOG_ERROR("JpegDecoder:Decode: AVPacket allocation failed");
        return false;
    }

    std::memcpy(avpkt->data, packet->buffer->Data(), packet_size);
    avpkt->pts = packet->pts;
    avpkt->dts = packet->dts;
    avpkt->duration = packet->duration;
    if (packet->keyframe) {
        avpkt->flags |= AV_PKT_FLAG_KEY;
    }

    // 送入解码器
    int ret = avcodec_send_packet(codec_ctx_, avpkt);
    av_packet_free(&avpkt);
    if (ret < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        LOG_ERROR("JpegDecoder: avcodec_send_packet failed: {}", buf);
        return false;
    }

    // 接收所有产生的帧
    return ReceiveFrames();
}

void JpegDecoder::SetFrameCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    frame_cb_ = std::move(cb);
}

bool JpegDecoder::ReceiveFrames() {
    if (!codec_ctx_)
        return false;

    int ret = 0;
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_ERROR("av_frame_alloc failed");
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_ctx_, frame);

        if (ret == AVERROR(EAGAIN)) {
            // 解码器需要更多数据，正常
            break;
        }
        if (ret == AVERROR_EOF) {
            LOG_DEBUG("EOF");
            break;
        }
        if (ret < 0) {
            char buf[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
            LOG_ERROR("JpegDecoder: avcodec_receive_frame failed: {}", buf);
            av_frame_free(&frame);
            return false;
        }

        // 计算 packed 大小
        int size = av_image_get_buffer_size(
            static_cast<AVPixelFormat>(frame->format),
            frame->width, frame->height, 1);

        if (size <= 0) {
            LOG_ERROR("av_image_get_buffer_size failed");
            av_frame_free(&frame);
            return false;
        }

        // 构建 FFmpegFrameBuffer（接管 frame 所有权）
        auto fb = std::make_shared<FFmpegFrameBuffer>(frame, static_cast<size_t>(size));

        // 分配新 frame 用于下一轮
        frame = av_frame_alloc();
        if (!frame) {
            LOG_ERROR("av_frame_alloc OOM after decode");
            av_frame_free(&frame);
            return false;
        }

        // 填充 MediaFrame
        auto mf = std::make_shared<MediaFrame>();
        mf->type = MediaType::VIDEO;
        mf->pixel_format = MapAVPixelFormat(static_cast<AVPixelFormat>(fb->GetFrame()->format));
        mf->width = fb->GetFrame()->width;
        mf->height = fb->GetFrame()->height;
        mf->pts = fb->GetFrame()->pts;
        mf->dts = fb->GetFrame()->pkt_dts;
        mf->duration = fb->GetFrame()->duration;
        mf->keyframe = (fb->GetFrame()->flags & AV_FRAME_FLAG_KEY) != 0;
        mf->buffer = fb;

        mf->backend.type = BackendHandle::FFMPEG;
        mf->backend.ptr = fb->GetFrame();

        // 回调通知
        FrameCallback cb;
        {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            cb = frame_cb_;
        }
        if (cb) {
            cb(std::move(mf));
        }
    }

    av_frame_free(&frame);
    return true;
}

PixelFormat JpegDecoder::MapAVPixelFormat(AVPixelFormat fmt) {
    switch (fmt) {
        case AV_PIX_FMT_YUVJ420P: return PixelFormat::kI420;
        case AV_PIX_FMT_YUVJ422P: return PixelFormat::kI420;
        case AV_PIX_FMT_YUVJ444P: return PixelFormat::kI420;
        case AV_PIX_FMT_YUV420P:  return PixelFormat::kI420;
        case AV_PIX_FMT_YUV422P:  return PixelFormat::kI420;
        case AV_PIX_FMT_YUV444P:  return PixelFormat::kI420;
        case AV_PIX_FMT_NV12:     return PixelFormat::kNV12;
        case AV_PIX_FMT_NV21:     return PixelFormat::kNV21;
        case AV_PIX_FMT_BGR24:    return PixelFormat::kBGR24;
        case AV_PIX_FMT_RGB24:    return PixelFormat::kRGB24;
        case AV_PIX_FMT_GRAY8:    return PixelFormat::kGRAY8;
        default:                  return PixelFormat::kUnknown;
    }
}
