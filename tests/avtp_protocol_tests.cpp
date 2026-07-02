#include "avtp/avtp_h264_assembler.h"
#include "avtp/avtp_packet_parser.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::vector<uint8_t> BuildCvfPayload(uint8_t sequence,
                                     bool marker,
                                     const std::vector<uint8_t>& payload,
                                     uint64_t stream_id = 0,
                                     uint32_t timestamp = 0x5201B754,
                                     bool timestamp_valid = false,
                                     bool h264_ptv = false) {
    std::vector<uint8_t> pdu(avtp::kCvfHeaderSize + payload.size());
    pdu[0] = avtp::kSubtypeCvf;
    pdu[1] = static_cast<uint8_t>(0x80U | (timestamp_valid ? 0x01U : 0));
    pdu[2] = sequence;
    pdu[3] = 0;
    for (int i = 7; i >= 0; --i) {
        pdu[4 + (7 - i)] = static_cast<uint8_t>(stream_id >> (i * 8));
    }
    pdu[12] = static_cast<uint8_t>(timestamp >> 24);
    pdu[13] = static_cast<uint8_t>(timestamp >> 16);
    pdu[14] = static_cast<uint8_t>(timestamp >> 8);
    pdu[15] = static_cast<uint8_t>(timestamp);
    pdu[16] = avtp::kCvfFormatRfc;
    pdu[17] = avtp::kCvfFormatSubtypeH264;
    pdu[18] = 0;
    pdu[19] = 0;
    pdu[20] = static_cast<uint8_t>(payload.size() >> 8);
    pdu[21] = static_cast<uint8_t>(payload.size());
    pdu[22] = static_cast<uint8_t>((h264_ptv ? 0x20U : 0) |
                                   (marker ? 0x10U : 0));
    pdu[23] = 0;
    std::copy(payload.begin(), payload.end(),
              pdu.begin() + avtp::kCvfHeaderSize);
    return pdu;
}

std::vector<uint8_t> BuildCustomSubtypePayload(
    uint8_t sequence,
    bool marker,
    const std::vector<uint8_t>& payload) {
    auto pdu = BuildCvfPayload(sequence, marker, payload);
    pdu[0] = avtp::kSubtypeCustom;
    pdu[17] = avtp::kCvfFormatSubtypeMjpeg;
    return pdu;
}

std::vector<uint8_t> WrapEthernet(const std::vector<uint8_t>& pdu,
                                  bool vlan = false) {
    const std::vector<uint8_t> dst = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    const std::vector<uint8_t> src = {
        0xaa, 0x87, 0x26, 0x53, 0xbb, 0x6c};

    std::vector<uint8_t> frame;
    frame.insert(frame.end(), dst.begin(), dst.end());
    frame.insert(frame.end(), src.begin(), src.end());
    if (vlan) {
        frame.insert(frame.end(), {0x81, 0x00, 0x00, 0x07});
    }
    frame.push_back(0x22);
    frame.push_back(0xf0);
    frame.insert(frame.end(), pdu.begin(), pdu.end());
    return frame;
}

std::vector<uint8_t> DecodableKeyframePayload() {
    return {
        0, 0, 0, 1, 0x67, 0x42, 0xc0, 0x28,
        0, 0, 0, 1, 0x68, 0xce,
        0, 0, 0, 1, 0x65, 0xaa, 0xbb};
}

avtp::ParsedCvfPacket ParseFrame(const std::vector<uint8_t>& frame) {
    avtp::ParsedCvfPacket packet;
    avtp::ParseError error = avtp::ParseError::None;
    Require(avtp::AvtpPacketParser::Parse(
                frame.data(), frame.size(), packet, &error),
            std::string("parse frame: ") +
                avtp::AvtpPacketParser::ErrorToString(error));
    return packet;
}

void TestParserPreservesFullStreamData() {
    const std::vector<uint8_t> payload = {
        0, 0, 0, 1, 0x67, 0x42, 0xc0, 0x28,
        0, 0, 0, 1, 0x68, 0xce, 0x31, 0xb2,
        0, 0, 0, 1, 0x65, 0xb8};
    const auto frame = WrapEthernet(
        BuildCvfPayload(0, false, payload, 0, 0x5201B754, true, false));

    const avtp::ParsedCvfPacket packet = ParseFrame(frame);
    Require(packet.has_ethernet_header, "Ethernet header detected");
    Require(packet.sequence_num == 0, "sequence number");
    Require(packet.timestamp_valid, "AVTP timestamp valid flag");
    Require(packet.avtp_timestamp == 0x5201B754, "AVTP timestamp");
    Require(packet.format == avtp::kCvfFormatRfc, "CVF format");
    Require(packet.format_subtype == avtp::kCvfFormatSubtypeH264,
            "CVF H264 subtype");
    Require(packet.stream_data_length == payload.size(),
            "stream data length");
    Require(!packet.h264_payload_timestamp_valid, "H264 PTV flag");
    Require(!packet.marker, "marker clear");
    Require(packet.payload_size == payload.size(), "payload size");
    Require(std::equal(payload.begin(), payload.end(), packet.payload),
            "payload starts at Annex-B start code and is not stripped");
}

