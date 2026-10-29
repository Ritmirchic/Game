#include "steam_library.h"

#include "common.h"
#include "media_tools.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_set>
#include <utility>

namespace wp {
namespace fs = std::filesystem;

namespace {

std::wstring ReadUtf8File(const std::wstring& path, size_t limitBytes = 4 * 1024 * 1024) {
    std::ifstream f(fs::path(path), std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    auto size = f.tellg();
    if (size <= 0 || static_cast<uint64_t>(size) > limitBytes) return {};
    std::string data(static_cast<size_t>(size), '\0');
    f.seekg(0, std::ios::beg);
    f.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (!f) return {};
    if (data.size() >= 3 && static_cast<uint8_t>(data[0]) == 0xEF &&
        static_cast<uint8_t>(data[1]) == 0xBB && static_cast<uint8_t>(data[2]) == 0xBF) {
        data.erase(0, 3);
    }
    return Utf8ToWide(data);
}

std::wstring ReadRegistryString(HKEY hive, const wchar_t* subkey, const wchar_t* valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(hive, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) return {};
    DWORD type = 0, bytes = 0;
    LSTATUS rc = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t)) {
        RegCloseKey(key);
        return {};
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    rc = RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &bytes);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS) return {};
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    if (type == REG_EXPAND_SZ) {
        DWORD needed = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
        if (needed > 1 && needed < 32768) {
            std::wstring expanded(needed, L'\0');
            ExpandEnvironmentStringsW(value.c_str(), expanded.data(), needed);
            while (!expanded.empty() && expanded.back() == L'\0') expanded.pop_back();
            value = std::move(expanded);
        }
    }
    return value;
}

std::wstring NormalizeSteamPath(std::wstring path) {
    for (auto& c : path) if (c == L'/') c = L'\\';
    while (!path.empty() && (path.back() == L'\\' || path.back() == L' ')) path.pop_back();
    return path;
}

std::vector<std::wstring> FindSteamRoots() {
    std::vector<std::wstring> roots;
    const std::pair<HKEY, const wchar_t*> keys[] = {
        {HKEY_CURRENT_USER, L"Software\\Valve\\Steam"},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam"},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam"},
    };
    for (const auto& entry : keys) {
        for (const wchar_t* value : {L"SteamPath", L"InstallPath"}) {
            std::wstring root = NormalizeSteamPath(ReadRegistryString(entry.first, entry.second, value));
            if (!root.empty() && fs::exists(fs::path(root) / L"steamapps")) roots.push_back(root);
        }
    }
    if (roots.empty()) {
        for (const wchar_t* candidate : {L"C:\\Program Files (x86)\\Steam", L"C:\\Program Files\\Steam"}) {
            if (fs::exists(fs::path(candidate) / L"steamapps")) roots.emplace_back(candidate);
        }
    }

    std::vector<std::wstring> expanded = roots;
    std::wregex pathRx(LR"VDF("path"\s*"([^"]+)")VDF", std::regex::icase);
    for (const auto& root : roots) {
        std::wstring vdf = ReadUtf8File((fs::path(root) / L"steamapps" / L"libraryfolders.vdf").wstring());
        for (std::wsregex_iterator it(vdf.begin(), vdf.end(), pathRx), end; it != end; ++it) {
            std::wstring p = (*it)[1].str();
            for (size_t pos = 0; (pos = p.find(L"\\\\", pos)) != std::wstring::npos;) p.replace(pos, 2, L"\\");
            p = NormalizeSteamPath(p);
            if (!p.empty() && fs::exists(fs::path(p) / L"steamapps") &&
                std::none_of(expanded.begin(), expanded.end(), [&](const std::wstring& x) { return _wcsicmp(x.c_str(), p.c_str()) == 0; })) {
                expanded.push_back(std::move(p));
            }
        }
    }
    return expanded;
}

std::unordered_set<std::wstring> ReadInstalledWorkshopIds(const std::wstring& steamApps) {
    std::unordered_set<std::wstring> ids;
    std::wstring acf = ReadUtf8File((fs::path(steamApps) / L"workshop" / L"appworkshop_431960.acf").wstring(), 16 * 1024 * 1024);
    size_t key = acf.find(L"\"WorkshopItemsInstalled\"");
    if (key == std::wstring::npos) return ids;
    size_t open = acf.find(L'{', key);
    if (open == std::wstring::npos) return ids;
    int depth = 0;
    size_t close = open;
    for (; close < acf.size(); ++close) {
        if (acf[close] == L'{') ++depth;
        else if (acf[close] == L'}' && --depth == 0) break;
    }
    if (close <= open || close >= acf.size()) return ids;
    const std::wstring block = acf.substr(open, close - open + 1);
    const std::wregex idRx(LR"VDF("([0-9]{5,})"\s*\{)VDF");
    for (std::wsregex_iterator it(block.begin(), block.end(), idRx), end; it != end; ++it) ids.insert((*it)[1].str());
    return ids;
}

std::wstring JsonUnescape(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != L'\\' || i + 1 >= value.size()) { out.push_back(value[i]); continue; }
        wchar_t c = value[++i];
        if (c == L'n') out.push_back(L' ');
        else if (c == L'r' || c == L't') out.push_back(L' ');
        else if (c == L'\\' || c == L'/' || c == L'\"') out.push_back(c);
        else out.push_back(c);
    }
    return out;
}

