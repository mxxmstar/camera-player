#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP

#include "server/http/http_parser.hpp"
#include "server/http/http_response.hpp"
#include "server/http/router.hpp"

#include <asio.hpp>
#include <memory>

namespace http {

// -------------------------------------------------------------------
// Session: manages a single TCP connection.
//
//   Reads data from socket -> parses HTTP -> dispatches via Router
//   -> writes response back -> continues reading (keep-alive).
// -------------------------------------------------------------------
class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(asio::ip::tcp::socket socket, const Router &router);

    /// Start reading the first request.
    void start();

private:
    void do_read();
    void do_write(Response response);

    asio::ip::tcp::socket socket_;
    const Router        &router_;
    Parser               parser_;
    std::array<char, 4096> read_buf_;
};

} // namespace http

#endif // HTTP_SESSION_HPP