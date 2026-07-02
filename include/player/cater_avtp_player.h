#pragma once

#include "player/avtp_player.h"

#include <memory>
#include <string>
#include <vector>

class MediaFrame;

/// Compatibility wrapper for the old Cater AVTP UI entry.
/// All AVTP variants now use AvtpPlayer internally.
class CaterAvtpPlayer {
public:
    using FrameCallback = AvtpPlayer::FrameCallback;
    using MessageCallback = AvtpPlayer::MessageCallback;
    using CaptureDevice = AvtpPlayer::CaptureDevice;

    CaterAvtpPlayer();
    ~CaterAvtpPlayer();

    CaterAvtpPlayer(const CaterAvtpPlayer&) = delete;
    CaterAvtpPlayer& operator=(const CaterAvtpPlayer&) = delete;

    bool Start(const std::string& device_name,
               const std::string& source_mac,
               const std::string& stream_id = std::string());
    void Stop();
    bool IsRunning() const;

    void SetFrameCallback(FrameCallback callback);
    void SetStateCallback(MessageCallback callback);
    void SetErrorCallback(MessageCallback callback);

    static std::vector<CaptureDevice> ListCaptureDevices();

private:
    std::unique_ptr<AvtpPlayer> player_;
};