void TestParserVlanAndMarker() {
    const std::vector<uint8_t> payload = {
        0, 0, 0, 1, 0x41, 0x88, 0x99};
    const auto frame = WrapEthernet(
        BuildCvfPayload(42, true, payload, 0xAABBCCDDEEFF0001ULL,
                        0x01020304, false, true),
        true);

    const avtp::ParsedCvfPacket packet = ParseFrame(frame);
    Require(packet.avtp_offset == 18, "VLAN AVTP offset");
    Require(packet.sequence_num == 42, "VLAN sequence");
    Require(packet.stream_id == 0xAABBCCDDEEFF0001ULL, "stream id");
    Require(packet.marker, "marker bit");
    Require(packet.h264_payload_timestamp_valid, "H264 PTV flag");
    Require(packet.payload[0] == 0 && packet.payload[3] == 1,
            "VLAN payload offset");
}

void TestParserRejectsTruncatedStreamData() {
    auto pdu = BuildCvfPayload(1, false, {0, 0, 0, 1, 0x65});
    pdu[20] = 0x01;
    pdu[21] = 0x00;
    avtp::ParsedCvfPacket packet;
    avtp::ParseError error = avtp::ParseError::None;
    Require(!avtp::AvtpPacketParser::ParseCvfPdu(
                pdu.data(), pdu.size(), packet, &error),
            "truncated stream data rejected");
    Require(error == avtp::ParseError::StreamDataLengthTooLarge,
            "truncated stream data error");
}

void TestParserRejectsUnsupportedSubtype() {
    auto pdu = BuildCvfPayload(1, false, {0, 0, 0, 1, 0x65});
    pdu[0] = 0x7f;
    avtp::ParsedCvfPacket packet;
    avtp::ParseError error = avtp::ParseError::None;
    Require(!avtp::AvtpPacketParser::ParseCvfPdu(
                pdu.data(), pdu.size(), packet, &error),
            "unsupported subtype rejected");
    Require(error == avtp::ParseError::UnsupportedSubtype,
            "unsupported subtype error");
}

void TestParserAcceptsCustomSubtype() {
    const std::vector<uint8_t> payload = {
        0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10};
    const auto frame =
        WrapEthernet(BuildCustomSubtypePayload(7, true, payload));

    const avtp::ParsedCvfPacket packet = ParseFrame(frame);
    Require(packet.subtype == avtp::kSubtypeCustom,
            "custom subtype parsed");
    Require(packet.marker, "custom subtype marker parsed");
    Require(packet.payload_size == payload.size(),
            "custom subtype payload size");
    Require(std::equal(payload.begin(), payload.end(), packet.payload),
            "custom subtype payload preserved");
}

void TestCustomSubtypeCanCarryH264Payload() {
    const auto frame = WrapEthernet(BuildCustomSubtypePayload(
        9, true, DecodableKeyframePayload()));

    const avtp::ParsedCvfPacket packet = ParseFrame(frame);
    Require(packet.subtype == avtp::kSubtypeCustom,
            "custom subtype parsed for H264 payload");
    Require(packet.format_subtype == avtp::kCvfFormatSubtypeMjpeg,
            "custom subtype test starts with MJPEG format_subtype");
    Require(avtp::StartsWithAnnexBStartCode(
                packet.payload, packet.payload_size),
            "custom subtype H264 payload keeps Annex-B start code");

    avtp::AvtpH264Assembler assembler;
    avtp::H264AccessUnit output;
    Require(assembler.Push(packet, 1000, output) ==
                avtp::AvtpH264Assembler::Result::AccessUnitReady,
            "custom subtype H264 payload can assemble");
    Require(output.keyframe, "custom subtype H264 output keyframe");
}

