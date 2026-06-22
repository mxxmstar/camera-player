#pragma once
#include <asio.hpp>
#include <memory>
#include <functional>
#include <cstdint>
#include "net/asio_io_context_pool.h"

class AsioTCPServer {
public:
    using AcceptHandler = std::function<void(asio::ip::tcp::socket)>;

    AsioTCPServer(asio::io_context& io_context, AsioIOContextPool& pool, uint16_t port)
        : acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {
        (void)pool;
    }

    void SetAcceptHandler(AcceptHandler handler) {
        accept_handler_ = std::move(handler);
    }

    void Start() {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](asio::error_code ec, asio::ip::tcp::socket socket) {
                if (!ec && accept_handler_) {
                    accept_handler_(std::move(socket));
                }
                do_accept();
            });
    }

    asio::ip::tcp::acceptor acceptor_;
    AcceptHandler accept_handler_;
};