size_t FindJsonObjectEnd(const std::wstring& text, size_t open) {
    int depth = 0;
    bool quoted = false, escaped = false;
    for (size_t i = open; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == L'\\') escaped = true;
            else if (c == L'\"') quoted = false;
        } else {
            if (c == L'\"') quoted = true;
            else if (c == L'{') ++depth;
            else if (c == L'}' && --depth == 0) return i;
        }
    }
    return std::wstring::npos;
}

std::wstring JsonStringValue(const std::wstring& json, const std::wstring& key) {
    std::wstring needle = L"\"" + key + L"\"";
    size_t pos = json.find(needle);
    while (pos != std::wstring::npos) {
        size_t colon = json.find(L':', pos + needle.size());
        if (colon == std::wstring::npos) return {};
        size_t quote = json.find(L'\"', colon + 1);
        size_t comma = json.find_first_of(L",}\r\n", colon + 1);
        if (quote == std::wstring::npos || (comma != std::wstring::npos && quote > comma)) return {};
        std::wstring value;
        bool escaped = false;
        for (size_t i = quote + 1; i < json.size(); ++i) {
            wchar_t c = json[i];
            if (escaped) { value.push_back(L'\\'); value.push_back(c); escaped = false; }
            else if (c == L'\\') escaped = true;
            else if (c == L'\"') return JsonUnescape(value);
            else value.push_back(c);
        }
        return {};
    }
    return {};
}

std::wstring JsonSection(const std::wstring& json, const std::wstring& name) {
    size_t key = json.find(L"\"" + name + L"\"");
    if (key == std::wstring::npos) return json;
    size_t open = json.find(L'{', key);
    if (open == std::wstring::npos) return json;
    size_t close = FindJsonObjectEnd(json, open);
    if (close == std::wstring::npos) return json;
    return json.substr(open, close - open + 1);
}

struct ProjectMetadata {
    std::wstring title, type, preview, author, description;
};

ProjectMetadata ReadProjectMetadata(const fs::path& folder) {
    ProjectMetadata m;
    std::wstring json = ReadUtf8File((folder / L"project.json").wstring());
    if (json.empty()) return m;
    std::wstring general = JsonSection(json, L"general");
    m.title = JsonStringValue(general, L"title");
    m.type = JsonStringValue(general, L"type");
    m.preview = JsonStringValue(general, L"preview");
    m.author = JsonStringValue(general, L"author");
    m.description = JsonStringValue(general, L"description");
    if (m.title.empty()) m.title = JsonStringValue(json, L"title");
    if (m.type.empty()) m.type = JsonStringValue(json, L"type");
    if (m.preview.empty()) m.preview = JsonStringValue(json, L"preview");
    if (m.author.empty()) m.author = JsonStringValue(json, L"author");
    if (m.description.empty()) m.description = JsonStringValue(json, L"description");
    return m;
}

