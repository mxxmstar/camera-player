#include "rtp/h264_rtp_depacketizer.h"
#include "rtp/rtp_packet_parser.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::vector<uint8_t> BuildRtp(
    uint16_t sequence, uint32_t timestamp, bool marker,
    const std::vector<uint8_t>& payload, uint32_t ssrc = 0x95C12177) {
    std::vector<uint8_t> datagram(12 + payload.size());
    datagram[0] = 0x80;
    datagram[1] = static_cast<uint8_t>((marker ? 0x80 : 0) | 96);
    datagram[2] = static_cast<uint8_t>(sequence >> 8);
    datagram[3] = static_cast<uint8_t>(sequence);
    datagram[4] = static_cast<uint8_t>(timestamp >> 24);
    datagram[5] = static_cast<uint8_t>(timestamp >> 16);
    datagram[6] = static_cast<uint8_t>(timestamp >> 8);
    datagram[7] = static_cast<uint8_t>(timestamp);
    datagram[8] = static_cast<uint8_t>(ssrc >> 24);
    datagram[9] = static_cast<uint8_t>(ssrc >> 16);
    datagram[10] = static_cast<uint8_t>(ssrc >> 8);
    datagram[11] = static_cast<uint8_t>(ssrc);
    std::copy(payload.begin(), payload.end(), datagram.begin() + 12);
    return datagram;
}

rtp::H264RtpDepacketizer::Result PushDatagram(
    rtp::H264RtpDepacketizer& depacketizer,
    const std::vector<uint8_t>& datagram,
    rtp::H264AccessUnit& output) {
    rtp::ParsedRtpPacket packet;
    Require(
        rtp::RtpPacketParser::Parse(
            datagram.data(), datagram.size(), packet),
        "test RTP packet must parse");
    return depacketizer.Push(packet, output);
}

void TestRtpHeaderOptions() {
    std::vector<uint8_t> datagram = {
        0xB1, 0xE0, 0x12, 0x34,
        0x01, 0x02, 0x03, 0x04,
        0x11, 0x22, 0x33, 0x44,
        0xAA, 0xBB, 0xCC, 0xDD,
        0xBE, 0xDE, 0x00, 0x01,
        0x10, 0x20, 0x30, 0x40,
        0x65, 0x88,
        0x00, 0x02,
    };

    rtp::ParsedRtpPacket packet;
    Require(
        rtp::RtpPacketParser::Parse(
            datagram.data(), datagram.size(), packet),
        "RTP packet with CSRC, extension and padding");
    Require(packet.marker, "marker bit");
    Require(packet.payload_type == 96, "payload type");
    Require(packet.sequence_number == 0x1234, "sequence number");
    Require(packet.timestamp == 0x01020304, "timestamp");
    Require(packet.ssrc == 0x11223344, "SSRC");
    Require(
        packet.payload_size == 2 &&
            packet.payload[0] == 0x65 &&
            packet.payload[1] == 0x88,
        "payload boundaries");
}

void TestSingleNalAndStapA() {
    rtp::H264RtpDepacketizer depacketizer;
    rtp::H264AccessUnit output;

    auto result = PushDatagram(
        depacketizer,
        BuildRtp(1, 90000, false, {0x67, 0x64, 0x00, 0x32}),
        output);
    Require(
        result == rtp::H264RtpDepacketizer::Result::NeedMore,
        "single SPS waits for marker");

    result = PushDatagram(
        depacketizer,
        BuildRtp(
            2, 90000, true,
            {0x78, 0x00, 0x02, 0x68, 0xEE,
             0x00, 0x03, 0x65, 0xAA, 0xBB}),
        output);
    Require(
        result ==
            rtp::H264RtpDepacketizer::Result::AccessUnitReady,
        "STAP-A finishes access unit");
    Require(output.keyframe, "STAP-A IDR detection");
    Require(
        output.data ==
            std::vector<uint8_t>({
                0, 0, 0, 1, 0x67, 0x64, 0x00, 0x32,
                0, 0, 0, 1, 0x68, 0xEE,
                0, 0, 0, 1, 0x65, 0xAA, 0xBB}),
        "single NAL and STAP-A Annex-B output");
}

void TestStandardFuA() {
    rtp::H264RtpDepacketizer depacketizer;
    rtp::H264AccessUnit output;

    Require(
        PushDatagram(
            depacketizer,
            BuildRtp(10, 1234, false, {0x7C, 0x85, 0x11, 0x22}),
            output) ==
            rtp::H264RtpDepacketizer::Result::NeedMore,
        "FU-A start");
    Require(
        PushDatagram(
            depacketizer,
            BuildRtp(11, 1234, false, {0x7C, 0x05, 0x33}),
            output) ==
            rtp::H264RtpDepacketizer::Result::NeedMore,
        "FU-A continuation");
    Require(
        PushDatagram(
            depacketizer,
            BuildRtp(12, 1234, true, {0x7C, 0x45, 0x44}),
            output) ==
            rtp::H264RtpDepacketizer::Result::AccessUnitReady,
        "FU-A end");
    Require(
        output.data ==
            std::vector<uint8_t>(
                {0, 0, 0, 1, 0x65, 0x11, 0x22, 0x33, 0x44}),
        "FU-A reconstructed NAL");
    Require(output.keyframe, "FU-A IDR detection");
}

