#include "bcm/bcm_rpc.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <winsock2.h>
#include <ws2tcpip.h>

namespace bcm {

namespace {
constexpr uint16_t kDefaultPort = 5555;
constexpr uint32_t kDefaultTimeoutMs = 60000;

// 把 BCM 错误码格式化为 0xXX 字符串
std::string formatHex(int32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%02x", static_cast<unsigned>(v));
    return buf;
}
} // namespace

BcmRPC::BcmRPC(std::string deviceIP)
    : deviceIP_(std::move(deviceIP)) {}

BcmRPC::~BcmRPC() = default;

int32_t BcmRPC::reboot()
{
    int32_t retVal;
    BCM_HandleType hdl = 0;

    retVal = RPC_Open(deviceIP_.c_str(), kDefaultPort, kDefaultTimeoutMs, &hdl);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC connection failed, ret: " + formatHex(retVal), PrintLevel::Error);
        return retVal;
    }

    retVal = DMON_Reboot(hdl);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC reboot failed, ret: " + formatHex(retVal), PrintLevel::Error);
    } else {
        emitLog("Device restarted successfully.");
    }

    RPC_Close(hdl);
    return retVal;
}

void BcmRPC::showConfig(CONFIG_RpcMsg* readMsg)
{
    uint32_t index = 0;
    char name[32];
    char val[32];
    uint32_t len;

    emitLog(" ");
    emitLog("***GET CONFIG***");

    while (index < readMsg->len) {
        BCM_MemSet(name, 0, sizeof(name));
        BCM_MemSet(val, 0, sizeof(val));
        len = CONFIG_ExtractItem(&readMsg->ctx[index], name, val);
        if (len > 4) {
            emitLog(std::string(name) + ": " + val);
            index += len;
        } else {
            break;
        }
    }

    emitLog(" ");
}

int32_t BcmRPC::readConfig()
{
    int32_t retVal;
    BCM_HandleType hdl = 0;
    CONFIG_RpcMsg readMsg{};

    retVal = RPC_Open(deviceIP_.c_str(), kDefaultPort, kDefaultTimeoutMs, &hdl);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC connection failed, ret: " + formatHex(retVal), PrintLevel::Error);
        return retVal;
    }

    retVal = CONFIG_RpcRead(hdl, &readMsg);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC read config failed, ret: " + formatHex(retVal), PrintLevel::Error);
    } else {
        showConfig(&readMsg);
    }

    RPC_Close(hdl);
    return retVal;
}

uint32_t BcmRPC::writeConfigMsg(const std::string& name, const std::string& val, uint8_t* msg)
{
    uint32_t index = 0;
    uint32_t header;
    uint32_t uval = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));

    if (name == "mirror mode") {
        header = CONFIG_ITEM_HEADER_R(CONFIG_MEDIA_MIRROR, 1);
        BCM_MemCpy(msg + index, &header, 4);
        index += 4;
        msg[index] = static_cast<uint8_t>(uval);
    } else if (name == "FPS") {
        header = CONFIG_ITEM_HEADER_R(CONFIG_MEDIA_FPS, 1);
        BCM_MemCpy(msg + index, &header, 4);
        index += 4;
        msg[index] = static_cast<uint8_t>(uval);
    } else if (name == "SOMEIP UDP port") {
        header = CONFIG_ITEM_HEADER_R(CONFIG_MEDIA_SOMEIPUDPPORT, 2);
        BCM_MemCpy(msg + index, &header, 4);
        index += 4;
        msg[index] = (uval >> 8) & 0xFF;
        msg[index + 1] = uval & 0xFF;
    } else if (name == "SOMEIP RTP port") {
        header = CONFIG_ITEM_HEADER_R(CONFIG_MEDIA_SOMEIPRTPPORT, 2);
        BCM_MemCpy(msg + index, &header, 4);
        index += 4;
        msg[index] = (uval >> 8) & 0xFF;
        msg[index + 1] = uval & 0xFF;
    } else if (name == "DHCP") {
        header = CONFIG_ITEM_HEADER_R(CONFIG_NETWORK_DHCP, 1);
        BCM_MemCpy(msg + index, &header, 4);
        index += 4;
        msg[index] = static_cast<uint8_t>(uval);
    } else if (name == "IP") {
        header = CONFIG_ITEM_HEADER_R(CONFIG_NETWORK_IP, 16);
        BCM_MemCpy(msg + index, &header, 4);
        index += 4;
        BCM_MemCpy(msg + index, val.c_str(), 16);
    } else if (name == "MAC") {
        header = CONFIG_ITEM_HEADER_R(CONFIG_NETWORK_MAC, 18);
        BCM_MemCpy(msg + index, &header, 4);
        index += 4;
        BCM_MemCpy(msg + index, val.c_str(), 18);
    } else {
        return BCM_ERR_INVAL_PARAMS;
    }

    return BCM_ERR_OK;
}

