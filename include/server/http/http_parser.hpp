#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <algorithm>
#include <map>
#include <string>
#include <string_view>

namespace http {

// -------------------------------------------------------------------
// HTTP request method
// -------------------------------------------------------------------
enum class Method
{
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    UNKNOWN
};

// -------------------------------------------------------------------
// HTTP request
// -------------------------------------------------------------------
struct Request
{
    Method                    method = Method::UNKNOWN;
    std::string               uri;
    std::string               http_version;
    std::map<std::string, std::string> headers;
    std::string               body;

    std::string path() const
    {
        // Strip query string from URI to get the path
        auto pos = uri.find('?');
        return (pos == std::string::npos) ? uri : uri.substr(0, pos);
    }

    std::string query_string() const
    {
        auto pos = uri.find('?');
        return (pos == std::string::npos) ? "" : uri.substr(pos + 1);
    }
};

// -------------------------------------------------------------------
// HTTP parser state machine
// -------------------------------------------------------------------
enum class ParseState
{
    METHOD,
    URI,
    VERSION,
    HEADER_KEY,
    HEADER_VALUE,
    BODY,
    DONE,
    ERROR
};

class Parser
{
public:
    /// Feed data into the parser.  Returns the number of bytes consumed,
    /// or 0 if more data is needed, or -1 on parse error.
    std::size_t feed(const char *data, std::size_t size);

    /// Returns true when a complete HTTP request has been parsed.
    bool completed() const { return state_ == ParseState::DONE; }

    /// Returns true if a parse error occurred.
    bool has_error() const { return state_ == ParseState::ERROR; }

    /// Returns the parsed request (valid only when completed() is true).
    const Request &request() const { return req_; }

    /// Reset the parser for re-use.
    void reset();

private:
    ParseState state_ = ParseState::METHOD;
    Request    req_;
    std::string buffer_;
    std::string current_key_;

    bool try_consume_line(std::string_view &input, std::string_view &line);
};

} // namespace http

#endif // HTTP_PARSER_HPP