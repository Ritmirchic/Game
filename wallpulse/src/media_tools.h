#pragma once

#include "common.h"
#include "config.h"

namespace wp {

struct VideoProbeInfo {
    bool valid = false;
    uint32_t width = 0;
    uint32_t height = 0;
    double durationSec = 0.0;
    std::wstring thumbPath;
};

// Probe video dimensions + duration and extract a 320x180 BGRA BMP thumbnail
VideoProbeInfo ProbeVideoAndSaveThumbnail(const std::wstring& videoPath);

// Load a saved 32-bit BGRA BMP thumbnail into memory (returns true on success)
bool LoadBmpBGRA(const std::wstring& bmpPath, uint32_t& outW, uint32_t& outH, std::vector<uint8_t>& outBGRA);
bool SaveBmpBGRA(const std::wstring& bmpPath, uint32_t w, uint32_t h, const uint8_t* bgraData);

// Generate 3 starter real H.264 .mp4 live wallpapers on first run so the user can test immediately
std::vector<WallpaperItem> EnsureStarterSampleMp4s();

}  // namespace wp
