#include "server/ws/ws_server.hpp"
#include "log/logger.h"

#include <iostream>

namespace ws {

WsServer::WsServer(asio::io_context* io_context,
                   const std::string& address,
                   unsigned short port)
    : owns_io_context_(io_context == nullptr),
      internal_io_(io_context ? nullptr : std::make_unique<asio::io_context>()),
      io_context_(io_context ? *io_context : *internal_io_),
      acceptor_(io_context_,
                asio::ip::tcp::endpoint(asio::ip::make_address(address), port))
{
    if (owns_io_context_) {
        work_guard_ = std::make_unique<
            asio::executor_work_guard<asio::io_context::executor_type>>(
                io_context_.get_executor());
    }
    LOG_INFO("[WsServer] Will listen on {}:{}", address, port);
}

WsServer::~WsServer() {
    Stop();
}

void WsServer::Start() {
    if (running_.exchange(true)) {
        return;
    }

    DoAccept();

    if (owns_io_context_) {
        io_thread_ = std::thread([this]() {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                LOG_ERROR("[WsServer] io_context exception: {}", e.what());
            }
        });
        LOG_INFO("[WsServer] Started with internal io_context thread.");
    } else {
        LOG_INFO("[WsServer] Started (using external io_context).");
    }
}

void WsServer::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

    asio::error_code ec;
    acceptor_.close(ec);

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& kv : sessions_) {
            if (kv.second) {
                kv.second->Close(CloseCode::GoingAway);
            }
        }
        sessions_.clear();
    }

    if (owns_io_context_) {
        work_guard_.reset();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }
    LOG_INFO("[WsServer] Stopped.");
}

void WsServer::DoAccept() {
    if (!running_) return;

    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<WsSession>(std::move(socket), this);

                if (on_open_)     session->SetOpenHandler(on_open_);
                if (on_message_)  session->SetMessageHandler(on_message_);
                if (on_ping_)     session->SetPingHandler(on_ping_);
                if (on_close_)    session->SetCloseHandler(on_close_);

                AddSession(session);
                session->Start();
            } else if (ec != asio::error::operation_aborted) {
                LOG_ERROR("[WsServer] Accept error: {}", ec.message());
            }

            if (running_) {
                DoAccept();
            }
        });
}

// ============================================================
// 会话管理
// ============================================================
void WsServer::AddSession(std::shared_ptr<WsSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_[session->GetId()] = std::move(session);
}

void WsServer::RemoveSession(const std::string& id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(id);
}

std::size_t WsServer::GetSessionCount() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

// ============================================================
// 广播
// ============================================================
void WsServer::BroadcastText(const std::string& text) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& kv : sessions_) {
        if (kv.second && kv.second->IsOpen()) {
            kv.second->SendText(text);
        }
    }
}

void WsServer::BroadcastBinary(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& kv : sessions_) {
        if (kv.second && kv.second->IsOpen()) {
            kv.second->SendBinary(data);
        }
    }
}

} // namespace ws