bool IsPreviewName(const std::wstring& name) {
    std::wstring lower = ToLowerUnicode(name);
    return lower == L"preview.jpg" || lower == L"preview.jpeg" || lower == L"preview.png" ||
           lower == L"preview.webp" || lower == L"preview.gif" || lower == L"thumbnail.jpg" || lower == L"thumbnail.png";
}

struct ProjectFiles {
    std::vector<std::wstring> media;
    std::wstring preview;
    std::wstring html;
};

ProjectFiles FindProjectFiles(const fs::path& folder) {
    ProjectFiles result;
    std::error_code ec;
    size_t visited = 0;
    fs::recursive_directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec), end;
    for (; it != end && visited < 20000; it.increment(ec), ++visited) {
        if (ec) { ec.clear(); continue; }
        if (it.depth() > 5) { it.disable_recursion_pending(); continue; }
        if (!it->is_regular_file(ec)) continue;
        std::wstring ext = ToLowerUnicode(it->path().extension().wstring());
        std::wstring name = ToLowerUnicode(it->path().filename().wstring());
        if (IsPreviewName(name) && result.preview.empty()) result.preview = it->path().wstring();
        if (ext == L".html" || ext == L".htm") {
            if (name == L"index.html" || name == L"index.htm") result.html = it->path().wstring();
            else if (result.html.empty()) result.html = it->path().wstring();
        }
        if (ext == L".mp4" || ext == L".m4v" || ext == L".mov" || ext == L".webm" ||
            ext == L".mkv" || ext == L".avi" || ext == L".wmv" || ext == L".gif") {
            if (!IsPreviewName(name)) result.media.push_back(it->path().wstring());
        }
    }
    std::sort(result.media.begin(), result.media.end());
    return result;
}

std::wstring ResolvePreview(const fs::path& folder, const std::wstring& metadataPreview,
                            const std::wstring& discoveredPreview) {
    if (!metadataPreview.empty()) {
        fs::path relative(metadataPreview);
        bool safeRelative = !relative.is_absolute();
        for (const auto& component : relative) if (component == L"..") safeRelative = false;
        if (safeRelative) {
            fs::path p = (folder / relative).lexically_normal();
            std::error_code ec;
            if (fs::is_regular_file(p, ec)) return p.wstring();
        }
    }
    if (!discoveredPreview.empty()) return discoveredPreview;
    return {};
}

