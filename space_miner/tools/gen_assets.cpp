// ============================================================
// SPACE MINER  -  tools/gen_assets.cpp
// Build-time asset generation (runs on the build machine):
//   * application icon (.ico, 16/32/48/64/128/256)
//   * Windows resource script compiled to an object
//   * embedded setup payload header for the installer
// This is what replaces Blender/Krita/GIMP in this toolchain:
// every visual asset is procedurally painted by the game's own
// art pipeline, and the icon is generated the same way.
// ============================================================
#include "core.h"
#include "gfx.h"
#include "texture.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <string>
#ifdef _WIN32
// icon + resource compilation with the Windows SDK (MinGW resource compiler)
#include <windows.h>
static bool runRC(const std::string& dir) {
    std::string cmd = "windres -i " + dir + "/space_miner.rc -o " + dir + "/rc_resources.o -O coff 2>nul";
    if (system(cmd.c_str()) != 0) {
        printf("windres failed, retrying with full path\n");
        return false;
    }
    return true;
}
#endif

using namespace sm;

static void buildIcon(Canvas& c, int size) {
    c.resize(size, size);
    c.clear(0);
    float s = size / 64.f;
    // plate
    c.circleFill(size * .5f, size * .5f, size * .5f, rgba(8, 14, 24, 255));
    c.circleFill(size * .5f, size * .5f, size * .5f - 1.6f * s, rgba(18, 30, 46, 255));
    // hull hexagon
    Vec2 hex[6];
    for (int i = 0; i < 6; i++) {
        float a = i * TAU / 6 + PI / 6;
        hex[i] = Vec2(size * .5f + cosf(a) * 18.f * s, size * .5f + sinf(a) * 18.f * s);
    }
    c.polyFill(hex, 6, rgba(52, 76, 100, 255));
    c.polyOutline(hex, 6, rgba(150, 215, 240, 255), std::max(1.f, 1.4f * s));
    // inner arc reactor
    c.ring(size * .5f, size * .5f, 9.f * s, std::max(1.f, 1.2f * s), rgba(110, 220, 255, 220));
    c.circleFill(size * .5f, size * .5f, 3.4f * s, rgba(120, 235, 255, 240));
    c.radialGlow(size * .5f, size * .5f, 16.f * s, rgba(90, 200, 255, 150), 1.f);
    // mining beam and gem spark
    Vec2 a(size * .5f + 6 * s, size * .5f - 6 * s), b(size * .5f + 24 * s, size * .5f - 24 * s);
    c.line(a, b, rgba(140, 245, 255, 255), 3.2f * s);
    c.line(a, b, rgba(255, 255, 255, 255), 1.3f * s);
    Vec2 star[8];
    for (int i = 0; i < 8; i++) {
        float ang = i * TAU / 8;
        float rr = (i % 2 == 0) ? 8.f * s : 3.f * s;
        star[i] = Vec2(size * .5f + cosf(ang) * rr + 14 * s, size * .5f + sinf(ang) * rr - 14 * s);
    }
    c.polyFill(star, 8, rgba(255, 232, 160, 250));
    c.radialGlow(size * .5f + 14 * s, size * .5f - 14 * s, 14 * s, rgba(255, 210, 120, 140), 1.f);
}

