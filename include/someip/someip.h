#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace someip {

using ByteArray = std::vector<uint8_t>;

/// @brief 将 IP 地址字符串转换为 32 位整数（主机字节序）
/// @param ipString IP 地址字符串，例如 "192.168.66.166"
/// @return 32 位整数，例如 0xC0A842A6；解析失败返回 0
inline uint32_t IpStringToUint32(const std::string &ipString)
{
    uint32_t result = 0;
    int count = 0;
    std::string token;

    for (char c : ipString) {
        if (c == '.') {
            if (token.empty() || count >= 4) {
                return 0;
            }
            try {
                size_t pos = 0;
                int val = std::stoi(token, &pos);
                if (pos != token.size() || val < 0 || val > 255) {
                    return 0;
                }
                result = (result << 8) | static_cast<uint8_t>(val);
                ++count;
                token.clear();
            } catch (...) {
                return 0;
            }
        } else {
            token.push_back(c);
        }
    }

    if (!token.empty() && count == 3) {
        try {
            size_t pos = 0;
            int val = std::stoi(token, &pos);
            if (pos != token.size() || val < 0 || val > 255) {
                return 0;
            }
            result = (result << 8) | static_cast<uint8_t>(val);
            ++count;
        } catch (...) {
            return 0;
        }
    }

    return count == 4 ? result : 0;
}

/// @brief 将 32 位整数（主机字节序）转换为 IP 地址字符串
/// @param ipValue 32 位整数，例如 0xC0A842A6
/// @return IP 地址字符串，例如 "192.168.66.166"
inline std::string IpUint32ToString(uint32_t ipValue)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                  (ipValue >> 24) & 0xFF,
                  (ipValue >> 16) & 0xFF,
                  (ipValue >> 8) & 0xFF,
                  ipValue & 0xFF);
    return buf;
}

/// @brief 将 MAC 地址字符串转换为字节数组
/// @param macString MAC 地址字符串，支持多种格式：
///        "aa:27:7a:9c:bd:d2" (冒号分隔)
///        "aa-27-7a-9c-bd-d2" (短横线分隔)
///        "aa27.7a9c.bdd2" (点分隔，Cisco 格式)
///        "aa277a9cbdd2" (无分隔符)
/// @return 6 字节的字节数组
inline ByteArray MacStringToByteArray(const std::string &macString)
{
    std::string cleanMac;
    cleanMac.reserve(12);
    for (char c : macString) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            cleanMac.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }

    if (cleanMac.length() != 12) {
        return ByteArray(6, 0);
    }

    ByteArray macArray(6, 0);
    for (size_t i = 0; i < 6; ++i) {
        try {
            int byte = std::stoi(cleanMac.substr(i * 2, 2), nullptr, 16);
            macArray[i] = static_cast<uint8_t>(byte);
        } catch (...) {
            return ByteArray(6, 0);
        }
    }
    return macArray;
}

/// @brief 将字节数组转换为 MAC 地址字符串
/// @param macArray 6 字节的字节数组
/// @param separator 分隔符，默认为冒号 ":"
/// @param uppercase 是否使用大写，默认为 false
/// @return MAC 地址字符串，例如 "aa:27:7a:9c:bd:d2"
inline std::string MacByteArrayToString(const ByteArray &macArray,
                                        const std::string &separator = ":",
                                        bool uppercase = false)
{
    if (macArray.size() != 6) {
        return {};
    }

    const char *fmt = uppercase ? "%02X" : "%02x";
    std::string result;
    for (size_t i = 0; i < 6; ++i) {
        if (i > 0) {
            result += separator;
        }
        char buf[8];
        std::snprintf(buf, sizeof(buf), fmt, macArray[i]);
        result += buf;
    }
    return result;
}

/// @brief 将 MAC 地址字符串转换为 uint64（高 16 位补 0）
/// @param macString MAC 地址字符串
/// @return uint64 值
inline uint64_t MacStringToUint64(const std::string &macString)
{
    ByteArray macArray = MacStringToByteArray(macString);
    uint64_t result = 0;
    for (size_t i = 0; i < 6; ++i) {
        result = (result << 8) | macArray[i];
    }
    return result;
}

