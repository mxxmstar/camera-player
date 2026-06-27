#pragma once
#include <memory>
#include <vector>
#include <queue>
#include <utility>
#include <cstdint>
#include "net/tcpsession.h"
#include "rtsp/rtsp.h"
#include "rtp/rtptransport.h"
#include "rtp/rtpsender.h"
#include "rtp/media.h"
#include "rtp/rtp.h"
#include "rtp/h264source.h"
#include "rtp/nalsource.h"
#include <asio.hpp>

namespace rtsp {

struct RtpPacketQueue {
    std::queue<rtp::RtpPacket> queue;

    bool Push(rtp::MediaChannelId, rtp::RtpPacket pkt) {
        queue.push(std::move(pkt));
        return true;
    }

    std::shared_ptr<rtp::RtpPacket> Pop() {
        if (queue.empty()) return nullptr;
        auto pkt = std::make_shared<rtp::RtpPacket>(std::move(queue.front()));
        queue.pop();
        return pkt;
    }

    bool HasData() const { return !queue.empty(); }
};

class RtspSession : public AsioTCPSession {
public:
    explicit RtspSession(asio::ip::tcp::socket socket);

    void SetRTPSources(const std::vector<std::shared_ptr<rtp::RTPSource>>& sources);
    void LoadVideoFile(const std::string& filepath);
    bool HasVideoFile() const { return nal_source_ != nullptr; }

protected:
    void OnBytes(const uint8_t* data, size_t size) override;
    void OnClose() override;

private:
    void ProcessInterleavedData();
    void HandleRtcpData(const uint8_t* data, size_t size);
    void HandleRtpData(const uint8_t* data, size_t size);
    
    void HandleRtspRequest(const std::string& request);
    std::string BuildResponse(const RtspResponse& response, const std::string& cseq);

    std::string HandleOptions(const std::map<std::string, std::string>& headers);
    std::string HandleDescribe(const std::map<std::string, std::string>& headers);
    std::string HandleSetup(const std::map<std::string, std::string>& headers);
    std::string HandlePlay(const std::map<std::string, std::string>& headers);
    std::string HandleTeardown(const std::map<std::string, std::string>& headers);
    std::string HandlePause(const std::map<std::string, std::string>& headers);

    /// @brief 解析Range头中的数字范围
    /// @param str Range头中的字符串，例如"npt=10.0-20.0"
    /// @return 包含范围开始和结束的std::pair，例如{"10.0", "20.0"}
    inline std::pair<std::string, std::string> parseRangeNum(const std::string& str);
    inline const std::string buildResponse(int status_code, const std::string& reason);

    void SendSpsPps();

    std::shared_ptr<rtp::RtpPacket> ProduceNextPacket();
    void FeedNextNALFrame();
    
    RtspContext context_;
    ///@brief RTP 传输层
    std::shared_ptr<rtp::AsioRtpTransport> rtp_transport_;
    ///@brief 媒体源列表
    std::vector<std::shared_ptr<rtp::RTPSource>> media_sources_;

    std::shared_ptr<rtp::NALSource> nal_source_;
    std::shared_ptr<rtp::H264Source> h264_source_;
    RtpPacketQueue packet_queue_;
    std::vector<uint8_t> real_sps_;
    std::vector<uint8_t> real_pps_;
    std::string video_filepath_;
    uint32_t frame_index_ = 0;

    std::vector<uint8_t> read_buffer_;
};

}