int32_t BcmRPC::writeConfig(const std::string& name, const std::string& val)
{
    int32_t retVal;
    BCM_HandleType hdl = 0;
    CONFIG_RpcMsg writeMsg{};

    retVal = static_cast<int32_t>(writeConfigMsg(name, val, writeMsg.ctx));
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC write config msg failed, ret: " + formatHex(retVal), PrintLevel::Error);
        return retVal;
    }

    {
        // 输出前 20 字节 hex
        std::string hexStr;
        hexStr.reserve(20 * 3);
        char buf[8];
        for (size_t i = 0; i < 20; ++i) {
            std::snprintf(buf, sizeof(buf), "%02x ", writeMsg.ctx[i]);
            hexStr += buf;
        }
        emitLog(hexStr);
    }

    retVal = RPC_Open(deviceIP_.c_str(), kDefaultPort, kDefaultTimeoutMs, &hdl);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC connection failed, ret: " + formatHex(retVal), PrintLevel::Error);
        return retVal;
    }

    retVal = CONFIG_RpcWrite(hdl, &writeMsg);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC write config failed, ret: " + formatHex(retVal), PrintLevel::Error);
    } else {
        emitLog("RPC write config successfully.");
    }

    RPC_Close(hdl);
    return retVal;
}

int32_t BcmRPC::healthCheck(uint16_t pid)
{
    int32_t retVal;
    BCM_HandleType hdl = 0;
    IMGL_VersionType aVersion{};

    retVal = RPC_Open(deviceIP_.c_str(), kDefaultPort, kDefaultTimeoutMs, &hdl);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC connection failed, ret: " + formatHex(retVal), PrintLevel::Error);
        return retVal;
    }

    retVal = UPDATE_HealthCheck(hdl, pid, &aVersion);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC health check failed, ret: " + formatHex(retVal), PrintLevel::Error);
    } else {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "RPC health check, magic: 0x%x, major: %u, minor: %u",
                      aVersion.magic, aVersion.major, aVersion.minor);
        emitLog(buf);
    }

    RPC_Close(hdl);
    return retVal;
}

int32_t BcmRPC::getVersion()
{
    int32_t retVal;
    BCM_HandleType hdl = 0;
    DMON_SwVersionMsgType aVersion{};

    retVal = RPC_Open(deviceIP_.c_str(), kDefaultPort, kDefaultTimeoutMs, &hdl);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC connection failed, ret: " + formatHex(retVal), PrintLevel::Error);
        return retVal;
    }

    retVal = DMON_GetSwVersion(hdl, &aVersion);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC get version failed, ret: " + formatHex(retVal), PrintLevel::Error);
    } else {
        std::string str(aVersion.str);
        auto pos = str.find("v1.");
        if (pos != std::string::npos) {
            emitLog("[Version] " + str.substr(pos));
        } else {
            emitLog("[Version] " + str);
        }
    }

    RPC_Close(hdl);
    return retVal;
}

int32_t BcmRPC::fullInstall(uint16_t serverPort,
                            const std::string& fileName,
                            uint32_t fileSize,
                            const std::string& ipv4_str_server)
{
    UPDATE_InstallCfgMsgType install{};
    BCM_HandleType hdl = 0;
    uint32_t eraseSize = 0x1b0000;
    uint32_t flsId = 0;
    struct sockaddr_in address{};
    uint32_t rcvdSz = 0;
    int32_t retVal;

    inet_pton(AF_INET, ipv4_str_server.c_str(), &address.sin_addr);
    install.ipAddr = CPU_BEToNative32(address.sin_addr.s_addr);
    install.nvmChannel = IMGL_CHANNEL_ID_NVM_0 + flsId;
    install.fetchChannel = IMGL_CHANNEL_ID_RPC_FTP;
    install.nvmEraseSize = eraseSize;
    install.fileSize = fileSize;
    install.portNum = CPU_BEToNative16(serverPort);
    BCM_MemCpy(install.name, fileName.c_str(), fileName.size() + 1);

    retVal = RPC_Open(deviceIP_.c_str(), kDefaultPort, kDefaultTimeoutMs, &hdl);
    if (retVal != BCM_ERR_OK) {
        emitLog("RPC connection failed, ret: " + formatHex(retVal), PrintLevel::Error);
        return retVal;
    }

    retVal = UPDATE_FullInstall(hdl, &install, &rcvdSz);
    if (retVal == BCM_ERR_OK) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "The upgrade is successful, the size of the file received by client is %u.",
                      rcvdSz);
        emitLog(buf);
    } else {
        emitLog("Upgrade failed, ret: " + formatHex(retVal), PrintLevel::Error);
    }

    RPC_Close(hdl);

    // 升级成功后自动 reboot
    if (retVal == BCM_ERR_OK) {
        reboot();
    }
    return retVal;
}

} // namespace bcm
