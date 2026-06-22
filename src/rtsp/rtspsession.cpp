#include "rtsp/rtspsession.h"
#include "rtsp/rtsp.h"
#include "rtsp/rtspmacro.h"
#include "rtp/rtptransport.h"
#include "rtp/rtpsender.h"
#include "rtp/media.h"
#include "rtp/h264source.h"
#include "rtp/nalsource.h"
#include "rtp/rawh264source.h"
#include <sstream>
#include <chrono>
#include <random>

namespace rtsp {

    RtspSession::RtspSession(asio::ip::tcp::socket socket)
        : AsioTCPSession(std::move(socket)) {
        LOG_RTSP_INFO_AT("RtspSession created, session_id: {}", GetSessionID());
    }

    void RtspSession::SetRTPSources(const std::vector<std::shared_ptr<rtp::RTPSource>>& sources) {
        media_sources_ = sources;
    }

    void RtspSession::LoadVideoFile(const std::string& filepath) {
        video_filepath_ = filepath;

        nal_source_ = std::make_shared<rtp::RawH264NALSource>();
        if (!nal_source_->Open(filepath)) {
            LOG_RTSP_ERROR_AT("Failed to open video file: {}", filepath);
            nal_source_.reset();
            return;
        }

        h264_source_ = std::make_shared<rtp::H264Source>(nal_source_->GetFrameRate());
        h264_source_->SetSendFrameCallback(
            [this](rtp::MediaChannelId ch, rtp::RtpPacket pkt) -> bool {
                return packet_queue_.Push(ch, std::move(pkt));
            });

        real_sps_.clear();
        real_pps_.clear();

        auto raw_source = std::static_pointer_cast<rtp::RawH264NALSource>(nal_source_);
        (void)raw_source;

        auto frame = nal_source_->ReadNextFrame();
        if (frame) {
            for (auto& nal : *frame) {
                if (nal.type == 7) {
                    real_sps_.resize(nal.size);
                    std::memcpy(real_sps_.data(), nal.data.get(), nal.size);
                } else if (nal.type == 8) {
                    real_pps_.resize(nal.size);
                    std::memcpy(real_pps_.data(), nal.data.get(), nal.size);
                }
            }
            nal_source_->Reset();
        }

        media_sources_.clear();
        media_sources_.push_back(h264_source_);

        LOG_RTSP_INFO_AT("Video file loaded: {} ({}fps, {}x{})",
            filepath,
            nal_source_->GetFrameRate(),
            nal_source_->GetWidth(),
            nal_source_->GetHeight());
    }

    void RtspSession::OnBytes(const uint8_t* data, size_t size) {
        read_buffer_.insert(read_buffer_.end(), data, data + size);

        if (context_.state == RtspState::PLAYING || context_.state == RtspState::READY) {
            ProcessInterleavedData();
        } else {
            HandleRtspRequest(std::string(
                reinterpret_cast<const char*>(read_buffer_.data()), read_buffer_.size()));
            read_buffer_.clear();
        }
    }

    void RtspSession::ProcessInterleavedData() {
        while (read_buffer_.size() >= 4) {
            if (read_buffer_[0] == '$') {
                uint8_t channel = read_buffer_[1];
                uint16_t length = (static_cast<uint16_t>(read_buffer_[2]) << 8) | read_buffer_[3];
                size_t frame_size = 4 + length;

                if (read_buffer_.size() < frame_size) {
                    break;
                }

                if (channel == context_.rtcp_channel) {
                    HandleRtcpData(read_buffer_.data() + 4, length);
                } else if (channel == context_.rtp_channel) {
                    HandleRtpData(read_buffer_.data() + 4, length);
                }

                read_buffer_.erase(read_buffer_.begin(),
                    read_buffer_.begin() + static_cast<ptrdiff_t>(frame_size));
            } else {
                if (read_buffer_[0] == 'O' || read_buffer_[0] == 'D' ||
                    read_buffer_[0] == 'S' || read_buffer_[0] == 'P' ||
                    read_buffer_[0] == 'T') {
                    HandleRtspRequest(std::string(
                        reinterpret_cast<const char*>(read_buffer_.data()), read_buffer_.size()));
                }
                read_buffer_.clear();
                break;
            }
        }
    }

