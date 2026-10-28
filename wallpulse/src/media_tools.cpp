#include "media_tools.h"

#include <cguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace wp {

namespace {

#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t bfType = 0x4D42;  // 'BM'
    uint32_t bfSize = 0;
    uint16_t bfReserved1 = 0;
    uint16_t bfReserved2 = 0;
    uint32_t bfOffBits = 54;
};

struct BmpInfoHeader {
    uint32_t biSize = 40;
    int32_t  biWidth = 0;
    int32_t  biHeight = 0;     // Negative = top-down!
    uint16_t biPlanes = 1;
    uint16_t biBitCount = 32;
    uint32_t biCompression = 0;  // BI_RGB
    uint32_t biSizeImage = 0;
    int32_t  biXPelsPerMeter = 2835;
    int32_t  biYPelsPerMeter = 2835;
    uint32_t biClrUsed = 0;
    uint32_t biClrImportant = 0;
};
#pragma pack(pop)

inline uint8_t ClampByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

void ResizeCoverBGRA(
    const uint8_t* src, uint32_t srcW, uint32_t srcH, int32_t srcStride,
    uint32_t dstW, uint32_t dstH, std::vector<uint8_t>& dst) {
    dst.resize(static_cast<size_t>(dstW) * dstH * 4);
    if (!src || srcW == 0 || srcH == 0) return;

    const float srcAspect = static_cast<float>(srcW) / static_cast<float>(srcH);
    const float dstAspect = static_cast<float>(dstW) / static_cast<float>(dstH);

    float cropW = static_cast<float>(srcW);
    float cropH = static_cast<float>(srcH);
    float cropX = 0.0f;
    float cropY = 0.0f;

    if (srcAspect > dstAspect) {
        cropW = static_cast<float>(srcH) * dstAspect;
        cropX = (static_cast<float>(srcW) - cropW) * 0.5f;
    } else {
        cropH = static_cast<float>(srcW) / dstAspect;
        cropY = (static_cast<float>(srcH) - cropH) * 0.5f;
    }

    const uint8_t* baseRow = (srcStride >= 0)
        ? src
        : (src + static_cast<ptrdiff_t>(srcH - 1) * (-srcStride));

    for (uint32_t y = 0; y < dstH; ++y) {
        float sy = cropY + (static_cast<float>(y) + 0.5f) / static_cast<float>(dstH) * cropH;
        uint32_t iy = std::min(srcH - 1, static_cast<uint32_t>(std::max(0.0f, sy)));
        const uint8_t* rowPtr = baseRow + static_cast<ptrdiff_t>(iy) * srcStride;
        uint8_t* outRow = dst.data() + static_cast<size_t>(y) * dstW * 4;

        for (uint32_t x = 0; x < dstW; ++x) {
            float sx = cropX + (static_cast<float>(x) + 0.5f) / static_cast<float>(dstW) * cropW;
            uint32_t ix = std::min(srcW - 1, static_cast<uint32_t>(std::max(0.0f, sx)));
            const uint8_t* px = rowPtr + static_cast<size_t>(ix) * 4;
            outRow[x * 4 + 0] = px[0];
            outRow[x * 4 + 1] = px[1];
            outRow[x * 4 + 2] = px[2];
            outRow[x * 4 + 3] = 255;
        }
    }
}

