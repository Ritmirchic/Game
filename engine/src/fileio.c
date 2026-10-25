#include "core.h"
#include <stdio.h>

static char g_asset_root[512] = "";

void file_set_asset_root(const char *root) {
    snprintf(g_asset_root, sizeof g_asset_root, "%s", root ? root : "");
}

const char *file_asset_root(void) { return g_asset_root; }

uint8_t *file_read_all(const char *path, size_t *size) {
    char full[1024];
    if (g_asset_root[0] && path[0] != '/')
        snprintf(full, sizeof full, "%s/%s", g_asset_root, path);
    else
        snprintf(full, sizeof full, "%s", path);
    FILE *f = fopen(full, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = 0;
    if (size) *size = rd;
    return buf;
}

int file_exists(const char *path) {
    char full[1024];
    if (g_asset_root[0] && path[0] != '/')
        snprintf(full, sizeof full, "%s/%s", g_asset_root, path);
    else
        snprintf(full, sizeof full, "%s", path);
    FILE *f = fopen(full, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

uint8_t *file_read_optional(const char *path, size_t *size) { return file_read_all(path, size); }