/// @brief 将 uint64 转换为 MAC 地址字符串（仅使用低 48 位）
/// @param value uint64 值
/// @return MAC 地址字符串
inline std::string MacUint64ToString(uint64_t value)
{
    ByteArray macArray(6, 0);
    for (int i = 5; i >= 0; --i) {
        macArray[i] = static_cast<uint8_t>(value & 0xFF);
        value >>= 8;
    }
    return MacByteArrayToString(macArray);
}

// ========================================================================
// 字节序辅助函数
// ========================================================================

namespace detail {

inline uint16_t ByteSwap16(uint16_t v) noexcept
{
    return static_cast<uint16_t>((v << 8) | (v >> 8));
}

inline uint32_t ByteSwap32(uint32_t v) noexcept
{
    return ((v >> 24) & 0xFFu) |
           ((v >> 8)  & 0xFF00u) |
           ((v << 8)  & 0xFF0000u) |
           ((v << 24) & 0xFF000000u);
}

inline uint64_t ByteSwap64(uint64_t v) noexcept
{
    return ((v >> 56) & 0xFFull) |
           ((v >> 40) & 0xFF00ull) |
           ((v >> 24) & 0xFF0000ull) |
           ((v >> 8)  & 0xFF000000ull) |
           ((v << 8)  & 0xFF00000000ull) |
           ((v << 24) & 0xFF0000000000ull) |
           ((v << 40) & 0xFF000000000000ull) |
           ((v << 56) & 0xFF00000000000000ull);
}

inline bool IsLittleEndian() noexcept
{
    const uint16_t test = 0x0001;
    return *reinterpret_cast<const uint8_t *>(&test) == 0x01;
}

} // namespace detail

/// @brief 将主机字节序值转换为大端序值（无操作数/8位直接返回）
template <typename T>
T ToBigEndian(T value) noexcept;

template <> inline uint8_t  ToBigEndian<uint8_t>(uint8_t v)  noexcept { return v; }
template <> inline uint16_t ToBigEndian<uint16_t>(uint16_t v) noexcept { return detail::IsLittleEndian() ? detail::ByteSwap16(v) : v; }
template <> inline uint32_t ToBigEndian<uint32_t>(uint32_t v) noexcept { return detail::IsLittleEndian() ? detail::ByteSwap32(v) : v; }
template <> inline uint64_t ToBigEndian<uint64_t>(uint64_t v) noexcept { return detail::IsLittleEndian() ? detail::ByteSwap64(v) : v; }

/// @brief 将大端序值转换为主机字节序值
template <typename T>
T FromBigEndian(T value) noexcept { return ToBigEndian(value); }

/// @brief 以大端序方式向字节数组追加一个无符号整数
template <typename T>
void AppendBigEndian(ByteArray &out, T value)
{
    static_assert(std::is_unsigned<T>::value, "T must be an unsigned integer");
    T net = ToBigEndian(value);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&net);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

/// @brief 以大端序方式读取一个无符号整数
template <typename T>
T ReadBigEndian(const uint8_t *data)
{
    static_assert(std::is_unsigned<T>::value, "T must be an unsigned integer");
    T host;
    std::memcpy(&host, data, sizeof(T));
    return FromBigEndian(host);
}

// ========================================================================
// SOME/IP 协议结构
// ========================================================================

struct SomeIPHeader {
    uint16_t serviceId = 0;      // 服务ID 16 bit
    uint16_t methodId = 0;       // 方法ID 16 bit
    uint32_t length = 0;         // 从length字段开始到消息结束的字节数 32 bit
    uint16_t clientId = 0;       // 客户端ID 16 bit
    uint16_t sessionId = 0;      // 会话ID 16 bit
    uint8_t  protocolVersion = 0x01;  // 协议版本，固定为0x01 8 bit
    uint8_t  interfaceVersion = 0x01; // 接口版本，用于标识服务ID升级 8 bit
    uint8_t  messageType = 0;    // 消息类型 8 bit
    uint8_t  returnCode = 0;     // 返回码 8 bit

    SomeIPHeader() = default;
    SomeIPHeader(uint16_t serviceId, uint16_t methodId, uint16_t clientId, uint16_t sessionId,
                 uint8_t messageType, uint8_t returnCode)
        : serviceId(serviceId), methodId(methodId), clientId(clientId), sessionId(sessionId),
          protocolVersion(0x01), interfaceVersion(0x01),
          messageType(messageType), returnCode(returnCode) {}

    /// @brief 设置 length 字段，payloadLength 为payload字节数
    void SetLength(uint32_t payloadLength) { length = payloadLength + 8; }

