#pragma once

#include <QObject>
#include <QImage>
#include <QString>

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

class FFmpegDecoder;
class MediaFrame;
class RtpUdpPuller;
struct SwsContext;

/// Owns the RTP receiver and decoder worker used by the Qt UI.
class RtpPlayer : public QObject {
    Q_OBJECT

public:
    explicit RtpPlayer(QObject* parent = nullptr);
    ~RtpPlayer() override;

    bool Start(const QString& local_address, uint16_t local_port,
               const QString& camera_address);
    void Stop();
    bool IsRunning() const { return running_.load(); }

signals:
    void FrameReady(const QImage& image);
    void StateChanged(const QString& state);
    void ErrorOccurred(const QString& error);

private:
    void Run();
    void ConvertAndPublishFrame(std::shared_ptr<MediaFrame> frame);

    std::unique_ptr<RtpUdpPuller> puller_;
    std::unique_ptr<FFmpegDecoder> decoder_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    SwsContext* sws_context_{nullptr};
};
