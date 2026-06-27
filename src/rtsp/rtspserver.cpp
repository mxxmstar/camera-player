#include "rtsp/rtspserver.h"
#include "log/logger.h"
#include "rtp/mediasession.h"
namespace rtsp {

RtspServer::RtspServer(asio::io_context& io_context, 
                       AsioIOContextPool& work_pool, 
                       uint16_t port)
    : server_(io_context, work_pool, port) {
    LOG_INFO("RtspServer created on port {}", port);
}

void RtspServer::Start() {
    LOG_INFO("RtspServer starting...");
    server_.SetAcceptHandler([this](asio::ip::tcp::socket socket) {
        OnCreateSession(std::move(socket));
    });
    server_.Start();
}

void RtspServer::OnCreateSession(asio::ip::tcp::socket socket) {
    try {
        LOG_INFO("New RTSP connection from: {}", 
                        socket.remote_endpoint().address().to_string());

        auto session = std::make_shared<RtspSession>(std::move(socket));

        if (!video_filepath_.empty()) {
            session->LoadVideoFile(video_filepath_);
        }
        if (!session->HasVideoFile()) {
            std::vector<std::shared_ptr<rtp::RTPSource>> sources;
            sources.push_back(std::make_shared<rtp::H264Source>(25));
            session->SetRTPSources(sources);
        }

        session->SetCloseHandler([this, session]() {
            LOG_INFO("RTSP session closed: {}", session->GetSessionID());
        });

        session->Start();
    } catch (std::exception& e) {
        LOG_ERROR("Failed to create RTSP session: {}", e.what());
    }
}

}