#include "player/avtp_player.h"

#include "decoder/ffmpeg_decoder.hpp"
#include "log/logger.h"
#include "media/media_frame.hpp"
#include "puller/i_puller.hpp"

#ifdef ENABLE_PCAP
#include "puller/avtp_puller.hpp"
#include "puller/pcap_puller.hpp"
#endif

#include <chrono>
#include <utility>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

AvtpPlayer::AvtpPlayer(QObject* parent)
    : QObject(parent),
      decoder_(std::make_unique<FFmpegDecoder>()) {
}

AvtpPlayer::~AvtpPlayer() {
    Stop();
}

bool AvtpPlayer::Start(const QString& device_name,
                       const QString& source_mac,
                       const QString& stream_id) {
    Stop();

#ifdef ENABLE_PCAP
    const QString device =
        device_name.trimmed().isEmpty()
            ? QStringLiteral("default")
            : device_name.trimmed();

    QString url = QStringLiteral(
        "avtp://%1?queue=1024&read_timeout=100&fps=25")
                      .arg(device);
    if (!source_mac.trimmed().isEmpty()) {
        url += QStringLiteral("&src=%1").arg(source_mac.trimmed());
    }
    if (!stream_id.trimmed().isEmpty()) {
        url += QStringLiteral("&stream=%1").arg(stream_id.trimmed());
    }

    auto puller = std::make_unique<AvtpPuller>();
    puller->SetReadTimeoutMs(100);
    puller->SetEventCallback([this](const std::string& message) {
        emit ErrorOccurred(QString::fromStdString(message));
    });

    if (!puller->Open(url.toStdString())) {
        return false;
    }

    decoder_->SetFrameCallback(
        [this](std::shared_ptr<MediaFrame> frame) {
            ConvertAndPublishFrame(std::move(frame));
        });
    if (!decoder_->Open(puller->GetStreamInfo())) {
        emit ErrorOccurred(QStringLiteral("无法打开 FFmpeg H.264 解码器"));
        decoder_->SetFrameCallback({});
        puller->Close();
        return false;
    }

    puller_ = std::move(puller);
    running_ = true;
    worker_ = std::thread(&AvtpPlayer::Run, this);
    emit StateChanged(QStringLiteral("监听中，等待摄像头 AVTP/CVF"));
    return true;
#else
    (void)device_name;
    (void)source_mac;
    (void)stream_id;
    emit ErrorOccurred(QStringLiteral("当前构建未启用 pcap，无法监听 AVTP"));
    return false;
#endif
}

void AvtpPlayer::Stop() {
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

void AvtpPlayer::Run() {
    auto last_report = std::chrono::steady_clock::now();
    while (running_) {
        std::shared_ptr<MediaPacket> packet;
        if (!puller_->ReadPacket(packet)) {
            if (running_) {
                emit ErrorOccurred(QStringLiteral("AVTP 接收已中断"));
            }
            break;
        }
        if (packet) {
            (void)decoder_->Decode(std::move(packet));
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_report >= std::chrono::seconds(2)) {
            ReportStats();
            last_report = now;
        }
    }
}

void AvtpPlayer::ReportStats() {
#ifdef ENABLE_PCAP
    auto* avtp_puller = dynamic_cast<AvtpPuller*>(puller_.get());
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
    emit StateChanged(
        QStringLiteral("AVTP 包=%1, H264帧=%2, 丢包=%3, 解析错误=%4")
            .arg(stats.raw_packets)
            .arg(stats.access_units)
            .arg(stats.assembler.lost_packets)
            .arg(stats.parse_errors));
#endif
}

QVector<AvtpPlayer::CaptureDevice> AvtpPlayer::ListCaptureDevices() {
    QVector<CaptureDevice> result;
#ifdef ENABLE_PCAP
    const auto devices = PcapPuller::ListDevices();
    result.reserve(static_cast<int>(devices.size()));
    for (const auto& device : devices) {
        result.push_back(
            CaptureDevice{
                QString::fromStdString(device.name),
                QString::fromStdString(device.description)});
    }
#endif
    return result;
}

void AvtpPlayer::ConvertAndPublishFrame(
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