void TestParserKeepsLooseFormatSubtype() {
    auto pdu = BuildCvfPayload(1, true, {0, 0, 0, 1, 0x65});
    pdu[17] = 0x7e;
    avtp::ParsedCvfPacket packet;
    avtp::ParseError error = avtp::ParseError::None;
    Require(avtp::AvtpPacketParser::ParseCvfPdu(
                pdu.data(), pdu.size(), packet, &error),
            std::string("loose format_subtype accepted: ") +
                avtp::AvtpPacketParser::ErrorToString(error));
    Require(packet.format_subtype == 0x7e, "loose format_subtype preserved");
}

void TestAssemblerBuildsAccessUnit() {
    avtp::AvtpH264Assembler assembler;
    avtp::H264AccessUnit output;

    const auto first_frame = WrapEthernet(BuildCvfPayload(
        10, false,
        {0, 0, 0, 1, 0x67, 0x42, 0xc0, 0x28,
         0, 0, 0, 1, 0x68, 0xce}));
    const auto first = ParseFrame(first_frame);
    Require(assembler.Push(first, 1000, output) ==
                avtp::AvtpH264Assembler::Result::NeedMore,
            "first fragment waits for marker");

    const std::vector<uint8_t> second_payload = {
        0, 0, 0, 1, 0x65, 0xaa, 0xbb};
    const auto second_frame = WrapEthernet(BuildCvfPayload(
        11, true, second_payload));
    const auto second = ParseFrame(second_frame);
    Require(assembler.Push(second, 2000, output) ==
                avtp::AvtpH264Assembler::Result::AccessUnitReady,
            "marker finishes AU");
    Require(output.keyframe, "IDR keyframe");
    Require(output.capture_timestamp_us == 2000, "capture timestamp");
    Require(output.data ==
                std::vector<uint8_t>({
                    0, 0, 0, 1, 0x67, 0x42, 0xc0, 0x28,
                    0, 0, 0, 1, 0x68, 0xce,
                    0, 0, 0, 1, 0x65, 0xaa, 0xbb}),
            "assembled Annex-B AU");
    Require(assembler.GetStats().access_units == 1, "AU count");
}

void TestAssemblerSequenceWrap() {
    avtp::AvtpH264Assembler assembler;
    avtp::H264AccessUnit output;

    auto prime_frame = WrapEthernet(BuildCvfPayload(
        254, true, DecodableKeyframePayload()));
    auto first_frame = WrapEthernet(BuildCvfPayload(
        255, false, {0, 0, 0, 1, 0x41, 0x01}));
    auto second_frame = WrapEthernet(BuildCvfPayload(
        0, true, {0x02, 0x03}));
    auto prime = ParseFrame(prime_frame);
    auto first = ParseFrame(first_frame);
    auto second = ParseFrame(second_frame);
    Require(assembler.Push(prime, 0, output) ==
                avtp::AvtpH264Assembler::Result::AccessUnitReady,
            "wrap prime decoder");
    Require(assembler.Push(first, 1, output) ==
                avtp::AvtpH264Assembler::Result::NeedMore,
            "wrap first");
    Require(assembler.Push(second, 2, output) ==
                avtp::AvtpH264Assembler::Result::AccessUnitReady,
            "wrap second");
    Require(assembler.GetStats().lost_packets == 0, "wrap has no loss");
}

void TestAssemblerGapDropsOnlyCorruptAccessUnit() {
    avtp::AvtpH264Assembler assembler;
    avtp::H264AccessUnit output;

    auto prime_frame = WrapEthernet(BuildCvfPayload(
        0, true, DecodableKeyframePayload()));
    auto first_frame = WrapEthernet(BuildCvfPayload(
        1, false, {0, 0, 0, 1, 0x41, 0x11}));
    auto gap_tail_frame = WrapEthernet(BuildCvfPayload(
        3, true, {0x22, 0x33}));
    auto recovered_frame = WrapEthernet(BuildCvfPayload(
        4, true, {0, 0, 0, 1, 0x41, 0xaa}));
    auto prime = ParseFrame(prime_frame);
    auto first = ParseFrame(first_frame);
    auto gap_tail = ParseFrame(gap_tail_frame);
    auto recovered = ParseFrame(recovered_frame);

    Require(assembler.Push(prime, 0, output) ==
                avtp::AvtpH264Assembler::Result::AccessUnitReady,
            "gap prime decoder");
    Require(assembler.Push(first, 1, output) ==
                avtp::AvtpH264Assembler::Result::NeedMore,
            "gap first");
    Require(assembler.Push(gap_tail, 2, output) ==
                avtp::AvtpH264Assembler::Result::Dropped,
            "gap tail dropped");
    Require(assembler.Push(recovered, 3, output) ==
                avtp::AvtpH264Assembler::Result::AccessUnitReady,
            "recovers after marker");
    Require(!output.keyframe, "recovered inter frame");
    Require(assembler.GetStats().lost_packets == 1, "lost packet count");
    Require(assembler.GetStats().dropped_access_units == 1,
            "dropped AU count");
}

