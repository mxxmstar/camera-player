#ifndef BCM_BCMTOOL_H
#define BCM_BCMTOOL_H

#include <cstdint>
#include <string>
#include <vector>

#include <winsock2.h>

#include "bcm/bcm_types.h"

namespace bcm {

// 纯 C++ 封装，替代原 Qt BcmTool 类
//
// - RAII 管理 WSAStartup/WSACleanup
// - 组合 BcmRPC 与 FileTransferServer
// - 仅同步 API；调用方如需异步请自行 std::thread 包装
class BcmTool {
public:
    BcmTool();
    ~BcmTool();

    BcmTool(const BcmTool&) = delete;
    BcmTool& operator=(const BcmTool&) = delete;

    void setLogCallback(LogCallback cb) { logCb_ = std::move(cb); }

    // 同步 API —— 返回 BCM 错误码（0 = 成功）
    int32_t reboot(const std::string& deviceIP);
    int32_t readConfig(const std::string& deviceIP);
    int32_t writeConfig(const std::string& deviceIP,
                        const std::string& name,
                        const std::string& val);
    int32_t healthCheck(const std::string& deviceIP, uint16_t pid);
    int32_t getVersion(const std::string& deviceIP);

    // 完整升级流程：内部启动 FileTransferServer 线程 + BcmRPC fullInstall
    int32_t fullInstall(const std::string& fileName,
                        const std::vector<uint8_t>& fileBytes,
                        const std::string& deviceIP,
                        const std::string& hostIP);

private:
    WSADATA wsaData_{};
    LogCallback logCb_;
};

} // namespace bcm

#endif // BCM_BCMTOOL_H
