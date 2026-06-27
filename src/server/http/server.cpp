#include "server/http/server.hpp"
#include "server/http/session.hpp"
#include "log/logger.h"

#include <iostream>

namespace http {

Server::Server(const std::string &address, unsigned short port)
    : acceptor_(io_context_,
                asio::ip::tcp::endpoint(asio::ip::make_address(address), port))
{
    LOG_INFO("[Server] Listening on {}:{}", address, port);
}

void Server::run()
{
    do_accept();
    LOG_INFO("[Server] Event loop started.");
    io_context_.run();
}

void Server::stop()
{
    LOG_INFO("[Server] Stopping...");
    io_context_.stop();
}

void Server::do_accept()
{
    acceptor_.async_accept(
        [this](asio::error_code ec, asio::ip::tcp::socket socket)
        {
            if (!ec)
            {
                auto session = std::make_shared<Session>(std::move(socket), router_);
                session->start();
            }
            else
            {
                LOG_ERROR("[Server] Accept error: {}", ec.message());
            }

            do_accept();
        });
}

} // namespace http