/* =========================================================================
   SPACE MINER — pure URL/path helpers used by the native launcher.
   Kept free of Win32 so the same code can be compiled and unit-tested on any
   host (see tools/testhttp.js).
   ========================================================================= */
#ifndef SPACE_MINER_HTTP_UTIL_H
#define SPACE_MINER_HTTP_UTIL_H

#include <string.h>
#include <stdio.h>

#if !defined(_WIN32)
#define _snprintf snprintf
#endif

/* native path separator used when file paths are composed */
#if defined(_WIN32)
#define SM_PATH_SEP '\\'
#else
#define SM_PATH_SEP '/'
#endif

static int sm_stricmp(const char *a, const char *b)
{
    for (;;) {
        int ca = (unsigned char)*a++, cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        if (!ca) return 0;
    }
}

/* Map a request target to a path relative to the game folder.
   Rejects traversal, NUL bytes, drive letters and encoded separators.
   Returns 1 on success; empty targets resolve to index.html, folders get a
   trailing index.html appended. */
static int url_to_rel(const char *url, char *out, size_t n)
{
    size_t o = 0, i = 0;
    int value;

    if (!url || !out || n < 32) return 0;
    while (url[i] == '/') i++;                       /* skip leading slash */
    for (; url[i] && url[i] != '?' && url[i] != '#' && o + 8 < n; i++) {
        unsigned char c = (unsigned char)url[i];
        if (c == '%' && url[i + 1] && url[i + 2]) {
            int hi = url[i + 1], lo = url[i + 2];
            if (hi >= '0' && hi <= '9') hi -= '0';
            else if (hi >= 'a' && hi <= 'f') hi = hi - 'a' + 10;
            else if (hi >= 'A' && hi <= 'F') hi = hi - 'A' + 10;
            else return 0;
            if (lo >= '0' && lo <= '9') lo -= '0';
            else if (lo >= 'a' && lo <= 'f') lo = lo - 'a' + 10;
            else if (lo >= 'A' && lo <= 'F') lo = lo - 'A' + 10;
            else return 0;
            value = hi * 16 + lo;
            if (value == 0 || value == '\\') return 0;  /* never NUL or separator */
            if (value < 32) return 0;                   /* no control characters */
            c = (unsigned char)value;
            i += 2;
        }
        if (c < 32) return 0;
        out[o++] = (char)c;
    }
    out[o] = 0;
    if (url[i] && url[i] != '?' && url[i] != '#') return 0;   /* too long */

    if (strstr(out, "..")) return 0;                  /* no traversal        */
    if (o >= 2 && out[1] == ':') return 0;            /* no drive letters    */
    if (o == 0) {
        strcpy(out, "index.html");
        return 1;
    }
    if (out[o - 1] == '/') {
        if (o + 12 >= n) return 0;
        strcat(out, "index.html");
    }
    return 1;
}

static const char *mime_for(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (!sm_stricmp(ext, ".html") || !sm_stricmp(ext, ".htm")) return "text/html; charset=utf-8";
    if (!sm_stricmp(ext, ".js") || !sm_stricmp(ext, ".mjs")) return "text/javascript; charset=utf-8";
    if (!sm_stricmp(ext, ".css")) return "text/css; charset=utf-8";
    if (!sm_stricmp(ext, ".json")) return "application/json; charset=utf-8";
    if (!sm_stricmp(ext, ".png")) return "image/png";
    if (!sm_stricmp(ext, ".jpg") || !sm_stricmp(ext, ".jpeg")) return "image/jpeg";
    if (!sm_stricmp(ext, ".ico")) return "image/x-icon";
    if (!sm_stricmp(ext, ".svg")) return "image/svg+xml";
    if (!sm_stricmp(ext, ".woff2")) return "font/woff2";
    if (!sm_stricmp(ext, ".txt")) return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

/* "C:\Games\SpaceMiner" + "game/index.html" -> "...\SpaceMiner\game\index.html" */
static void path_join(char *out, size_t n, const char *a, const char *b)
{
    size_t la = strlen(a), o;
    if (la >= n - 2) la = n - 3;
    memcpy(out, a, la);
    out[la] = 0;
    if (la && (out[la - 1] == '\\' || out[la - 1] == '/')) out[--la] = 0;
    out[la] = SM_PATH_SEP;
    o = la + 1;
    for (; *b && o + 2 < n; b++) {
        out[o++] = (*b == '/' || *b == '\\') ? SM_PATH_SEP : *b;
    }
    out[o] = 0;
}

#endif /* SPACE_MINER_HTTP_UTIL_H */