void TestAssemblerWaitsForAnnexBStart() {
    avtp::AvtpH264Assembler assembler;
    avtp::H264AccessUnit output;

    auto middle_frame = WrapEthernet(BuildCvfPayload(
        1, false, {0x12, 0x34, 0x56}));
    auto tail_frame = WrapEthernet(BuildCvfPayload(
        2, true, {0x78, 0x9a}));
    auto recovered_frame = WrapEthernet(BuildCvfPayload(
        3, true, DecodableKeyframePayload()));
    auto middle = ParseFrame(middle_frame);
    auto tail = ParseFrame(tail_frame);
    auto recovered = ParseFrame(recovered_frame);

    Require(assembler.Push(middle, 1, output) ==
                avtp::AvtpH264Assembler::Result::Dropped,
            "middle packet ignored");
    Require(assembler.Push(tail, 2, output) ==
                avtp::AvtpH264Assembler::Result::Dropped,
            "tail packet ignored until marker");
    Require(assembler.Push(recovered, 3, output) ==
                avtp::AvtpH264Assembler::Result::AccessUnitReady,
            "recovers on next start code");
    Require(output.keyframe, "recovered starts with decodable keyframe");
    Require(assembler.GetStats().waiting_for_start_packets == 1,
            "waiting for start count");
}

void TestAssemblerDropsUntilParameterSets() {
    avtp::AvtpH264Assembler assembler;
    avtp::H264AccessUnit output;

    auto p_frame_data = WrapEthernet(BuildCvfPayload(
        1, true, {0, 0, 0, 1, 0x41, 0x01}));
    auto idr_without_params_data = WrapEthernet(BuildCvfPayload(
        2, true, {0, 0, 0, 1, 0x65, 0xaa}));
    auto decodable_idr_data = WrapEthernet(BuildCvfPayload(
        3, true, DecodableKeyframePayload()));
    auto p_frame = ParseFrame(p_frame_data);
    auto idr_without_params = ParseFrame(idr_without_params_data);
    auto decodable_idr = ParseFrame(decodable_idr_data);

    Require(assembler.Push(p_frame, 1, output) ==
                avtp::AvtpH264Assembler::Result::Dropped,
            "initial P frame dropped before decoder ready");
    Require(assembler.Push(idr_without_params, 2, output) ==
                avtp::AvtpH264Assembler::Result::Dropped,
            "IDR without SPS/PPS dropped before decoder ready");
    Require(assembler.Push(decodable_idr, 3, output) ==
                avtp::AvtpH264Assembler::Result::AccessUnitReady,
            "decodable IDR opens stream");
    Require(output.keyframe, "first output is keyframe");
    Require(assembler.GetStats().access_units == 1,
            "only decodable AU emitted");
    Require(assembler.GetStats().dropped_access_units == 2,
            "undecodable startup AUs dropped");
}

void TestAssemblerOversizeProtection() {
    avtp::AvtpH264Assembler assembler(8);
    avtp::H264AccessUnit output;

    auto frame = WrapEthernet(BuildCvfPayload(
        1, true, {0, 0, 0, 1, 0x65, 1, 2, 3, 4, 5}));
    auto packet = ParseFrame(frame);
    Require(assembler.Push(packet, 1, output) ==
                avtp::AvtpH264Assembler::Result::Malformed,
            "oversized AU rejected");
    Require(assembler.GetStats().malformed_packets == 1,
            "oversized AU malformed count");
}

} // namespace

int main() {
    TestParserPreservesFullStreamData();
    TestParserVlanAndMarker();
    TestParserRejectsTruncatedStreamData();
    TestParserRejectsUnsupportedSubtype();
    TestParserAcceptsCustomSubtype();
    TestCustomSubtypeCanCarryH264Payload();
    TestParserKeepsLooseFormatSubtype();
    TestAssemblerBuildsAccessUnit();
    TestAssemblerSequenceWrap();
    TestAssemblerGapDropsOnlyCorruptAccessUnit();
    TestAssemblerWaitsForAnnexBStart();
    TestAssemblerDropsUntilParameterSets();
    TestAssemblerOversizeProtection();
    std::cout << "All AVTP protocol tests passed.\n";
    return 0;
}
