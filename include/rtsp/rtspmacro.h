#pragma once
#include <iostream>
#include <sstream>

namespace rtsp {
namespace detail {
    inline void log_build_args(std::ostringstream&) {}

    template<typename T, typename... Args>
    void log_build_args(std::ostringstream& os, const T& val, Args&&... args) {
        size_t pos = os.str().find("{}", 0);
        if (pos != std::string::npos) {
            std::string s = os.str();
            os.str("");
            os << s.substr(0, pos) << val << s.substr(pos + 2);
        }
        log_build_args(os, std::forward<Args>(args)...);
    }

    template<typename... Args>
    std::string log_format(const std::string& fmt, Args&&... args) {
        std::ostringstream os;
        os << fmt;
        log_build_args(os, std::forward<Args>(args)...);
        return os.str();
    }
}
}

#define LOG_RTSP_DEBUG(...)   do { std::cout << "[DEBUG] " << rtsp::detail::log_format(__VA_ARGS__) << std::endl; } while(0)
#define LOG_RTSP_INFO(...)    do { std::cout << "[INFO]  " << rtsp::detail::log_format(__VA_ARGS__) << std::endl; } while(0)
#define LOG_RTSP_WARN(...)    do { std::cout << "[WARN]  " << rtsp::detail::log_format(__VA_ARGS__) << std::endl; } while(0)
#define LOG_RTSP_ERROR(...)   do { std::cout << "[ERROR] " << rtsp::detail::log_format(__VA_ARGS__) << std::endl; } while(0)
#define LOG_RTSP_CRITICAL(...) do { std::cout << "[CRIT]  " << rtsp::detail::log_format(__VA_ARGS__) << std::endl; } while(0)

#define LOG_RTSP_DEBUG_FL(file, line, ...)   do { std::string _m = rtsp::detail::log_format(__VA_ARGS__); std::cout << "[DEBUG] [" << file << "#" << line << "] " << _m << std::endl; } while(0)
#define LOG_RTSP_INFO_FL(file, line, ...)    do { std::string _m = rtsp::detail::log_format(__VA_ARGS__); std::cout << "[INFO]  [" << file << "#" << line << "] " << _m << std::endl; } while(0)
#define LOG_RTSP_WARN_FL(file, line, ...)    do { std::string _m = rtsp::detail::log_format(__VA_ARGS__); std::cout << "[WARN]  [" << file << "#" << line << "] " << _m << std::endl; } while(0)
#define LOG_RTSP_ERROR_FL(file, line, ...)   do { std::string _m = rtsp::detail::log_format(__VA_ARGS__); std::cout << "[ERROR] [" << file << "#" << line << "] " << _m << std::endl; } while(0)
#define LOG_RTSP_CRITICAL_FL(file, line, ...) do { std::string _m = rtsp::detail::log_format(__VA_ARGS__); std::cout << "[CRIT]  [" << file << "#" << line << "] " << _m << std::endl; } while(0)

#define LOG_RTSP_DEBUG_AT(...)   LOG_RTSP_DEBUG_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_RTSP_INFO_AT(...)    LOG_RTSP_INFO_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_RTSP_WARN_AT(...)    LOG_RTSP_WARN_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_RTSP_ERROR_AT(...)   LOG_RTSP_ERROR_FL(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_RTSP_CRITICAL_AT(...) LOG_RTSP_CRITICAL_FL(__FILE__, __LINE__, __VA_ARGS__)
