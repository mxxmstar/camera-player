#include "server/http/http_parser.hpp"

#include <cctype>
#include <cstring>
#include <sstream>

namespace http {

// -------------------------------------------------------------------
// Helper: consume one line ("\r\n" or "\n") from input
// -------------------------------------------------------------------
bool Parser::try_consume_line(std::string_view &input, std::string_view &line)
{
    auto pos = input.find('\n');
    if (pos == std::string_view::npos)
        return false;

    // pos points to '\n'
    line = input.substr(0, pos);
    // If line ends with '\r', strip it
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);

    input.remove_prefix(pos + 1);
    return true;
}

// -------------------------------------------------------------------
// Reset parser
// -------------------------------------------------------------------
void Parser::reset()
{
    state_ = ParseState::METHOD;
    req_ = Request{};
    buffer_.clear();
    current_key_.clear();
}

// -------------------------------------------------------------------
// Feed data
// -------------------------------------------------------------------
std::size_t Parser::feed(const char *data, std::size_t size)
{
    if (state_ == ParseState::DONE || state_ == ParseState::ERROR)
        return 0;

    buffer_.append(data, size);
    std::string_view view(buffer_);
    std::size_t consumed = 0;

    // ---------- Request line ----------
    if (state_ == ParseState::METHOD || state_ == ParseState::URI || state_ == ParseState::VERSION)
    {
        std::string_view line;
        if (!try_consume_line(view, line))
            return 0; // need more data

        // Advance consumed past this line
        consumed = buffer_.size() - view.size();

        // Parse: METHOD URI VERSION
        std::string line_str(line);
        std::istringstream iss(line_str);
        std::string method_str, uri, version;

        if (!(iss >> method_str >> uri >> version))
        {
            state_ = ParseState::ERROR;
            return consumed;
        }

        // Method
        if (method_str == "GET")         req_.method = Method::GET;
        else if (method_str == "POST")    req_.method = Method::POST;
        else if (method_str == "PUT")     req_.method = Method::PUT;
        else if (method_str == "DELETE")  req_.method = Method::DELETE;
        else if (method_str == "HEAD")    req_.method = Method::HEAD;
        else if (method_str == "OPTIONS") req_.method = Method::OPTIONS;
        else if (method_str == "PATCH")   req_.method = Method::PATCH;
        else                              req_.method = Method::UNKNOWN;

        req_.uri = uri;
        req_.http_version = version;

        // Check version
        if (version != "HTTP/1.0" && version != "HTTP/1.1")
        {
            state_ = ParseState::ERROR;
            return consumed;
        }

        state_ = ParseState::HEADER_KEY;
    }

    // ---------- Headers ----------
    if (state_ == ParseState::HEADER_KEY || state_ == ParseState::HEADER_VALUE)
    {
        while (true)
        {
            std::string_view line;
            if (!try_consume_line(view, line))
            {
                // Not enough data yet
                buffer_.erase(0, consumed);
                return consumed;
            }

            std::size_t new_consumed = buffer_.size() - view.size();
            std::size_t line_consumed = new_consumed - consumed;
            consumed = new_consumed;

            // Empty line marks end of headers
            if (line.empty())
            {
                state_ = ParseState::BODY;
                break;
            }

            // Header line: "Key: Value"
            auto colon = line.find(':');
            if (colon == std::string_view::npos)
            {
                state_ = ParseState::ERROR;
                return consumed;
            }

            std::string key(line.substr(0, colon));
            // Skip leading whitespace after ':'
            auto val_start = colon + 1;
            while (val_start < line.size() && std::isspace(static_cast<unsigned char>(line[val_start])))
                ++val_start;
            std::string value(line.substr(val_start));

            // Lowercase the key for case-insensitive lookups
            for (auto &ch : key)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

            req_.headers[std::move(key)] = std::move(value);
        }
    }

    // ---------- Body ----------
    if (state_ == ParseState::BODY)
    {
        auto it = req_.headers.find("content-length");
        if (it != req_.headers.end())
        {
            std::size_t content_length = std::stoul(it->second);
            std::size_t body_size = view.size();

            if (body_size < content_length)
            {
                // Need more body data
                buffer_.erase(0, consumed);
                return consumed;
            }

            req_.body = std::string(view.substr(0, content_length));
            consumed += content_length;
        }

        state_ = ParseState::DONE;
    }

    buffer_.erase(0, consumed);
    return consumed;
}

} // namespace http