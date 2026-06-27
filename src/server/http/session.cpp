#include "server/http/session.hpp"
#include "server/http/http_parser.hpp"
#include "server/http/http_response.hpp"

#include "log/logger.h"

namespace http {

Session::Session(asio::ip::tcp::socket socket, const Router &router)
    : socket_(std::move(socket)), router_(router)
{
}

void Session::start()
{    
    LOG_INFO("[Session] New connection from {}:{}",
            socket_.remote_endpoint().address().to_string(),
            socket_.remote_endpoint().port());
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
                    LOG_ERROR("[Session] Read error: {}", ec.message());
                return;
            }

            std::size_t consumed = parser_.feed(read_buf_.data(), length);

            if (parser_.has_error())
            {
                do_write(Response::bad_request("Malformed HTTP request"));
                return;
            }

            if (parser_.completed())
            {
                const auto &req = parser_.request();
                LOG_DEBUG("[Session] {} {}", static_cast<int>(req.method), req.uri);
                Response resp = router_.dispatch(req);
                do_write(std::move(resp));
            }
            else
            {
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
                LOG_ERROR("[Session] Write error: {}", ec.message());
                return;
            }

            parser_.reset();
            do_read();
        });
}

} // namespace http