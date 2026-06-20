#include "rtp/nalsource.h"

namespace rtp {

    uint8_t NALSource::GetH264NALType(const std::vector<uint8_t>& nal_data) {
        if (nal_data.empty()) {
            return 0;
        }
        // H.264 NAL 头（1字节）：forbidden_zero_bit(1) + nal_ref_idc(2) + nal_unit_type(5)
        // 1 P帧B帧；5 IDR帧；6 SEI帧；7 SPS帧；8 PPS帧；9 单元分隔符AUD
        return nal_data[0] & 0x1F;
    }

    uint8_t NALSource::GetH265NALType(const std::vector<uint8_t>& nal_data) {
        if (nal_data.empty()) {
            return 0;
        }

        // H.265 NAL 头（2字节）：forbidden_zero_bit(1) + nuh_layer_id(6) + nuh_temporal_id_plus1(3) + nal_unit_type(6)
        return ((nal_data[0] >> 1) & 0x3F);
    }

    bool NALSource::IsH264IDR(uint8_t nal_type) {
        // H.264: Type 5 = IDR帧
        return nal_type == 5;
    }

    bool NALSource::IsH265IDR(uint8_t nal_type) {
        // H.265: Type 19 = IDR (BLA), Type 20 = IDR (CRA)
        return nal_type == 19 || nal_type == 20;
    }
}
