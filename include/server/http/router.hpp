#ifndef HTTP_ROUTER_HPP
#define HTTP_ROUTER_HPP

#include "server/http/http_parser.hpp"
#include "server/http/http_response.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace http {

// -------------------------------------------------------------------
// Route handler type:  takes a Request and returns a Response
// -------------------------------------------------------------------
using RouteHandler = std::function<Response(const Request &)>;

// -------------------------------------------------------------------
// Router: maps (Method + Path) -> Handler
// -------------------------------------------------------------------
class Router
{
public:
    Router() = default;

    /// Register a handler for a specific method + path.
    void add_route(Method method, const std::string &path, RouteHandler handler);

    /// Convenience wrappers.
    void get(const std::string &path, RouteHandler handler);
    void post(const std::string &path, RouteHandler handler);
    void put(const std::string &path, RouteHandler handler);
    void del(const std::string &path, RouteHandler handler);

    /// Dispatch a request to the matching handler.
    /// Returns the handler's response, or a 404 / 405 response.
    Response dispatch(const Request &req) const;

    /// Set a fallback handler for when no route matches.
    void set_fallback(RouteHandler handler);

private:
    using RouteMap = std::unordered_map<std::string, RouteHandler>;

    RouteMap routes_[static_cast<int>(Method::UNKNOWN) + 1];
    RouteHandler fallback_;
};

} // namespace http

#endif // HTTP_ROUTER_HPP