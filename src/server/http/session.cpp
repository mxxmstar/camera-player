#include "server/http/session.hpp"
#include "server/http/http_parser.hpp"
#include "server/http/http_response.hpp"

#include <iostream>

namespace http {

Session::Session(asio::ip::tcp::socket socket, const Router &router)
    : socket_(std::move(socket)), router_(router)
{
}

void Session::start()
{
    std::cout << "[Session] New connection from "
              << socket_.remote_endpoint().address().to_string() << ":"
              << socket_.remote_endpoint().port() << std::endl;
    do_read();
}

void Session::do_read()
{
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buf_),
        [this, self](asio::error_code ec, std::size_t length)
        {
            if (ec)
            {
                if (ec != asio::error::eof)
                    std::cerr << "[Session] Read error: " << ec.message() << std::endl;
                return;
            }

            // Feed data to the parser
            std::size_t consumed = parser_.feed(read_buf_.data(), length);

            if (parser_.has_error())
            {
                do_write(Response::bad_request("Malformed HTTP request"));
                return;
            }

            if (parser_.completed())
            {
                // Route and respond
                const auto &req = parser_.request();
                std::cout << "[Session] " << static_cast<int>(req.method)
                          << " " << req.uri << std::endl;
                Response resp = router_.dispatch(req);
                do_write(std::move(resp));
            }
            else
            {
                // Need more data
                do_read();
            }
        });
}

void Session::do_write(Response response)
{
    auto self = shared_from_this();
    auto response_str = std::make_shared<std::string>(response.to_string());

    asio::async_write(
        socket_,
        asio::buffer(*response_str),
        [this, self, response_str](asio::error_code ec, std::size_t /*bytes*/)
        {
            if (ec)
            {
                std::cerr << "[Session] Write error: " << ec.message() << std::endl;
                return;
            }

            parser_.reset();
            do_read();
        });
}

} // namespace http