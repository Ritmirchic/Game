// Native 64-bit Windows Uninstaller for DON'T LOOK BACK
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <wchar.h>

static void delete_dir_contents(const wchar_t* dir) {
    wchar_t pattern[MAX_PATH];
    wsprintfW(pattern, L"%s\\*", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        wchar_t full[MAX_PATH];
        wsprintfW(full, L"%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            delete_dir_contents(full);
            RemoveDirectoryW(full);
        } else {
            SetFileAttributesW(full, FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(full);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    const wchar_t* cmdLine = GetCommandLineW();
    bool silent = (cmdLine && (wcsstr(cmdLine, L" /S") || wcsstr(cmdLine, L" /SILENT")));
    if (!silent) {
        int res = MessageBoxW(
            NULL,
            L"Are you sure you want to completely uninstall DON'T LOOK BACK and remove its shortcuts?",
            L"DON'T LOOK BACK — Uninstaller",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
        );
        if (res != IDYES) return 0;
    }

    wchar_t selfPath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    wchar_t installDir[MAX_PATH] = {0};
    wcscpy(installDir, selfPath);
    wchar_t* slash = wcsrchr(installDir, L'\\');
    if (slash) *slash = 0;

    // 1. Delete main game files in install directory
    wchar_t fpath[MAX_PATH];
    wsprintfW(fpath, L"%s\\DONT_LOOK_BACK.exe", installDir);
    DeleteFileW(fpath);
    wsprintfW(fpath, L"%s\\README.txt", installDir);
    DeleteFileW(fpath);

    // 2. Remove Start Menu folder & shortcuts
    wchar_t programsDir[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, programsDir))) {
        wchar_t smFolder[MAX_PATH];
        wsprintfW(smFolder, L"%s\\DON'T LOOK BACK", programsDir);
        delete_dir_contents(smFolder);
        RemoveDirectoryW(smFolder);
    }

    // 3. Remove Desktop shortcut if present
    wchar_t desktopDir[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopDir))) {
        wchar_t lnk[MAX_PATH];
        wsprintfW(lnk, L"%s\\DON'T LOOK BACK.lnk", desktopDir);
        DeleteFileW(lnk);
    }

    // 4. Remove Windows Apps & Features Uninstall Registry entry
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\DontLookBack");

    if (!silent) {
        MessageBoxW(
            NULL,
            L"DON'T LOOK BACK has been uninstalled from your computer.",
            L"DON'T LOOK BACK — Uninstaller",
            MB_OK | MB_ICONINFORMATION
        );
    }

    // 5. Schedule self-removal of UNINSTALL.exe & empty install directory
    wchar_t cmdArgs[MAX_PATH * 3];
    wsprintfW(
        cmdArgs,
        L"/C ping 127.0.0.1 -n 2 > nul & del /F /Q \"%s\" & rmdir \"%s\"",
        selfPath, installDir
    );
    ShellExecuteW(NULL, L"open", L"cmd.exe", cmdArgs, L"C:\\", SW_HIDE);
    return 0;
}
