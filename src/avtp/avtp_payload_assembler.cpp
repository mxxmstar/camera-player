#include "avtp/avtp_payload_assembler.h"

#include <algorithm>
#include <utility>

namespace avtp {

AvtpPayloadAssembler::AvtpPayloadAssembler(
    std::size_t max_access_unit_size)
    : max_access_unit_size_(std::max<std::size_t>(max_access_unit_size, 1)) {
    access_unit_.reserve(512 * 1024);
}

void AvtpPayloadAssembler::Reset() {
    ResetAccessUnit();
    has_stream_ = false;
    current_stream_id_ = 0;
    current_source_mac_ = {};
    has_expected_sequence_ = false;
    expected_sequence_ = 0;
    stats_ = {};
}

void AvtpPayloadAssembler::ResetAccessUnit() {
    access_unit_.clear();
    access_unit_.reserve(512 * 1024);
}

bool AvtpPayloadAssembler::Append(const uint8_t* data, std::size_t size) {
    if (!data || size == 0) {
        return true;
    }
    if (size > max_access_unit_size_ - access_unit_.size()) {
        return false;
    }
    access_unit_.insert(access_unit_.end(), data, data + size);
    return true;
}

bool AvtpPayloadAssembler::SameStream(const ParsedCvfPacket& packet) const {
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

void AvtpPayloadAssembler::SwitchStream(const ParsedCvfPacket& packet) {
    has_stream_ = true;
    current_stream_id_ = packet.stream_id;
    current_source_mac_ = packet.has_ethernet_header
                              ? packet.source_mac
                              : ZeroMac();
    has_expected_sequence_ = false;
    expected_sequence_ = 0;
    ResetAccessUnit();
}

AvtpPayloadAssembler::Result AvtpPayloadAssembler::FinishAccessUnit(
    const ParsedCvfPacket& packet,
    int64_t capture_timestamp_us,
    AvtpAccessUnit& output) {
    AvtpAccessUnit candidate;
    candidate.data = std::move(access_unit_);
    candidate.stream_id = packet.stream_id;
    candidate.source_mac = packet.has_ethernet_header ? packet.source_mac
                                                      : ZeroMac();
    candidate.sequence_num = packet.sequence_num;
    candidate.avtp_timestamp = packet.avtp_timestamp;
    candidate.timestamp_valid = packet.timestamp_valid;
    candidate.capture_timestamp_us = capture_timestamp_us;

    ++stats_.access_units;
    ResetAccessUnit();
    output = std::move(candidate);
    return Result::AccessUnitReady;
}

AvtpPayloadAssembler::Result AvtpPayloadAssembler::DropCurrentAccessUnit(
    bool malformed) {
    if (!access_unit_.empty()) {
        ++stats_.dropped_access_units;
    }
    ResetAccessUnit();
    if (malformed) {
        ++stats_.malformed_packets;
        return Result::Malformed;
    }
    return Result::Dropped;
}

AvtpPayloadAssembler::Result AvtpPayloadAssembler::Push(
    const ParsedCvfPacket& packet,
    const uint8_t* payload,
    std::size_t payload_size,
    int64_t capture_timestamp_us,
    AvtpAccessUnit& output) {
    output = {};
    ++stats_.packets;
    stats_.payload_bytes += payload_size;

    if (!payload || payload_size == 0) {
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

    if (has_expected_sequence_) {
        if (packet.sequence_num != expected_sequence_) {
            const uint8_t delta =
                static_cast<uint8_t>(packet.sequence_num - expected_sequence_);
            if (delta < 128) {
                stats_.lost_packets += delta;
            } else {
                ++stats_.out_of_order_packets;
            }
        }
    }
    expected_sequence_ = static_cast<uint8_t>(packet.sequence_num + 1);
    has_expected_sequence_ = true;

    if (!Append(payload, payload_size)) {
        return DropCurrentAccessUnit(true);
    }

    if (packet.marker) {
        return FinishAccessUnit(packet, capture_timestamp_us, output);
    }
    return Result::NeedMore;
}

} // namespace avtp
