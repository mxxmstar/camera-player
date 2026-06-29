#include "bcm/file_transfer_server.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

#include "bcm_common.h"

namespace bcm {

FileTransferServer::FileTransferServer(std::string fileName,
                                       std::vector<uint8_t> fileBytes,
                                       std::string hostIP)
    : fileName_(std::move(fileName)),
      fileBytes_(std::move(fileBytes)),
      hostIP_(std::move(hostIP)) {}

FileTransferServer::~FileTransferServer() = default;

int32_t FileTransferServer::start()
{
    int32_t ret = 0;
    SOCKET fd0 = INVALID_SOCKET;
    SOCKET fd1 = INVALID_SOCKET;
    struct sockaddr_in address{};
    int32_t addrlen = sizeof(address);
    const int fileSize = static_cast<int>(fileBytes_.size());

    fd0 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd0 == INVALID_SOCKET) {
        if (logCb_) logCb_("The server startup failed.", PrintLevel::Error);
        return BCM_ERR_NODEV;
    }

    address.sin_family = AF_INET;
    if (inet_pton(AF_INET, hostIP_.c_str(), &address.sin_addr) != 1) {
        if (logCb_) logCb_("Invalid host IP.", PrintLevel::Error);
        closesocket(fd0);
        return BCM_ERR_INVAL_PARAMS;
    }
    address.sin_port = htons(0);   // 让系统分配端口

    if (bind(fd0, reinterpret_cast<struct sockaddr*>(&address), addrlen) == SOCKET_ERROR) {
        if (logCb_) logCb_("The server startup failed.", PrintLevel::Error);
        closesocket(fd0);
        return BCM_ERR_NOPERM;
    }

    if (listen(fd0, 2) == SOCKET_ERROR) {
        if (logCb_) logCb_("The server startup failed.", PrintLevel::Error);
        closesocket(fd0);
        return BCM_ERR_NODEV;
    }

    memset(&address, 0, addrlen);
    if (getsockname(fd0, reinterpret_cast<struct sockaddr*>(&address), &addrlen) == SOCKET_ERROR) {
        if (logCb_) logCb_("The server startup failed.", PrintLevel::Error);
        closesocket(fd0);
        return BCM_ERR_NODEV;
    }

    if (logCb_) logCb_("The server has been started, please wait...", PrintLevel::Info);

    // 通知调用方实际端口，调用方可据此发起 RPC fullInstall
    if (readyCb_) {
        readyCb_(ntohs(address.sin_port), fileName_, static_cast<uint32_t>(fileSize), hostIP_);
    }

    fd1 = accept(fd0, reinterpret_cast<struct sockaddr*>(&address), &addrlen);
    if (fd1 == INVALID_SOCKET) {
        if (logCb_) logCb_("Accept failed.", PrintLevel::Error);
        closesocket(fd0);
        return BCM_ERR_NODEV;
    }

    if (logCb_) logCb_("The device is connected and update start.", PrintLevel::Info);

    int done = 0;
    while (done < fileSize) {
        int tmp = (fileSize - done) < 256 ? (fileSize - done) : 256;
        const char* buffer = reinterpret_cast<const char*>(fileBytes_.data() + done);
        int sent = send(fd1, buffer, tmp, 0);
        if (sent < 0) {
            if (logCb_) logCb_("Send failed.", PrintLevel::Error);
            break;
        }
        done += sent;
        if (logCb_) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Upgrade in progress %d%%.", done * 100 / fileSize);
            logCb_(buf, PrintLevel::Info);
        }
    }

    closesocket(fd1);
    closesocket(fd0);

    return (done == fileSize) ? BCM_ERR_OK : BCM_ERR_UNKNOWN;
}

} // namespace bcm
