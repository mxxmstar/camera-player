#include "player/rtp_player.h"

#include "decoder/ffmpeg_decoder.hpp"
#include "log/logger.h"
#include "media/media_frame.hpp"
#include "media/simple_buffer.hpp"
#include "net/asio_io_context_pool.h"
#include "puller/rtp_udp_puller.hpp"
#include "stream/session/stream_session.h"

#include <asio/dispatch.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <future>
#include <sstream>
#include <utility>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) {
        return !std::isspace(ch);
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
                value.end());
    return value;
}

}  // namespace

struct RtpPlayer::Runtime {
    Runtime()
        : io(AsioIOContextPool::GetInstance().GetIOContext()) {
    }

    asio::io_context& io;
};

RtpPlayer::RtpPlayer()
    : runtime_(std::make_unique<Runtime>()),
      decoder_(std::make_unique<FFmpegDecoder>()) {
}

RtpPlayer::~RtpPlayer() {
    Stop();
}

bool RtpPlayer::Start(const std::string& local_address, uint16_t local_port,
                      const std::string& camera_address) {
    return StartRtp(local_address, local_port, camera_address);
}

bool RtpPlayer::StartRtp(const std::string& local_address, uint16_t local_port,
                         const std::string& camera_address) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();

    asio::dispatch(runtime_->io,
                   [this,
                    local_address,
                    local_port,
                    camera_address,
                    promise]() mutable {
                       promise->set_value(
                           StartOnIo(local_address, local_port, camera_address));
                   });

    return future.get();
}

void RtpPlayer::Stop() {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    asio::dispatch(runtime_->io, [this, promise]() {
        StopOnIo();
        promise->set_value();
    });

    future.get();
}

bool RtpPlayer::StartOnIo(const std::string& local_address, uint16_t local_port,
                          const std::string& camera_address) {
    StopOnIo();

    auto puller = std::make_unique<RtpUdpPuller>();
    rtp_puller_ = puller.get();
    rtp_puller_->SetReadTimeoutMs(100);
    rtp_puller_->SetEventCallback([this](const std::string& message) {
        PublishError(message);
    });

    const std::string local = Trim(local_address);
    const std::string camera = Trim(camera_address);

    std::string url = "rtp://" + local + ":" + std::to_string(local_port) +
                      "?pt=96&recv_buffer=4194304&queue=64";
    if (!camera.empty()) {
        url += "&remote=" + camera;
    }

    session_ = std::make_shared<StreamSession>(runtime_->io);
    session_->SetPuller(std::move(puller));
    session_->SetUrl(url);
    session_->SetMaxReconnectCount(0);
    session_->SetPacketCallback([this](std::shared_ptr<MediaPacket> packet) {
        HandlePacket(std::move(packet));
    });
    session_->SetStateCallback([this](StreamSession::State state) {
        if (state == StreamSession::State::KERROR) {
            PublishError("RTP 接收已中断");
        }
    });

    auto decoder_opened = std::make_shared<bool>(false);
    session_->SetStreamInfoCallback([this, decoder_opened](
                                        const StreamInfo& info) {
        decoder_->SetFrameCallback([this](std::shared_ptr<MediaFrame> frame) {
            ConvertAndPublishFrame(std::move(frame));
        });
        *decoder_opened = decoder_->Open(info);
        if (!*decoder_opened) {
            PublishError("无法打开 FFmpeg H.264 解码器");
            decoder_->SetFrameCallback({});
        }
    });

    if (!session_->Start() || !*decoder_opened) {
        StopOnIo();
        return false;
    }

    running_ = true;
    last_report_ = std::chrono::steady_clock::now();
    PublishState("监听中，等待摄像头 RTP");
    return true;
}

void RtpPlayer::StopOnIo() {
    const bool was_running = running_.exchange(false);

    if (session_) {
        session_->Stop();
        session_.reset();
    }
    rtp_puller_ = nullptr;

    if (decoder_) {
        decoder_->SetFrameCallback({});
        decoder_->Close();
    }
    if (sws_context_) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }

    if (was_running) {
        PublishState("已停止");
    }
}

void RtpPlayer::SetFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    frame_callback_ = std::move(callback);
}

