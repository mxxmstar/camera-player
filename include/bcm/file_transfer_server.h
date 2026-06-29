#ifndef BCM_FILE_TRANSFER_SERVER_H
#define BCM_FILE_TRANSFER_SERVER_H

#include <cstdint>
#include <string>
#include <vector>

#include "bcm/bcm_types.h"

namespace bcm {

// 纯 C++ 实现的文件传输服务器（替代原 Qt FileTransferServer）
//
// 用法：
//   FileTransferServer s(fileName, fileBytes, hostIP);
//   s.setLogCallback(...);
//   s.setReadyCallback([...](port, ...){ /* 在此处触发 BcmRPC::fullInstall */ });
//   int ret = s.start();   // 阻塞，直到设备拉取完文件或出错
class FileTransferServer {
public:
    FileTransferServer(std::string fileName,
                       std::vector<uint8_t> fileBytes,
                       std::string hostIP);
    ~FileTransferServer();

    FileTransferServer(const FileTransferServer&) = delete;
    FileTransferServer& operator=(const FileTransferServer&) = delete;

    void setLogCallback(LogCallback cb) { logCb_ = std::move(cb); }
    void setReadyCallback(ReadyCallback cb) { readyCb_ = std::move(cb); }

    // 同步阻塞执行：socket -> bind -> listen -> getsockname -> 回调 ready
    // -> accept -> 循环 send 全部字节 -> 关闭
    // 返回 0 表示成功，非 0 为 BCM 错误码
    int32_t start();

private:
    std::string fileName_;
    std::vector<uint8_t> fileBytes_;
    std::string hostIP_;
    LogCallback logCb_;
    ReadyCallback readyCb_;
};

} // namespace bcm

#endif // BCM_FILE_TRANSFER_SERVER_H
