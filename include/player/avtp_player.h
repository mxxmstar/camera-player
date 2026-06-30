#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>
#include <thread>

class FFmpegDecoder;
class IPuller;
class MediaFrame;
struct SwsContext;

/// Owns the AVTP/CVF receiver and decoder worker used by the Qt UI.
class AvtpPlayer : public QObject {
    Q_OBJECT

public:
    struct CaptureDevice {
        QString name;
        QString description;
    };

    explicit AvtpPlayer(QObject* parent = nullptr);
    ~AvtpPlayer() override;

    bool Start(const QString& device_name,
               const QString& source_mac,
               const QString& stream_id = QString());
    void Stop();
    bool IsRunning() const { return running_.load(); }

    static QVector<CaptureDevice> ListCaptureDevices();

signals:
    void FrameReady(const QImage& image);
    void StateChanged(const QString& state);
    void ErrorOccurred(const QString& error);

private:
    void Run();
    void ReportStats();
    void ConvertAndPublishFrame(std::shared_ptr<MediaFrame> frame);

    std::unique_ptr<IPuller> puller_;
    std::unique_ptr<FFmpegDecoder> decoder_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    SwsContext* sws_context_{nullptr};
};
