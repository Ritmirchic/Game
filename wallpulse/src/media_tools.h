#pragma once

#include "common.h"
#include "config.h"

#include <wincodec.h>

namespace wp {

// Low-memory streaming GIF89a compositor (~2-16 MB RAM regardless of frame count!)
class StreamingGifDecoder {
public:
    StreamingGifDecoder();
    ~StreamingGifDecoder();

    StreamingGifDecoder(const StreamingGifDecoder&) = delete;
    StreamingGifDecoder& operator=(const StreamingGifDecoder&) = delete;
    StreamingGifDecoder(StreamingGifDecoder&&) noexcept = default;
    StreamingGifDecoder& operator=(StreamingGifDecoder&&) noexcept = default;

    bool Open(const std::wstring& gifPath);
    void Close();

    // Advances and composites the next frame in the loop
    bool StepNextFrame();
    bool ResetToFirstFrame();

    bool IsOpen() const { return decoder_ != nullptr && frameCount_ > 0; }
    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }
    uint32_t FrameCount() const { return frameCount_; }
    uint32_t CurrentFrameIndex() const { return currentFrame_; }
    uint32_t CurrentDelayMs() const { return currentDelayMs_; }
    double TotalDurationSec() const { return totalDurationSec_; }
    const uint8_t* CanvasBGRA() const { return canvas_.data(); }
    size_t CanvasByteSize() const { return canvas_.size(); }

private:
    bool CompositeFrame(uint32_t idx);
    void ApplyPendingDisposal();

private:
    ComPtr<IWICImagingFactory> wic_;
    ComPtr<IWICBitmapDecoder> decoder_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t frameCount_ = 0;
    uint32_t currentFrame_ = 0;
    uint32_t currentDelayMs_ = 60;
    double totalDurationSec_ = 0.0;

    uint16_t pendingDisposal_ = 0;
    uint16_t prevLeft_ = 0, prevTop_ = 0, prevW_ = 0, prevH_ = 0;
    std::vector<uint8_t> canvas_;
    std::vector<uint8_t> savedBefore_;
    std::vector<uint8_t> patchScratch_;
};

struct GifFrame {
    std::vector<uint8_t> bgra;
    uint32_t delayMs = 60;
};

struct AnimatedGifData {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<GifFrame> frames;
};

struct VideoProbeInfo {
    bool valid = false;
    WallpaperKind kind = WallpaperKind::Video;
    uint32_t width = 0;
    uint32_t height = 0;
    double durationSec = 0.0;
    uint32_t dominantColorRGB = 0x0EA5E9;
    std::wstring thumbPath;
};

// Probe any supported wallpaper (.mp4, .gif, shader://, .hlsl, .html), save 320x180 thumb & dominant accent
VideoProbeInfo ProbeWallpaperAndSaveThumbnail(const std::wstring& path);
VideoProbeInfo ProbeImagePreviewAndSaveThumbnail(const std::wstring& imagePath);

// Compatibility batch GIF helper (built atop StreamingGifDecoder)
bool DecodeAnimatedGif(const std::wstring& gifPath, AnimatedGifData& outGif, uint32_t maxFrames = 64);

// Procedural H.264 MP4 generator (used for built-in samples & 9:16 vertical self-test)
bool GenerateProceduralMp4File(const std::wstring& outPath, int themeId, uint32_t width, uint32_t height, uint32_t fps, uint32_t totalFrames);

// Extract vibrant UI accent color (0xRRGGBB) from BGRA image
uint32_t ComputeVibrantAccentRGB(const uint8_t* bgra, uint32_t w, uint32_t h);

bool LoadBmpBGRA(const std::wstring& bmpPath, uint32_t& outW, uint32_t& outH, std::vector<uint8_t>& outBGRA);
bool SaveBmpBGRA(const std::wstring& bmpPath, uint32_t w, uint32_t h, const uint8_t* bgraData);

// Create starter MP4s + Animated GIF + Built-in 60-FPS GPU Shaders + Interactive Audio/Mouse HTML5 scene
std::vector<WallpaperItem> EnsureStarterLibraryItems();

}  // namespace wp
