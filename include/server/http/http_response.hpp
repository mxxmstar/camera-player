#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <map>
#include <string>

namespace http {

// -------------------------------------------------------------------
// HTTP status code
// -------------------------------------------------------------------
enum class StatusCode
{
    OK                  = 200,
    CREATED             = 201,
    NO_CONTENT          = 204,
    MOVED_PERMANENTLY   = 301,
    FOUND               = 302,
    BAD_REQUEST         = 400,
    NOT_FOUND           = 404,
    METHOD_NOT_ALLOWED  = 405,
    INTERNAL_SERVER_ERROR = 500
};

// -------------------------------------------------------------------
// HTTP response builder
// -------------------------------------------------------------------
class Response
{
public:
    Response() = default;

    // --- Builder pattern ---
    Response &status(StatusCode code);
    Response &header(const std::string &key, const std::string &value);
    Response &body(const std::string &body);
    Response &content_type(const std::string &type);

    // --- Build the full HTTP response string ---
    std::string to_string() const;

    // --- Shortcuts ---
    static Response ok(const std::string &body, const std::string &content_type = "text/plain");
    static Response not_found(const std::string &msg = "Not Found");
    static Response bad_request(const std::string &msg = "Bad Request");
    static Response internal_error(const std::string &msg = "Internal Server Error");

    // --- Accessors ---
    StatusCode status_code() const { return code_; }
    const std::map<std::string, std::string> &headers() const { return headers_; }
    const std::string &body_str() const { return body_; }

private:
    static std::string reason_phrase(StatusCode code);

    StatusCode code_ = StatusCode::OK;
    std::map<std::string, std::string> headers_;
    std::string body_;
};

} // namespace http

#endif // HTTP_RESPONSE_HPP