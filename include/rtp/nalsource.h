#pragma once
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <cstdint>
#include "rtp/media.h"
namespace rtp {
/// @brief NAL单元封装
struct NALUnit { 
    std::shared_ptr<uint8_t> data;  ///< NAL单元数据
    size_t size;                    ///< NAL单元数据大小
    uint8_t type;                   ///< NAL单元类型
    uint64_t pts;                   ///< NAL单元时间戳
    uint64_t dts;                   ///< NAL单元解码时间戳
    bool is_keyframe;               ///< 是否为关键帧
    NALUnit() : data(nullptr), size(0), type(0), pts(0), dts(0), is_keyframe(false) {}
    explicit NALUnit(uint32_t sz)
     : data(new uint8_t[sz], std::default_delete<uint8_t[]>())
     , size(sz), type(0), pts(0), dts(0), is_keyframe(false) {} 
};

class NALSource {
public:
    using Ptr = std::shared_ptr<NALSource>;
    using NALUnitList = std::vector<NALUnit>;
    using FrameNAL = std::optional<NALUnitList>;   ///< 一帧的所有NAL单元

    virtual ~NALSource() = default;

    // 禁止拷贝和移动
    NALSource(const NALSource&) = delete;
    NALSource& operator=(const NALSource&) = delete;
    NALSource(NALSource&&) = delete;
    NALSource& operator=(NALSource&&) = delete;

    /// @brief 打开媒体源
    /// @param source 媒体源路径（文件路径、URL 等）
    /// @return 是否成功
    virtual bool Open(const std::string& source) = 0;

    /// @brief 关闭媒体源
    virtual void Close() = 0;

    /// @brief 读取下一帧的所有 NAL 单元
    /// @return FrameNAL，如果到达文件末尾或出错则返回 std::nullopt
    virtual FrameNAL ReadNextFrame() = 0;

    /// @brief 尝试读取下一帧（非阻塞版本）
    /// @param frame_nals 输出参数，读取到的 NAL 单元列表
    /// @return 是否成功读取
    virtual bool TryReadNextFrame(FrameNAL& frame_nals) = 0;

    /// @brief 是否还有更多数据
    virtual bool HasMoreData() const = 0;

    /// @brief 获取媒体类型（H.264/H.265 等）
    virtual MediaType GetMediaType() const = 0;

    /// @brief 获取编码格式名称（如 "h264", "hevc"）
    virtual std::string GetCodecName() const = 0;

    /// @brief 获取帧率
    virtual uint32_t GetFrameRate() const = 0;

    /// @brief 获取视频宽度
    virtual int GetWidth() const = 0;

    /// @brief 获取视频高度
    virtual int GetHeight() const = 0;

    /// @brief 获取时长（毫秒）
    virtual int64_t GetDuration() const = 0;

    /// @brief 跳转到指定时间（毫秒）
    /// @param timestamp_ms 时间戳（毫秒）
    /// @return 是否成功
    virtual bool Seek(int64_t timestamp_ms) = 0;

    /// @brief 重置到文件开头
    virtual void Reset() = 0;

protected:
    NALSource() = default;

    /// @brief 判断 NAL 单元类型（H.264）
    /// @param nal_data NAL 数据
    /// @return NAL 类型 (0-31)
    static uint8_t GetH264NALType(const std::vector<uint8_t>& nal_data);

    /// @brief 判断 NAL 单元类型（H.265）
    /// @param nal_data NAL 数据
    /// @return NAL 类型 (0-63)
    static uint8_t GetH265NALType(const std::vector<uint8_t>& nal_data);

    /// @brief 检查是否为 H.264 IDR 帧
    /// @param nal_type NAL 类型
    /// @return 是否为 IDR 帧
    static bool IsH264IDR(uint8_t nal_type);

    /// @brief 检查是否为 H.265 IDR 帧
    /// @param nal_type NAL 类型
    /// @return 是否为 IDR 帧
    static bool IsH265IDR(uint8_t nal_type);
};


/// @brief NALSource 工厂类
class NALSourceFactory {
public:
    /// @brief 根据文件扩展名创建合适的 NALSource
    static NALSource::Ptr CreateFromFile(const std::string& filepath);
    
    // /// @brief 根据 URL 创建网络流 NALSource
    // static NALSource::Ptr CreateFromURL(const std::string& url);
    
    // /// @brief 通用创建方法（自动识别）
    // static NALSource::Ptr Create(const std::string& source);
};


}