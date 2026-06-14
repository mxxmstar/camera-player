#include "server/http/http_response.hpp"

#include <sstream>

namespace http {

// -------------------------------------------------------------------
// Static: reason phrase lookup
// -------------------------------------------------------------------
std::string Response::reason_phrase(StatusCode code)
{
    switch (code)
    {
    case StatusCode::OK:                  return "OK";
    case StatusCode::CREATED:             return "Created";
    case StatusCode::NO_CONTENT:          return "No Content";
    case StatusCode::MOVED_PERMANENTLY:   return "Moved Permanently";
    case StatusCode::FOUND:               return "Found";
    case StatusCode::BAD_REQUEST:         return "Bad Request";
    case StatusCode::NOT_FOUND:           return "Not Found";
    case StatusCode::METHOD_NOT_ALLOWED:  return "Method Not Allowed";
    case StatusCode::INTERNAL_SERVER_ERROR: return "Internal Server Error";
    default:                              return "Unknown";
    }
}

// -------------------------------------------------------------------
// Builder methods
// -------------------------------------------------------------------
Response &Response::status(StatusCode code)
{
    code_ = code;
    return *this;
}

Response &Response::header(const std::string &key, const std::string &value)
{
    headers_[key] = value;
    return *this;
}

Response &Response::body(const std::string &body)
{
    body_ = body;
    return *this;
}

Response &Response::content_type(const std::string &type)
{
    headers_["Content-Type"] = type;
    return *this;
}

// -------------------------------------------------------------------
// Build wire-format HTTP response
// -------------------------------------------------------------------
std::string Response::to_string() const
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << static_cast<int>(code_) << " "
        << reason_phrase(code_) << "\r\n";

    for (const auto &[key, value] : headers_)
    {
        oss << key << ": " << value << "\r\n";
    }

    oss << "Content-Length: " << body_.size() << "\r\n";
    oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    oss << body_;

    return oss.str();
}

// -------------------------------------------------------------------
// Static shortcuts
// -------------------------------------------------------------------
Response Response::ok(const std::string &body, const std::string &content_type)
{
    Response r;
    r.status(StatusCode::OK)
     .content_type(content_type)
     .body(body);
    return r;
}

Response Response::not_found(const std::string &msg)
{
    Response r;
    r.status(StatusCode::NOT_FOUND)
     .content_type("text/plain")
     .body(msg);
    return r;
}

Response Response::bad_request(const std::string &msg)
{
    Response r;
    r.status(StatusCode::BAD_REQUEST)
     .content_type("text/plain")
     .body(msg);
    return r;
}

Response Response::internal_error(const std::string &msg)
{
    Response r;
    r.status(StatusCode::INTERNAL_SERVER_ERROR)
     .content_type("text/plain")
     .body(msg);
    return r;
}

} // namespace http