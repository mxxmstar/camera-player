#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "media/media_packet.hpp"

class FFmpegDecoder;
class IPuller;
class MediaFrame;
class StreamSession;
struct SwsContext;

/// Owns the AVTP/CVF receiver and decoder worker.
class AvtpPlayer {
public:
    using FrameCallback = std::function<void(std::shared_ptr<const MediaFrame>)>;
    using MessageCallback = std::function<void(const std::string&)>;

    struct CaptureDevice {
        std::string name;
        std::string description;
    };

    AvtpPlayer();
    ~AvtpPlayer();

    AvtpPlayer(const AvtpPlayer&) = delete;
    AvtpPlayer& operator=(const AvtpPlayer&) = delete;

    bool Start(const std::string& device_name,
               const std::string& source_mac,
               const std::string& stream_id = std::string());
    void Stop();
    bool IsRunning() const { return running_.load(); }

    void SetFrameCallback(FrameCallback callback);
    void SetStateCallback(MessageCallback callback);
    void SetErrorCallback(MessageCallback callback);

    static std::vector<CaptureDevice> ListCaptureDevices();

private:
    struct Runtime;

    bool StartOnIo(const std::string& device_name,
                   const std::string& source_mac,
                   const std::string& stream_id);
    void StopOnIo();
    void HandlePacket(std::shared_ptr<MediaPacket> packet);
    bool EnsureDecoder(const MediaPacket& packet);
    void ReportStats();
    void ConvertAndPublishFrame(std::shared_ptr<MediaFrame> frame);
    void PublishFrame(std::shared_ptr<const MediaFrame> frame);
    void PublishState(const std::string& state);
    void PublishError(const std::string& error);

    std::unique_ptr<Runtime> runtime_;
    std::shared_ptr<StreamSession> session_;
    IPuller* puller_{nullptr};
    std::unique_ptr<FFmpegDecoder> decoder_;
    CodecType decoder_codec_{CodecType::UNKNOWN};
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point last_report_{};
    SwsContext* sws_context_{nullptr};

    mutable std::mutex callback_mutex_;
    FrameCallback frame_callback_;
    MessageCallback state_callback_;
    MessageCallback error_callback_;
};
