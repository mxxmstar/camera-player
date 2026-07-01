#include "rtp/h264_rtp_depacketizer.h"

#include <algorithm>

namespace rtp {
namespace {

constexpr uint8_t kNalTypeMask = 0x1FU; ///< NAL 类型掩码
constexpr uint8_t kStapAType = 24; ///< STAP-A NAL 类型
constexpr uint8_t kFuAType = 28; ///< FU-A NAL 类型
constexpr uint8_t kFuStartBit = 0x80U; ///< FU-A NAL 类型起始位
constexpr uint8_t kFuEndBit = 0x40U; ///< FU-A NAL 类型结束位
constexpr uint8_t kFuReservedBit = 0x20U; ///< FU-A NAL 类型保留位
constexpr uint8_t kAnnexBStartCode[] = {0, 0, 0, 1}; ///< Annex B 起始码

uint16_t ReadBigEndian16(const uint8_t* data) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[0]) << 8) |
        static_cast<uint16_t>(data[1]));
}

bool IsKeyNal(uint8_t nal_type) {
    return nal_type == 5;
}

bool ContainsNalType(const std::vector<uint8_t>& data,
                     uint8_t nal_type) {
    for (std::size_t i = 0; i + 4 < data.size(); ++i) {
        std::size_t nal_offset = 0;
        if (data[i] == 0 && data[i + 1] == 0 &&
            data[i + 2] == 1) {
            nal_offset = i + 3;
        } else if (i + 5 < data.size() &&
                   data[i] == 0 && data[i + 1] == 0 &&
                   data[i + 2] == 0 && data[i + 3] == 1) {
            nal_offset = i + 4;
        }
        if (nal_offset != 0 &&
            (data[nal_offset] & kNalTypeMask) == nal_type) {
            return true;
        }
    }
    return false;
}

} // namespace

H264RtpDepacketizer::H264RtpDepacketizer(
    std::size_t max_access_unit_size)
    : max_access_unit_size_(std::max<std::size_t>(
          max_access_unit_size, sizeof(kAnnexBStartCode) + 1)) {
    access_unit_.reserve(512 * 1024);
}

void H264RtpDepacketizer::Reset() {
    ResetAccessUnit();
    dropping_timestamp_ = false;
    dropped_timestamp_ = 0;
    has_ssrc_ = false;
    current_ssrc_ = 0;
    has_expected_sequence_ = false;
    expected_sequence_ = 0;
    cached_sps_.clear();
    cached_pps_.clear();
    stats_ = {};
}

void H264RtpDepacketizer::ResetAccessUnit() {
    access_unit_.clear();
    keyframe_ = false;
    fu_active_ = false;
    has_timestamp_ = false;
    current_timestamp_ = 0;
}

void H264RtpDepacketizer::CacheParameterSetsFromAccessUnit() {
    const std::vector<uint8_t>& d = access_unit_;
    std::size_t i = 0;
    // 起始码 00 00 00 01 或 00 00 01
    while (i + 3 <= d.size()) {
        std::size_t sc_len = 0;
        if (i + 4 <= d.size() && d[i] == 0 && d[i + 1] == 0 &&
            d[i + 2] == 0 && d[i + 3] == 1) {
            sc_len = 4;
        } else if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
            sc_len = 3;
        } else {
            ++i;
            continue;
        }
        
        const std::size_t nal_start = i + sc_len;   // NALU 数据起始位置
        if (nal_start >= d.size()) {
            break;  // 无更多 NALU 数据
        }
        const uint8_t nal_type = d[nal_start] & kNalTypeMask;   // NALU 类型

        // 从 NALU 起始位置扫描到下一个起始码
        std::size_t j = nal_start + 1;
        while (j < d.size()) {
            // 查找下一个 4 字节起始码
            if (j + 4 <= d.size() && d[j] == 0 && d[j + 1] == 0 &&
                d[j + 2] == 0 && d[j + 3] == 1) {
                break;
            }
            // 查找下一个 3 字节起始码
            if (j + 3 <= d.size() && d[j] == 0 && d[j + 1] == 0 &&
                d[j + 2] == 1) {
                break;
            }
            ++j;
        }

        if (nal_type == 7 && j > nal_start) {
            cached_sps_.assign(d.begin() + nal_start, d.begin() + j);
        } else if (nal_type == 8 && j > nal_start) {
            cached_pps_.assign(d.begin() + nal_start, d.begin() + j);
        }

        i = j;
    }
}

bool H264RtpDepacketizer::Append(const uint8_t* data, std::size_t size) {
    if (!data || size == 0) {
        return true;
    }
    if (size > max_access_unit_size_ - access_unit_.size()) {
        return false;
    }
    access_unit_.insert(access_unit_.end(), data, data + size);
    return true;
}

bool H264RtpDepacketizer::AppendStartCode() {
    return Append(kAnnexBStartCode, sizeof(kAnnexBStartCode));
}

