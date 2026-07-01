#include "avtp/avtp_h264_assembler.h"

#include <algorithm>
#include <utility>

namespace avtp {
namespace {

constexpr uint8_t kNalTypeMask = 0x1FU;
constexpr uint8_t kNalTypeIdr = 5;
constexpr uint8_t kNalTypeSps = 7;
constexpr uint8_t kNalTypePps = 8;
constexpr uint8_t kAnnexBStartCode[] = {0, 0, 0, 1};

std::size_t FindStartCode(const std::vector<uint8_t>& data,
                          std::size_t offset,
                          std::size_t& start_code_size) {
    for (std::size_t i = offset; i + 3 <= data.size(); ++i) {
        if (i + 4 <= data.size() && data[i] == 0 && data[i + 1] == 0 &&
            data[i + 2] == 0 && data[i + 3] == 1) {
            start_code_size = 4;
            return i;
        }
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            start_code_size = 3;
            return i;
        }
    }
    start_code_size = 0;
    return data.size();
}

void AppendStartCodeAndNal(std::vector<uint8_t>& target,
                           const std::vector<uint8_t>& nal) {
    target.insert(target.end(), kAnnexBStartCode,
                  kAnnexBStartCode + sizeof(kAnnexBStartCode));
    target.insert(target.end(), nal.begin(), nal.end());
}

} // namespace

bool StartsWithAnnexBStartCode(const uint8_t* data, std::size_t size) {
    if (!data) {
        return false;
    }
    if (size >= 4 && data[0] == 0 && data[1] == 0 &&
        data[2] == 0 && data[3] == 1) {
        return true;
    }
    return size >= 3 && data[0] == 0 && data[1] == 0 && data[2] == 1;
}

bool ContainsNalType(const std::vector<uint8_t>& data, uint8_t nal_type) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        std::size_t start_code_size = 0;
        const std::size_t start = FindStartCode(data, offset, start_code_size);
        if (start == data.size()) {
            return false;
        }
        const std::size_t nal_start = start + start_code_size;
        if (nal_start < data.size() &&
            (data[nal_start] & kNalTypeMask) == nal_type) {
            return true;
        }
        offset = nal_start + 1;
    }
    return false;
}

AvtpH264Assembler::AvtpH264Assembler(std::size_t max_access_unit_size)
    : max_access_unit_size_(std::max<std::size_t>(
          max_access_unit_size, sizeof(kAnnexBStartCode) + 1)) {
    access_unit_.reserve(512 * 1024);
}

void AvtpH264Assembler::Reset() {
    ResetAccessUnit();
    has_stream_ = false;
    current_stream_id_ = 0;
    current_source_mac_ = {};
    has_expected_sequence_ = false;
    expected_sequence_ = 0;
    dropping_until_marker_ = false;
    cached_sps_.clear();
    cached_pps_.clear();
    decoder_ready_ = false;
    stats_ = {};
}

void AvtpH264Assembler::ResetAccessUnit() {
    access_unit_.clear();
    access_unit_.reserve(512 * 1024);
}

bool AvtpH264Assembler::Append(const uint8_t* data, std::size_t size) {
    if (!data || size == 0) {
        return true;
    }
    if (size > max_access_unit_size_ - access_unit_.size()) {
        return false;
    }
    access_unit_.insert(access_unit_.end(), data, data + size);
    return true;
}

bool AvtpH264Assembler::SameStream(const ParsedCvfPacket& packet) const {
    if (!has_stream_) {
        return false;
    }
    if (packet.stream_id != current_stream_id_) {
        return false;
    }
    if (packet.has_ethernet_header &&
        !IsSameMac(packet.source_mac, current_source_mac_)) {
        return false;
    }
    return true;
}

void AvtpH264Assembler::SwitchStream(const ParsedCvfPacket& packet) {
    has_stream_ = true;
    current_stream_id_ = packet.stream_id;
    current_source_mac_ = packet.has_ethernet_header
                              ? packet.source_mac
                              : ZeroMac();
    has_expected_sequence_ = false;
    expected_sequence_ = 0;
    dropping_until_marker_ = false;
    cached_sps_.clear();
    cached_pps_.clear();
    decoder_ready_ = false;
    ResetAccessUnit();
}

AvtpH264Assembler::Result AvtpH264Assembler::DropCurrentAccessUnit(
    bool malformed) {
    if (!access_unit_.empty()) {
        CacheParameterSetsFromAccessUnit(access_unit_);
        ++stats_.dropped_access_units;
    }
    ResetAccessUnit();
    if (malformed) {
        ++stats_.malformed_packets;
        return Result::Malformed;
    }
    return Result::Dropped;
}