    void RtspSession::OnClose() {
        LOG_RTSP_INFO_AT("RtspSession closed, session_id: {}", GetSessionID());
    }

    void RtspSession::HandleRtcpData(const uint8_t* data, size_t size) {
        LOG_RTSP_INFO_AT("Received RTCP data over interleaved, size: {}", size);
    }

    void RtspSession::HandleRtpData(const uint8_t* data, size_t size) {
        LOG_RTSP_INFO_AT("Received RTP data over interleaved, size: {}", size);
    }

    void RtspSession::HandleRtspRequest(const std::string& request) {
        LOG_RTSP_INFO_AT("handleRtspRequest: {}", request);
        std::istringstream iss(request);
        std::string request_line;
        std::getline(iss, request_line);

        RtspMethod method;
        std::string url;
        std::string version;
        if (!RtspProtocol::ParseRtspRequestLine(request_line, method, url, version)) {
            LOG_RTSP_ERROR_AT("Invalid RTSP request line: {}", request_line);
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            std::string response = BuildResponse(resp, "-1");
            Send(response);
            return;
        }

        std::map<std::string, std::string> headers = RtspProtocol::ParseHeaders(request);

        if (headers.find("CSeq") != headers.end()) {
            try {
                context_.cseq = headers["CSeq"];
            }
            catch (const std::exception& e) {
                context_.cseq = "";
                LOG_RTSP_ERROR_AT("Invalid CSeq: {}", headers["CSeq"]);
            }
        }

        context_.url = url;

        std::string response;
        switch (method) {
        case RtspMethod::OPTIONS:
            response = HandleOptions(headers);
            break;
        case RtspMethod::DESCRIBE:
            response = HandleDescribe(headers);
            break;
        case RtspMethod::SETUP:
            response = HandleSetup(headers);
            break;
        case RtspMethod::PLAY:
            response = HandlePlay(headers);
            break;
        case RtspMethod::TEARDOWN:
            response = HandleTeardown(headers);
            break;
        case RtspMethod::PAUSE:
            response = HandlePause(headers);
            break;
        case RtspMethod::GET_PARAMETER:
        {
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::OK);
            resp.status_reason = "OK";
            resp.headers["Session"] = context_.session_id;
            resp.headers["Content-Length"] = "0";
            response = BuildResponse(resp, context_.cseq);
            break;
        }
        case RtspMethod::SET_PARAMETER:
        {
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::OK);
            resp.status_reason = "OK";
            resp.headers["Session"] = context_.session_id;
            response = BuildResponse(resp, context_.cseq);
            break;
        }
        default:
        {
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::MethodNotAllowed);
            resp.status_reason = "Method Not Allowed";
            response = BuildResponse(resp, context_.cseq);
            break;
        }
        }

        if (!response.empty()) {
            Send(response);
        }
    }

    std::string RtspSession::BuildResponse(const RtspResponse& response, const std::string& cseq) {
        return RtspProtocol::BuildResponse(response, cseq);
    }

    std::string RtspSession::HandleOptions(const std::map<std::string, std::string>& headers) {
        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Public"] = "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, PAUSE, GET_PARAMETER, SET_PARAMETER";
        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandleDescribe(const std::map<std::string, std::string>& headers) {
        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";

        if (context_.session_id.empty()) {
            context_.session_id = GetSessionID();
        }

        std::string sdp = RtspProtocol::GenerateSdp(GetLocalAddress(), context_.session_id, media_sources_);
        resp.body = sdp;
        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandleSetup(const std::map<std::string, std::string>& headers) {
        auto transport_it = headers.find("Transport");
        if (transport_it == headers.end()) {
            LOG_RTSP_ERROR_AT("Transport header not found in SETUP request");
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }

        std::string transport_header = transport_it->second;
        LOG_RTSP_INFO_AT("Transport header: {}", transport_header);

        if (context_.session_id.empty()) {
            context_.session_id = GetSessionID();
        }

        bool is_tcp = transport_header.find("RTP/AVP/TCP") != std::string::npos;
        bool is_udp = !is_tcp && transport_header.find("RTP/AVP") != std::string::npos;
        bool is_multi = transport_header.find("multicast") != std::string::npos;

        if (is_tcp) {
            context_.mode = rtp::TransportMode::RTP_OVER_TCP;
            std::size_t pos = transport_header.find("interleaved=");
            if (pos != std::string::npos) {
                std::string interleaved_str = transport_header.substr(pos + 12);
                std::size_t pos2 = interleaved_str.find("-");
                if (pos2 != std::string::npos) {
                    context_.rtp_channel = std::stoi(interleaved_str.substr(0, pos2));
                } else {
                    LOG_RTSP_ERROR_AT("Invalid interleaved parameter: {}", interleaved_str);
                    context_.rtp_channel = 0;
                    context_.rtcp_channel = 1;
                }
                std::size_t end = interleaved_str.find(";");
                if (end == std::string::npos) {
                    context_.rtcp_channel = std::stoi(interleaved_str.substr(pos2 + 1));
                } else {
                    context_.rtcp_channel = std::stoi(interleaved_str.substr(pos2 + 1, end - pos2 - 1));
                }
            }
        } else if (is_udp) {
            context_.mode = rtp::TransportMode::RTP_OVER_UDP;
            auto client_port_it = transport_header.find("client_port=");
            if (client_port_it == std::string::npos) {
                LOG_RTSP_ERROR_AT("client_port header not found in UDP SETUP request");
                RtspResponse resp;
                resp.status_code = static_cast<int>(RtspStatus::BadRequest);
                resp.status_reason = "Bad Request";
                return BuildResponse(resp, context_.cseq);
            }
            auto client_port_pair = parseRangeNum(transport_header.substr(client_port_it + 12));
            if (client_port_pair.first.empty() || client_port_pair.second.empty()) {
                LOG_RTSP_ERROR_AT("Invalid client-Port range: {}", transport_header.substr(client_port_it + 12));
                RtspResponse resp;
                resp.status_code = static_cast<int>(RtspStatus::BadRequest);
                resp.status_reason = "Bad Request";
                return BuildResponse(resp, context_.cseq);
            }
            context_.client_rtp_port = std::stoi(client_port_pair.first);
            context_.client_rtcp_port = std::stoi(client_port_pair.second);
        } else if (is_multi) {
            context_.mode = rtp::TransportMode::RTP_OVER_MULTICAST;
        } else {
            LOG_RTSP_ERROR_AT("Invalid Transport header: {}", transport_header);
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }

        auto peer_endpoint = socket_.remote_endpoint();
        std::string peer_ip = peer_endpoint.address().to_string();

        context_.state = RtspState::READY;

        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Session"] = context_.session_id;

        if (context_.mode == rtp::TransportMode::RTP_OVER_TCP) {
            auto sender = std::make_shared<rtp::AsioTcpRtpSender>(socket_);
            rtp_transport_ = std::make_shared<rtp::AsioRtpTransport>(GetIOContext());
            rtp_transport_->SetSender(sender);
            rtp_transport_->SetRtpOverTcp(rtp::MediaChannelId::channel_0,
                static_cast<uint16_t>(context_.rtp_channel),
                static_cast<uint16_t>(context_.rtcp_channel));
            rtp_transport_->SetClockRate(rtp::MediaChannelId::channel_0, 90000);
            rtp_transport_->SetPayloadType(rtp::MediaChannelId::channel_0, 96);
            rtp_transport_->SetSsrc(rtp::MediaChannelId::channel_0, static_cast<uint32_t>(std::rand()));
            rtp_transport_->Start();
            LOG_RTSP_INFO_AT("RTP over TCP transport created for channel {}", context_.rtp_channel);

            resp.headers["Transport"] = "RTP/AVP/TCP;interleaved=" +
                std::to_string(context_.rtp_channel) + "-" +
                std::to_string(context_.rtcp_channel);
        }
        else if (context_.mode == rtp::TransportMode::RTP_OVER_UDP) {
            auto sender = std::make_shared<rtp::AsioUdpRtpSender>(GetIOContext(), peer_ip,
                context_.client_rtp_port, context_.client_rtcp_port);
            rtp_transport_ = std::make_shared<rtp::AsioRtpTransport>(GetIOContext());
            rtp_transport_->SetSender(sender);
            rtp_transport_->SetRtpOverUdp(rtp::MediaChannelId::channel_0,
                context_.client_rtp_port,
                context_.client_rtcp_port);
            rtp_transport_->SetClockRate(rtp::MediaChannelId::channel_0, 90000);
            rtp_transport_->SetPayloadType(rtp::MediaChannelId::channel_0, 96);
            rtp_transport_->SetSsrc(rtp::MediaChannelId::channel_0, static_cast<uint32_t>(std::rand()));
            rtp_transport_->Start();
            LOG_RTSP_INFO_AT("RTP over UDP transport created");

            resp.headers["Transport"] = "RTP/AVP;unicast;client_port=" +
                std::to_string(context_.client_rtp_port) + "-" +
                std::to_string(context_.client_rtcp_port) + ";" +
                "server_port=" + std::to_string(sender->GetLocalPort()) + "-" +
                std::to_string(sender->GetLocalPort() + 1);
        }
        else if (context_.mode == rtp::TransportMode::RTP_OVER_MULTICAST) {
            // TODO
        }

        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandlePlay(const std::map<std::string, std::string>& headers) {
        if (context_.state != RtspState::READY) {
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }

        context_.state = RtspState::PLAYING;

        std::string rtp_info = "url=" + context_.url + ";";
        if (context_.rtp_timestamp == 0) {
            auto now = std::chrono::system_clock::now();
            auto epoch = now.time_since_epoch();
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
            context_.rtp_timestamp = static_cast<uint32_t>((millis % 864000000) * 90 / 1000);
            context_.rtp_seq_num = static_cast<uint16_t>(std::rand() % 65536);
        }

        rtp_info += "seq=" + std::to_string(context_.rtp_seq_num) + ";";
        rtp_info += "rtptime=" + std::to_string(context_.rtp_timestamp);

        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Session"] = context_.session_id;
        resp.headers["RTP-Info"] = rtp_info;

        if (rtp_transport_) {
            if (!h264_source_) {
                h264_source_ = std::make_shared<rtp::H264Source>(25);
                h264_source_->SetSendFrameCallback(
                    [this](rtp::MediaChannelId ch, rtp::RtpPacket pkt) -> bool {
                        return packet_queue_.Push(ch, std::move(pkt));
                    });
            }

            if (nal_source_ && nal_source_->HasMoreData()) {
                nal_source_->Reset();
            }

            uint32_t fps = 25;
            if (nal_source_) fps = nal_source_->GetFrameRate();
            frame_index_ = 0;

            rtp_transport_->SetFrameProvider(rtp::MediaChannelId::channel_0,
                [this]() -> std::shared_ptr<rtp::RtpPacket> {
                    return ProduceNextPacket();
                });
            rtp_transport_->SetFrameRate(rtp::MediaChannelId::channel_0, fps);

            SendSpsPps();
            rtp_transport_->SetPlaying(rtp::MediaChannelId::channel_0, true);
        }

        return BuildResponse(resp, context_.cseq);
    }

    void RtspSession::SendSpsPps() {
        auto SendNal = [this](const uint8_t* nal, size_t size) {
            auto pkt = std::make_shared<rtp::RtpPacket>();
            uint8_t* data = pkt->data.get();

            memcpy(data + rtp::RTP_HEADER_SIZE, nal, size);

            pkt->size = rtp::RTP_HEADER_SIZE + size;
            pkt->timestamp = context_.rtp_timestamp;
            pkt->type = 0;
            pkt->last = 1;

            rtp_transport_->SendRtpPacket(rtp::MediaChannelId::channel_0, *pkt);
        };

        if (!real_sps_.empty()) {
            SendNal(real_sps_.data(), real_sps_.size());
            LOG_RTSP_INFO_AT("Sent real SPS ({} bytes)", real_sps_.size());
        }
        if (!real_pps_.empty()) {
            SendNal(real_pps_.data(), real_pps_.size());
            LOG_RTSP_INFO_AT("Sent real PPS ({} bytes)", real_pps_.size());
        }
    }

    std::string RtspSession::HandleTeardown(const std::map<std::string, std::string>& headers) {
        context_.state = RtspState::TEARDOWN;

        if (rtp_transport_) {
            rtp_transport_->SetPlaying(rtp::MediaChannelId::channel_0, false);
        }

        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Session"] = context_.session_id;
        return BuildResponse(resp, context_.cseq);
    }

    std::string RtspSession::HandlePause(const std::map<std::string, std::string>& headers) {
        if (context_.state != RtspState::PLAYING) {
            RtspResponse resp;
            resp.status_code = static_cast<int>(RtspStatus::BadRequest);
            resp.status_reason = "Bad Request";
            return BuildResponse(resp, context_.cseq);
        }

        context_.state = RtspState::PAUSED;

        if (rtp_transport_) {
            rtp_transport_->SetPlaying(rtp::MediaChannelId::channel_0, false);
        }

        RtspResponse resp;
        resp.status_code = static_cast<int>(RtspStatus::OK);
        resp.status_reason = "OK";
        resp.headers["Session"] = context_.session_id;
        return BuildResponse(resp, context_.cseq);
    }

    std::pair<std::string, std::string> RtspSession::parseRangeNum(const std::string& str) {
        auto separator = str.find("-");
        if (separator == std::string::npos) {
            return std::pair{ "", "" };
        }

        auto start_part = str.substr(0, separator);

        auto end_part = str.substr(separator + 1);
        const char* delimiters = ";\r\n\t";
        const auto last_valid = end_part.find_last_not_of(delimiters);
        if (last_valid != std::string::npos) {
            end_part = end_part.substr(0, last_valid + 1);
        }
        else {
            end_part.clear();
        }
        return std::pair{ std::move(start_part), std::move(end_part) };
    }

    const std::string RtspSession::buildResponse(int status_code, const std::string& reason) {
        RtspResponse resp;
        resp.status_code = status_code;
        resp.status_reason = reason;
        resp.headers["Session"] = context_.session_id;
        return BuildResponse(resp, context_.cseq);
    }

    std::shared_ptr<rtp::RtpPacket> RtspSession::ProduceNextPacket() {
        if (packet_queue_.HasData()) {
            return packet_queue_.Pop();
        }

        if (nal_source_) {
            FeedNextNALFrame();
        }

        if (packet_queue_.HasData()) {
            return packet_queue_.Pop();
        }

        return nullptr;
    }

    void RtspSession::FeedNextNALFrame() {
        if (!nal_source_ || !h264_source_) return;

        auto frame = nal_source_->ReadNextFrame();
        if (!frame) {
            LOG_RTSP_INFO_AT("Video file end, looping from start");
            frame_index_ = 0;
            nal_source_->Reset();
            frame = nal_source_->ReadNextFrame();
            if (!frame) return;
        }

        uint32_t ts = context_.rtp_timestamp + frame_index_ * (90000 / nal_source_->GetFrameRate());

        for (auto& nal : *frame) {
            rtp::NALFrame nal_frame(static_cast<uint32_t>(nal.size));
            std::memcpy(nal_frame.buffer.get(), nal.data.get(), nal.size);
            nal_frame.type = nal.is_keyframe ? rtp::VIDEO_FRAME_I : rtp::VIDEO_FRAME_P;
            nal_frame.timestamp = ts;

            h264_source_->HandleFrame(rtp::MediaChannelId::channel_0, nal_frame);
        }

        frame_index_++;
    }
}
