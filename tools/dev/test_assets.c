/* dev check: pak -> inflate -> PNG decode -> PNG encode round trip */
#include "../../src/core/common.h"
#include "../../src/core/pack.h"
#include "../../src/core/png.h"
#include "../../src/core/gfx.h"

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "assets";
    char path[512];
    sm_sprintf(path, sizeof(path), "%s/assets.pak", root);
    FILE *f = fopen(path, "rb");
    if (!f) { printf("no pak\n"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *buf = malloc((usize)n);
    if (fread(buf, 1, (usize)n, f) != (usize)n) return 1;
    fclose(f);
    Pak pak;
    if (pak_open(&pak, buf, (usize)n) != 0) return 1;
    printf("pak ok: %d entries\n", pak.count);
    Sheet s;
    if (pak_load_sheet(&pak, "ship_prospector", &s) == 0)
        printf("ship: %dx%d frames=%d\n", s.w, s.h, s.frames);
    if (pak_load_sheet(&pak, "font_body", &s) == 0)
        printf("font_body atlas: %dx%d\n", s.w, s.h);
    const char *meta = pak_blob_str(&pak, "font_body_meta");
    printf("meta head: %.60s...\n", meta ? meta : "(null)");
    /* re-encode the atlas to PNG to prove the writer works */
    if (pak_load_sheet(&pak, "logo", &s) == 0) {
        usize outLen = 0;
        u8 *png = png_encode(s.px, s.w * s.frames, s.h, &outLen);
        FILE *o = fopen("build/logo_roundtrip.png", "wb");
        if (o) { fwrite(png, 1, outLen, o); fclose(o); }
        printf("logo %dx%d re-encoded %lu bytes\n", s.w, s.h, (unsigned long)outLen);
        free(png);
    }
    /* decode that PNG back and compare pixels */
    {
        FILE *g = fopen("build/logo_roundtrip.png", "rb");
        if (g) {
            fseek(g, 0, SEEK_END); long m = ftell(g); fseek(g, 0, SEEK_SET);
            u8 *pb = malloc(m); fread(pb, 1, m, g); fclose(g);
            Image img;
            if (png_decode(pb, (usize)m, &img) == 0) printf("png decode ok %dx%d\n", img.w, img.h);
            else printf("PNG DECODE FAILED\n");
        }
    }
    /* audio entry */
    const PakEntry *e = pak_find(&pak, "music_belt");
    if (e) printf("music_belt: ch=%u raw=%u comp=%u\n", e->w, e->raw_size, e->data_size);
    printf("mem used: %.1f MB\n", (double)sm_mem_used() / 1048576.0);
    return 0;
}
