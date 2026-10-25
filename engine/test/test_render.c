// Renderer smoke test: builds a room, renders frames, writes PNGs + timings.
#include "../src/render.h"

int png_write_rgba(const char *path, const uint8_t *px, int w, int h);
extern uint8_t *png_load_file(const char *path, int *w, int *h);

static Tex *make_noise_tex(int size, Vec3 base, float contrast, int seed, const char *name) {
    Tex *t = tex_create(size, size, name);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float u = (float)x / size, v = (float)y / size;
            float n = fbm2(u * 8.f, v * 8.f, 5, seed);
            float n2 = fbm2(u * 64.f, v * 64.f, 3, seed + 7);
            float val = 0.65f + (n - 0.5f) * contrast + (n2 - 0.5f) * contrast * 0.5f;
            uint8_t *p = t->px + ((size_t)y * size + x) * 4;
            p[0] = (uint8_t)clampi((int)(base.x * val * 255.f), 0, 255);
            p[1] = (uint8_t)clampi((int)(base.y * val * 255.f), 0, 255);
            p[2] = (uint8_t)clampi((int)(base.z * val * 255.f), 0, 255);
            p[3] = 255;
        }
    return t;
}

int main(int argc, char **argv) {
    file_set_asset_root("assets");
    Renderer rd;
    int W = 960, H = 540;
    if (argc > 1) W = atoi(argv[1]);
    if (argc > 2) H = atoi(argv[2]);
    rd_init(&rd, W, H);

    Tex *concrete = make_noise_tex(256, v3(0.42f, 0.42f, 0.44f), 0.55f, 3, "concrete");
    Tex *tile = make_noise_tex(256, v3(0.62f, 0.60f, 0.55f), 0.4f, 9, "tile");
    Material mats[4];
    mats[0] = mat_make("floor", tile, NULL, v3(1, 1, 1), 0.55f, 0.f, 0.f, MF_NONE);
    mats[0].uvscale = 0.5f;
    mats[1] = mat_make("wall", concrete, NULL, v3(1, 1, 1), 0.9f, 0.f, 0.f, MF_NONE);
    mats[1].uvscale = 0.4f;
    mats[2] = mat_make("metal", NULL, NULL, v3(0.35f, 0.37f, 0.40f), 0.35f, 0.6f, 0.f, MF_METAL);
    mats[3] = mat_make("lamp", NULL, NULL, v3(1.0f, 0.87f, 0.65f), 0.5f, 0.f, 3.2f, MF_EMISSIVE);

    Mesh *floor_m = mesh_new(64, 64); floor_m->mat = 0;
    mesh_add_boxf(floor_m, v3(-12, -0.2f, -12), v3(12, 0, 12), 255, 255, 255, 1.f);

    Mesh *walls = mesh_new(512, 512); walls->mat = 1;
    mesh_add_boxf(walls, v3(-12, 0, -12.4f), v3(12, 4, -12), 255, 255, 255, 1.f);
    mesh_add_boxf(walls, v3(-12, 0, 12), v3(12, 4, 12.4f), 255, 255, 255, 1.f);
    mesh_add_boxf(walls, v3(-12.4f, 0, -12), v3(-12, 4, 12), 255, 255, 255, 1.f);
    mesh_add_boxf(walls, v3(12, 0, -12), v3(12.4f, 4, 12), 200, 200, 205, 1.f);
    mesh_add_boxf(walls, v3(-12, 4, -12), v3(12, 4.3f, 12), 60, 60, 65, 1.f);

    Mesh *props = mesh_new(512, 512); props->mat = 2;
    mesh_add_box(props, v3(-4, 0.9f, -2), v3(1.5f, 0.9f, 0.6f), 150, 150, 160, 1.f);  // shelf
    mesh_add_box(props, v3(2, 1.2f, 3), v3(1.2f, 1.2f, 1.2f), 140, 140, 150, 1.f);
    mesh_add_box(props, v3(5, 0.6f, -5), v3(0.6f, 0.6f, 2.5f), 120, 120, 130, 1.f);

    Mesh *lamp = mesh_new(32, 32); lamp->mat = 3;
    mesh_add_box(lamp, v3(0, 3.6f, 0), v3(0.6f, 0.08f, 1.4f), 255, 255, 255, 1.f);

    Camera cam;
    cam.pos = v3(0, 1.68f, 8);
    cam.yaw = 0.f;
    cam.pitch = -0.06f;
    cam.roll = 0.f;
    cam.fov = DEG2RAD(74.f);
    cam.sway_x = cam.sway_y = 0.f;

    Environment env;
    memset(&env, 0, sizeof env);
    env.ambient_sky = v3(0.10f, 0.12f, 0.18f);
    env.ambient_ground = v3(0.06f, 0.055f, 0.05f);
    env.ambient_intensity = 1.f;
    env.fog_color = v3(0.075f, 0.08f, 0.10f);
    env.fog_density = 0.018f;
    env.exposure = 1.25f;
    env.bloom = 0.55f;
    env.vignette = 0.42f;
    env.grain = 0.035f;
    env.saturation = 1.04f;
    env.grade_shadow = v3(0.86f, 0.94f, 1.12f);
    env.grade_high = v3(1.08f, 1.0f, 0.94f);
    env.sky_moon_intensity = 1.f;
    env.sky_tint = v3(1, 1, 1);

    // static baked light for the room (fake: brighter near lamp)
    Mesh *all[] = {floor_m, walls, props, lamp};
    for (int mi = 0; mi < 4; mi++) {
        Mesh *m = all[mi];
        for (uint32_t i = 0; i < m->vert_used; i++) {
            Vec3 p = m->pos[i];
            float d = vdist(p, v3(0, 3.4f, 0));
            float l = 1.f / (1.f + d * d * 0.22f);
            float v = 0.10f + 0.85f * l;
            uint8_t *lit = &m->lit[i * 3];
            if (mi == 3) { v = 1.f; }
            lit[0] = (uint8_t)(clampf(v * 255.f, 0, 255) * 1.0f);
            lit[1] = (uint8_t)(clampf(v * 245.f, 0, 255));
            lit[2] = (uint8_t)(clampf(v * 225.f, 0, 255));
        }
    }

    Mat4 id = mat4_id();
    for (int frame = 0; frame < 3; frame++) {
        cam.yaw = -0.25f + frame * 0.25f;
        env.time = (float)frame * 0.5f;
        rd_begin(&rd, &cam, &env);
        Light fl;
        fl.pos = cam.pos;
        fl.dir = dir_from_yaw_pitch(cam.yaw, cam.pitch);
        fl.color = v3(1.0f, 0.95f, 0.85f);
        fl.intensity = 3.2f;
        fl.range = 22.f;
        fl.cos_inner = cosf(DEG2RAD(14.f));
        fl.cos_outer = cosf(DEG2RAD(26.f));
        fl.type = 1;
        fl.shadows = 1;
        rd_add_light(&rd, fl);
        Light lampL;
        lampL.pos = v3(0, 3.4f, 0);
        lampL.color = v3(1.0f, 0.86f, 0.62f);
        lampL.intensity = 1.6f;
        lampL.range = 14.f;
        lampL.type = 0;
        lampL.shadows = 1;
        lampL.dir = v3(0, -1, 0);
        rd_add_light(&rd, lampL);

        rd_draw_mesh(&rd, floor_m, &id, mats, 4);
        rd_draw_mesh(&rd, walls, &id, mats, 4);
        rd_draw_mesh(&rd, props, &id, mats, 4);
        rd_draw_mesh(&rd, lamp, &id, mats, 4);
        rd_lighting(&rd);
        // dust motes / test billboards
        Rng rng;
        rng_seed(&rng, 7);
        for (int i = 0; i < 60; i++) {
            Vec3 p = v3(rng_range(&rng, -6, 6), rng_range(&rng, 0.4f, 3.2f), rng_range(&rng, -8, 6));
            rd_draw_billboard(&rd, p, 0.06f, 0.06f, rd.soft_particle, rgba8(230, 225, 210, 90), 1, 0.f);
        }
        rd_draw_beam(&rd, cam.pos, vmad(cam.pos, fl.dir, 9.f), 1.2f, v3(1.0f, 0.9f, 0.7f), 1.6f);
        rd_post(&rd);

        char path[128];
        snprintf(path, sizeof path, "shots/test_%d.png", frame);
        // convert to rgba8 buffer
        uint8_t *out = (uint8_t *)malloc((size_t)rd.w * rd.h * 4);
        for (size_t i = 0; i < (size_t)rd.w * rd.h; i++) {
            out[i * 4] = (uint8_t)c_r(rd.color[i]);
            out[i * 4 + 1] = (uint8_t)c_g(rd.color[i]);
            out[i * 4 + 2] = (uint8_t)c_b(rd.color[i]);
            out[i * 4 + 3] = 255;
        }
        png_write_rgba(path, out, rd.w, rd.h);
        free(out);
        LOGI("frame %d: tris=%u culled=%u light=%.1fms sky=%.1fms post=%.1fms", frame, rd.tris_drawn, rd.tris_culled, rd.light_ms, rd.sky_ms, rd.post_ms);
    }
    LOGI("render test done");
    return 0;
}
