#pragma once
#include <cstdint>
#include <string>
#include "net/tcpserver.h"
#include "net/asio_io_context_pool.h"
#include "rtsp/rtspsession.h"

namespace rtsp {

class RtspServer {
public:
    RtspServer(asio::io_context& io_context, 
               AsioIOContextPool& work_pool, 
               uint16_t port);
    void Start();
    void SetVideoFile(const std::string& filepath) { video_filepath_ = filepath; }
    void OnCreateSession(asio::ip::tcp::socket socket);

private:
    AsioTCPServer server_;
    std::string video_filepath_;
};

}