bool H264RtpDepacketizer::PacketCanStartUnit(
    const ParsedRtpPacket& packet) const {
    if (!packet.payload || packet.payload_size == 0) {
        return false;
    }

    const uint8_t nal_type = packet.payload[0] & kNalTypeMask;
    if (nal_type >= 1 && nal_type <= kStapAType) {
        return true;
    }
    return nal_type == kFuAType && packet.payload_size >= 2 &&
           (packet.payload[1] & kFuStartBit) != 0;
}

H264RtpDepacketizer::Result H264RtpDepacketizer::DropCurrentTimestamp(const ParsedRtpPacket& packet, bool malformed) {
    // Preserve any SPS/PPS accumulated so far, so they can be prepended
    // to a later keyframe if this access unit is discarded due to a gap.
    CacheParameterSetsFromAccessUnit();

    if (!dropping_timestamp_ || dropped_timestamp_ != packet.timestamp) {
        ++stats_.dropped_access_units;
    }
    dropping_timestamp_ = true;
    dropped_timestamp_ = packet.timestamp;
    ResetAccessUnit();

    if (malformed) {
        ++stats_.malformed_packets;
    }
    if (packet.marker) {
        dropping_timestamp_ = false;
    }
    return malformed ? Result::Malformed : Result::Dropped;
}

H264RtpDepacketizer::Result H264RtpDepacketizer::HandleSingleNal(const ParsedRtpPacket& packet) {
    const uint8_t nal_type = packet.payload[0] & kNalTypeMask;
    if (!AppendStartCode() || !Append(packet.payload, packet.payload_size)) {
        return DropCurrentTimestamp(packet, true);
    }
    keyframe_ = keyframe_ || IsKeyNal(nal_type);
    return Result::NeedMore;
}

H264RtpDepacketizer::Result H264RtpDepacketizer::HandleStapA(const ParsedRtpPacket& packet) {
    std::size_t offset = 1;
    bool appended_nal = false;
    while (offset < packet.payload_size) {
        if (packet.payload_size - offset < 2) {
            return DropCurrentTimestamp(packet, true);
        }
        const std::size_t nal_size =
            ReadBigEndian16(packet.payload + offset);
        offset += 2;
        if (nal_size == 0 || nal_size > packet.payload_size - offset) {
            return DropCurrentTimestamp(packet, true);
        }

        const uint8_t nal_type = packet.payload[offset] & kNalTypeMask;
        if (!AppendStartCode() ||
            !Append(packet.payload + offset, nal_size)) {
            return DropCurrentTimestamp(packet, true);
        }
        keyframe_ = keyframe_ || IsKeyNal(nal_type);
        appended_nal = true;
        offset += nal_size;
    }

    if (!appended_nal) {
        return DropCurrentTimestamp(packet, true);
    }
    return Result::NeedMore;
}

H264RtpDepacketizer::Result H264RtpDepacketizer::HandleFuA(const ParsedRtpPacket& packet) {
    if (packet.payload_size < 3) {
        return DropCurrentTimestamp(packet, true);
    }

    const uint8_t fu_indicator = packet.payload[0];
    const uint8_t fu_header = packet.payload[1];
    const bool start = (fu_header & kFuStartBit) != 0;
    const bool end = (fu_header & kFuEndBit) != 0;
    const uint8_t nal_type = fu_header & kNalTypeMask;

    if ((fu_header & kFuReservedBit) != 0 || (start && end)) {
        return DropCurrentTimestamp(packet, true);
    }

    const uint8_t* fragment = packet.payload + 2;
    const std::size_t fragment_size = packet.payload_size - 2;

    if (start) {
        if (fu_active_) {
            return DropCurrentTimestamp(packet, true);
        }

        const uint8_t reconstructed_nal =
            static_cast<uint8_t>((fu_indicator & 0xE0U) | nal_type);

        // The target camera fragments a whole Annex-B buffer as FU-A.
        // Its first byte is 0x00, so the FU type is the reserved value 0.
        // Re-inserting that byte (without another start code) recreates the
        // original 00 00 00 01 prefix exactly.
        if (nal_type == 0) {
            if (fragment_size < 3 ||
                fragment[0] != 0 || fragment[1] != 0 ||
                fragment[2] != 1 ||
                !Append(&reconstructed_nal, 1)) {
                return DropCurrentTimestamp(packet, true);
            }
        } else {
            if (!AppendStartCode() ||
                !Append(&reconstructed_nal, 1)) {
                return DropCurrentTimestamp(packet, true);
            }
        }

        if (!Append(fragment, fragment_size)) {
            return DropCurrentTimestamp(packet, true);
        }
        keyframe_ = keyframe_ || IsKeyNal(nal_type);
        fu_active_ = true;
    } else {
        if (!fu_active_) {
            return DropCurrentTimestamp(packet, false);
        }
        if (!Append(fragment, fragment_size)) {
            return DropCurrentTimestamp(packet, true);
        }
    }

    if (end) {
        fu_active_ = false;
    }
    return Result::NeedMore;
}

