#ifndef BCM_BCM_RPC_H
#define BCM_BCM_RPC_H

#include <cstdint>
#include <string>

#include "bcm/bcm_types.h"

// 引入 bcmtool 纯 C 库头文件（用 extern "C" 包裹，确保 C++ 调用方按 C 符号链接）
extern "C" {
#include "bcm_common.h"
#include "bcm_config.h"
#include "bcm_dmon.h"
#include "bcm_update.h"
#include "rpc_connect.h"
}

namespace bcm {

// 纯 C++ 封装，替代原 Qt BcmRPC 类
//
// 仅提供同步 API；调用方如需异步，请自行 std::thread 包装
class BcmRPC {
public:
    explicit BcmRPC(std::string deviceIP);
    ~BcmRPC();

    BcmRPC(const BcmRPC&) = delete;
    BcmRPC& operator=(const BcmRPC&) = delete;

    void setLogCallback(LogCallback cb) { logCb_ = std::move(cb); }

    // 同步 API —— 返回 BCM 错误码（0 = 成功）
    int32_t reboot();
    int32_t readConfig();
    int32_t writeConfig(const std::string& name, const std::string& val);
    int32_t healthCheck(uint16_t pid);
    int32_t getVersion();
    int32_t fullInstall(uint16_t serverPort,
                        const std::string& fileName,
                        uint32_t fileSize,
                        const std::string& ipv4_str_server);

    // 工具：根据配置项名称构造写消息
    // 返回写入字节数（成功）或 BCM_ERR_INVAL_PARAMS（失败）
    static uint32_t writeConfigMsg(const std::string& name,
                                   const std::string& val,
                                   uint8_t* msg);

private:
    void emitLog(const std::string& s, PrintLevel lv = PrintLevel::Info) {
        if (logCb_) logCb_(s, lv);
    }

    void showConfig(CONFIG_RpcMsg* readMsg);

    std::string deviceIP_;
    LogCallback logCb_;
};

} // namespace bcm

#endif // BCM_BCM_RPC_H
