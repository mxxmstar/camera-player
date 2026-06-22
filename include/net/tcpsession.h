#pragma once
#include <asio.hpp>
#include <memory>
#include <functional>
#include <string>
#include <array>
#include <sstream>
#include <iomanip>
#include <random>

class AsioTCPSession : public std::enable_shared_from_this<AsioTCPSession> {
public:
    explicit AsioTCPSession(asio::ip::tcp::socket socket)
        : socket_(std::move(socket))
    {
        std::ostringstream oss;
        std::random_device rd;
        std::mt19937 gen(rd());
        oss << std::hex << std::setw(8) << std::setfill('0')
            << std::uniform_int_distribution<uint32_t>(0, 0xFFFFFFFF)(gen);
        session_id_ = oss.str();
    }

    virtual ~AsioTCPSession() = default;

    void Start() {
        do_read();
    }

    void Send(const std::string& data) {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::string>(data);
        asio::async_write(socket_, asio::buffer(*buf),
            [this, self, buf](asio::error_code ec, size_t) {
                if (ec && ec != asio::error::eof) {
                    handle_error(ec);
                }
            });
    }

    void SetCloseHandler(std::function<void()> handler) {
        close_handler_ = std::move(handler);
    }

    std::string GetSessionID() const { return session_id_; }

    std::string GetLocalAddress() const {
        asio::error_code ec;
        auto ep = socket_.local_endpoint(ec);
        if (ec) return "0.0.0.0";
        return ep.address().to_string();
    }

    asio::io_context& GetIOContext() {
        return static_cast<asio::io_context&>(socket_.get_executor().context());
    }

protected:
    virtual void OnBytes(const uint8_t* data, size_t size) = 0;
    virtual void OnClose() {}

    void handle_error(asio::error_code ec) {
        if (ec == asio::error::eof || ec == asio::error::connection_reset ||
            ec == asio::error::connection_aborted) {
            OnClose();
            if (close_handler_) close_handler_();
        }
    }

    void do_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(read_buf_),
            [this, self](asio::error_code ec, size_t length) {
                if (!ec) {
                    OnBytes(read_buf_.data(), length);
                    do_read();
                } else {
                    handle_error(ec);
                }
            });
    }

    asio::ip::tcp::socket socket_;
    std::array<uint8_t, 65536> read_buf_{};
    std::string session_id_;
    std::function<void()> close_handler_;
};
