#include "server/http/router.hpp"

namespace http {

// -------------------------------------------------------------------
// Index into routes_ array by Method
// -------------------------------------------------------------------
static int method_index(Method m)
{
    return static_cast<int>(m);
}

// -------------------------------------------------------------------
// Register routes
// -------------------------------------------------------------------
void Router::add_route(Method method, const std::string &path, RouteHandler handler)
{
    routes_[method_index(method)][path] = std::move(handler);
}

void Router::get(const std::string &path, RouteHandler handler)
{
    add_route(Method::GET, path, std::move(handler));
}

void Router::post(const std::string &path, RouteHandler handler)
{
    add_route(Method::POST, path, std::move(handler));
}

void Router::put(const std::string &path, RouteHandler handler)
{
    add_route(Method::PUT, path, std::move(handler));
}

void Router::del(const std::string &path, RouteHandler handler)
{
    add_route(Method::DELETE, path, std::move(handler));
}

void Router::set_fallback(RouteHandler handler)
{
    fallback_ = std::move(handler);
}

// -------------------------------------------------------------------
// Dispatch
// -------------------------------------------------------------------
Response Router::dispatch(const Request &req) const
{
    const auto &method_routes = routes_[method_index(req.method)];

    // 1. Try exact-path match for this method
    auto it = method_routes.find(req.path());
    if (it != method_routes.end())
    {
        return it->second(req);
    }

    // 2. If method is not found at all, check whether the path exists
    //    under any other method -> 405 Method Not Allowed
    bool path_exists = false;
    for (int m = 0; m <= method_index(Method::UNKNOWN); ++m)
    {
        if (routes_[m].count(req.path()))
        {
            path_exists = true;
            break;
        }
    }

    if (path_exists)
    {
        return Response::internal_error("Method Not Allowed")
            .status(StatusCode::METHOD_NOT_ALLOWED);
    }

    // 3. No match at all
    if (fallback_)
        return fallback_(req);

    return Response::not_found("Not Found: " + req.path());
}

} // namespace http