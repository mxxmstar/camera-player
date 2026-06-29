#ifndef BCM_BCM_TYPES_H
#define BCM_BCM_TYPES_H

#include <cstdint>
#include <functional>
#include <string>

namespace bcm {

// 日志级别（替代原 console.h 中的 PrintLevel）
enum class PrintLevel : uint8_t {
    Info = 0,
    Warn,
    Error,
};

// 日志回调（替代原 Qt logPrint 信号）
using LogCallback = std::function<void(const std::string&, PrintLevel)>;

// FileTransferServer 在 listen 就绪后回调，向调用方报告实际端口、文件名、文件大小、主机 IP
using ReadyCallback = std::function<void(uint16_t serverPort,
                                         const std::string& fileName,
                                         uint32_t fileSize,
                                         const std::string& hostIP)>;

} // namespace bcm

#endif // BCM_BCM_TYPES_H
