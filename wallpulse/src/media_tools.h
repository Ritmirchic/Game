#pragma once

#include "common.h"
#include "config.h"

namespace wp {

struct GifFrame {
    std::vector<uint8_t> bgra;  // Full canvas width * height * 4
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

// Decode all composited frames of an animated .gif via Windows Imaging Component (WIC)
bool DecodeAnimatedGif(const std::wstring& gifPath, AnimatedGifData& outGif);

// Extract vibrant UI accent color (0xRRGGBB) from BGRA image (#8)
uint32_t ComputeVibrantAccentRGB(const uint8_t* bgra, uint32_t w, uint32_t h);

bool LoadBmpBGRA(const std::wstring& bmpPath, uint32_t& outW, uint32_t& outH, std::vector<uint8_t>& outBGRA);
bool SaveBmpBGRA(const std::wstring& bmpPath, uint32_t w, uint32_t h, const uint8_t* bgraData);

// Create starter MP4s + Animated GIF + Built-in 60-FPS GPU Shaders + Interactive HTML5 scene
std::vector<WallpaperItem> EnsureStarterLibraryItems();

}  // namespace wp
