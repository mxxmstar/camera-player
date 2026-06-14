#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "server/http/router.hpp"
#include "server/http/session.hpp"

#include <asio.hpp>
#include <memory>
#include <string>

namespace http {

// -------------------------------------------------------------------
// Server: accepts TCP connections and creates Session objects.
// -------------------------------------------------------------------
class Server
{
public:
    /// Construct a server that will listen on the given address and port.
    explicit Server(const std::string &address, unsigned short port);

    /// Start accepting connections.  This call will block until the
    /// io_context is stopped.
    void run();

    /// Returns a reference to the router so the caller can register routes.
    Router &router() { return router_; }

    /// Graceful shutdown.
    void stop();

private:
    void do_accept();

    asio::io_context        io_context_;
    asio::ip::tcp::acceptor acceptor_;
    Router                  router_;
};

} // namespace http

#endif // HTTP_SERVER_HPP