void AvtpH264Assembler::CacheParameterSetsFromAccessUnit(
    const std::vector<uint8_t>& data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        std::size_t start_code_size = 0;
        const std::size_t start = FindStartCode(data, offset, start_code_size);
        if (start == data.size()) {
            return;
        }

        const std::size_t nal_start = start + start_code_size;
        if (nal_start >= data.size()) {
            return;
        }

        std::size_t next_start_code_size = 0;
        const std::size_t next =
            FindStartCode(data, nal_start + 1, next_start_code_size);
        const std::size_t nal_end = next == data.size() ? data.size() : next;
        const uint8_t nal_type = data[nal_start] & kNalTypeMask;
        if (nal_type == kNalTypeSps && nal_end > nal_start) {
            cached_sps_.assign(data.begin() + nal_start,
                               data.begin() + nal_end);
        } else if (nal_type == kNalTypePps && nal_end > nal_start) {
            cached_pps_.assign(data.begin() + nal_start,
                               data.begin() + nal_end);
        }

        offset = nal_end;
    }
}

AvtpH264Assembler::Result AvtpH264Assembler::FinishAccessUnit(
    const ParsedCvfPacket& packet,
    int64_t capture_timestamp_us,
    H264AccessUnit& output) {
    H264AccessUnit candidate;
    candidate.data = std::move(access_unit_);
    candidate.stream_id = packet.stream_id;
    candidate.source_mac = packet.has_ethernet_header ? packet.source_mac
                                                      : ZeroMac();
    candidate.sequence_num = packet.sequence_num;
    candidate.avtp_timestamp = packet.avtp_timestamp;
    candidate.timestamp_valid = packet.timestamp_valid;
    candidate.capture_timestamp_us = capture_timestamp_us;

    CacheParameterSetsFromAccessUnit(candidate.data);
    candidate.keyframe = ContainsNalType(candidate.data, kNalTypeIdr);
    bool has_sps = ContainsNalType(candidate.data, kNalTypeSps);
    bool has_pps = ContainsNalType(candidate.data, kNalTypePps);

    if (candidate.keyframe) {
        std::vector<uint8_t> prefix;
        if (!cached_sps_.empty() && !has_sps) {
            AppendStartCodeAndNal(prefix, cached_sps_);
            has_sps = true;
        }
        if (!cached_pps_.empty() && !has_pps) {
            AppendStartCodeAndNal(prefix, cached_pps_);
            has_pps = true;
        }
        if (!prefix.empty()) {
            candidate.data.insert(candidate.data.begin(),
                                  prefix.begin(), prefix.end());
        }
    }

    if (!decoder_ready_) {
        if (!candidate.keyframe || !has_sps || !has_pps) {
            ++stats_.dropped_access_units;
            ResetAccessUnit();
            output = {};
            return Result::Dropped;
        }
        decoder_ready_ = true;
    }

    ++stats_.access_units;
    ResetAccessUnit();
    output = std::move(candidate);
    return Result::AccessUnitReady;
}

AvtpH264Assembler::Result AvtpH264Assembler::Push(
    const ParsedCvfPacket& packet,
    int64_t capture_timestamp_us,
    H264AccessUnit& output) {
    output = {};
    ++stats_.packets;
    stats_.payload_bytes += packet.payload_size;

    if (packet.payload == nullptr || packet.payload_size == 0) {
        ++stats_.malformed_packets;
        return Result::Malformed;
    }

    if (!has_stream_) {
        SwitchStream(packet);
    } else if (!SameStream(packet)) {
        if (!access_unit_.empty()) {
            ++stats_.dropped_access_units;
        }
        SwitchStream(packet);
    }

    if (has_expected_sequence_ && packet.sequence_num != expected_sequence_) {
        const uint8_t delta =
            static_cast<uint8_t>(packet.sequence_num - expected_sequence_);
        if (delta >= 128) {
            ++stats_.out_of_order_packets;
            return Result::Dropped;
        }

        stats_.lost_packets += delta;
        (void)DropCurrentAccessUnit(false);
        dropping_until_marker_ = true;
    }

    has_expected_sequence_ = true;
    expected_sequence_ = static_cast<uint8_t>(packet.sequence_num + 1);

    if (dropping_until_marker_) {
        if (packet.marker) {
            dropping_until_marker_ = false;
        }
        return Result::Dropped;
    }

    if (access_unit_.empty() &&
        !StartsWithAnnexBStartCode(packet.payload, packet.payload_size)) {
        ++stats_.waiting_for_start_packets;
        if (!packet.marker) {
            dropping_until_marker_ = true;
        }
        return Result::Dropped;
    }

    if (!Append(packet.payload, packet.payload_size)) {
        const Result result = DropCurrentAccessUnit(true);
        dropping_until_marker_ = !packet.marker;
        return result;
    }

    if (!packet.marker) {
        return Result::NeedMore;
    }

    return FinishAccessUnit(packet, capture_timestamp_us, output);
}

} // namespace avtp
