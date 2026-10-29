#pragma once
#include <cstdint>
bool writePNG(const char* path, const uint32_t* pixels, int w, int h);
bool writeBMP(const char* path, const uint32_t* pixels, int w, int h);
