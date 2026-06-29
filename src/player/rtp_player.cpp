#include "player/rtp_player.h"

#include "decoder/ffmpeg_decoder.hpp"
#include "log/logger.h"
#include "media/media_frame.hpp"
#include "puller/rtp_udp_puller.hpp"

#include <chrono>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

RtpPlayer::RtpPlayer(QObject* parent)
    : QObject(parent),
      decoder_(std::make_unique<FFmpegDecoder>()) {
}

RtpPlayer::~RtpPlayer() {
    Stop();
}

bool RtpPlayer::Start(const QString& local_address, uint16_t local_port,
                      const QString& camera_address) {
    Stop();

    puller_ = std::make_unique<RtpUdpPuller>();
    puller_->SetReadTimeoutMs(100);
    puller_->SetEventCallback([this](const std::string& message) {
        emit ErrorOccurred(QString::fromStdString(message));
    });

    QString url = QStringLiteral(
        "rtp://%1:%2?pt=96&recv_buffer=4194304&queue=64")
                      .arg(local_address)
                      .arg(local_port);
    if (!camera_address.trimmed().isEmpty()) {
        url += QStringLiteral("&remote=%1")
                   .arg(camera_address.trimmed());
    }

    if (!puller_->Open(url.toStdString())) {
        puller_.reset();
        return false;
    }

    decoder_->SetFrameCallback(
        [this](std::shared_ptr<MediaFrame> frame) {
            ConvertAndPublishFrame(std::move(frame));
        });
    if (!decoder_->Open(puller_->GetStreamInfo())) {
        emit ErrorOccurred(QStringLiteral("无法打开 FFmpeg H.264 解码器"));
        puller_->Close();
        puller_.reset();
        return false;
    }

    running_ = true;
    worker_ = std::thread(&RtpPlayer::Run, this);
    emit StateChanged(QStringLiteral("监听中，等待摄像头 RTP"));
    return true;
}

void RtpPlayer::Stop() {
    const bool was_running = running_.exchange(false);
    if (puller_) {
        puller_->Close();
    }
    if (worker_.joinable()) {
        worker_.join();
    }

    if (decoder_) {
        decoder_->SetFrameCallback({});
        decoder_->Close();
    }
    if (sws_context_) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }
    puller_.reset();

    if (was_running) {
        emit StateChanged(QStringLiteral("已停止"));
    }
}

void RtpPlayer::Run() {
    auto last_report = std::chrono::steady_clock::now();
    while (running_) {
        std::shared_ptr<MediaPacket> packet;
        if (!puller_->ReadPacket(packet)) {
            if (running_) {
                emit ErrorOccurred(QStringLiteral("RTP 接收已中断"));
            }
            break;
        }
        if (packet) {
            (void)decoder_->Decode(std::move(packet));
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_report >= std::chrono::seconds(2)) {
            const RtpUdpPuller::Stats stats = puller_->GetStats();
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
            emit StateChanged(
                QStringLiteral(
                    "RTP UDP=%1, H264帧=%2, 丢包=%3, 过滤=%4")
                    .arg(stats.udp_packets)
                    .arg(stats.access_units)
                    .arg(stats.depacketizer.lost_packets)
                    .arg(stats.filtered_packets));
            last_report = now;
        }
    }
}

void RtpPlayer::ConvertAndPublishFrame(
    std::shared_ptr<MediaFrame> frame) {
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
        emit ErrorOccurred(QStringLiteral("无法创建视频像素转换器"));
        return;
    }

    QImage image(
        av_frame->width, av_frame->height, QImage::Format_RGB888);
    if (image.isNull()) {
        emit ErrorOccurred(QStringLiteral("无法分配视频图像缓冲区"));
        return;
    }

    uint8_t* destination[] = {image.bits(), nullptr, nullptr, nullptr};
    int destination_stride[] = {
        static_cast<int>(image.bytesPerLine()), 0, 0, 0};
    const int converted = sws_scale(
        sws_context_,
        av_frame->data,
        av_frame->linesize,
        0,
        av_frame->height,
        destination,
        destination_stride);
    if (converted != av_frame->height) {
        emit ErrorOccurred(QStringLiteral("视频像素转换失败"));
        return;
    }

    emit FrameReady(image);
}
