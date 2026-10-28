#include "media_tools.h"

#include <cguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace wp {

namespace {

#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t bfType = 0x4D42;
    uint32_t bfSize = 0;
    uint16_t bfReserved1 = 0;
    uint16_t bfReserved2 = 0;
    uint32_t bfOffBits = 54;
};

struct BmpInfoHeader {
    uint32_t biSize = 40;
    int32_t  biWidth = 0;
    int32_t  biHeight = 0;
    uint16_t biPlanes = 1;
    uint16_t biBitCount = 32;
    uint32_t biCompression = 0;
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

void RgbToHsv(float r, float g, float b, float& hDeg, float& s, float& v) {
    float mx = std::max({r, g, b});
    float mn = std::min({r, g, b});
    float d = mx - mn;
    v = mx;
    s = (mx > 0.0001f) ? (d / mx) : 0.0f;
    if (d <= 0.0001f) {
        hDeg = 0.0f;
    } else if (mx == r) {
        hDeg = 60.0f * std::fmod((g - b) / d + 6.0f, 6.0f);
    } else if (mx == g) {
        hDeg = 60.0f * (((b - r) / d) + 2.0f);
    } else {
        hDeg = 60.0f * (((r - g) / d) + 4.0f);
    }
}

uint32_t HsvToRgbHex(float hDeg, float s, float v) {
    hDeg = std::fmod(std::fmod(hDeg, 360.0f) + 360.0f, 360.0f);
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(hDeg / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    if (hDeg < 60.0f)       { r = c; g = x; b = 0; }
    else if (hDeg < 120.0f) { r = x; g = c; b = 0; }
    else if (hDeg < 180.0f) { r = 0; g = c; b = x; }
    else if (hDeg < 240.0f) { r = 0; g = x; b = c; }
    else if (hDeg < 300.0f) { r = x; g = 0; b = c; }
    else                    { r = c; g = 0; b = x; }

    uint32_t ir = ClampByte((r + m) * 255.0f);
    uint32_t ig = ClampByte((g + m) * 255.0f);
    uint32_t ib = ClampByte((b + m) * 255.0f);
    return (ir << 16) | (ig << 8) | ib;
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
        const float uvY = static_cast<float>(y) / static_cast<float>(std::max<uint32_t>(1, h - 1));
        uint8_t* row = bgra.data() + static_cast<size_t>(y) * w * 4;

        for (uint32_t x = 0; x < w; ++x) {
            const float uvX = static_cast<float>(x) / static_cast<float>(std::max<uint32_t>(1, w - 1));
            const float nx = (uvX - 0.5f) * 2.0f * (static_cast<float>(w) / static_cast<float>(h));
            const float ny = (uvY - 0.5f) * 2.0f;

            float r = 10.0f, g = 14.0f, b = 26.0f;

            if (themeId == 0) {
                // Theme 0: Neon Synthwave Sun & Endless Grid
                if (uvY < 0.56f) {
                    float skyT = uvY / 0.56f;
                    r = 12.0f + skyT * 68.0f;
                    g = 10.0f + skyT * 24.0f;
                    b = 34.0f + skyT * 88.0f;

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
                    float dz = (uvY - 0.555f) + 0.015f;
                    float worldZ = 0.28f / dz + phase01 * 2.0f;
                    float worldX = nx / dz * 0.35f;

                    float gx = std::fabs(worldX - std::round(worldX));
                    float gz = std::fabs(worldZ - std::round(worldZ));
                    float grid = std::max(std::exp(-gx * 28.0f), std::exp(-gz * 22.0f)) * std::min(1.0f, dz * 3.5f);
                    float horizonGlow = std::exp(-dz * 11.0f);

                    r = 16.0f + grid * 55.0f + horizonGlow * 210.0f;
                    g = 12.0f + grid * 225.0f + horizonGlow * 60.0f;
                    b = 38.0f + grid * 255.0f + horizonGlow * 195.0f;
                }
            } else if (themeId == 1) {
                // Theme 1: Aurora Borealis
                r = 6.0f + uvY * 14.0f;
                g = 14.0f + uvY * 28.0f;
                b = 28.0f + uvY * 52.0f;

                float wave1 = std::sin(nx * 2.2f + t) * 0.16f + std::cos(nx * 4.1f - t * 2.0f) * 0.06f;
                float wave2 = std::cos(nx * 1.7f - t) * 0.18f + std::sin(nx * 3.3f + t) * 0.07f;
                float band1 = std::exp(-std::pow((ny + 0.28f - wave1) * 4.8f, 2.0f));
                float band2 = std::exp(-std::pow((ny + 0.10f - wave2) * 4.2f, 2.0f));

                r += band1 * 20.0f + band2 * 125.0f;
                g += band1 * 215.0f + band2 * 85.0f;
                b += band1 * 155.0f + band2 * 220.0f;

                float ridge = 0.34f + 0.12f * std::sin(nx * 1.9f + 0.5f) + 0.06f * std::cos(nx * 4.7f);
                if (ny > ridge) { r = 7.0f; g = 11.0f; b = 19.0f; }
            } else if (themeId == 2) {
                // Theme 2: Cyber Nebula
                float dist = std::sqrt(nx * nx + ny * ny);
                float angle = std::atan2(ny, nx);
                float swirl1 = std::sin(dist * 5.0f - t * 2.0f + angle * 2.0f);
                float ring1 = std::exp(-std::pow((dist - 0.42f - 0.04f * std::sin(t)) * 18.0f, 2.0f));
                float ring2 = std::exp(-std::pow((dist - 0.68f + 0.03f * std::cos(t)) * 16.0f, 2.0f));
                float core = std::exp(-dist * 3.4f);

                r = 10.0f + core * 65.0f + std::max(0.0f, swirl1) * 42.0f + ring2 * 165.0f;
                g = 15.0f + core * 145.0f + ring1 * 215.0f;
                b = 32.0f + core * 235.0f + (ring1 + ring2) * 240.0f;
            } else if (themeId == 3) {
                // Theme 3:Cyber Tunnel Shader preview
                float rDist = std::max(0.06f, std::sqrt(nx * nx + ny * ny));
                float ang = std::atan2(ny, nx);
                float z = 0.4f / rDist + phase01 * 4.0f;
                float grid = std::max(std::exp(-std::fabs(std::sin(z * 3.14159f)) * 9.0f),
                                      std::exp(-std::fabs(std::sin(ang * 4.0f)) * 9.0f));
                float fog = std::min(1.0f, rDist * 1.3f);
                r = 14.0f + grid * fog * 195.0f;
                g = 16.0f + grid * fog * 75.0f;
                b = 36.0f + grid * fog * 255.0f;
            } else if (themeId == 4) {
                // Theme 4: Sunset Ocean Shader preview
                if (uvY < 0.52f) {
                    float st = uvY / 0.52f;
                    r = 45.0f + st * 210.0f;
                    g = 28.0f + st * 115.0f;
                    b = 85.0f + st * 45.0f;
                    float sd = std::hypot(nx, ny + 0.08f);
                    if (sd < 0.24f) { r = 255.0f; g = 225.0f; b = 150.0f; }
                } else {
                    float wt = (uvY - 0.52f) / 0.48f;
                    float wave = std::sin(nx * 14.0f + wt * 26.0f - t * 2.0f) * 0.5f + 0.5f;
                    float spec = std::exp(-std::fabs(nx) * 4.5f) * wave;
                    r = 18.0f + spec * 240.0f;
                    g = 36.0f + wt * 45.0f + spec * 155.0f;
                    b = 76.0f + wt * 65.0f + spec * 85.0f;
                }
            } else {
                // Theme 5: Matrix / Constellation preview
                float colX = std::floor((uvX + 0.01f) * 28.0f);
                float drop = std::fmod(uvY * 2.0f - phase01 * 2.0f + std::sin(colX * 12.7f) * 0.5f + 4.0f, 1.0f);
                float glow = std::exp(-drop * 6.5f) * (0.4f + 0.6f * std::fabs(std::sin(colX * 3.1f)));
                r = 6.0f + glow * 25.0f;
                g = 18.0f + glow * 235.0f;
                b = 14.0f + glow * 130.0f;
            }

            row[x * 4 + 0] = ClampByte(b);
            row[x * 4 + 1] = ClampByte(g);
            row[x * 4 + 2] = ClampByte(r);
            row[x * 4 + 3] = 255;
        }
    }
}

bool EncodeProceduralMp4(const std::wstring& outPath, int themeId, uint32_t width, uint32_t height, uint32_t fps, uint32_t totalFrames) {
    if (FileExists(outPath) && GetFileSizeBytes(outPath) > 4096) return true;

    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(attrs.GetAddressOf(), 2);
    if (attrs) attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

    std::wstring tmpPath = outPath + L".part.mp4";
    DeleteFileW(tmpPath.c_str());

    ComPtr<IMFSinkWriter> writer;
    if (FAILED(MFCreateSinkWriterFromURL(tmpPath.c_str(), nullptr, attrs.Get(), writer.GetAddressOf())) || !writer) {
        return false;
    }

    ComPtr<IMFMediaType> outType;
    MFCreateMediaType(outType.GetAddressOf());
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    outType->SetUINT32(MF_MT_AVG_BITRATE, 4200000);
    outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(outType.Get(), MF_MT_FRAME_RATE, fps, 1);
    MFSetAttributeRatio(outType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    DWORD streamIndex = 0;
    if (FAILED(writer->AddStream(outType.Get(), &streamIndex))) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    ComPtr<IMFMediaType> inType;
    MFCreateMediaType(inType.GetAddressOf());
    inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(inType.Get(), MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(inType.Get(), MF_MT_FRAME_RATE, fps, 1);
    MFSetAttributeRatio(inType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    if (FAILED(writer->SetInputMediaType(streamIndex, inType.Get(), nullptr)) ||
        FAILED(writer->BeginWriting())) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    const LONGLONG frameDuration100ns = 10000000LL / fps;
    const DWORD frameBytes = width * height * 4;
    std::vector<uint8_t> topDownFrame;
    HRESULT hr = S_OK;

    for (uint32_t f = 0; f < totalFrames; ++f) {
        RenderSampleFrame(themeId, width, height, static_cast<float>(f) / static_cast<float>(totalFrames), topDownFrame);

        ComPtr<IMFMediaBuffer> mediaBuf;
        hr = MFCreateMemoryBuffer(frameBytes, mediaBuf.GetAddressOf());
        if (FAILED(hr)) break;

        BYTE* dstPtr = nullptr;
        hr = mediaBuf->Lock(&dstPtr, nullptr, nullptr);
        if (FAILED(hr)) break;

        const size_t rowBytes = static_cast<size_t>(width) * 4;
        for (uint32_t y = 0; y < height; ++y) {
            std::memcpy(
                dstPtr + static_cast<size_t>(y) * rowBytes,
                topDownFrame.data() + static_cast<size_t>(height - 1 - y) * rowBytes,
                rowBytes);
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

bool EncodeProceduralAnimatedGif(const std::wstring& outPath, uint32_t w, uint32_t h, uint32_t frameCount) {
    if (FileExists(outPath) && GetFileSizeBytes(outPath) > 1024) return true;

    ComPtr<IWICImagingFactory> wic;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                __uuidof(IWICImagingFactory), reinterpret_cast<void**>(wic.GetAddressOf()))) || !wic) {
        return false;
    }

    ComPtr<IWICStream> stream;
    if (FAILED(wic->CreateStream(stream.GetAddressOf())) || !stream) return false;
    if (FAILED(stream->InitializeFromFilename(outPath.c_str(), GENERIC_WRITE))) return false;

    ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(wic->CreateEncoder(GUID_ContainerFormatGif, nullptr, encoder.GetAddressOf())) || !encoder) return false;
    if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) return false;

    // Write NETSCAPE2.0 infinite loop application extension
    ComPtr<IWICMetadataQueryWriter> containerMeta;
    if (SUCCEEDED(encoder->GetMetadataQueryWriter(containerMeta.GetAddressOf())) && containerMeta) {
        PROPVARIANT pvApp;
        PropVariantInit(&pvApp);
        BYTE appBytes[11] = {'N','E','T','S','C','A','P','E','2','.','0'};
        pvApp.vt = VT_UI1 | VT_VECTOR;
        pvApp.caub.cElems = 11;
        pvApp.caub.pElems = appBytes;
        containerMeta->SetMetadataByName(L"/appext/Application", &pvApp);

        PROPVARIANT pvData;
        PropVariantInit(&pvData);
        BYTE loopBytes[4] = {3, 1, 0, 0};
        pvData.vt = VT_UI1 | VT_VECTOR;
        pvData.caub.cElems = 4;
        pvData.caub.pElems = loopBytes;
        containerMeta->SetMetadataByName(L"/appext/Data", &pvData);
    }

    std::vector<uint8_t> bgra;
    for (uint32_t f = 0; f < frameCount; ++f) {
        RenderSampleFrame(2, w, h, static_cast<float>(f) / static_cast<float>(frameCount), bgra);

        ComPtr<IWICBitmap> srcBmp;
        if (FAILED(wic->CreateBitmapFromMemory(w, h, GUID_WICPixelFormat32bppBGRA, w * 4,
                                               static_cast<UINT>(bgra.size()), bgra.data(), srcBmp.GetAddressOf()))) {
            break;
        }

        ComPtr<IWICBitmapFrameEncode> frameEnc;
        if (FAILED(encoder->CreateNewFrame(frameEnc.GetAddressOf(), nullptr))) break;
        if (FAILED(frameEnc->Initialize(nullptr))) break;
        frameEnc->SetSize(w, h);

        ComPtr<IWICMetadataQueryWriter> fmeta;
        if (SUCCEEDED(frameEnc->GetMetadataQueryWriter(fmeta.GetAddressOf())) && fmeta) {
            PROPVARIANT pvDelay;
            PropVariantInit(&pvDelay);
            pvDelay.vt = VT_UI2;
            pvDelay.uiVal = 6;  // 6 * 10ms = 60ms (~16.6 FPS)
            fmeta->SetMetadataByName(L"/grctlext/Delay", &pvDelay);
        }

        WICPixelFormatGUID pixFmt = GUID_WICPixelFormat8bppIndexed;
        frameEnc->SetPixelFormat(&pixFmt);

        ComPtr<IWICPalette> pal;
        if (SUCCEEDED(wic->CreatePalette(pal.GetAddressOf())) && pal) {
            if (SUCCEEDED(pal->InitializeFromBitmap(srcBmp.Get(), 256, FALSE))) {
                frameEnc->SetPalette(pal.Get());
            }
        }

        frameEnc->WriteSource(srcBmp.Get(), nullptr);
        frameEnc->Commit();
    }

    HRESULT hr = encoder->Commit();
    return SUCCEEDED(hr) && FileExists(outPath);
}

bool EnsureStarterHtmlFile(const std::wstring& htmlPath) {
    if (FileExists(htmlPath) && GetFileSizeBytes(htmlPath) > 256) return true;

    const char* html = R"(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge">
<title>Interactive Constellation</title>
<style>
  html, body { margin: 0; padding: 0; width: 100%; height: 100%; overflow: hidden; background: #070b14; }
  canvas { display: block; width: 100%; height: 100%; }
</style>
</head>
<body>
<canvas id="c"></canvas>
<script>
(function() {
  var c = document.getElementById('c');
  var ctx = c.getContext('2d');
  var W = 1280, H = 720, mx = -999, my = -999;
  function resize() { W = c.width = window.innerWidth || 1280; H = c.height = window.innerHeight || 720; }
  window.onresize = resize; resize();
  window.onmousemove = function(e) { mx = e.clientX; my = e.clientY; };
  var pts = [];
  for (var i = 0; i < 85; i++) {
    pts.push({
      x: Math.random() * W, y: Math.random() * H,
      vx: (Math.random() - 0.5) * 0.9, vy: (Math.random() - 0.5) * 0.9,
      r: 1.4 + Math.random() * 2.0
    });
  }
  function tick() {
    var g = ctx.createLinearGradient(0, 0, W, H);
    g.addColorStop(0, '#070b16'); g.addColorStop(0.5, '#0d192b'); g.addColorStop(1, '#120d26');
    ctx.fillStyle = g; ctx.fillRect(0, 0, W, H);
    for (var i = 0; i < pts.length; i++) {
      var p = pts[i];
      p.x += p.vx; p.y += p.vy;
      if (p.x < 0 || p.x > W) p.vx = -p.vx;
      if (p.y < 0 || p.y > H) p.vy = -p.vy;
      ctx.beginPath(); ctx.arc(p.x, p.y, p.r, 0, 6.283);
      ctx.fillStyle = '#38bdf8'; ctx.fill();
      for (var j = i + 1; j < pts.length; j++) {
        var q = pts[j];
        var dx = p.x - q.x, dy = p.y - q.y;
        var d2 = dx * dx + dy * dy;
        if (d2 < 16500) {
          ctx.strokeStyle = 'rgba(56,189,248,' + (1 - d2 / 16500) * 0.45 + ')';
          ctx.lineWidth = 1;
          ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(q.x, q.y); ctx.stroke();
        }
      }
      var mdx = p.x - mx, mdy = p.y - my, md2 = mdx * mdx + mdy * mdy;
      if (md2 < 26000) {
        ctx.strokeStyle = 'rgba(168,85,247,' + (1 - md2 / 26000) * 0.75 + ')';
        ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(mx, my); ctx.stroke();
      }
    }
    requestAnimationFrame(tick);
  }
  tick();
})();
</script>
</body>
</html>
)";

    HANDLE hf = CreateFileW(htmlPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    BOOL ok = WriteFile(hf, html, static_cast<DWORD>(std::strlen(html)), &wr, nullptr);
    CloseHandle(hf);
    return ok != FALSE;
}

uint16_t ReadMetaU16(IWICMetadataQueryReader* rdr, const wchar_t* name, uint16_t defVal) {
    if (!rdr) return defVal;
    PROPVARIANT pv;
    PropVariantInit(&pv);
    uint16_t out = defVal;
    if (SUCCEEDED(rdr->GetMetadataByName(name, &pv))) {
        if (pv.vt == VT_UI2) out = pv.uiVal;
        else if (pv.vt == VT_UI1) out = pv.bVal;
    }
    PropVariantClear(&pv);
    return out;
}

}  // namespace

uint32_t ComputeVibrantAccentRGB(const uint8_t* bgra, uint32_t w, uint32_t h) {
    if (!bgra || w == 0 || h == 0) return 0x0EA5E9;

    float bucketWeight[18]{};
    float bucketHueSum[18]{};
    const size_t step = std::max<size_t>(1, (static_cast<size_t>(w) * h) / 2048);
    const size_t totalPx = static_cast<size_t>(w) * h;

    for (size_t i = 0; i < totalPx; i += step) {
        float b = bgra[i * 4 + 0] / 255.0f;
        float g = bgra[i * 4 + 1] / 255.0f;
        float r = bgra[i * 4 + 2] / 255.0f;

        float hDeg = 0, s = 0, v = 0;
        RgbToHsv(r, g, b, hDeg, s, v);
        if (s < 0.18f || v < 0.15f) continue;

        float weight = s * std::sqrt(v);
        int idx = std::clamp(static_cast<int>(hDeg / 20.0f), 0, 17);
        bucketWeight[idx] += weight;
        bucketHueSum[idx] += hDeg * weight;
    }

    int best = -1;
    float bestW = 0.0f;
    for (int i = 0; i < 18; ++i) {
        if (bucketWeight[i] > bestW) {
            bestW = bucketWeight[i];
            best = i;
        }
    }

    if (best < 0 || bestW < 0.2f) {
        return 0x0EA5E9;
    }
    float avgHue = bucketHueSum[best] / bucketWeight[best];
    return HsvToRgbHex(avgHue, 0.78f, 0.94f);
}

bool DecodeAnimatedGif(const std::wstring& gifPath, AnimatedGifData& outGif) {
    outGif = {};
    if (!FileExists(gifPath)) return false;

    ComPtr<IWICImagingFactory> wic;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                __uuidof(IWICImagingFactory), reinterpret_cast<void**>(wic.GetAddressOf()))) || !wic) {
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromFilename(
            gifPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf())) || !decoder) {
        return false;
    }

    UINT frameCount = 0;
    if (FAILED(decoder->GetFrameCount(&frameCount)) || frameCount == 0) return false;
    frameCount = std::min<UINT>(frameCount, 300);  // Cap RAM usage on huge GIFs

    UINT canvasW = 0, canvasH = 0;
    ComPtr<IWICMetadataQueryReader> globalMeta;
    if (SUCCEEDED(decoder->GetMetadataQueryReader(globalMeta.GetAddressOf())) && globalMeta) {
        canvasW = ReadMetaU16(globalMeta.Get(), L"/logscrdesc/Width", 0);
        canvasH = ReadMetaU16(globalMeta.Get(), L"/logscrdesc/Height", 0);
    }

    ComPtr<IWICBitmapFrameDecode> firstFrame;
    if ((canvasW == 0 || canvasH == 0) && SUCCEEDED(decoder->GetFrame(0, firstFrame.GetAddressOf())) && firstFrame) {
        firstFrame->GetSize(&canvasW, &canvasH);
    }
    if (canvasW == 0 || canvasH == 0 || canvasW > 4096 || canvasH > 4096) return false;

    outGif.width = canvasW;
    outGif.height = canvasH;

    std::vector<uint8_t> canvas(static_cast<size_t>(canvasW) * canvasH * 4, 0);
    for (size_t i = 3; i < canvas.size(); i += 4) canvas[i] = 255;

    for (UINT idx = 0; idx < frameCount; ++idx) {
        ComPtr<IWICBitmapFrameDecode> rawFrame;
        if (FAILED(decoder->GetFrame(idx, rawFrame.GetAddressOf())) || !rawFrame) continue;

        UINT fw = 0, fh = 0;
        rawFrame->GetSize(&fw, &fh);
        uint16_t left = 0, top = 0, delayCs = 6, disposal = 0;

        ComPtr<IWICMetadataQueryReader> fmeta;
        if (SUCCEEDED(rawFrame->GetMetadataQueryReader(fmeta.GetAddressOf())) && fmeta) {
            left = ReadMetaU16(fmeta.Get(), L"/imgdesc/Left", 0);
            top = ReadMetaU16(fmeta.Get(), L"/imgdesc/Top", 0);
            delayCs = ReadMetaU16(fmeta.Get(), L"/grctlext/Delay", 6);
            disposal = ReadMetaU16(fmeta.Get(), L"/grctlext/Disposal", 0);
        }

        ComPtr<IWICFormatConverter> conv;
        if (FAILED(wic->CreateFormatConverter(conv.GetAddressOf())) || !conv) continue;
        if (FAILED(conv->Initialize(rawFrame.Get(), GUID_WICPixelFormat32bppBGRA,
                                    WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
            continue;
        }

        std::vector<uint8_t> patch(static_cast<size_t>(fw) * fh * 4);
        if (FAILED(conv->CopyPixels(nullptr, fw * 4, static_cast<UINT>(patch.size()), patch.data()))) {
            continue;
        }

        std::vector<uint8_t> savedBefore = (disposal == 3) ? canvas : std::vector<uint8_t>{};

        // Alpha-blend frame patch onto canvas
        for (UINT py = 0; py < fh && (top + py) < canvasH; ++py) {
            uint8_t* dstRow = canvas.data() + static_cast<size_t>(top + py) * canvasW * 4;
            const uint8_t* srcRow = patch.data() + static_cast<size_t>(py) * fw * 4;
            for (UINT px = 0; px < fw && (left + px) < canvasW; ++px) {
                const uint8_t* sp = srcRow + px * 4;
                uint8_t* dp = dstRow + (left + px) * 4;
                uint32_t sa = sp[3];
                if (sa == 255 || idx == 0) {
                    dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = 255;
                } else if (sa > 0) {
                    uint32_t inv = 255 - sa;
                    dp[0] = static_cast<uint8_t>((sp[0] * sa + dp[0] * inv) / 255);
                    dp[1] = static_cast<uint8_t>((sp[1] * sa + dp[1] * inv) / 255);
                    dp[2] = static_cast<uint8_t>((sp[2] * sa + dp[2] * inv) / 255);
                    dp[3] = 255;
                }
            }
        }

        GifFrame gf;
        gf.bgra = canvas;
        gf.delayMs = std::max<uint32_t>(20, static_cast<uint32_t>(delayCs) * 10);
        outGif.frames.push_back(std::move(gf));

        if (disposal == 2) {
            for (UINT py = 0; py < fh && (top + py) < canvasH; ++py) {
                uint8_t* dstRow = canvas.data() + static_cast<size_t>(top + py) * canvasW * 4;
                for (UINT px = 0; px < fw && (left + px) < canvasW; ++px) {
                    uint8_t* dp = dstRow + (left + px) * 4;
                    dp[0] = 0; dp[1] = 0; dp[2] = 0; dp[3] = 255;
                }
            }
        } else if (disposal == 3 && !savedBefore.empty()) {
            canvas = std::move(savedBefore);
        }
    }

    return !outGif.frames.empty();
}

bool SaveBmpBGRA(const std::wstring& bmpPath, uint32_t w, uint32_t h, const uint8_t* bgraData) {
    if (!bgraData || w == 0 || h == 0) return false;
    const uint32_t imgBytes = w * h * 4;
    BmpFileHeader fh{};
    fh.bfSize = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + imgBytes;
    fh.bfOffBits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

    BmpInfoHeader ih{};
    ih.biWidth = static_cast<int32_t>(w);
    ih.biHeight = -static_cast<int32_t>(h);
    ih.biSizeImage = imgBytes;

    HANDLE hFile = CreateFileW(bmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok1 = WriteFile(hFile, &fh, sizeof(fh), &written, nullptr);
    BOOL ok2 = WriteFile(hFile, &ih, sizeof(ih), &written, nullptr);
    BOOL ok3 = WriteFile(hFile, bgraData, imgBytes, &written, nullptr);
    CloseHandle(hFile);
    return ok1 && ok2 && ok3;
}

bool LoadBmpBGRA(const std::wstring& bmpPath, uint32_t& outW, uint32_t& outH, std::vector<uint8_t>& outBGRA) {
    HANDLE hFile = CreateFileW(bmpPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BmpFileHeader fh{};
    BmpInfoHeader ih{};
    DWORD rd = 0;
    if (!ReadFile(hFile, &fh, sizeof(fh), &rd, nullptr) || rd != sizeof(fh) || fh.bfType != 0x4D42) {
        CloseHandle(hFile); return false;
    }
    if (!ReadFile(hFile, &ih, sizeof(ih), &rd, nullptr) || rd != sizeof(ih) || ih.biBitCount != 32 || ih.biWidth <= 0) {
        CloseHandle(hFile); return false;
    }

    const bool topDown = (ih.biHeight < 0);
    const uint32_t w = static_cast<uint32_t>(ih.biWidth);
    const uint32_t h = static_cast<uint32_t>(topDown ? -ih.biHeight : ih.biHeight);
    if (w == 0 || h == 0 || w > 4096 || h > 4096) { CloseHandle(hFile); return false; }

    SetFilePointer(hFile, static_cast<LONG>(fh.bfOffBits), nullptr, FILE_BEGIN);
    const size_t totalBytes = static_cast<size_t>(w) * h * 4;
    std::vector<uint8_t> raw(totalBytes);
    if (!ReadFile(hFile, raw.data(), static_cast<DWORD>(totalBytes), &rd, nullptr) || rd != totalBytes) {
        CloseHandle(hFile); return false;
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

VideoProbeInfo ProbeWallpaperAndSaveThumbnail(const std::wstring& path) {
    VideoProbeInfo info{};
    if (!WallpaperSourceExists(path)) return info;

    info.kind = ClassifyWallpaperPath(path);
    wchar_t hashName[64];
    swprintf_s(hashName, L"%016llx.bmp", static_cast<unsigned long long>(HashStringFNV1a(path)));
    std::wstring thumbPath = GetThumbsDir() + L"\\" + hashName;

    // 1. Procedural GPU Shader or HTML5 Web scene
    if (info.kind == WallpaperKind::Shader || info.kind == WallpaperKind::Web) {
        int themeId = 5;
        if (path == L"shader://cyber_tunnel") themeId = 3;
        else if (path == L"shader://sunset_ocean") themeId = 4;
        else if (path == L"shader://matrix_pulse") themeId = 5;

        std::vector<uint8_t> thumb;
        RenderSampleFrame(themeId, 320, 180, 0.25f, thumb);
        if (SaveBmpBGRA(thumbPath, 320, 180, thumb.data())) {
            info.thumbPath = thumbPath;
        }
        info.dominantColorRGB = ComputeVibrantAccentRGB(thumb.data(), 320, 180);
        info.width = 1920;
        info.height = 1080;
        info.durationSec = 0.0;
        info.valid = true;
        return info;
    }

    // 2. Animated GIF
    if (info.kind == WallpaperKind::Gif) {
        AnimatedGifData gif;
        if (!DecodeAnimatedGif(path, gif) || gif.frames.empty()) return info;
        info.width = gif.width;
        info.height = gif.height;
        uint64_t totalMs = 0;
        for (const auto& f : gif.frames) totalMs += f.delayMs;
        info.durationSec = static_cast<double>(totalMs) / 1000.0;

        size_t midIdx = gif.frames.size() / 3;
        std::vector<uint8_t> thumb;
        ResizeCoverBGRA(gif.frames[midIdx].bgra.data(), gif.width, gif.height,
                        static_cast<int32_t>(gif.width * 4), 320, 180, thumb);
        if (!thumb.empty()) {
            if (SaveBmpBGRA(thumbPath, 320, 180, thumb.data())) {
                info.thumbPath = thumbPath;
            }
            info.dominantColorRGB = ComputeVibrantAccentRGB(thumb.data(), 320, 180);
        }
        info.valid = true;
        return info;
    }

    // 3. Video (.mp4 / .webm / .mkv / .mov / .avi / .wmv) via Media Foundation
    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(attrs.GetAddressOf(), 2);
    if (attrs) {
        attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }

    ComPtr<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromURL(path.c_str(), attrs.Get(), reader.GetAddressOf())) || !reader) {
        return info;
    }

    PROPVARIANT varDur;
    PropVariantInit(&varDur);
    if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &varDur))) {
        if (varDur.vt == VT_UI8) info.durationSec = static_cast<double>(varDur.uhVal.QuadPart) / 10000000.0;
        else if (varDur.vt == VT_I8) info.durationSec = static_cast<double>(varDur.hVal.QuadPart) / 10000000.0;
    }
    PropVariantClear(&varDur);

    ComPtr<IMFMediaType> rgbType;
    MFCreateMediaType(rgbType.GetAddressOf());
    rgbType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    rgbType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, rgbType.Get()))) {
        return info;
    }

    int32_t defaultStride = 0;
    ComPtr<IMFMediaType> actualType;
    if (SUCCEEDED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, actualType.GetAddressOf())) && actualType) {
        UINT32 w = 0, h = 0;
        if (SUCCEEDED(MFGetAttributeSize(actualType.Get(), MF_MT_FRAME_SIZE, &w, &h))) {
            info.width = w;
            info.height = h;
        }
        defaultStride = static_cast<int32_t>(info.width * 4);
        UINT32 strideU32 = 0;
        if (SUCCEEDED(actualType->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideU32))) {
            defaultStride = static_cast<int32_t>(strideU32);
        }
    }

    info.valid = (info.width > 0 && info.height > 0);
    if (!info.valid) return info;

    if (info.durationSec > 0.5) {
        PROPVARIANT varPos;
        PropVariantInit(&varPos);
        varPos.vt = VT_I8;
        varPos.hVal.QuadPart = static_cast<LONGLONG>(std::min(0.8, info.durationSec * 0.15) * 10000000.0);
        reader->SetCurrentPosition(GUID_NULL, varPos);
        PropVariantClear(&varPos);
    }

    auto tryReadSample = [&](ComPtr<IMFSample>& outSample) -> bool {
        for (int attempt = 0; attempt < 16; ++attempt) {
            DWORD streamFlags = 0;
            ComPtr<IMFSample> s;
            HRESULT r = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &streamFlags, nullptr, s.GetAddressOf());
            if (FAILED(r) || (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)) return false;
            if (s) { outSample = s; return true; }
        }
        return false;
    };

    ComPtr<IMFSample> sample;
    if (!tryReadSample(sample)) {
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

            if (!thumbBGRA.empty()) {
                if (SaveBmpBGRA(thumbPath, 320, 180, thumbBGRA.data())) {
                    info.thumbPath = thumbPath;
                }
                info.dominantColorRGB = ComputeVibrantAccentRGB(thumbBGRA.data(), 320, 180);
            }
        }
    }

    return info;
}