    ByteArray ToByteArray() const
    {
        ByteArray out;
        AppendBigEndian(out, serviceId);
        AppendBigEndian(out, methodId);
        AppendBigEndian(out, length);
        AppendBigEndian(out, clientId);
        AppendBigEndian(out, sessionId);
        out.push_back(protocolVersion);
        out.push_back(interfaceVersion);
        out.push_back(messageType);
        out.push_back(returnCode);
        return out;
    }

    static SomeIPHeader FromBytes(const ByteArray &data)
    {
        SomeIPHeader header;
        if (data.size() >= 16) {
            const uint8_t *p = data.data();
            header.serviceId = ReadBigEndian<uint16_t>(p); p += 2;
            header.methodId = ReadBigEndian<uint16_t>(p); p += 2;
            header.length = ReadBigEndian<uint32_t>(p); p += 4;
            header.clientId = ReadBigEndian<uint16_t>(p); p += 2;
            header.sessionId = ReadBigEndian<uint16_t>(p); p += 2;
            header.protocolVersion = *p++;
            header.interfaceVersion = *p++;
            header.messageType = *p++;
            header.returnCode = *p++;
        }
        return header;
    }
};

/// @brief SomeIP服务发现或提供服务条目
struct SomeIPEntry {
    uint8_t  type = 0x00;            // 类型
    uint8_t  index1 = 0x00;          // 索引第一个字段，用于标识服务组或类型
    uint8_t  index2 = 0x00;          // 索引第二个字段，用于标识服务信息
    uint8_t  numOpt = 0x00;          // 选项数量
    uint16_t serviceId = 0x433F;     // 服务ID
    uint16_t instanceId = 0xFFFF;    // 实例ID
    uint32_t majorVersionAndttl = 0xFF | 0xFFFFFF00; // 主版本号和TTL
    uint32_t minorVersion = 0xFFFFFFFF; // 次版本号

    SomeIPEntry() = default;

    ByteArray ToByteArray() const
    {
        ByteArray out;
        out.push_back(type);
        out.push_back(index1);
        out.push_back(index2);
        out.push_back(numOpt);
        AppendBigEndian(out, serviceId);
        AppendBigEndian(out, instanceId);
        AppendBigEndian(out, majorVersionAndttl);
        AppendBigEndian(out, minorVersion);
        return out;
    }
};

enum class SomeIPStatus {
    Init = 1,
    Found,
    Wait
};

/// @brief SomeIP方法ID
enum class SomeIPMethod {
    unKnown = 0x0000,

    GetDataSheet = 0x0001,    // 获取数据表
    SetCamExclusive = 0x0011, // 设置摄像头独占模式
    EraseCamExclusive = 0x0019, // 释放摄像头独占模式

    SetROI = 0x0101,  // 设置ROI区域
    GetROI = 0x0103,  // 获取ROI区域

    Subscribe  = 0x0131,  // 订阅事件
    UnSubscribe = 0x0132, // 取消订阅事件

    SubscribeAlarm = 0x0141,   // 订阅报警事件
    UnSubscribeAlarm = 0x0142, // 取消订阅报警事件

    GetAlgROI = 0x0143, // 获取算法ROI区域
    SetAlgROI = 0x0144, // 设置算法ROI区域

    GetNetwork = 0x0191, // 获取网络配置
    SetNetwork = 0x0192, // 设置网络配置

    GetMedia = 0x0171, // 获取媒体配置
    SetMedia = 0x0172, // 设置媒体配置

    GetAlg = 0x0181, // 获取算法配置
    SetAlg = 0x0182, // 设置算法配置

    GetSystem = 0x0201, // 获取系统配置

    ResetFactory = 0x0211, // 恢复出厂设置

    GetConfig = 0x0150, // 获取配置
    SetConfig = 0x0151, // 设置配置

    FindOrOffer = 0x8100 // 服务发现或提供服务
};

// ========================================================================
// Payload 结构（所有多字节字段均按主机字节序存储，序列化时转大端）
// ========================================================================

struct MediaPayload {
    bool     enableMirror = true;
    bool     enableFlip = false;
    uint8_t  encode = 0x00;
    uint8_t  resolution = 0x00;
    uint8_t  fps = 30;
    uint16_t bitrate = 0x1000;
    uint8_t  rcMode = 0x00;
    uint8_t  IFrameInterval = 30;

