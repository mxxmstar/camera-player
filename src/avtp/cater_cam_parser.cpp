#include "avtp/cater_cam_parser.h"

namespace avtp {

bool CaterCamParser::IsCaterCamFormat(const ParsedCvfPacket& cvf) {
    // Cater CAM uses format_subtype=0x00 with raw encrypted payload
    if (cvf.format_subtype != kCvfFormatSubtypeCustom) {
        return false;
    }

    // Cater CAM payload should not have Annex-B start codes
    // (it's encrypted data)
    if (cvf.payload && cvf.payload_size >= 4) {
        // Check first few bytes for start codes - if found, likely not Cater CAM
        if ((cvf.payload[0] == 0 && cvf.payload[1] == 0 &&
             cvf.payload[2] == 0 && cvf.payload[3] == 1) ||
            (cvf.payload_size >= 3 && cvf.payload[0] == 0 &&
             cvf.payload[1] == 0 && cvf.payload[2] == 1)) {
            return false;
        }
    }

    return true;
}

bool CaterCamParser::Parse(const ParsedCvfPacket& cvf, CaterCamPacket& output) {
    if (!IsCaterCamFormat(cvf)) {
        return false;
    }

    output.stream_id = cvf.stream_id;
    output.sequence_num = cvf.sequence_num;
    output.avtp_timestamp = cvf.avtp_timestamp;
    output.timestamp_valid = cvf.timestamp_valid;
    output.marker = cvf.marker;
    output.event = cvf.event;
    output.source_mac = cvf.source_mac;
    output.payload = cvf.payload;
    output.payload_size = cvf.payload_size;

    return true;
}

} // namespace avtp
