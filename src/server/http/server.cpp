#include "server/http/server.hpp"
#include "server/http/session.hpp"

#include <iostream>

namespace http {

Server::Server(const std::string &address, unsigned short port)
    : acceptor_(io_context_,
                asio::ip::tcp::endpoint(asio::ip::make_address(address), port))
{
    std::cout << "[Server] Listening on " << address << ":" << port << std::endl;
}

void Server::run()
{
    do_accept();
    std::cout << "[Server] Event loop started." << std::endl;
    io_context_.run();
}

void Server::stop()
{
    std::cout << "[Server] Stopping..." << std::endl;
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
                std::cerr << "[Server] Accept error: " << ec.message() << std::endl;
            }

            // Accept the next connection
            do_accept();
        });
}

} // namespace http