    MediaPayload() = default;
    MediaPayload(bool mirror, bool flip, uint8_t enc, uint8_t res, uint8_t f,
                 uint16_t bitrateVal, uint8_t rc, uint8_t iFrameInt)
        : enableMirror(mirror), enableFlip(flip), encode(enc), resolution(res),
          fps(f), bitrate(bitrateVal), rcMode(rc), IFrameInterval(iFrameInt) {}

    ByteArray ToByteArray() const
    {
        ByteArray out;
        AppendBigEndian<uint32_t>(out, enableMirror ? 1u : 0u);
        AppendBigEndian<uint32_t>(out, enableFlip ? 1u : 0u);
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(encode));
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(resolution));
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(fps));
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(bitrate));
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(rcMode));
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(IFrameInterval));
        return out;
    }

    static MediaPayload FromBytes(const ByteArray &payload)
    {
        MediaPayload media;
        if (payload.size() >= 32) {
            const uint8_t *p = payload.data();
            media.enableMirror = ReadBigEndian<uint32_t>(p + 0) != 0;
            media.enableFlip = ReadBigEndian<uint32_t>(p + 4) != 0;
            media.encode = static_cast<uint8_t>(ReadBigEndian<uint32_t>(p + 8));
            media.resolution = static_cast<uint8_t>(ReadBigEndian<uint32_t>(p + 12));
            media.fps = static_cast<uint8_t>(ReadBigEndian<uint32_t>(p + 16));
            media.bitrate = static_cast<uint16_t>(ReadBigEndian<uint32_t>(p + 20));
            media.rcMode = static_cast<uint8_t>(ReadBigEndian<uint32_t>(p + 24));
            media.IFrameInterval = static_cast<uint8_t>(ReadBigEndian<uint32_t>(p + 28));
        }
        return media;
    }
};

struct AlgROIPayload {
    std::vector<std::pair<double, double>> roiList; // ROI坐标
    uint8_t roiType = 0;                            // ROI类型 0x00梯形 0x01矩形 0x02矩形

    AlgROIPayload() = default;
    AlgROIPayload(const std::vector<std::pair<double, double>> &list, uint8_t type)
        : roiList(list), roiType(type) {}

    ByteArray ToByteArray() const
    {
        ByteArray out;
        out.reserve(196);
        for (size_t i = 0; i < 12; ++i) {
            double x = (i < roiList.size()) ? roiList[i].first : 0.0;
            double y = (i < roiList.size()) ? roiList[i].second : 0.0;
            const uint8_t *px = reinterpret_cast<const uint8_t *>(&x);
            const uint8_t *py = reinterpret_cast<const uint8_t *>(&y);
            out.insert(out.end(), px, px + sizeof(double));
            out.insert(out.end(), py, py + sizeof(double));
        }
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(roiType));
        return out;
    }

    static AlgROIPayload FromBytes(const ByteArray &payload)
    {
        AlgROIPayload algRoi;
        if (payload.size() >= 196) {
            for (size_t i = 0; i < 12; ++i) {
                double x, y;
                std::memcpy(&x, payload.data() + i * 16, sizeof(double));
                std::memcpy(&y, payload.data() + i * 16 + 8, sizeof(double));
                algRoi.roiList.emplace_back(x, y);
            }
            algRoi.roiType = static_cast<uint8_t>(ReadBigEndian<uint32_t>(payload.data() + 192));
        }
        return algRoi;
    }
};

struct SetCameraROIPayload {
    uint32_t index = 1;

    // ROI 区域配置
    uint16_t quad1 = 1920;
    uint16_t quad2 = 0;
    uint16_t quad3 = 0;
    uint16_t quad4 = 1080;

    // 使能
    uint8_t  enable = 1;
    uint8_t  transMethod = 0;
    uint16_t transCycle = 0;

    // 视频设置
    uint16_t width = 1280;
    uint16_t height = 800;
    uint32_t frameRate = 30u << 16;
    uint8_t  interlaced = 0; // 0逐行扫描 1隔行扫描

    // 色彩和编码
    uint8_t  colorSpace = 0x80;
    uint32_t maxBitrate = 10;
    uint8_t  videoCompression = 0x02;