H264RtpDepacketizer::Result H264RtpDepacketizer::FinishIfMarked(const ParsedRtpPacket& packet, H264AccessUnit& output) {
    if (!packet.marker) {
        return Result::NeedMore;
    }
    if (fu_active_ || access_unit_.empty()) {
        return DropCurrentTimestamp(packet, true);
    }

    CacheParameterSetsFromAccessUnit();

    output.data = std::move(access_unit_);
    output.timestamp = packet.timestamp;
    output.ssrc = packet.ssrc;
    output.keyframe = keyframe_ || ContainsNalType(output.data, 5);

    if (output.keyframe) {
        std::vector<uint8_t> prefix;
        if (!cached_sps_.empty() &&
            !ContainsNalType(output.data, 7)) {
            prefix.insert(prefix.end(), kAnnexBStartCode,
                          kAnnexBStartCode + sizeof(kAnnexBStartCode));
            prefix.insert(prefix.end(), cached_sps_.begin(),
                          cached_sps_.end());
        }
        if (!cached_pps_.empty() &&
            !ContainsNalType(output.data, 8)) {
            prefix.insert(prefix.end(), kAnnexBStartCode,
                          kAnnexBStartCode + sizeof(kAnnexBStartCode));
            prefix.insert(prefix.end(), cached_pps_.begin(),
                          cached_pps_.end());
        }
        if (!prefix.empty()) {
            output.data.insert(output.data.begin(),
                               prefix.begin(), prefix.end());
        }
    }

    ++stats_.access_units;

    access_unit_.clear();
    access_unit_.reserve(512 * 1024);
    keyframe_ = false;
    fu_active_ = false;
    has_timestamp_ = false;
    current_timestamp_ = 0;
    return Result::AccessUnitReady;
}

H264RtpDepacketizer::Result H264RtpDepacketizer::Push(const ParsedRtpPacket& packet, H264AccessUnit& output) {
    output = {};
    ++stats_.packets;

    if (!packet.payload || packet.payload_size == 0) {
        ++stats_.malformed_packets;
        return Result::Malformed;
    }

    if (has_ssrc_ && current_ssrc_ != packet.ssrc) {
        if (!access_unit_.empty()) {
            CacheParameterSetsFromAccessUnit();
            ++stats_.dropped_access_units;
        }
        ResetAccessUnit();
        dropping_timestamp_ = false;
        has_expected_sequence_ = false;
    }
    has_ssrc_ = true;
    current_ssrc_ = packet.ssrc;

    if (has_expected_sequence_ &&
        packet.sequence_number != expected_sequence_) {
        const int16_t delta = static_cast<int16_t>(
            packet.sequence_number - expected_sequence_);
        if (delta <= 0) {
            ++stats_.out_of_order_packets;
            return Result::Dropped;
        }

        stats_.lost_packets += static_cast<uint16_t>(delta);
        const bool gap_inside_current_unit =
            has_timestamp_ && current_timestamp_ == packet.timestamp;
        if (gap_inside_current_unit || !PacketCanStartUnit(packet)) {
            expected_sequence_ =
                static_cast<uint16_t>(packet.sequence_number + 1);
            return DropCurrentTimestamp(packet, false);
        }

        if (!access_unit_.empty()) {
            CacheParameterSetsFromAccessUnit();
            ++stats_.dropped_access_units;
        }
        ResetAccessUnit();
    }
    has_expected_sequence_ = true;
    expected_sequence_ =
        static_cast<uint16_t>(packet.sequence_number + 1);

    if (dropping_timestamp_) {
        if (packet.timestamp == dropped_timestamp_) {
            if (packet.marker) {
                dropping_timestamp_ = false;
            }
            return Result::Dropped;
        }
        dropping_timestamp_ = false;
    }

    if (has_timestamp_ && current_timestamp_ != packet.timestamp) {
        if (!access_unit_.empty()) {
            CacheParameterSetsFromAccessUnit();
            ++stats_.dropped_access_units;
        }
        ResetAccessUnit();
    }
    has_timestamp_ = true;
    current_timestamp_ = packet.timestamp;

    const uint8_t nal_type = packet.payload[0] & kNalTypeMask;
    Result result = Result::Malformed;
    if (nal_type >= 1 && nal_type <= 23) {
        result = HandleSingleNal(packet);
    } else if (nal_type == kStapAType) {
        result = HandleStapA(packet);
    } else if (nal_type == kFuAType) {
        result = HandleFuA(packet);
    } else {
        return DropCurrentTimestamp(packet, true);
    }

    if (result != Result::NeedMore) {
        return result;
    }
    return FinishIfMarked(packet, output);
}

} // namespace rtp