std::vector<WallpaperItem> EnsureStarterLibraryItems() {
    std::wstring samplesDir = GetSamplesDir();
    std::vector<WallpaperItem> out;

    auto registerStarter = [&](const std::wstring& path, const wchar_t* title) {
        VideoProbeInfo probe = ProbeWallpaperAndSaveThumbnail(path);
        if (!probe.valid) return;
        WallpaperItem item;
        item.path = path;
        item.title = title;
        item.kind = probe.kind;
        item.thumbPath = probe.thumbPath;
        item.width = probe.width;
        item.height = probe.height;
        item.durationSec = probe.durationSec;
        item.fileSize = GetFileSizeBytes(path);
        item.dominantColorRGB = probe.dominantColorRGB;
        item.isBuiltIn = true;
        out.push_back(std::move(item));
    };

    // 1. 3 real H.264 .mp4 live wallpapers
    struct Mp4Def { const wchar_t* file; const wchar_t* title; int theme; };
    const Mp4Def mp4s[] = {
        {L"Neon_Synthwave_Grid.mp4", L"Неоновый Синтвейв (MP4)", 0},
        {L"Northern_Aurora_Sky.mp4", L"Северное Сияние (MP4)", 1},
        {L"Cosmic_Nebula_Pulse.mp4", L"Космическая Туманность (MP4)", 2},
    };
    for (const auto& d : mp4s) {
        std::wstring full = samplesDir + L"\\" + d.file;
        if (EncodeProceduralMp4(full, d.theme, 640, 360, 30, 90)) {
            registerStarter(full, d.title);
        }
    }

    // 2. Real multi-frame animated .gif (#10)
    std::wstring gifPath = samplesDir + L"\\Cyber_Pulse_Orbit.gif";
    if (EncodeProceduralAnimatedGif(gifPath, 320, 180, 24)) {
        registerStarter(gifPath, L"Кибер-Орбита (Анимация GIF)");
    }

    // 3. 3 built-in 60-FPS Audio-Reactive GPU Shaders (#10)
    registerStarter(L"shader://cyber_tunnel", L"Киберпанк Туннель (3D Shader)");
    registerStarter(L"shader://sunset_ocean", L"Закатный Океан (GPU Shader)");
    registerStarter(L"shader://matrix_pulse", L"Неоновая Матрица (GPU Shader)");

    // 4. Built-in Interactive HTML5 Canvas scene (#10)
    std::wstring htmlPath = samplesDir + L"\\Interactive_Constellation.html";
    if (EnsureStarterHtmlFile(htmlPath)) {
        registerStarter(htmlPath, L"Созвездия (Интерактивный HTML5)");
    }

    return out;
}

}  // namespace wp
