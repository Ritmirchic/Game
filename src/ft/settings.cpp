// FRACTURE — settings persistence (INI-style)
#include "settings.h"

namespace ft {

static void iniWrite(FILE* f, const char* key, int v) { std::fprintf(f, "%s=%d\n", key, v); }
static void iniWrite(FILE* f, const char* key, f32 v) { std::fprintf(f, "%s=%.4f\n", key, (f64)v); }

void Settings::save(const char* path) const {
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    std::fprintf(f, "# FRACTURE settings\n");
    iniWrite(f, "width", width); iniWrite(f, "height", height);
    iniWrite(f, "windowMode", windowMode); iniWrite(f, "vsync", vsync);
    iniWrite(f, "fpsLimit", fpsLimit); iniWrite(f, "renderScale", renderScale);
    iniWrite(f, "shadowQuality", shadowQuality); iniWrite(f, "textureQuality", textureQuality);
    iniWrite(f, "lightingQuality", lightingQuality); iniWrite(f, "effectsQuality", effectsQuality);
    iniWrite(f, "viewDistance", viewDistance); iniWrite(f, "aa", aa);
    iniWrite(f, "motionBlur", motionBlur); iniWrite(f, "fov", fov);
    iniWrite(f, "destructionQuality", destructionQuality);
    iniWrite(f, "debrisDensity", debrisDensity); iniWrite(f, "physicsQuality", physicsQuality);
    iniWrite(f, "master", master); iniWrite(f, "sfx", sfx); iniWrite(f, "music", music);
    iniWrite(f, "maxVoices", maxVoices);
    iniWrite(f, "mouseSens", mouseSens); iniWrite(f, "invertY", invertY);
    iniWrite(f, "showFps", showFps); iniWrite(f, "preset", preset);
    std::fclose(f);
}

bool Settings::load(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char line[256];
    auto match = [&](const char* k, f32& out) {
        size_t kl = std::strlen(k);
        if (std::strncmp(line, k, kl) == 0 && line[kl] == '=') { out = (f32)std::atof(line + kl + 1); return true; }
        return false;
    };
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        f32 v;
        if (match("width", v)) width = (int)v;
        else if (match("height", v)) height = (int)v;
        else if (match("windowMode", v)) windowMode = (int)v;
        else if (match("vsync", v)) vsync = (int)v;
        else if (match("fpsLimit", v)) fpsLimit = (int)v;
        else if (match("renderScale", v)) renderScale = tclamp(v, 0.5f, 1.f);
        else if (match("shadowQuality", v)) shadowQuality = (int)v;
        else if (match("textureQuality", v)) textureQuality = (int)v;
        else if (match("lightingQuality", v)) lightingQuality = (int)v;
        else if (match("effectsQuality", v)) effectsQuality = (int)v;
        else if (match("viewDistance", v)) viewDistance = (int)v;
        else if (match("aa", v)) aa = (int)v;
        else if (match("motionBlur", v)) motionBlur = (int)v;
        else if (match("fov", v)) fov = tclamp(v, 60.f, 120.f);
        else if (match("destructionQuality", v)) destructionQuality = (int)v;
        else if (match("debrisDensity", v)) debrisDensity = (int)v;
        else if (match("physicsQuality", v)) physicsQuality = (int)v;
        else if (match("master", v)) master = tclamp(v, 0.f, 1.f);
        else if (match("sfx", v)) sfx = tclamp(v, 0.f, 1.f);
        else if (match("music", v)) music = tclamp(v, 0.f, 1.f);
        else if (match("maxVoices", v)) maxVoices = (int)v;
        else if (match("mouseSens", v)) mouseSens = tclamp(v, 0.1f, 5.f);
        else if (match("invertY", v)) invertY = v;
        else if (match("showFps", v)) showFps = (int)v;
        else if (match("preset", v)) preset = (int)v;
    }
    std::fclose(f);
    return true;
}

} // namespace ft
