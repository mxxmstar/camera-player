#pragma once
#include <memory>
#include <cstdint>
#include "rtsp/rtspserver.h"

namespace rtsp {

class RtspManager {
public:
    static RtspManager& Instance() {
        static RtspManager instance;
        return instance;
    }

    void SetVideoFile(const std::string& filepath) {
        video_filepath_ = filepath;
    }

    void Start(uint16_t port = 8554) {
        if (server_) {
            return;
        }
        server_ = std::make_shared<RtspServer>(io_context_, worker_pool_, port);
        if (!video_filepath_.empty()) {
            server_->SetVideoFile(video_filepath_);
        }
        server_->Start();
        io_context_thread_ = std::thread([this]() {
            io_context_.run();
        });
    }

    void Stop() {
        if (server_) {
            server_.reset();
        }
        if (io_context_.stopped()) {
            io_context_.restart();
        }
        if (io_context_thread_.joinable()) {
            io_context_thread_.join();
        }
    }

    bool IsRunning() const {
        return server_ != nullptr;
    }

private:
    RtspManager() = default;
    ~RtspManager() = default;

    RtspManager(const RtspManager&) = delete;
    RtspManager& operator=(const RtspManager&) = delete;

    asio::io_context io_context_;
    AsioIOContextPool& worker_pool_ = AsioIOContextPool::GetInstance(
        AsioIOContextPool::ServiceType::TCP);
    std::shared_ptr<RtspServer> server_;
    std::thread io_context_thread_;
    std::string video_filepath_;
};

}