WallpaperItem MakeWorkshopItem(const fs::path& itemDir, const std::wstring& id) {
    WallpaperItem item;
    item.isSteamWorkshop = true;
    item.isWorkshopInstalled = true;
    item.steamId = id;
    item.sourceRoot = itemDir.wstring();
    item.title = L"Steam Workshop #" + id;
    item.author = L"";
    item.workshopType = L"Wallpaper Engine";
    item.inPlaylist = true;

    ProjectMetadata meta = ReadProjectMetadata(itemDir);
    ProjectFiles files = FindProjectFiles(itemDir);
    if (!meta.title.empty()) item.title = meta.title.substr(0, 240);
    if (!meta.type.empty()) item.workshopType = meta.type.substr(0, 80);
    item.author = meta.author.substr(0, 160);
    item.description = meta.description.substr(0, 1200);
    item.thumbPath = {};

    std::wstring selected;
    const std::wstring type = ToLowerUnicode(meta.type);
    if (type == L"web" || type == L"website" || type == L"html") {
        selected = files.html;
    } else if (type == L"video") {
        for (const auto& candidate : files.media) {
            if (GetFileExtensionLower(candidate) != L".gif") { selected = candidate; break; }
        }
    } else if (type == L"gif") {
        for (const auto& candidate : files.media) if (GetFileExtensionLower(candidate) == L".gif") { selected = candidate; break; }
    } else if (type.empty()) {
        // Infer only when metadata has no declared project type. Explicit Wallpaper Engine
        // scene/application/image types stay visible but are never guessed as playable.
        if (!files.html.empty()) { selected = files.html; item.workshopType = L"web"; }
        else if (!files.media.empty()) { selected = files.media.front(); item.workshopType = GetFileExtensionLower(selected) == L".gif" ? L"gif" : L"video"; }
    }

    item.isCompatible = !selected.empty() && (IsSupportedWallpaperExtension(selected));
    if (item.isCompatible) {
        item.path = selected;
        item.kind = ClassifyWallpaperPath(selected);
        item.fileSize = GetFileSizeBytes(selected);
    } else {
        item.path = L"steam://workshop/" + id;
        item.kind = WallpaperKind::Video;
        item.fileSize = 0;
    }

    std::wstring preview = ResolvePreview(itemDir, meta.preview, files.preview);
    if (!preview.empty()) {
        VideoProbeInfo image = ProbeImagePreviewAndSaveThumbnail(preview);
        if (image.valid) {
            item.thumbPath = image.thumbPath;
            item.width = image.width;
            item.height = image.height;
            item.dominantColorRGB = image.dominantColorRGB;
        }
    }
    if (item.thumbPath.empty() && item.isCompatible && item.kind == WallpaperKind::Gif) {
        VideoProbeInfo probe = ProbeWallpaperAndSaveThumbnail(item.path);
        if (probe.valid) {
            item.thumbPath = probe.thumbPath;
            item.width = probe.width;
            item.height = probe.height;
            item.durationSec = probe.durationSec;
            item.dominantColorRGB = probe.dominantColorRGB;
        }
    }
    return item;
}

}  // namespace

SteamLibraryScanResult ScanInstalledSteamWorkshopLibrary() {
    SteamLibraryScanResult result;
    std::vector<std::wstring> steamRoots = FindSteamRoots();
    if (steamRoots.empty()) {
        result.status = L"Steam не найден. Проверьте установку Steam и повторите сканирование.";
        return result;
    }
    result.steamFound = true;

    std::unordered_set<std::wstring> seenIds;
    for (const auto& root : steamRoots) {
        fs::path steamApps = fs::path(root) / L"steamapps";
        fs::path workshopRoot = steamApps / L"workshop" / L"content" / L"431960";
        std::error_code ec;
        if (!fs::is_directory(workshopRoot, ec)) continue;
        result.collectionDiscovered = true;
        const std::wstring acfPath = (steamApps / L"workshop" / L"appworkshop_431960.acf").wstring();
        auto installedIds = ReadInstalledWorkshopIds(steamApps.wstring());
        const bool hasInstalledManifest = FileExists(acfPath);
        for (fs::directory_iterator it(workshopRoot, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (!it->is_directory(ec)) continue;
            std::wstring id = it->path().filename().wstring();
            if (id.empty() || !std::all_of(id.begin(), id.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) continue;
            if (hasInstalledManifest && installedIds.find(id) == installedIds.end()) continue;
            if (!seenIds.insert(id).second) continue;
            try {
                result.items.push_back(MakeWorkshopItem(it->path(), id));
            } catch (...) {
                LogWarn(L"Пропуск повреждённого элемента Steam Workshop #" + id);
            }
        }
    }

    std::sort(result.items.begin(), result.items.end(), [](const WallpaperItem& a, const WallpaperItem& b) {
        return ToLowerUnicode(a.title) < ToLowerUnicode(b.title);
    });
    size_t compatible = 0;
    for (const auto& item : result.items) if (item.isCompatible) ++compatible;
    if (!result.collectionDiscovered) {
        result.status = L"Steam найден, но папка локальной коллекции Workshop не обнаружена.";
    } else if (result.items.empty()) {
        result.status = L"Локальная коллекция Wallpaper Engine пуста. Онлайн-каталог Workshop здесь не отображается.";
    } else {
        result.status = L"Найдено " + std::to_wstring(result.items.size()) + L" локальных Workshop-обоев; " +
            std::to_wstring(compatible) + L" поддерживаются WallPulse. Онлайн-каталог не включён.";
    }
    return result;
}

}  // namespace wp
