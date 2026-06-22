#include "rtp/rawh264source.h"
#include <cstring>
#include <iostream>

namespace rtp {

RawH264NALSource::RawH264NALSource() = default;
RawH264NALSource::~RawH264NALSource() { Close(); }

bool RawH264NALSource::Open(const std::string& source) {
    filepath_ = source;
    file_data_ = ReadFile(source);
    if (file_data_.empty()) {
        std::cerr << "[RawH264] Failed to read file: " << source << std::endl;
        return false;
    }

    cached_nals_ = ExtractNALUnits(file_data_);
    if (cached_nals_.empty()) {
        std::cerr << "[RawH264] No NAL units found in file" << std::endl;
        return false;
    }

    current_pos_ = 0;
    nal_index_ = 0;
    is_opened_ = true;
    eof_reached_ = false;

    for (auto& nal : cached_nals_) {
        if (nal.nal_type == 7) sps_ = nal.data; // SPS
        if (nal.nal_type == 8) pps_ = nal.data; // PPS
    }

    width_ = 1920;
    height_ = 1080;

    std::cout << "[RawH264] Loaded " << cached_nals_.size()
              << " NAL units from " << source
              << " (SPS=" << (sps_.empty() ? 0 : sps_.size())
              << ", PPS=" << (pps_.empty() ? 0 : pps_.size()) << ")"
              << std::endl;
    return true;
}

void RawH264NALSource::Close() {
    file_data_.clear();
    cached_nals_.clear();
    sps_.clear();
    pps_.clear();
    is_opened_ = false;
    eof_reached_ = false;
    current_pos_ = 0;
    nal_index_ = 0;
}

NALSource::FrameNAL RawH264NALSource::ReadNextFrame() {
    if (!is_opened_ || eof_reached_ || nal_index_ >= cached_nals_.size()) {
        eof_reached_ = true;
        return std::nullopt;
    }

    NALUnitList frame_nals;
    uint8_t first_vcl_type = 0;

    while (nal_index_ < cached_nals_.size()) {
        auto& nal = cached_nals_[nal_index_];
        NALUnit unit(static_cast<uint32_t>(nal.data.size()));
        std::memcpy(unit.data.get(), nal.data.data(), nal.data.size());
        unit.type = nal.nal_type;
        unit.is_keyframe = (nal.nal_type == 5);
        unit.pts = nal_index_;
        unit.dts = nal_index_;

        bool is_vcl = (nal.nal_type >= 1 && nal.nal_type <= 5);

        if (frame_nals.empty()) {
            frame_nals.push_back(std::move(unit));
            if (is_vcl) first_vcl_type = nal.nal_type;
            nal_index_++;
            continue;
        }

        if (is_vcl && first_vcl_type != 0) {
            break;
        }

        if (nal.nal_type == 9) {
            nal_index_++;
            continue;
        }

        if (is_vcl) {
            first_vcl_type = nal.nal_type;
        }

        frame_nals.push_back(std::move(unit));
        nal_index_++;
    }

    if (frame_nals.empty()) {
        eof_reached_ = true;
        return std::nullopt;
    }

    return frame_nals;
}

bool RawH264NALSource::TryReadNextFrame(FrameNAL& frame_nals) {
    frame_nals = ReadNextFrame();
    return frame_nals.has_value();
}

bool RawH264NALSource::HasMoreData() const {
    return is_opened_ && !eof_reached_ && nal_index_ < cached_nals_.size();
}

bool RawH264NALSource::Seek(int64_t timestamp_ms) {
    (void)timestamp_ms;
    nal_index_ = 0;
    eof_reached_ = false;
    return true;
}

void RawH264NALSource::Reset() {
    nal_index_ = 0;
    eof_reached_ = false;
}

std::vector<uint8_t> RawH264NALSource::ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};

    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf(size);
    file.read(reinterpret_cast<char*>(buf.data()), size);
    return buf;
}

std::vector<RawH264NALSource::NALData> RawH264NALSource::ExtractNALUnits(
    const std::vector<uint8_t>& data) {
    std::vector<NALData> nals;
    size_t i = 0;

    while (i < data.size()) {
        size_t start_code_size = 0;

        if (i + 4 <= data.size() && data[i] == 0 && data[i+1] == 0 &&
            data[i+2] == 0 && data[i+3] == 1) {
            start_code_size = 4;
        } else if (i + 3 <= data.size() && data[i] == 0 && data[i+1] == 0 &&
                   data[i+2] == 1) {
            start_code_size = 3;
        }

        if (start_code_size == 0) {
            i++;
            continue;
        }

        size_t nal_start = i + start_code_size;
        size_t j = nal_start;

        while (j < data.size()) {
            if (j + 4 <= data.size() && data[j] == 0 && data[j+1] == 0 &&
                data[j+2] == 0 && data[j+3] == 1) break;
            if (j + 3 <= data.size() && data[j] == 0 && data[j+1] == 0 &&
                data[j+2] == 1) break;
            j++;
        }

        NALData nal;
        nal.data.assign(data.begin() + static_cast<ptrdiff_t>(nal_start),
                        data.begin() + static_cast<ptrdiff_t>(j));
        if (!nal.data.empty()) {
            nal.nal_type = nal.data[0] & 0x1F;
            nals.push_back(std::move(nal));
        }

        i = j;
    }

    return nals;
}

} // namespace rtp
