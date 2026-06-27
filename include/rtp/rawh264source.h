#pragma once
#include "rtp/nalsource.h"
#include <fstream>
#include <string>
#include <vector>

namespace rtp {

class RawH264NALSource : public NALSource {
public:
    RawH264NALSource();
    ~RawH264NALSource() override;

    bool Open(const std::string& source) override;
    void Close() override;
    FrameNAL ReadNextFrame() override;
    bool TryReadNextFrame(FrameNAL& frame_nals) override;
    bool HasMoreData() const override;
    MediaType GetMediaType() const override { return H264; }
    std::string GetCodecName() const override { return "h264"; }
    uint32_t GetFrameRate() const override { return frame_rate_; }
    int GetWidth() const override { return width_; }
    int GetHeight() const override { return height_; }
    int64_t GetDuration() const override { return duration_ms_; }
    bool Seek(int64_t timestamp_ms) override;
    void Reset() override;

    void SetFilePath(const std::string& path) { filepath_ = path; }
    void SetFrameRate(uint32_t fps) { frame_rate_ = fps; }

private:
    struct NALData {
        std::vector<uint8_t> data;
        uint8_t nal_type;
    };

    std::vector<uint8_t> ReadFile(const std::string& path);
    std::vector<NALData> ExtractNALUnits(const std::vector<uint8_t>& data);

    std::string filepath_;
    std::vector<uint8_t> file_data_;
    size_t current_pos_ = 0;
    std::vector<NALData> cached_nals_;
    size_t nal_index_ = 0;

    uint32_t frame_rate_ = 25;
    int width_ = 0;
    int height_ = 0;
    int64_t duration_ms_ = 0;
    bool is_opened_ = false;
    bool eof_reached_ = false;

    // SPS/PPS cache for determining frame boundaries
    std::vector<uint8_t> sps_;
    std::vector<uint8_t> pps_;
};

} // namespace rtp