void RenderSampleFrame(int themeId, uint32_t w, uint32_t h, float phase01, std::vector<uint8_t>& bgra) {
    constexpr float kTwoPi = 6.28318530718f;
    const float t = phase01 * kTwoPi;
    bgra.resize(static_cast<size_t>(w) * h * 4);

    for (uint32_t y = 0; y < h; ++y) {
        const float uvY = static_cast<float>(y) / static_cast<float>(h - 1);
        uint8_t* row = bgra.data() + static_cast<size_t>(y) * w * 4;

        for (uint32_t x = 0; x < w; ++x) {
            const float uvX = static_cast<float>(x) / static_cast<float>(w - 1);
            const float nx = (uvX - 0.5f) * 2.0f * (static_cast<float>(w) / static_cast<float>(h));
            const float ny = (uvY - 0.5f) * 2.0f;

            float r = 10.0f, g = 14.0f, b = 26.0f;

            if (themeId == 0) {
                // Theme 0: Neon Synthwave Sun & Endless Perspective Grid
                if (uvY < 0.56f) {
                    float skyT = uvY / 0.56f;
                    r = 12.0f + skyT * 68.0f;
                    g = 10.0f + skyT * 24.0f;
                    b = 34.0f + skyT * 88.0f;

                    // Retro sun with animated scanlines
                    float sunDx = nx;
                    float sunDy = ny + 0.16f - 0.02f * std::sin(t);
                    float sunDist = std::sqrt(sunDx * sunDx + sunDy * sunDy);
                    if (sunDist < 0.34f) {
                        float scan = std::sin((ny + 0.45f) * 55.0f - t * 2.0f);
                        if (ny < -0.10f || scan > -0.25f) {
                            float sunGrad = std::clamp((sunDy + 0.34f) / 0.68f, 0.0f, 1.0f);
                            r = 255.0f;
                            g = 215.0f - sunGrad * 155.0f;
                            b = 65.0f + sunGrad * 145.0f;
                        }
                    } else {
                        float halo = std::exp(-(sunDist - 0.34f) * 5.2f);
                        r += halo * 165.0f;
                        g += halo * 45.0f;
                        b += halo * 135.0f;
                    }
                } else {
                    // Perspective neon floor grid
                    float dz = (uvY - 0.555f) + 0.015f;
                    float worldZ = 0.28f / dz + phase01 * 2.0f;
                    float worldX = nx / dz * 0.35f;

                    float gx = std::fabs(worldX - std::round(worldX));
                    float gz = std::fabs(worldZ - std::round(worldZ));
                    float lineX = std::exp(-gx * 28.0f);
                    float lineZ = std::exp(-gz * 22.0f);
                    float grid = std::max(lineX, lineZ) * std::min(1.0f, dz * 3.5f);
                    float horizonGlow = std::exp(-dz * 11.0f);

                    r = 16.0f + grid * 55.0f + horizonGlow * 210.0f;
                    g = 12.0f + grid * 225.0f + horizonGlow * 60.0f;
                    b = 38.0f + grid * 255.0f + horizonGlow * 195.0f;
                }
            } else if (themeId == 1) {
                // Theme 1: Northern Aurora Borealis over Mountain Silhouette
                float skyT = uvY;
                r = 6.0f + skyT * 14.0f;
                g = 14.0f + skyT * 28.0f;
                b = 28.0f + skyT * 52.0f;

                float wave1 = std::sin(nx * 2.2f + t) * 0.16f + std::cos(nx * 4.1f - t * 2.0f) * 0.06f;
                float wave2 = std::cos(nx * 1.7f - t) * 0.18f + std::sin(nx * 3.3f + t) * 0.07f;

                float band1 = std::exp(-std::pow((ny + 0.28f - wave1) * 4.8f, 2.0f));
                float band2 = std::exp(-std::pow((ny + 0.10f - wave2) * 4.2f, 2.0f));

                r += band1 * 20.0f + band2 * 125.0f;
                g += band1 * 215.0f + band2 * 85.0f;
                b += band1 * 155.0f + band2 * 220.0f;

                // Mountain ridge silhouette
                float ridge = 0.34f + 0.12f * std::sin(nx * 1.9f + 0.5f)
                            + 0.06f * std::cos(nx * 4.7f) + 0.03f * std::sin(nx * 9.3f);
                if (ny > ridge) {
                    r = 7.0f;
                    g = 11.0f;
                    b = 19.0f;
                }
            } else {
                // Theme 2: Deep Space Cyber Nebula & Orbital Pulse Rings
                float dist = std::sqrt(nx * nx + ny * ny);
                float angle = std::atan2(ny, nx);

                float swirl1 = std::sin(dist * 5.0f - t * 2.0f + angle * 2.0f);
                float swirl2 = std::cos(dist * 3.5f + t + angle * 3.0f);
                float ring1 = std::exp(-std::pow((dist - 0.42f - 0.04f * std::sin(t)) * 18.0f, 2.0f));
                float ring2 = std::exp(-std::pow((dist - 0.68f + 0.03f * std::cos(t)) * 16.0f, 2.0f));
                float core = std::exp(-dist * 3.4f);

                r = 10.0f + core * 65.0f + std::max(0.0f, swirl1) * 42.0f + ring2 * 165.0f;
                g = 15.0f + core * 145.0f + std::max(0.0f, swirl2) * 58.0f + ring1 * 215.0f;
                b = 32.0f + core * 235.0f + (ring1 + ring2) * 240.0f + std::max(0.0f, swirl1) * 75.0f;
            }

            row[x * 4 + 0] = ClampByte(b);
            row[x * 4 + 1] = ClampByte(g);
            row[x * 4 + 2] = ClampByte(r);
            row[x * 4 + 3] = 255;
        }
    }
}

