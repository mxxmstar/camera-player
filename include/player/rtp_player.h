#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

class FFmpegDecoder;
class MediaFrame;
class MediaPacket;
class RtpUdpPuller;
class StreamSession;
struct SwsContext;

/// Owns the RTP receiver and decoder worker.
class RtpPlayer {
public:
    using FrameCallback = std::function<void(std::shared_ptr<const MediaFrame>)>;
    using MessageCallback = std::function<void(const std::string&)>;

    RtpPlayer();
    ~RtpPlayer();

    RtpPlayer(const RtpPlayer&) = delete;
    RtpPlayer& operator=(const RtpPlayer&) = delete;

    bool Start(const std::string& local_address, uint16_t local_port,
               const std::string& camera_address);
    bool StartRtp(const std::string& local_address, uint16_t local_port,
                  const std::string& camera_address);
    void Stop();
    bool IsRunning() const { return running_.load(); }

    void SetFrameCallback(FrameCallback callback);
    void SetStateCallback(MessageCallback callback);
    void SetErrorCallback(MessageCallback callback);

private:
    struct Runtime;

    bool StartOnIo(const std::string& local_address, uint16_t local_port,
                   const std::string& camera_address);
    void StopOnIo();
    void HandlePacket(std::shared_ptr<MediaPacket> packet);
    void ReportStats();
    void ConvertAndPublishFrame(std::shared_ptr<MediaFrame> frame);
    void PublishFrame(std::shared_ptr<const MediaFrame> frame);
    void PublishState(const std::string& state);
    void PublishError(const std::string& error);

    std::unique_ptr<Runtime> runtime_;
    std::shared_ptr<StreamSession> session_;
    RtpUdpPuller* rtp_puller_{nullptr};
    std::unique_ptr<FFmpegDecoder> decoder_;
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point last_report_{};
    SwsContext* sws_context_{nullptr};

    mutable std::mutex callback_mutex_;
    FrameCallback frame_callback_;
    MessageCallback state_callback_;
    MessageCallback error_callback_;
};
