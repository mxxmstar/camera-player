#include "player/avtp_player.h"

#include "decoder/ffmpeg_decoder.hpp"
#include "log/logger.h"
#include "media/media_frame.hpp"
#include "media/simple_buffer.hpp"
#include "net/asio_io_context_pool.h"
#include "puller/i_puller.hpp"
#include "stream/session/stream_session.h"

#ifdef ENABLE_PCAP
#include "puller/avtp_puller.hpp"
#include "puller/pcap_puller.hpp"
#endif

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

struct AvtpPlayer::Runtime {
    Runtime()
        : io(AsioIOContextPool::GetInstance().GetIOContext()) {
    }

    asio::io_context& io;
};

AvtpPlayer::AvtpPlayer()
    : runtime_(std::make_unique<Runtime>()),
      decoder_(std::make_unique<FFmpegDecoder>()) {
}

AvtpPlayer::~AvtpPlayer() {
    Stop();
}

bool AvtpPlayer::Start(const std::string& device_name,
                       const std::string& source_mac,
                       const std::string& stream_id) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();

    asio::dispatch(runtime_->io,
                   [this,
                    device_name,
                    source_mac,
                    stream_id,
                    promise]() mutable {
                       promise->set_value(
                           StartOnIo(device_name, source_mac, stream_id));
                   });

    return future.get();
}

void AvtpPlayer::Stop() {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    asio::dispatch(runtime_->io, [this, promise]() {
        StopOnIo();
        promise->set_value();
    });

    future.get();
}

bool AvtpPlayer::StartOnIo(const std::string& device_name,
                           const std::string& source_mac,
                           const std::string& stream_id) {
    StopOnIo();

#ifdef ENABLE_PCAP
    const std::string trimmed_device = Trim(device_name);
    const std::string device =
        trimmed_device.empty() ? std::string("default") : trimmed_device;
    const std::string source = Trim(source_mac);
    const std::string stream = Trim(stream_id);

    std::string url = "avtp://" + device + "?queue=1024&read_timeout=100&fps=25";
    if (!source.empty()) {
        url += "&src=" + source;
    }
    if (!stream.empty()) {
        url += "&stream=" + stream;
    }

    auto avtp_puller = std::make_unique<AvtpPuller>();
    puller_ = avtp_puller.get();
    puller_->SetReadTimeoutMs(100);
    puller_->SetEventCallback([this](const std::string& message) {
        PublishError(message);
    });

    session_ = std::make_shared<StreamSession>(runtime_->io);
    session_->SetPuller(std::move(avtp_puller));
    session_->SetUrl(url);
    session_->SetMaxReconnectCount(0);
    session_->SetPacketCallback([this](std::shared_ptr<MediaPacket> packet) {
        HandlePacket(std::move(packet));
    });
    session_->SetStateCallback([this](StreamSession::State state) {
        if (state == StreamSession::State::KERROR) {
            PublishError("AVTP 接收已中断");
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
    PublishState("监听中，等待摄像头 AVTP/CVF");
    return true;
#else
    (void)device_name;
    (void)source_mac;
    (void)stream_id;
    PublishError("当前构建未启用 pcap，无法监听 AVTP");
    return false;
#endif
}

void AvtpPlayer::StopOnIo() {
    const bool was_running = running_.exchange(false);

    if (session_) {
        session_->Stop();
        session_.reset();
    }
    puller_ = nullptr;

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

void AvtpPlayer::SetFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    frame_callback_ = std::move(callback);
}

void AvtpPlayer::SetStateCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_callback_ = std::move(callback);
}

void AvtpPlayer::SetErrorCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = std::move(callback);
}

void AvtpPlayer::HandlePacket(std::shared_ptr<MediaPacket> packet) {
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

void AvtpPlayer::ReportStats() {
#ifdef ENABLE_PCAP
    auto* avtp_puller = dynamic_cast<AvtpPuller*>(puller_);
    if (!avtp_puller) {
        return;
    }

    const AvtpPuller::Stats stats = avtp_puller->GetStats();
    LOG_INFO(
        "AVTP RX: raw={}, video={}, filtered={}, parse_errors={}, "
        "lost={}, malformed={}, access_units={}",
        stats.raw_packets,
        stats.parsed_video_packets,
        stats.filtered_packets,
        stats.parse_errors,
        stats.assembler.lost_packets,
        stats.assembler.malformed_packets,
        stats.access_units);

    std::ostringstream message;
    message << "AVTP 包=" << stats.raw_packets
            << ", H264帧=" << stats.access_units
            << ", 丢包=" << stats.assembler.lost_packets
            << ", 解析错误=" << stats.parse_errors;
    PublishState(message.str());
#endif
}

std::vector<AvtpPlayer::CaptureDevice> AvtpPlayer::ListCaptureDevices() {
    std::vector<CaptureDevice> result;
#ifdef ENABLE_PCAP
    const auto devices = PcapPuller::ListDevices();
    result.reserve(devices.size());
    for (const auto& device : devices) {
        result.push_back(CaptureDevice{device.name, device.description});
    }
#endif
    return result;
}

void AvtpPlayer::ConvertAndPublishFrame(std::shared_ptr<MediaFrame> frame) {
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

void AvtpPlayer::PublishFrame(std::shared_ptr<const MediaFrame> frame) {
    FrameCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = frame_callback_;
    }
    if (callback) {
        callback(std::move(frame));
    }
}

void AvtpPlayer::PublishState(const std::string& state) {
    MessageCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = state_callback_;
    }
    if (callback) {
        callback(state);
    }
}

void AvtpPlayer::PublishError(const std::string& error) {
    MessageCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = error_callback_;
    }
    if (callback) {
        callback(error);
    }
}
