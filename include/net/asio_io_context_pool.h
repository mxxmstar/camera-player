#pragma once
#include <asio.hpp>

class AsioIOContextPool {
public:
    enum class ServiceType { TCP, HTTP, RTSP };

    static AsioIOContextPool& GetInstance(ServiceType type = ServiceType::TCP) {
        (void)type;
        static AsioIOContextPool instance;
        return instance;
    }

    asio::io_context& GetIOContext() { return io_context_; }

private:
    AsioIOContextPool() = default;
    asio::io_context io_context_;
};