void TestCameraAnnexBFragmentation() {
    const std::vector<uint8_t> original = {
        0, 0, 0, 1, 0x67, 0x64, 0x00, 0x32,
        0, 0, 0, 1, 0x68, 0xEE, 0x3C, 0xB0,
        0, 0, 0, 1, 0x65, 0x88, 0x99, 0xAA,
    };

    rtp::H264RtpDepacketizer depacketizer;
    rtp::H264AccessUnit output;
    Require(
        PushDatagram(
            depacketizer,
            BuildRtp(
                1, 1228519357, false,
                {0x1C, 0x80, 0x00, 0x00, 0x01, 0x67,
                 0x64, 0x00, 0x32}),
            output) ==
            rtp::H264RtpDepacketizer::Result::NeedMore,
        "camera FU start");
    Require(
        PushDatagram(
            depacketizer,
            BuildRtp(
                2, 1228519357, false,
                {0x1C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x68,
                 0xEE, 0x3C, 0xB0}),
            output) ==
            rtp::H264RtpDepacketizer::Result::NeedMore,
        "camera FU continuation");
    Require(
        PushDatagram(
            depacketizer,
            BuildRtp(
                3, 1228519357, true,
                {0x1C, 0x40, 0x00, 0x00, 0x00, 0x01,
                 0x65, 0x88, 0x99, 0xAA}),
            output) ==
            rtp::H264RtpDepacketizer::Result::AccessUnitReady,
        "camera FU end");
    Require(output.data == original, "camera Annex-B exact reconstruction");
    Require(output.keyframe, "embedded Annex-B IDR detection");
}

void TestLossDropsOnlyCurrentAccessUnit() {
    rtp::H264RtpDepacketizer depacketizer;
    rtp::H264AccessUnit output;

    (void)PushDatagram(
        depacketizer,
        BuildRtp(100, 1000, false, {0x7C, 0x81, 0x11}),
        output);
    Require(
        PushDatagram(
            depacketizer,
            BuildRtp(102, 1000, true, {0x7C, 0x41, 0x22}),
            output) == rtp::H264RtpDepacketizer::Result::Dropped,
        "sequence gap drops incomplete access unit");
    Require(
        PushDatagram(
            depacketizer,
            BuildRtp(103, 2000, true, {0x65, 0xAA}),
            output) ==
            rtp::H264RtpDepacketizer::Result::AccessUnitReady,
        "next access unit recovers");
    Require(
        depacketizer.GetStats().lost_packets == 1,
        "lost packet count");
}

std::vector<uint8_t> DecodeHex(const std::string& text) {
    Require(text.size() % 2 == 0, "hex payload length");
    std::vector<uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(
            std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

void ReplayTsharkFields(std::istream& input,
                        std::size_t expected_packets,
                        std::size_t expected_access_units) {
    rtp::H264RtpDepacketizer depacketizer;
    std::size_t packet_count = 0;
    std::size_t access_unit_count = 0;
    std::size_t keyframe_count = 0;
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> fields;
        std::size_t start = 0;
        while (true) {
            const std::size_t tab = line.find('\t', start);
            fields.push_back(line.substr(
                start,
                tab == std::string::npos
                    ? std::string::npos
                    : tab - start));
            if (tab == std::string::npos) {
                break;
            }
            start = tab + 1;
        }
        Require(fields.size() == 5, "tshark field count");

        std::vector<uint8_t> payload = DecodeHex(fields[4]);
        rtp::ParsedRtpPacket packet;
        packet.sequence_number =
            static_cast<uint16_t>(std::stoul(fields[0], nullptr, 0));
        packet.timestamp =
            static_cast<uint32_t>(std::stoul(fields[1], nullptr, 0));
        packet.marker =
            fields[2] == "True" || fields[2] == "1";
        packet.ssrc =
            static_cast<uint32_t>(std::stoul(fields[3], nullptr, 0));
        packet.payload_type = 96;
        packet.payload = payload.data();
        packet.payload_size = payload.size();

        rtp::H264AccessUnit output;
        const auto result = depacketizer.Push(packet, output);
        ++packet_count;
        if (result ==
            rtp::H264RtpDepacketizer::Result::AccessUnitReady) {
            ++access_unit_count;
            if (output.keyframe) {
                ++keyframe_count;
            }
        }
    }

    const auto stats = depacketizer.GetStats();
    Require(
        packet_count == expected_packets,
        "capture RTP packet count");
    Require(
        access_unit_count == expected_access_units,
        "capture access unit count");
    Require(stats.lost_packets == 0, "capture has no sequence gaps");
    Require(
        stats.malformed_packets == 0,
        "capture has no malformed H.264 payloads");
    Require(keyframe_count >= 1, "capture contains a keyframe");
    std::cout
        << "Capture replay passed: " << packet_count
        << " RTP packets -> " << access_unit_count
        << " H.264 access units.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--tshark-fields") {
        const std::size_t expected_packets =
            argc > 2
                ? static_cast<std::size_t>(
                      std::stoull(argv[2], nullptr, 0))
                : 689;
        const std::size_t expected_access_units =
            argc > 3
                ? static_cast<std::size_t>(
                      std::stoull(argv[3], nullptr, 0))
                : 5;
        ReplayTsharkFields(
            std::cin, expected_packets, expected_access_units);
        return 0;
    }

    TestRtpHeaderOptions();
    TestSingleNalAndStapA();
    TestStandardFuA();
    TestCameraAnnexBFragmentation();
    TestLossDropsOnlyCurrentAccessUnit();
    std::cout << "All RTP protocol tests passed.\n";
    return 0;
}