bool EncodeProceduralMp4(const std::wstring& outPath, int themeId, uint32_t width, uint32_t height, uint32_t fps, uint32_t totalFrames) {
    if (FileExists(outPath) && GetFileSizeBytes(outPath) > 4096) {
        return true;
    }

    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(attrs.GetAddressOf(), 2);
    if (attrs) {
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }

    std::wstring tmpPath = outPath + L".part.mp4";
    DeleteFileW(tmpPath.c_str());

    ComPtr<IMFSinkWriter> writer;
    HRESULT hr = MFCreateSinkWriterFromURL(tmpPath.c_str(), nullptr, attrs.Get(), writer.GetAddressOf());
    if (FAILED(hr) || !writer) {
        LogHRESULT(L"MFCreateSinkWriterFromURL failed for sample MP4", hr);
        return false;
    }

    // Output stream: H.264 MP4
    ComPtr<IMFMediaType> outType;
    MFCreateMediaType(outType.GetAddressOf());
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    outType->SetUINT32(MF_MT_AVG_BITRATE, 4500000);
    outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(outType.Get(), MF_MT_FRAME_RATE, fps, 1);
    MFSetAttributeRatio(outType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    DWORD streamIndex = 0;
    hr = writer->AddStream(outType.Get(), &streamIndex);
    if (FAILED(hr)) {
        LogHRESULT(L"SinkWriter AddStream(H264) failed", hr);
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    // Input stream: RGB32 (BGRA). Note: In Media Foundation, RGB32 default orientation is bottom-up
    // unless negative/positive stride is matched. Passing bottom-up rows guarantees universal MFT orientation!
    ComPtr<IMFMediaType> inType;
    MFCreateMediaType(inType.GetAddressOf());
    inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(inType.Get(), MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(inType.Get(), MF_MT_FRAME_RATE, fps, 1);
    MFSetAttributeRatio(inType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    hr = writer->SetInputMediaType(streamIndex, inType.Get(), nullptr);
    if (FAILED(hr)) {
        LogHRESULT(L"SinkWriter SetInputMediaType(RGB32) failed", hr);
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    hr = writer->BeginWriting();
    if (FAILED(hr)) {
        LogHRESULT(L"SinkWriter BeginWriting failed", hr);
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    const LONGLONG frameDuration100ns = 10000000LL / fps;
    const DWORD frameBytes = width * height * 4;
    std::vector<uint8_t> topDownFrame;

    for (uint32_t f = 0; f < totalFrames; ++f) {
        float phase = static_cast<float>(f) / static_cast<float>(totalFrames);
        RenderSampleFrame(themeId, width, height, phase, topDownFrame);

        ComPtr<IMFMediaBuffer> mediaBuf;
        hr = MFCreateMemoryBuffer(frameBytes, mediaBuf.GetAddressOf());
        if (FAILED(hr)) break;

        BYTE* dstPtr = nullptr;
        hr = mediaBuf->Lock(&dstPtr, nullptr, nullptr);
        if (FAILED(hr)) break;

        // Copy bottom-up for standard MF RGB32 input so encoded H.264 is right-side-up on all decoders
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t* srcRow = topDownFrame.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
            std::memcpy(dstPtr + static_cast<size_t>(y) * rowBytes, srcRow, rowBytes);
        }

        mediaBuf->Unlock();
        mediaBuf->SetCurrentLength(frameBytes);

        ComPtr<IMFSample> sample;
        MFCreateSample(sample.GetAddressOf());
        sample->AddBuffer(mediaBuf.Get());
        sample->SetSampleTime(static_cast<LONGLONG>(f) * frameDuration100ns);
        sample->SetSampleDuration(frameDuration100ns);

        hr = writer->WriteSample(streamIndex, sample.Get());
        if (FAILED(hr)) break;
    }

    HRESULT finHr = writer->Finalize();
    writer.Reset();

    if (FAILED(hr) || FAILED(finHr)) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    return MoveFileExW(tmpPath.c_str(), outPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

}  // namespace

bool SaveBmpBGRA(const std::wstring& bmpPath, uint32_t w, uint32_t h, const uint8_t* bgraData) {
    if (!bgraData || w == 0 || h == 0) return false;

    const uint32_t imgBytes = w * h * 4;
    BmpFileHeader fh{};
    fh.bfSize = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + imgBytes;
    fh.bfOffBits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

    BmpInfoHeader ih{};
    ih.biWidth = static_cast<int32_t>(w);
    ih.biHeight = -static_cast<int32_t>(h);  // Negative height = top-down BGRA
    ih.biSizeImage = imgBytes;

    HANDLE hFile = CreateFileW(
        bmpPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok1 = WriteFile(hFile, &fh, sizeof(fh), &written, nullptr);
    BOOL ok2 = WriteFile(hFile, &ih, sizeof(ih), &written, nullptr);
    BOOL ok3 = WriteFile(hFile, bgraData, imgBytes, &written, nullptr);
    CloseHandle(hFile);
    return ok1 && ok2 && ok3;
}

bool LoadBmpBGRA(const std::wstring& bmpPath, uint32_t& outW, uint32_t& outH, std::vector<uint8_t>& outBGRA) {
    HANDLE hFile = CreateFileW(
        bmpPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BmpFileHeader fh{};
    BmpInfoHeader ih{};
    DWORD rd = 0;
    if (!ReadFile(hFile, &fh, sizeof(fh), &rd, nullptr) || rd != sizeof(fh) || fh.bfType != 0x4D42) {
        CloseHandle(hFile);
        return false;
    }
    if (!ReadFile(hFile, &ih, sizeof(ih), &rd, nullptr) || rd != sizeof(ih) || ih.biBitCount != 32 || ih.biWidth <= 0) {
        CloseHandle(hFile);
        return false;
    }

    const bool topDown = (ih.biHeight < 0);
    const uint32_t w = static_cast<uint32_t>(ih.biWidth);
    const uint32_t h = static_cast<uint32_t>(topDown ? -ih.biHeight : ih.biHeight);
    if (w == 0 || h == 0 || w > 4096 || h > 4096) {
        CloseHandle(hFile);
        return false;
    }

    SetFilePointer(hFile, static_cast<LONG>(fh.bfOffBits), nullptr, FILE_BEGIN);
    const size_t totalBytes = static_cast<size_t>(w) * h * 4;
    std::vector<uint8_t> raw(totalBytes);
    if (!ReadFile(hFile, raw.data(), static_cast<DWORD>(totalBytes), &rd, nullptr) || rd != totalBytes) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    outW = w;
    outH = h;
    if (topDown) {
        outBGRA = std::move(raw);
    } else {
        outBGRA.resize(totalBytes);
        const size_t rowBytes = static_cast<size_t>(w) * 4;
        for (uint32_t y = 0; y < h; ++y) {
            std::memcpy(outBGRA.data() + y * rowBytes, raw.data() + (h - 1 - y) * rowBytes, rowBytes);
        }
    }
    return true;
}

VideoProbeInfo ProbeVideoAndSaveThumbnail(const std::wstring& videoPath) {
    VideoProbeInfo info{};
    if (!FileExists(videoPath)) return info;

    wchar_t hashName[64];
    swprintf_s(hashName, L"%016llx.bmp", static_cast<unsigned long long>(HashStringFNV1a(videoPath)));
    std::wstring thumbPath = GetThumbsDir() + L"\\" + hashName;

    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(attrs.GetAddressOf(), 2);
    if (attrs) {
        attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }

    ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(videoPath.c_str(), attrs.Get(), reader.GetAddressOf());
    if (FAILED(hr) || !reader) {
        LogHRESULT(L"MFCreateSourceReaderFromURL failed for " + videoPath, hr);
        return info;
    }

    // Read duration
    PROPVARIANT varDur;
    PropVariantInit(&varDur);
    if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &varDur))) {
        if (varDur.vt == VT_UI8) {
            info.durationSec = static_cast<double>(varDur.uhVal.QuadPart) / 10000000.0;
        } else if (varDur.vt == VT_I8) {
            info.durationSec = static_cast<double>(varDur.hVal.QuadPart) / 10000000.0;
        }
    }
    PropVariantClear(&varDur);

    // Request RGB32 output from first video stream
    ComPtr<IMFMediaType> rgbType;
    MFCreateMediaType(rgbType.GetAddressOf());
    rgbType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    rgbType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, rgbType.Get());
    if (FAILED(hr)) {
        LogHRESULT(L"SetCurrentMediaType(RGB32) failed", hr);
        return info;
    }

    ComPtr<IMFMediaType> actualType;
    if (SUCCEEDED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, actualType.GetAddressOf())) && actualType) {
        UINT32 w = 0, h = 0;
        if (SUCCEEDED(MFGetAttributeSize(actualType.Get(), MF_MT_FRAME_SIZE, &w, &h))) {
            info.width = w;
            info.height = h;
        }
    }

    int32_t defaultStride = static_cast<int32_t>(info.width * 4);
    if (actualType) {
        UINT32 strideU32 = 0;
        if (SUCCEEDED(actualType->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideU32))) {
            defaultStride = static_cast<int32_t>(strideU32);
        }
    }

    info.valid = (info.width > 0 && info.height > 0);
    if (!info.valid) return info;

    // Seek slightly into the clip (0.6s or 15% of duration) to skip fade-from-black intros
    if (info.durationSec > 0.5) {
        double seekSec = std::min(0.8, info.durationSec * 0.15);
        PROPVARIANT varPos;
        PropVariantInit(&varPos);
        varPos.vt = VT_I8;
        varPos.hVal.QuadPart = static_cast<LONGLONG>(seekSec * 10000000.0);
        reader->SetCurrentPosition(GUID_NULL, varPos);
        PropVariantClear(&varPos);
    }

    auto tryReadSample = [&](ComPtr<IMFSample>& outSample) -> bool {
        for (int attempt = 0; attempt < 16; ++attempt) {
            DWORD streamFlags = 0;
            ComPtr<IMFSample> sample;
            HRESULT r = reader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &streamFlags, nullptr, sample.GetAddressOf());
            if (FAILED(r) || (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                return false;
            }
            if (sample) {
                outSample = sample;
                return true;
            }
        }
        return false;
    };

    ComPtr<IMFSample> sample;
    if (!tryReadSample(sample)) {
        // Fallback to t = 0
        PROPVARIANT varZero;
        PropVariantInit(&varZero);
        varZero.vt = VT_I8;
        varZero.hVal.QuadPart = 0;
        reader->SetCurrentPosition(GUID_NULL, varZero);
        PropVariantClear(&varZero);
        tryReadSample(sample);
    }

    if (sample) {
        ComPtr<IMFMediaBuffer> buf;
        if (SUCCEEDED(sample->ConvertToContiguousBuffer(buf.GetAddressOf())) && buf) {
            ComPtr<IMF2DBuffer> buf2d;
            std::vector<uint8_t> thumbBGRA;

            if (SUCCEEDED(buf.As(&buf2d)) && buf2d) {
                BYTE* scanline0 = nullptr;
                LONG pitch = 0;
                if (SUCCEEDED(buf2d->Lock2D(&scanline0, &pitch))) {
                    ResizeCoverBGRA(scanline0, info.width, info.height, pitch, 320, 180, thumbBGRA);
                    buf2d->Unlock2D();
                }
            } else {
                BYTE* rawPtr = nullptr;
                DWORD curLen = 0;
                if (SUCCEEDED(buf->Lock(&rawPtr, nullptr, &curLen))) {
                    if (curLen >= info.width * info.height * 4) {
                        const uint8_t* startPtr = (defaultStride >= 0)
                            ? rawPtr
                            : (rawPtr + static_cast<size_t>(info.height - 1) * static_cast<size_t>(-defaultStride));
                        ResizeCoverBGRA(startPtr, info.width, info.height, defaultStride, 320, 180, thumbBGRA);
                    }
                    buf->Unlock();
                }
            }

            if (!thumbBGRA.empty() && SaveBmpBGRA(thumbPath, 320, 180, thumbBGRA.data())) {
                info.thumbPath = thumbPath;
            }
        }
    }

    return info;
}

std::vector<WallpaperItem> EnsureStarterSampleMp4s() {
    struct StarterDef {
        const wchar_t* filename;
        const wchar_t* title;
        int themeId;
    };

    const StarterDef defs[] = {
        {L"Neon_Synthwave_Grid.mp4", L"Неоновый Синтвейв (MP4)", 0},
        {L"Northern_Aurora_Sky.mp4", L"Северное Сияние (MP4)", 1},
        {L"Cosmic_Nebula_Pulse.mp4", L"Космическая Туманность (MP4)", 2},
    };

    std::wstring samplesDir = GetSamplesDir();
    std::vector<WallpaperItem> out;

    for (const auto& d : defs) {
        std::wstring fullPath = samplesDir + L"\\" + d.filename;
        if (EncodeProceduralMp4(fullPath, d.themeId, 640, 360, 30, 90)) {
            VideoProbeInfo probe = ProbeVideoAndSaveThumbnail(fullPath);
            WallpaperItem item;
            item.path = fullPath;
            item.title = d.title;
            item.thumbPath = probe.thumbPath;
            item.width = probe.width ? probe.width : 640;
            item.height = probe.height ? probe.height : 360;
            item.durationSec = probe.durationSec > 0.0 ? probe.durationSec : 3.0;
            item.fileSize = GetFileSizeBytes(fullPath);
            item.isBuiltIn = true;
            out.push_back(std::move(item));
        }
    }

    return out;
}

}  // namespace wp
