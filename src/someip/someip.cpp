#include "someip/someip.h"

#include <fstream>

namespace someip {

void SomeIPMessage::SetHeader(uint16_t serviceId, uint16_t methodId, uint16_t clientId,
                              uint16_t sessionId, uint8_t messageType, uint8_t returnCode)
{
    header_ = SomeIPHeader(serviceId, methodId, clientId, sessionId, messageType, returnCode);
}

ByteArray SomeIPMessage::Build(SomeIPMethod /*method*/, const ByteArray &payload)
{
    header_.SetLength(static_cast<uint32_t>(payload.size()));
    ByteArray msg = header_.ToByteArray();
    msg.insert(msg.end(), payload.begin(), payload.end());
    return msg;
}

ByteArray SomeIPMessage::ToByteArray() const
{
    ByteArray out = header_.ToByteArray();
    out.insert(out.end(), data_.begin(), data_.end());
    return out;
}

ByteArray SomeIPMessage::setFindOrOfferPayload(const SomeIPEntry &entry, uint8_t flag,
                                               uint32_t reserved, uint32_t optionsLen)
{
    ByteArray payload;
    payload.push_back(flag);
    // 保留字段取 uint32 的低 3 字节，按小端序输出（与原实现 host 内存拷贝行为一致）
    payload.push_back(static_cast<uint8_t>((reserved >> 0) & 0xFF));
    payload.push_back(static_cast<uint8_t>((reserved >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>((reserved >> 16) & 0xFF));

    ByteArray entryBytes = entry.ToByteArray();
    AppendBigEndian<uint32_t>(payload, static_cast<uint32_t>(entryBytes.size()));
    payload.insert(payload.end(), entryBytes.begin(), entryBytes.end());
    AppendBigEndian<uint32_t>(payload, optionsLen);
    return payload;
}

ByteArray SomeIPMessage::setCamExclusivePayload(uint32_t index)
{
    // 与原实现一致：按主机字节序直接拷贝 uint32
    ByteArray payload(sizeof(index));
    std::memcpy(payload.data(), &index, sizeof(index));
    return payload;
}

ByteArray SomeIPMessage::setConfigPayload(const std::string &jsonFile)
{
    std::ifstream file(jsonFile, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) {
        return {};
    }

    ByteArray payload(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char *>(payload.data()), size)) {
        return {};
    }
    return payload;
}

ByteArray SomeIPMessage::setMediaPayload(bool enableMirror, bool enableFlip, uint8_t encode,
                                         uint8_t resolution, uint8_t fps, uint16_t bitrate,
                                         uint8_t rcMode, uint8_t iFrameInterval)
{
    MediaPayload payload(enableMirror, enableFlip, encode, resolution, fps,
                         bitrate, rcMode, iFrameInterval);
    return payload.ToByteArray();
}

MediaPayload SomeIPMessage::parseMediaPayload(const ByteArray &payload)
{
    return MediaPayload::FromBytes(payload);
}

GetCameraROIPayload SomeIPMessage::parseGetCameraROIPayload(const ByteArray &payload)
{
    return GetCameraROIPayload::FromBytes(payload);
}

AlgROIPayload SomeIPMessage::parseAlgROIPayload(const ByteArray &payload)
{
    return AlgROIPayload::FromBytes(payload);
}

ByteArray SomeIPMessage::setSubscribePayload(uint8_t index)
{
    SubscribePayload payload(index);
    return payload.ToByteArray();
}

} // namespace someip
