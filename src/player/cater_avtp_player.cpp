#include "player/cater_avtp_player.h"

#include <utility>

CaterAvtpPlayer::CaterAvtpPlayer()
    : player_(std::make_unique<AvtpPlayer>()) {
}

CaterAvtpPlayer::~CaterAvtpPlayer() {
    Stop();
}

bool CaterAvtpPlayer::Start(const std::string& device_name,
                            const std::string& source_mac,
                            const std::string& stream_id) {
    return player_->Start(device_name, source_mac, stream_id);
}

void CaterAvtpPlayer::Stop() {
    if (player_) {
        player_->Stop();
    }
}

bool CaterAvtpPlayer::IsRunning() const {
    return player_ && player_->IsRunning();
}

void CaterAvtpPlayer::SetFrameCallback(FrameCallback callback) {
    player_->SetFrameCallback(std::move(callback));
}

void CaterAvtpPlayer::SetStateCallback(MessageCallback callback) {
    player_->SetStateCallback(std::move(callback));
}

void CaterAvtpPlayer::SetErrorCallback(MessageCallback callback) {
    player_->SetErrorCallback(std::move(callback));
}

std::vector<CaterAvtpPlayer::CaptureDevice>
CaterAvtpPlayer::ListCaptureDevices() {
    return AvtpPlayer::ListCaptureDevices();
}