void RtpPlayer::SetStateCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_callback_ = std::move(callback);
}

void RtpPlayer::SetErrorCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = std::move(callback);
}

void RtpPlayer::HandlePacket(std::shared_ptr<MediaPacket> packet) {
    if (!running_ || !packet) {
        return;
    }

    (void)decoder_->Decode(std::move(packet));

    const auto now = std::chrono::steady_clock::now();
    if (now - last_report_ >= std::chrono::seconds(2)) {
        ReportStats();
        last_report_ = now;
    }
}

void RtpPlayer::ReportStats() {
    if (!rtp_puller_) {
        return;
    }

    const RtpUdpPuller::Stats stats = rtp_puller_->GetStats();
    LOG_INFO(
        "RTP RX: udp={}, filtered={}, invalid={}, lost={}, "
        "malformed={}, access_units={}, queue_drops={}",
        stats.udp_packets,
        stats.filtered_packets,
        stats.invalid_rtp_packets,
        stats.depacketizer.lost_packets,
        stats.depacketizer.malformed_packets,
        stats.access_units,
        stats.queue_drops);

    std::ostringstream message;
    message << "RTP UDP=" << stats.udp_packets
            << ", H264帧=" << stats.access_units
            << ", 丢包=" << stats.depacketizer.lost_packets
            << ", 过滤=" << stats.filtered_packets;
    PublishState(message.str());
}

void RtpPlayer::ConvertAndPublishFrame(std::shared_ptr<MediaFrame> frame) {
    if (!running_ || !frame ||
        frame->backend.type != BackendHandle::FFMPEG ||
        !frame->backend.ptr) {
        return;
    }

    AVFrame* av_frame = static_cast<AVFrame*>(frame->backend.ptr);
    if (av_frame->width <= 0 || av_frame->height <= 0 ||
        av_frame->format < 0) {
        return;
    }

    sws_context_ = sws_getCachedContext(
        sws_context_,
        av_frame->width,
        av_frame->height,
        static_cast<AVPixelFormat>(av_frame->format),
        av_frame->width,
        av_frame->height,
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr);
    if (!sws_context_) {
        PublishError("无法创建视频像素转换器");
        return;
    }

    const int width = av_frame->width;
    const int height = av_frame->height;
    const int stride = width * 3;
    auto rgb_buffer = std::make_shared<SimpleBuffer>();
    rgb_buffer->Resize(static_cast<std::size_t>(stride) *
                       static_cast<std::size_t>(height));
    if (!rgb_buffer->Data() || rgb_buffer->Size() == 0) {
        PublishError("无法分配视频图像缓冲区");
        return;
    }

    uint8_t* destination[] = {rgb_buffer->Data(), nullptr, nullptr, nullptr};
    int destination_stride[] = {stride, 0, 0, 0};
    const int converted = sws_scale(
        sws_context_,
        av_frame->data,
        av_frame->linesize,
        0,
        height,
        destination,
        destination_stride);
    if (converted != height) {
        PublishError("视频像素转换失败");
        return;
    }

    auto rgb_frame = std::make_shared<MediaFrame>();
    rgb_frame->type = MediaType::VIDEO;
    rgb_frame->pixel_format = PixelFormat::kRGB24;
    rgb_frame->width = width;
    rgb_frame->height = height;
    rgb_frame->stride[0] = stride;
    rgb_frame->plane_offset[0] = 0;
    rgb_frame->plane_count = 1;
    rgb_frame->pts = frame->pts;
    rgb_frame->dts = frame->dts;
    rgb_frame->duration = frame->duration;
    rgb_frame->keyframe = frame->keyframe;
    rgb_frame->buffer = std::move(rgb_buffer);

    PublishFrame(std::move(rgb_frame));
}

void RtpPlayer::PublishFrame(std::shared_ptr<const MediaFrame> frame) {
    FrameCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = frame_callback_;
    }
    if (callback) {
        callback(std::move(frame));
    }
}

void RtpPlayer::PublishState(const std::string& state) {
    MessageCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = state_callback_;
    }
    if (callback) {
        callback(state);
    }
}

void RtpPlayer::PublishError(const std::string& error) {
    MessageCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = error_callback_;
    }
    if (callback) {
        callback(error);
    }
}
