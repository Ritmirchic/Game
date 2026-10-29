// FRACTURE — persistent configuration (settings.cfg in %APPDATA%/FRACTURE).
#include "core/engine.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <map>

namespace cfg {

static std::string path() { return platform::dataDir() + "/settings.cfg"; }

void save(const Settings& s) {
    FILE* f = fopen(path().c_str(), "w");
    if (!f) return;
    fprintf(f, "# FRACTURE: TOTAL DESTRUCTION - configuration\n");
    fprintf(f, "resolutionW %d\n", s.resolutionW);
    fprintf(f, "resolutionH %d\n", s.resolutionH);
    fprintf(f, "windowMode %d\n", s.windowMode);
    fprintf(f, "vsync %d\n", (int)s.vsync);
    fprintf(f, "fpsLimit %d\n", s.fpsLimit);
    fprintf(f, "qualityPreset %d\n", s.qualityPreset);
    fprintf(f, "renderScale %.3f\n", s.renderScale);
    fprintf(f, "shadowQuality %d\n", s.shadowQuality);
    fprintf(f, "aaMode %d\n", s.aaMode);
    fprintf(f, "motionBlur %d\n", (int)s.motionBlur);
    fprintf(f, "viewDistance %d\n", s.viewDistance);
    fprintf(f, "fov %.2f\n", s.fov);
    fprintf(f, "destructionQuality %d\n", s.destructionQuality);
    fprintf(f, "debrisDensity %d\n", s.debrisDensity);
    fprintf(f, "physicsQuality %.2f\n", s.physicsQuality);
    fprintf(f, "particleQuality %d\n", s.particleQuality);
    fprintf(f, "exposure %.3f\n", s.exposure);
    fprintf(f, "mouseSensitivity %.3f\n", s.mouseSensitivity);
    fprintf(f, "invertY %d\n", (int)s.invertY);
    fprintf(f, "masterVolume %.3f\n", s.masterVolume);
    fprintf(f, "sfxVolume %.3f\n", s.sfxVolume);
    fprintf(f, "musicVolume %.3f\n", s.musicVolume);
    fprintf(f, "showFps %d\n", (int)s.showFps);
    fprintf(f, "reduceShake %d\n", (int)s.reduceShake);
    fprintf(f, "rendererMode %d\n", s.rendererMode);
    fclose(f);
    logMsg("[cfg] saved %s", path().c_str());
}

bool load(Settings& s) {
    FILE* f = fopen(path().c_str(), "r");
    if (!f) return false;
    char key[64];
    double v;
    int n = 0;
    while (fscanf(f, "%63s %lf", key, &v) == 2) {
        n++;
        float fv = (float)v;
        int iv = (int)v;
        if (!strcmp(key, "resolutionW")) s.resolutionW = iv;
        else if (!strcmp(key, "resolutionH")) s.resolutionH = iv;
        else if (!strcmp(key, "windowMode")) s.windowMode = iv;
        else if (!strcmp(key, "vsync")) s.vsync = iv != 0;
        else if (!strcmp(key, "fpsLimit")) s.fpsLimit = iv;
        else if (!strcmp(key, "qualityPreset")) s.qualityPreset = iv;
        else if (!strcmp(key, "renderScale")) s.renderScale = fv;
        else if (!strcmp(key, "shadowQuality")) s.shadowQuality = iv;
        else if (!strcmp(key, "aaMode")) s.aaMode = iv;
        else if (!strcmp(key, "motionBlur")) s.motionBlur = iv != 0;
        else if (!strcmp(key, "viewDistance")) s.viewDistance = iv;
        else if (!strcmp(key, "fov")) s.fov = fv;
        else if (!strcmp(key, "destructionQuality")) s.destructionQuality = iv;
        else if (!strcmp(key, "debrisDensity")) s.debrisDensity = iv;
        else if (!strcmp(key, "physicsQuality")) s.physicsQuality = fv;
        else if (!strcmp(key, "particleQuality")) s.particleQuality = iv;
        else if (!strcmp(key, "exposure")) s.exposure = fv;
        else if (!strcmp(key, "mouseSensitivity")) s.mouseSensitivity = fv;
        else if (!strcmp(key, "invertY")) s.invertY = iv != 0;
        else if (!strcmp(key, "masterVolume")) s.masterVolume = fv;
        else if (!strcmp(key, "sfxVolume")) s.sfxVolume = fv;
        else if (!strcmp(key, "musicVolume")) s.musicVolume = fv;
        else if (!strcmp(key, "showFps")) s.showFps = iv != 0;
        else if (!strcmp(key, "reduceShake")) s.reduceShake = iv != 0;
        else if (!strcmp(key, "rendererMode")) s.rendererMode = iv;
    }
    fclose(f);
    logMsg("[cfg] loaded %s (%d keys)", path().c_str(), n);
    return n > 0;
}

} // namespace cfg
