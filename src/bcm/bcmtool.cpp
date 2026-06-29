#include "bcm/bcmtool.h"

#include <atomic>
#include <thread>

#include "bcm/bcm_rpc.h"
#include "bcm/file_transfer_server.h"

namespace bcm {

BcmTool::BcmTool()
{
    WSAStartup(MAKEWORD(2, 2), &wsaData_);
}

BcmTool::~BcmTool()
{
    WSACleanup();
}

int32_t BcmTool::reboot(const std::string& deviceIP)
{
    BcmRPC rpc(deviceIP);
    rpc.setLogCallback(logCb_);
    return rpc.reboot();
}

int32_t BcmTool::readConfig(const std::string& deviceIP)
{
    BcmRPC rpc(deviceIP);
    rpc.setLogCallback(logCb_);
    return rpc.readConfig();
}

int32_t BcmTool::writeConfig(const std::string& deviceIP,
                             const std::string& name,
                             const std::string& val)
{
    BcmRPC rpc(deviceIP);
    rpc.setLogCallback(logCb_);
    return rpc.writeConfig(name, val);
}

int32_t BcmTool::healthCheck(const std::string& deviceIP, uint16_t pid)
{
    BcmRPC rpc(deviceIP);
    rpc.setLogCallback(logCb_);
    return rpc.healthCheck(pid);
}

int32_t BcmTool::getVersion(const std::string& deviceIP)
{
    BcmRPC rpc(deviceIP);
    rpc.setLogCallback(logCb_);
    return rpc.getVersion();
}

int32_t BcmTool::fullInstall(const std::string& fileName,
                             const std::vector<uint8_t>& fileBytes,
                             const std::string& deviceIP,
                             const std::string& hostIP)
{
    // 同步双阶段流程：
    // 1) FileTransferServer 在独立线程中 listen/accept/send
    // 2) 主线程在 ready 回调里发起 BcmRPC::fullInstall
    //
    // ready 回调发生在 server 线程 listen 完成之后，故 RPC 调用需在主线程进行
    // 这里用 promise/atomic + 主线程等待 ready，再发起 RPC

    std::atomic<bool> readyFlag{false};
    std::atomic<uint16_t> readyPort{0};
    std::atomic<int32_t> serverRet{BCM_ERR_UNKNOWN};

    FileTransferServer server(fileName, fileBytes, hostIP);
    server.setLogCallback(logCb_);
    server.setReadyCallback([&](uint16_t port, const std::string& /*name*/,
                                uint32_t /*size*/, const std::string& /*ip*/) {
        readyPort.store(port, std::memory_order_release);
        readyFlag.store(true, std::memory_order_release);
    });

    // 启动 server 线程
    std::thread serverThread([&] {
        int32_t r = server.start();
        serverRet.store(r, std::memory_order_release);
    });

    // 等待 server ready
    while (!readyFlag.load(std::memory_order_acquire)) {
        // 简单自旋；实际场景下可加短 sleep
        std::this_thread::yield();
    }

    uint16_t port = readyPort.load(std::memory_order_acquire);

    BcmRPC rpc(deviceIP);
    rpc.setLogCallback(logCb_);
    int32_t rpcRet = rpc.fullInstall(port, fileName,
                                     static_cast<uint32_t>(fileBytes.size()),
                                     hostIP);

    serverThread.join();

    // 优先返回 RPC 错误，RPC 成功则看 server 结果
    if (rpcRet != BCM_ERR_OK) {
        return rpcRet;
    }
    return serverRet.load(std::memory_order_acquire);
}

} // namespace bcm
