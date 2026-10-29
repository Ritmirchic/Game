#pragma once

#include "config.h"

namespace wp {

struct SteamLibraryScanResult {
    std::vector<WallpaperItem> items;
    std::wstring status;
    bool steamFound = false;
    bool collectionDiscovered = false;
};

// Scans the locally installed/subscribed Workshop collection for Steam app 431960.
// It intentionally does not query or represent the global online Workshop catalog.
SteamLibraryScanResult ScanInstalledSteamWorkshopLibrary();

}  // namespace wp