    uint8_t  histogramEnable = 0x00;
    uint8_t  histogramUpdateCycle = 0x00;
    uint8_t  usedVideoComponent = 0x00;
    uint8_t  dataType = 0x04;
    uint8_t  binSize = 0x01;
    uint16_t numberOfBins = 0x00;

    // 图片颜色设置
    uint8_t brightness = 50;
    uint8_t contrast = 50;
    uint8_t saturation = 50;
    uint8_t sharpness = 50;

    // 其它设置
    uint8_t streamAtBoot = false;
    uint8_t algStatusIndicator = true;

    SetCameraROIPayload() = default;

    ByteArray ToByteArray() const
    {
        ByteArray out;
        out.reserve(44);
        AppendBigEndian<uint32_t>(out, index);
        AppendBigEndian<uint16_t>(out, quad1);
        AppendBigEndian<uint16_t>(out, quad2);
        AppendBigEndian<uint16_t>(out, quad3);
        AppendBigEndian<uint16_t>(out, quad4);
        out.push_back(enable);
        out.push_back(transMethod);
        AppendBigEndian<uint16_t>(out, transCycle);
        AppendBigEndian<uint16_t>(out, width);
        AppendBigEndian<uint16_t>(out, height);
        AppendBigEndian<uint32_t>(out, frameRate);
        out.push_back(interlaced);
        out.push_back(colorSpace);
        AppendBigEndian<uint32_t>(out, maxBitrate);
        out.push_back(videoCompression);
        out.push_back(histogramEnable);
        out.push_back(histogramUpdateCycle);
        out.push_back(usedVideoComponent);
        out.push_back(dataType);
        out.push_back(binSize);
        AppendBigEndian<uint16_t>(out, numberOfBins);
        out.push_back(brightness);
        out.push_back(contrast);
        out.push_back(saturation);
        out.push_back(sharpness);
        out.push_back(streamAtBoot);
        out.push_back(algStatusIndicator);
        return out;
    }
};

struct GetCameraROIPayload {
    // ROI 区域配置
    uint16_t quad1 = 0;
    uint16_t quad2 = 0;
    uint16_t quad3 = 1920;
    uint16_t quad4 = 1080;

    // 使能
    uint8_t  enable = 1;
    uint8_t  transMethod = 0;
    uint16_t transCycle = 0;

    // 视频设置
    uint16_t width = 1280;
    uint16_t height = 800;
    uint32_t frameRate = 30u << 16;
    uint8_t  interlaced = 0;

    // 色彩和编码
    uint8_t  colorSpace = 0x80;
    uint32_t maxBitrate = 10;
    uint8_t  videoCompression = 0x02;

    uint8_t  histogramEnable = 0x00;
    uint8_t  histogramUpdateCycle = 0x00;
    uint8_t  usedVideoComponent = 0x00;
    uint8_t  dataType = 0x04;
    uint8_t  binSize = 0x01;
    uint16_t numberOfBins = 0x00;

    // 图片颜色设置
    uint8_t brightness = 50;
    uint8_t contrast = 50;
    uint8_t saturation = 50;
    uint8_t sharpness = 50;

    // 其它设置
    uint8_t streamAtBoot = false;
    uint8_t algStatusIndicator = true;

    GetCameraROIPayload() = default;

    static GetCameraROIPayload FromBytes(const ByteArray &payload)
    {
        GetCameraROIPayload camRoi;
        if (payload.size() >= 40) {
            const uint8_t *p = payload.data();
            camRoi.quad1 = ReadBigEndian<uint16_t>(p); p += 2;
            camRoi.quad2 = ReadBigEndian<uint16_t>(p); p += 2;
            camRoi.quad3 = ReadBigEndian<uint16_t>(p); p += 2;
            camRoi.quad4 = ReadBigEndian<uint16_t>(p); p += 2;
            camRoi.enable = *p++;
            camRoi.transMethod = *p++;
            camRoi.transCycle = ReadBigEndian<uint16_t>(p); p += 2;
            camRoi.width = ReadBigEndian<uint16_t>(p); p += 2;
            camRoi.height = ReadBigEndian<uint16_t>(p); p += 2;
            camRoi.frameRate = ReadBigEndian<uint32_t>(p); p += 4;
            camRoi.interlaced = *p++;
            camRoi.colorSpace = *p++;
            camRoi.maxBitrate = ReadBigEndian<uint32_t>(p); p += 4;
            camRoi.videoCompression = *p++;
            camRoi.histogramEnable = *p++;
            camRoi.histogramUpdateCycle = *p++;
            camRoi.usedVideoComponent = *p++;
            camRoi.dataType = *p++;
            camRoi.binSize = *p++;
            camRoi.numberOfBins = ReadBigEndian<uint16_t>(p); p += 2;
            camRoi.brightness = *p++;
            camRoi.contrast = *p++;
            camRoi.saturation = *p++;
            camRoi.sharpness = *p++;
            camRoi.streamAtBoot = *p++;
            camRoi.algStatusIndicator = *p++;
        }
        return camRoi;
    }
};