static bool writeICO(const char* path, const std::vector<Canvas>& images) {
    std::vector<uint8_t> out;
    auto put16 = [&](uint16_t v) { out.push_back(v & 255); out.push_back(v >> 8); };
    auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) out.push_back((v >> (i * 8)) & 255); };
    put16(0); put16(1); put16((uint16_t)images.size());
    struct Entry { int size; uint32_t offset; };
    std::vector<Entry> entries;
    uint32_t offset = 6 + 16 * (uint32_t)images.size();
    std::vector<std::vector<uint8_t>> blobs;
    for (auto& img : images) {
        // DIB (BITMAPINFOHEADER + BGRA bottom-up + AND mask)
        std::vector<uint8_t> dib;
        int w = img.w, h = img.h;
        auto p32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) dib.push_back((v >> (i * 8)) & 255); };
        p32(40); p32(w); p32(h * 2); p32(1); p32(32); p32(0); p32(0);
        p32(0); p32(0); p32(0); p32(0);
        for (int y = h - 1; y >= 0; y--)
            for (int x = 0; x < w; x++) {
                Color c = img.px[(size_t)y * w + x];
                int a = ca(c);
                // straight -> premultiplied for DIB sections with alpha
                dib.push_back((uint8_t)(cb(c) * a / 255));
                dib.push_back((uint8_t)(cg(c) * a / 255));
                dib.push_back((uint8_t)(cr(c) * a / 255));
                dib.push_back((uint8_t)a);
            }
        int maskStride = ((w + 31) / 32) * 4;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < maskStride; x++) dib.push_back(0);
        entries.push_back({w, offset});
        offset += (uint32_t)dib.size();
        blobs.push_back(std::move(dib));
    }
    for (size_t i = 0; i < images.size(); i++) {
        out.push_back((uint8_t)(entries[i].size >= 256 ? 0 : entries[i].size));
        out.push_back((uint8_t)(entries[i].size >= 256 ? 0 : entries[i].size));
        out.push_back(0); out.push_back(0);
        put16(1); put16(32);
        put32((uint32_t)blobs[i].size());
        put32(entries[i].offset);
    }
    for (auto& b : blobs) out.insert(out.end(), b.begin(), b.end());
    return writeFile(path, out.data(), out.size());
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "build";
    std::vector<Canvas> icons;
    int sizes[] = {16, 32, 48, 64, 128, 256};
    for (int s : sizes) {
        Canvas c;
        buildIcon(c, s);
        icons.push_back(std::move(c));
    }
    // absolute path so the resource compiler finds it from any cwd
    char cwdBuf[1024];
    std::string cwd = getcwd(cwdBuf, sizeof(cwdBuf)) ? cwdBuf : ".";
    std::string icoPath = dir + "/space_miner.ico";
    std::string icoAbs = (dir.size() && dir[0] == '/') ? icoPath : cwd + "/" + icoPath;
    if (!writeICO(icoPath.c_str(), icons)) {
        printf("failed to write icon\n");
        return 1;
    }
    // preview the icon at 256 for the docs
    writePNG((dir + "/icon_preview.png").c_str(), icons.back().px.data(), icons.back().w, icons.back().h);
    // ---- resource script
    {
        std::string rc;
        rc += "1 ICON \"" + icoAbs + "\"\n";
        rc += "1 VERSIONINFO\nFILEVERSION 3,0,0,0\nPRODUCTVERSION 3,0,0,0\nFILEFLAGSMASK 0\nFILEFLAGS 0\n"
              "FILEOS 0x40004L\nFILETYPE 0x1L\nFILESUBTYPE 0\nBEGIN\n"
              "  BLOCK \"StringFileInfo\"\n  BEGIN\n    BLOCK \"040904b0\"\n    BEGIN\n"
              "      VALUE \"CompanyName\", \"Arena.ai\"\n"
              "      VALUE \"FileDescription\", \"SPACE MINER - deep belt mining adventure\"\n"
              "      VALUE \"FileVersion\", \"3.0.0.0\"\n"
              "      VALUE \"InternalName\", \"SPACE_MINER\"\n"
              "      VALUE \"LegalCopyright\", \"(c) 2026\"\n"
              "      VALUE \"OriginalFilename\", \"SPACE_MINER.exe\"\n"
              "      VALUE \"ProductName\", \"SPACE MINER\"\n"
              "      VALUE \"ProductVersion\", \"3.0.0.0\"\n"
              "    END\n  END\n  BLOCK \"VarFileInfo\"\n  BEGIN\n    VALUE \"Translation\", 0x409, 1200\n  END\nEND\n";
        writeFile(dir + "/space_miner.rc", rc.data(), rc.size());
    }
    // ---- embedded payload for the installer (the exe bytes + readme)
    {
        std::vector<uint8_t> exe;
        std::string exePath = "release/SPACE_MINER.exe";
        if (!readFile(exePath, exe)) {
            printf("note: %s not found yet, writing empty payload\n", exePath.c_str());
        }
        std::vector<uint8_t> readme;
        readFile("release/README.txt", readme);
        std::string h;
        h += "// generated by tools/gen_assets.cpp - do not edit\n";
        h += "#pragma once\n#include <cstdint>\n";
        h += "static const unsigned char GAME_EXE[] = {";
        for (size_t i = 0; i < exe.size(); i++) {
            if (i % 32 == 0) h += "\n";
            h += std::to_string((int)exe[i]) + ",";
        }
        h += "};\n";
        h += "static const unsigned long GAME_EXE_SIZE = " + std::to_string(exe.size()) + "UL;\n";
        h += "static const unsigned char GAME_README[] = {";
        for (size_t i = 0; i < readme.size(); i++) {
            if (i % 32 == 0) h += "\n";
            h += std::to_string((int)readme[i]) + ",";
        }
        h += "};\n";
        h += "static const unsigned long GAME_README_SIZE = " + std::to_string(readme.size()) + "UL;\n";
        h += std::string("static const char GAME_VERSION[] = \"3.0.0\";\n");
        writeFile(dir + "/setup_payload.h", h.data(), h.size());
        // payload translation unit (keeps compile units small)
        std::string cpp = "#include \"setup_payload.h\"\nextern \"C\" {\nunsigned long sm_payload_exe_size() { return GAME_EXE_SIZE; }\n"
                          "const unsigned char* sm_payload_exe() { return GAME_EXE; }\n"
                          "unsigned long sm_payload_readme_size() { return GAME_README_SIZE; }\n"
                          "const unsigned char* sm_payload_readme() { return GAME_README; }\n"
                          "const char* sm_payload_version() { return GAME_VERSION; }\n}\n";
        writeFile(dir + "/setup_payload.cpp", cpp.data(), cpp.size());
        printf("icon: %s\npayload exe: %zu bytes, readme: %zu bytes\n", icoPath.c_str(), exe.size(), readme.size());
#ifdef _WIN32
        runRC(dir);
#else
        printf("(icon will be compiled into the exe by the host-side resource step)\n");
#endif
    }
    return 0;
}