struct SetNetworkPayload {
    bool     dhcpEnable = false;
    uint32_t dhcpTimeout = 16;
    std::string ipAddress = "192.168.66.166";
    std::string subnetMask = "255.255.255.0";
    std::string gateway = "192.168.66.1";
    std::string macAddress = "aa:27:7a:9c:bd:d2";
    uint16_t rtspPort = 554;
    uint16_t onvifPort = 80;
    bool     onvifDiscoverEnable = true;
    uint64_t avtpStreamId = 0x102;
    std::string avtpMacAddress = "00:11:22:33:44:55";
    uint16_t someipPort = 17215;

    SetNetworkPayload() = default;

    ByteArray ToByteArray() const
    {
        ByteArray out;
        out.reserve(64);
        AppendBigEndian<uint32_t>(out, dhcpEnable ? 1u : 0u);
        AppendBigEndian<uint32_t>(out, dhcpTimeout);
        AppendBigEndian<uint32_t>(out, IpStringToUint32(ipAddress));
        AppendBigEndian<uint32_t>(out, IpStringToUint32(subnetMask));
        AppendBigEndian<uint32_t>(out, IpStringToUint32(gateway));
        ByteArray mac = MacStringToByteArray(macAddress);
        out.insert(out.end(), mac.begin(), mac.end());
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(rtspPort));
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(onvifPort));
        AppendBigEndian<uint32_t>(out, onvifDiscoverEnable ? 1u : 0u);
        AppendBigEndian<uint64_t>(out, avtpStreamId);
        ByteArray avtpMac = MacStringToByteArray(avtpMacAddress);
        out.insert(out.end(), avtpMac.begin(), avtpMac.end());
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(someipPort));
        return out;
    }
};

struct SubscribePayload {
    uint8_t index = 0x01;

    SubscribePayload() = default;
    explicit SubscribePayload(uint8_t idx) : index(idx) {}

    ByteArray ToByteArray() const
    {
        ByteArray out;
        AppendBigEndian<uint32_t>(out, static_cast<uint32_t>(index));
        return out;
    }
};

// ========================================================================
// 消息封装类
// ========================================================================

class SomeIPMessage {
public:
    SomeIPMessage() = default;

    void SetHeader(uint16_t serviceId, uint16_t methodId, uint16_t clientId, uint16_t sessionId,
                   uint8_t messageType, uint8_t returnCode);

    /// @brief 根据已设置的 header 和 payload 构建完整 SOME/IP 消息
    /// @param method 方法ID（保留参数，仅用于兼容旧接口，实际使用 header 中方法）
    /// @param payload payload 字节流
    ByteArray Build(SomeIPMethod method, const ByteArray &payload = {});

    /// @brief 获取当前 header + data 的完整字节流
    ByteArray ToByteArray() const;

    ByteArray setMediaPayload(bool enableMirror, bool enableFlip, uint8_t encode, uint8_t resolution,
                              uint8_t fps, uint16_t bitrate, uint8_t rcMode, uint8_t iFrameInterval);

    static MediaPayload parseMediaPayload(const ByteArray &payload);
    static GetCameraROIPayload parseGetCameraROIPayload(const ByteArray &payload);
    static AlgROIPayload parseAlgROIPayload(const ByteArray &payload);
    ByteArray setSubscribePayload(uint8_t index = 0x01);

private:
    ByteArray setFindOrOfferPayload(const SomeIPEntry &entry, uint8_t flag = (0x80 | 0x40),
                                    uint32_t reserved = 0x00, uint32_t optionsLen = 0x00);
    ByteArray setCamExclusivePayload(uint32_t index = 0x0A);
    ByteArray setConfigPayload(const std::string &jsonFile = "config/camera_config.json");

    SomeIPHeader header_;
    ByteArray data_;
};

} // namespace someip
