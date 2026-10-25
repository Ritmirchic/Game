// Rigged character models: five zombie types + survivor player + weapons.
// Rigid-part skeletal rig (bone world matrix per part) — cheap enough for the
// software rasteriser while still giving full articulated animation.
#include "models.h"
#include <string.h>

// ---------------------------------------------------------------- bone ids
enum {
    B_ROOT = 0, B_PELVIS, B_SPINE, B_CHEST, B_NECK, B_HEAD, B_JAW,
    B_SHOULDER_L, B_ARM_L, B_FORE_L, B_HAND_L,
    B_SHOULDER_R, B_ARM_R, B_FORE_R, B_HAND_R,
    B_HIP_L, B_THIGH_L, B_SHIN_L, B_FOOT_L,
    B_HIP_R, B_THIGH_R, B_SHIN_R, B_FOOT_R,
    B_BONE_COUNT
};

static Skeleton *base_skeleton(float scale, int hunched) {
    Skeleton *s = skeleton_new(B_BONE_COUNT);
    skeleton_bone(s, B_ROOT, -1, v3(0, 0, 0), "root");
    skeleton_bone(s, B_PELVIS, B_ROOT, v3(0, 0.92f * scale, 0), "pelvis");
    skeleton_bone(s, B_SPINE, B_PELVIS, v3(0, 0.20f * scale, 0), "spine");
    skeleton_bone(s, B_CHEST, B_SPINE, v3(0, 0.22f * scale, 0), "chest");
    skeleton_bone(s, B_NECK, B_CHEST, v3(0, 0.24f * scale, hunched ? 0.04f : 0.0f), "neck");
    skeleton_bone(s, B_HEAD, B_NECK, v3(0, 0.10f * scale, hunched ? 0.05f : 0), "head");
    skeleton_bone(s, B_JAW, B_HEAD, v3(0, 0.02f * scale, 0.07f * scale), "jaw");
    skeleton_bone(s, B_SHOULDER_L, B_CHEST, v3(0.19f * scale, 0.16f * scale, 0), "shoulderL");
    skeleton_bone(s, B_ARM_L, B_SHOULDER_L, v3(0.05f * scale, -0.02f * scale, 0), "armL");
    skeleton_bone(s, B_FORE_L, B_ARM_L, v3(0, -0.28f * scale, 0), "foreL");
    skeleton_bone(s, B_HAND_L, B_FORE_L, v3(0, -0.26f * scale, 0), "handL");
    skeleton_bone(s, B_SHOULDER_R, B_CHEST, v3(-0.19f * scale, 0.16f * scale, 0), "shoulderR");
    skeleton_bone(s, B_ARM_R, B_SHOULDER_R, v3(-0.05f * scale, -0.02f * scale, 0), "armR");
    skeleton_bone(s, B_FORE_R, B_ARM_R, v3(0, -0.28f * scale, 0), "foreR");
    skeleton_bone(s, B_HAND_R, B_FORE_R, v3(0, -0.26f * scale, 0), "handR");
    skeleton_bone(s, B_HIP_L, B_PELVIS, v3(0.10f * scale, -0.04f * scale, 0), "hipL");
    skeleton_bone(s, B_THIGH_L, B_HIP_L, v3(0, -0.02f * scale, 0), "thighL");
    skeleton_bone(s, B_SHIN_L, B_THIGH_L, v3(0, -0.44f * scale, 0), "shinL");
    skeleton_bone(s, B_FOOT_L, B_SHIN_L, v3(0, -0.42f * scale, 0), "footL");
    skeleton_bone(s, B_HIP_R, B_PELVIS, v3(-0.10f * scale, -0.04f * scale, 0), "hipR");
    skeleton_bone(s, B_THIGH_R, B_HIP_R, v3(0, -0.02f * scale, 0), "thighR");
    skeleton_bone(s, B_SHIN_R, B_THIGH_R, v3(0, -0.44f * scale, 0), "shinR");
    skeleton_bone(s, B_FOOT_R, B_SHIN_R, v3(0, -0.42f * scale, 0), "footR");
    return s;
}

// ---------------------------------------------------------------- clip helper
static AnimClip *make_clip(const char *name, float dur, int loop, int nkeys, const float (*keys)[B_BONE_COUNT * 3]) {
    AnimClip *c = clip_make(name, dur, loop);
    for (int k = 0; k < nkeys; k++) {
        Vec3 e[MAX_BONES];
        for (int b = 0; b < MAX_BONES; b++) e[b] = v3(0, 0, 0);
        for (int b = 0; b < B_BONE_COUNT; b++) e[b] = e3(keys[k][b * 3 + 0], keys[k][b * 3 + 1], keys[k][b * 3 + 2]);
        float t = nkeys > 1 ? (float)k / (float)(nkeys - 1) * dur : 0.f;
        clip_key(c, t, e);
    }
    return c;
}

// Create the four standard locomotion / reaction clips for a humanoid.
static void make_zombie_clips(CharModel *cm, float limp, float arm_reach, int crawler) {
    // 2-key poses: [bone][x,y,z] in degrees
    static float idle[2][B_BONE_COUNT * 3];
    static float walk_a[B_BONE_COUNT * 3], walk_b[B_BONE_COUNT * 3];
    static float run_a[B_BONE_COUNT * 3], run_b[B_BONE_COUNT * 3];

    memset(idle, 0, sizeof idle);
    // idle: slumped, arms hanging slightly forward
    idle[0][B_SPINE * 3 + 0] = 6 * limp;
    idle[0][B_CHEST * 3 + 0] = 4 * limp;
    idle[0][B_HEAD * 3 + 0] = -6 * limp;
    idle[0][B_NECK * 3 + 0] = -4;
    idle[0][B_ARM_L * 3 + 0] = -12 * limp;
    idle[0][B_ARM_R * 3 + 0] = -12 * limp;
    idle[0][B_FORE_L * 3 + 0] = -20 * limp;
    idle[0][B_FORE_R * 3 + 0] = -20 * limp;
    idle[0][B_ARM_L * 3 + 2] = 4;
    idle[0][B_ARM_R * 3 + 2] = -4;
    idle[1][B_SPINE * 3 + 0] = 8 * limp;
    idle[1][B_CHEST * 3 + 0] = 5 * limp;
    idle[1][B_HEAD * 3 + 0] = -8 * limp;
    idle[1][B_NECK * 3 + 0] = -5;
    idle[1][B_ARM_L * 3 + 0] = -16 * limp;
    idle[1][B_ARM_R * 3 + 0] = -8 * limp;
    idle[1][B_FORE_L * 3 + 0] = -24 * limp;
    idle[1][B_FORE_R * 3 + 0] = -16 * limp;
    idle[1][B_SHOULDER_L * 3 + 2] = 3;
    idle[1][B_SHOULDER_R * 3 + 2] = -3;

    memset(walk_a, 0, sizeof walk_a);
    memset(walk_b, 0, sizeof walk_b);
    walk_a[B_THIGH_L * 3 + 0] = 22;
    walk_a[B_THIGH_R * 3 + 0] = -20;
    walk_a[B_SHIN_L * 3 + 0] = -10;
    walk_a[B_SHIN_R * 3 + 0] = 24;
    walk_a[B_ARM_L * 3 + 0] = -16;
    walk_a[B_ARM_R * 3 + 0] = -8;
    walk_a[B_SPINE * 3 + 0] = 5 * limp;
    walk_a[B_CHEST * 3 + 1] = 5;
    walk_a[B_HEAD * 3 + 1] = -4;
    walk_a[B_FORE_L * 3 + 0] = -30 * arm_reach;
    walk_a[B_FORE_R * 3 + 0] = -26 * arm_reach;
    walk_b[B_THIGH_L * 3 + 0] = -20;
    walk_b[B_THIGH_R * 3 + 0] = 22;
    walk_b[B_SHIN_L * 3 + 0] = 24;
    walk_b[B_SHIN_R * 3 + 0] = -10;
    walk_b[B_ARM_L * 3 + 0] = -8;
    walk_b[B_ARM_R * 3 + 0] = -16;
    walk_b[B_SPINE * 3 + 0] = 5 * limp;
    walk_b[B_CHEST * 3 + 1] = -5;
    walk_b[B_HEAD * 3 + 1] = 4;
    walk_b[B_FORE_L * 3 + 0] = -26 * arm_reach;
    walk_b[B_FORE_R * 3 + 0] = -30 * arm_reach;

    memcpy(run_a, walk_a, sizeof walk_a);
    memcpy(run_b, walk_b, sizeof walk_b);
    run_a[B_THIGH_L * 3 + 0] = 40;
    run_a[B_THIGH_R * 3 + 0] = -34;
    run_a[B_SHIN_L * 3 + 0] = -30;
    run_a[B_SHIN_R * 3 + 0] = 52;
    run_a[B_ARM_L * 3 + 0] = -46;
    run_a[B_ARM_R * 3 + 0] = 24;
    run_a[B_FORE_L * 3 + 0] = -60;
    run_a[B_FORE_R * 3 + 0] = -40;
    run_a[B_SPINE * 3 + 0] = 12;
    run_b[B_THIGH_L * 3 + 0] = -34;
    run_b[B_THIGH_R * 3 + 0] = 40;
    run_b[B_SHIN_L * 3 + 0] = 52;
    run_b[B_SHIN_R * 3 + 0] = -30;
    run_b[B_ARM_L * 3 + 0] = 24;
    run_b[B_ARM_R * 3 + 0] = -46;
    run_b[B_FORE_L * 3 + 0] = -40;
    run_b[B_FORE_R * 3 + 0] = -60;
    run_b[B_SPINE * 3 + 0] = 12;

    const float(*idle_keys)[B_BONE_COUNT * 3] = idle;
    cm->clip_idle = make_clip("idle", 2.2f, 1, 2, idle_keys);
    float walk2[2][B_BONE_COUNT * 3];
    memcpy(walk2[0], walk_a, sizeof walk_a);
    memcpy(walk2[1], walk_b, sizeof walk_b);
    cm->clip_walk = make_clip("walk", 1.15f, 1, 2, (const float(*)[B_BONE_COUNT * 3])walk2);
    float run2[2][B_BONE_COUNT * 3];
    memcpy(run2[0], run_a, sizeof run_a);
    memcpy(run2[1], run_b, sizeof run_b);
    cm->clip_run = make_clip("run", 0.62f, 1, 2, (const float(*)[B_BONE_COUNT * 3])run2);

    // attack: both arms swipe down
    float atk[3][B_BONE_COUNT * 3];
    memset(atk, 0, sizeof atk);
    atk[0][B_ARM_L * 3 + 0] = -100;
    atk[0][B_ARM_R * 3 + 0] = -95;
    atk[0][B_FORE_L * 3 + 0] = -40;
    atk[0][B_FORE_R * 3 + 0] = -40;
    atk[0][B_SPINE * 3 + 0] = -6;
    atk[1][B_ARM_L * 3 + 0] = -140;
    atk[1][B_ARM_R * 3 + 0] = -135;
    atk[1][B_FORE_L * 3 + 0] = -20;
    atk[1][B_FORE_R * 3 + 0] = -20;
    atk[1][B_SPINE * 3 + 0] = 16;
    atk[1][B_HEAD * 3 + 0] = 8;
    atk[2][B_ARM_L * 3 + 0] = -60;
    atk[2][B_ARM_R * 3 + 0] = -55;
    atk[2][B_FORE_L * 3 + 0] = -50;
    atk[2][B_FORE_R * 3 + 0] = -50;
    atk[2][B_SPINE * 3 + 0] = 8;
    cm->clip_attack = make_clip("attack", 0.85f, 0, 3, (const float(*)[B_BONE_COUNT * 3])atk);

    // hit reaction: recoil back
    float hit[2][B_BONE_COUNT * 3];
    memset(hit, 0, sizeof hit);
    hit[0][B_SPINE * 3 + 0] = -22;
    hit[0][B_CHEST * 3 + 0] = -12;
    hit[0][B_HEAD * 3 + 0] = -22;
    hit[0][B_ARM_L * 3 + 0] = 30;
    hit[0][B_ARM_R * 3 + 0] = 25;
    hit[1][B_SPINE * 3 + 0] = 6;
    cm->clip_hit = make_clip("hit", 0.34f, 0, 2, (const float(*)[B_BONE_COUNT * 3])hit);

    // death: crumple
    float dth[3][B_BONE_COUNT * 3];
    memset(dth, 0, sizeof dth);
    dth[0][B_SPINE * 3 + 0] = 20;
    dth[0][B_HEAD * 3 + 0] = 15;
    dth[1][B_SPINE * 3 + 0] = 70;
    dth[1][B_THIGH_L * 3 + 0] = -60;
    dth[1][B_THIGH_R * 3 + 0] = -50;
    dth[1][B_SHIN_L * 3 + 0] = 80;
    dth[1][B_SHIN_R * 3 + 0] = 70;
    dth[1][B_ARM_L * 3 + 0] = 20;
    dth[1][B_ARM_R * 3 + 0] = 15;
    dth[2][B_SPINE * 3 + 0] = 88;
    dth[2][B_CHEST * 3 + 0] = 20;
    dth[2][B_HEAD * 3 + 0] = 25;
    dth[2][B_THIGH_L * 3 + 0] = -85;
    dth[2][B_THIGH_R * 3 + 0] = -75;
    dth[2][B_SHIN_L * 3 + 0] = 100;
    dth[2][B_SHIN_R * 3 + 0] = 95;
    dth[2][B_ARM_L * 3 + 0] = 55;
    dth[2][B_ARM_R * 3 + 0] = 45;
    cm->clip_death = make_clip("death", 1.0f, 0, 3, (const float(*)[B_BONE_COUNT * 3])dth);

    // scream: head back, arms wide
    float scr[3][B_BONE_COUNT * 3];
    memset(scr, 0, sizeof scr);
    scr[0][B_HEAD * 3 + 0] = -20;
    scr[0][B_ARM_L * 3 + 2] = 20;
    scr[0][B_ARM_R * 3 + 2] = -20;
    scr[1][B_HEAD * 3 + 0] = -46;
    scr[1][B_JAW * 3 + 0] = 35;
    scr[1][B_ARM_L * 3 + 2] = 55;
    scr[1][B_ARM_R * 3 + 2] = -55;
    scr[1][B_ARM_L * 3 + 0] = -60;
    scr[1][B_ARM_R * 3 + 0] = -60;
    scr[1][B_SPINE * 3 + 0] = -16;
    scr[2][B_HEAD * 3 + 0] = -10;
    scr[2][B_SPINE * 3 + 0] = 6;
    cm->clip_scream = make_clip("scream", 1.5f, 0, 3, (const float(*)[B_BONE_COUNT * 3])scr);

    if (crawler) {
        float cr[2][B_BONE_COUNT * 3];
        memset(cr, 0, sizeof cr);
        for (int k = 0; k < 2; k++) {
            cr[k][B_PELVIS * 3 + 0] = 78;
            cr[k][B_SPINE * 3 + 0] = -20;
            cr[k][B_CHEST * 3 + 0] = -14;
            cr[k][B_NECK * 3 + 0] = -40;
            cr[k][B_HEAD * 3 + 0] = -30;
            cr[k][B_ARM_L * 3 + 0] = -120 + k * 30;
            cr[k][B_ARM_R * 3 + 0] = -120 + (1 - k) * 30;
            cr[k][B_FORE_L * 3 + 0] = -30;
            cr[k][B_FORE_R * 3 + 0] = -30;
            cr[k][B_THIGH_L * 3 + 0] = 20 - k * 24;
            cr[k][B_THIGH_R * 3 + 0] = 20 - (1 - k) * 24;
            cr[k][B_SHIN_L * 3 + 0] = 30;
            cr[k][B_SHIN_R * 3 + 0] = 30;
        }
        cm->clip_crawl = make_clip("crawl", 1.3f, 1, 2, (const float(*)[B_BONE_COUNT * 3])cr);
    }
}

// ---------------------------------------------------------------- part builders
static CharPart *add_part(CharModel *cm, int mat, int bone, int cap) {
    if (cm->nparts >= 26) return NULL;
    Mesh *ms = mesh_new(cap, cap * 2);
    ms->mat = mat;
    ms->dynamic = 1;
    CharPart *p = &cm->parts[cm->nparts++];
    p->mesh = ms;
    p->bone = bone;
    p->jiggle = 0;
    return p;
}

void char_draw(Renderer *r, CharModel *cm, const Material *mats, const Pose *pose, Mat4 root, float scale) {
    Mat4 bone[MAX_BONES];
    anim_world_mats(cm->sk, pose, bone);
    Mat4 s = mat4_scale(v3s(scale));
    for (int i = 0; i < cm->nparts; i++) {
        CharPart *p = &cm->parts[i];
        if (!p->mesh || !p->mesh->tri_used) continue;
        Mat4 wm = mat4_mul(root, mat4_mul(bone[p->bone], s));
        rd_draw_mesh(r, p->mesh, &wm, mats, MAT_COUNT);
    }
}
void char_free(CharModel *cm) {
    if (!cm) return;
    for (int i = 0; i < cm->nparts; i++) mesh_free(cm->parts[i].mesh);
    AnimClip *clips[9] = {cm->clip_idle, cm->clip_walk, cm->clip_run, cm->clip_attack, cm->clip_hit,
                          cm->clip_death, cm->clip_scream, cm->clip_special, cm->clip_crawl};
    for (int i = 0; i < 9; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++)
            if (clips[j] == clips[i]) dup = 1;
        if (!dup) clip_free(clips[i]);
    }
    if (cm->sk) free(cm->sk);
    free(cm);
}

CharModel *char_zombie(int type, int variant, Assets *as) {
    (void)as;
    CharModel *cm = (CharModel *)calloc(1, sizeof(CharModel));
    cm->type = type;
    float scale = 1.0f;
    int crawler = 0;
    int skin_mat = MAT_SKIN_ZOMBIE;
    int shirt_mat = MAT_SHIRT_BLUE;
    switch (type) {
        case ZB_WALKER: scale = 1.0f; break;
        case ZB_RUNNER: scale = 0.97f; break;
        case ZB_CRAWLER: scale = 1.0f; crawler = 1; break;
        case ZB_BRUTE: scale = 1.32f; skin_mat = MAT_SKIN_BRUTE; break;
        case ZB_SCREAMER: scale = 0.94f; skin_mat = MAT_SKIN_PALE; break;
        case ZB_MINIBOSS: scale = 1.7f; skin_mat = MAT_SKIN_BRUTE; break;
        case ZB_BOSS: scale = 2.2f; skin_mat = MAT_SKIN_BRUTE; break;
    }
    // clothing variation
    if (variant == 0) shirt_mat = MAT_SHIRT_BLUE;
    else if (variant == 1) shirt_mat = MAT_SHIRT_WHITE;
    else if (variant == 2) shirt_mat = MAT_SHIRT_BROWN;
    else shirt_mat = MAT_HIVIS;
    if (type == ZB_BRUTE || type == ZB_MINIBOSS || type == ZB_BOSS) shirt_mat = MAT_LEATHER;
    if (type == ZB_SCREAMER) shirt_mat = MAT_SCRUBS;

    cm->sk = base_skeleton(scale, type == ZB_WALKER || type == ZB_CRAWLER);
    float s = scale;
    int detail = (type == ZB_BOSS || type == ZB_MINIBOSS) ? 14 : (type == ZB_BRUTE ? 12 : 10);

    // ---- pelvis + torso
    CharPart *p = add_part(cm, variant % 2 ? MAT_DENIM : MAT_SHIRT_BROWN, B_PELVIS, 400);
    if (p) {
        mesh_ellipsoid(p->mesh, v3(0, 0.0f, 0), v3(0.16f * s, 0.13f * s, 0.11f * s), detail, 6, 255, 255, 255, 1.2f);
        mesh_cyl_axis(p->mesh, v3(-0.09f * s, -0.06f * s, 0), v3(0.09f * s, -0.06f * s, 0), 0.10f * s, 0.10f * s, detail, 255, 255, 255, 1.2f, 1);
    }
    p = add_part(cm, shirt_mat, B_SPINE, 600);
    if (p) {
        mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, 0.24f * s, 0.01f), 0.17f * s, 0.19f * s, detail, 255, 255, 255, 1.1f, 0);
        // torn shirt edges
        mesh_cyl_axis(p->mesh, v3(0, -0.02f * s, 0), v3(0, 0.02f * s, 0), 0.175f * s, 0.175f * s, detail, 255, 255, 255, 1.1f, 0);
    }
    p = add_part(cm, shirt_mat, B_CHEST, 700);
    if (p) {
        mesh_ellipsoid(p->mesh, v3(0, 0.10f * s, 0), v3(0.21f * s, 0.17f * s, 0.14f * s), detail, 7, 255, 255, 255, 1.0f);
        // collarbone / neck base skin
        mesh_cyl_axis(p->mesh, v3(0, 0.18f * s, 0), v3(0, 0.24f * s, 0), 0.07f * s, 0.055f * s, detail, 230, 230, 230, 1.2f, 0);
    }
    // ---- neck & head
    p = add_part(cm, skin_mat, B_NECK, 200);
    if (p) mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, 0.11f * s, 0), 0.052f * s, 0.048f * s, detail, 255, 255, 255, 1.4f, 0);
    p = add_part(cm, skin_mat, B_HEAD, 700);
    if (p) {
        mesh_ellipsoid(p->mesh, v3(0, 0.085f * s, 0.01f * s), v3(0.093f * s, 0.115f * s, 0.10f * s), detail + 2, 8, 255, 255, 255, 1.3f);
        // brow ridge, nose
        mesh_ellipsoid(p->mesh, v3(0, 0.11f * s, 0.085f * s), v3(0.075f * s, 0.022f * s, 0.025f * s), 8, 4, 255, 255, 255, 1.6f);
        mesh_cyl_axis(p->mesh, v3(0, 0.065f * s, 0.075f * s), v3(0, 0.045f * s, 0.105f * s), 0.018f * s, 0.012f * s, 6, 255, 255, 255, 2.f, 1);
        // cheeks / jaw
        mesh_ellipsoid(p->mesh, v3(0, 0.03f * s, 0.05f * s), v3(0.075f * s, 0.035f * s, 0.06f * s), 8, 4, 255, 255, 255, 1.6f);
    }
    // hair / scalp
    p = add_part(cm, MAT_HAIR, B_HEAD, 300);
    if (p && type != ZB_BRUTE && type != ZB_BOSS) {
        mesh_ellipsoid(p->mesh, v3(0, 0.13f * s, -0.01f * s), v3(0.098f * s, 0.075f * s, 0.103f * s), detail, 6, 255, 255, 255, 1.4f);
    }
    // jaw
    p = add_part(cm, skin_mat, B_JAW, 200);
    if (p) {
        mesh_ellipsoid(p->mesh, v3(0, -0.02f * s, 0.01f * s), v3(0.058f * s, 0.032f * s, 0.055f * s), 10, 5, 255, 255, 255, 1.6f);
    }
    // eyes (dark sockets + glow for bosses)
    p = add_part(cm, type == ZB_BOSS || type == ZB_MINIBOSS ? MAT_EMISSIVE_RED : MAT_RUBBER, B_HEAD, 200);
    if (p) {
        mesh_sphere(p->mesh, v3(0.035f * s, 0.085f * s, 0.086f * s), 0.016f * s, 8, 6, 255, 255, 255, 2.f);
        mesh_sphere(p->mesh, v3(-0.035f * s, 0.085f * s, 0.086f * s), 0.016f * s, 8, 6, 255, 255, 255, 2.f);
    }
    // teeth
    p = add_part(cm, MAT_STEEL, B_JAW, 120);
    if (p) {
        for (int i = 0; i < 5; i++) {
            float x = -0.03f + i * 0.015f;
            mesh_cyl_axis(p->mesh, v3(x * s, 0.0f, 0.05f * s), v3(x * s, 0.012f * s, 0.055f * s), 0.004f * s, 0.002f * s, 4, 255, 255, 255, 3.f, 1);
        }
    }
    // ---- arms
    for (int side = 0; side < 2; side++) {
        int armBone = side == 0 ? B_ARM_L : B_ARM_R;
        int foreBone = side == 0 ? B_FORE_L : B_FORE_R;
        int handBone = side == 0 ? B_HAND_L : B_HAND_R;
        float sx = side == 0 ? 1.f : -1.f;
        p = add_part(cm, shirt_mat, armBone, 400);
        if (p) {
            mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, -0.27f * s, 0), 0.052f * s, 0.042f * s, detail, 255, 255, 255, 1.6f, 0);
            mesh_ellipsoid(p->mesh, v3(sx * 0.01f * s, 0.01f * s, 0), v3(0.062f * s, 0.065f * s, 0.062f * s), 10, 5, 255, 255, 255, 1.6f);
        }
        p = add_part(cm, skin_mat, foreBone, 400);
        if (p) {
            mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, -0.25f * s, 0), 0.042f * s, 0.033f * s, detail, 255, 255, 255, 1.8f, 0);
            mesh_ellipsoid(p->mesh, v3(0, -0.01f * s, 0), v3(0.045f * s, 0.05f * s, 0.045f * s), 8, 4, 255, 255, 255, 1.8f);
        }
        p = add_part(cm, skin_mat, handBone, 300);
        if (p) {
            mesh_ellipsoid(p->mesh, v3(0, -0.04f * s, 0.01f * s), v3(0.035f * s, 0.055f * s, 0.022f * s), 8, 5, 255, 255, 255, 2.f);
            for (int f = 0; f < 4; f++) {
                float fx = (-0.022f + f * 0.0145f) * s;
                mesh_cyl_axis(p->mesh, v3(fx, -0.075f * s, 0.012f * s), v3(fx, -0.115f * s, 0.03f * s), 0.0075f * s, 0.005f * s, 4, 255, 255, 255, 3.f, 1);
            }
            mesh_cyl_axis(p->mesh, v3(0.03f * s, -0.05f * s, 0.02f * s), v3(0.045f * s, -0.085f * s, 0.045f * s), 0.008f * s, 0.006f * s, 4, 255, 255, 255, 3.f, 1);
        }
    }
    // ---- legs
    for (int side = 0; side < 2; side++) {
        int thighBone = side == 0 ? B_THIGH_L : B_THIGH_R;
        int shinBone = side == 0 ? B_SHIN_L : B_SHIN_R;
        int footBone = side == 0 ? B_FOOT_L : B_FOOT_R;
        p = add_part(cm, variant % 2 ? MAT_DENIM : MAT_LEATHER, thighBone, 400);
        if (p) {
            mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, -0.42f * s, 0), 0.075f * s, 0.058f * s, detail, 255, 255, 255, 1.4f, 0);
        }
        p = add_part(cm, variant % 2 ? MAT_DENIM : MAT_LEATHER, shinBone, 400);
        if (p) {
            mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, -0.40f * s, 0), 0.058f * s, 0.042f * s, detail, 255, 255, 255, 1.6f, 0);
            mesh_ellipsoid(p->mesh, v3(0, -0.01f * s, 0), v3(0.058f * s, 0.05f * s, 0.058f * s), 8, 4, 255, 255, 255, 1.8f);
        }
        p = add_part(cm, MAT_RUBBER, footBone, 300);
        if (p) {
            mesh_add_boxf(p->mesh, v3(-0.05f * s, -0.06f * s, -0.08f * s), v3(0.05f * s, 0.02f * s, 0.13f * s), 255, 255, 255, 2.f);
            mesh_ellipsoid(p->mesh, v3(0, -0.03f * s, 0.05f * s), v3(0.052f * s, 0.035f * s, 0.10f * s), 8, 4, 255, 255, 255, 2.f);
        }
    }
    // ---- brute/boss armour plates
    if (type == ZB_BRUTE || type == ZB_MINIBOSS || type == ZB_BOSS) {
        p = add_part(cm, MAT_STEEL, B_CHEST, 500);
        if (p) {
            for (int i = 0; i < 3; i++) {
                mesh_add_boxf(p->mesh, v3(-0.16f * s, (0.02f + i * 0.09f) * s, 0.12f * s), v3(0.16f * s, (0.08f + i * 0.09f) * s, 0.17f * s),
                              255, 255, 255, 2.f);
            }
        }
        p = add_part(cm, MAT_FLESH, B_SPINE, 400);
        if (p) {
            // exposed ribcage / growths
            for (int i = 0; i < 4; i++) {
                mesh_sphere(p->mesh, v3((i % 2 ? 0.12f : -0.12f) * s, (0.06f + i * 0.05f) * s, 0.10f * s), 0.035f * s, 8, 5, 255, 255, 255, 2.f);
            }
        }
    }
    if (type == ZB_SCREAMER) {
        p = add_part(cm, MAT_EMISSIVE_RED, B_CHEST, 200);
        if (p) {
            mesh_sphere(p->mesh, v3(0, 0.10f * s, 0.13f * s), 0.03f * s, 8, 6, 255, 255, 255, 2.f);
        }
    }
    make_zombie_clips(cm, type == ZB_RUNNER ? 0.3f : 1.0f, type == ZB_RUNNER ? 0.5f : 1.0f, crawler);
    cm->height = 1.8f * s;
    float hy = 1.8f * s;
    if (crawler) hy = 0.6f;
    cm->bbmin = v3(-0.35f * s, 0, -0.35f * s);
    cm->bbmax = v3(0.35f * s, hy, 0.35f * s);
    return cm;
}

CharModel *char_player(int variant, Assets *as) {
    (void)as;
    CharModel *cm = (CharModel *)calloc(1, sizeof(CharModel));
    cm->type = -1;
    cm->sk = base_skeleton(1.0f, 0);
    int detail = 12;
    CharPart *p = add_part(cm, MAT_DENIM, B_PELVIS, 400);
    if (p) {
        mesh_ellipsoid(p->mesh, v3(0, 0, 0), v3(0.16f, 0.13f, 0.11f), detail, 6, 255, 255, 255, 1.2f);
        mesh_cyl_axis(p->mesh, v3(-0.09f, -0.06f, 0), v3(0.09f, -0.06f, 0), 0.10f, 0.10f, detail, 255, 255, 255, 1.2f, 1);
        // belt
        mesh_cyl_axis(p->mesh, v3(0, 0.02f, 0), v3(0, 0.07f, 0), 0.17f, 0.17f, detail, 200, 200, 200, 2.f, 0);
    }
    p = add_part(cm, MAT_LEATHER, B_SPINE, 500);
    if (p) {
        mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, 0.24f, 0.01f), 0.17f, 0.19f, detail, 255, 255, 255, 1.1f, 0);
    }
    p = add_part(cm, MAT_LEATHER, B_CHEST, 700);
    if (p) {
        mesh_ellipsoid(p->mesh, v3(0, 0.10f, 0), v3(0.21f, 0.17f, 0.14f), detail, 7, 255, 255, 255, 1.0f);
        // vest pouches + straps
        for (int i = 0; i < 3; i++) {
            mesh_add_boxf(p->mesh, v3(-0.12f, (0.02f + i * 0.06f), 0.12f), v3(0.12f, (0.065f + i * 0.06f), 0.155f), 255, 255, 255, 2.5f);
        }
        mesh_add_boxf(p->mesh, v3(-0.19f, 0.05f, -0.13f), v3(-0.13f, 0.24f, 0.13f), 255, 255, 255, 2.f);
        mesh_add_boxf(p->mesh, v3(0.13f, 0.05f, -0.13f), v3(0.19f, 0.24f, 0.13f), 255, 255, 255, 2.f);
    }
    p = add_part(cm, MAT_SKIN_PALE, B_NECK, 150);
    if (p) mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, 0.10f, 0), 0.05f, 0.048f, 10, 255, 255, 255, 1.4f, 0);
    p = add_part(cm, MAT_SKIN_PALE, B_HEAD, 600);
    if (p) {
        mesh_ellipsoid(p->mesh, v3(0, 0.085f, 0.01f), v3(0.092f, 0.113f, 0.10f), detail + 2, 8, 255, 255, 255, 1.3f);
        mesh_ellipsoid(p->mesh, v3(0, 0.11f, 0.085f), v3(0.072f, 0.02f, 0.024f), 8, 4, 255, 255, 255, 1.6f);
        mesh_cyl_axis(p->mesh, v3(0, 0.065f, 0.075f), v3(0, 0.045f, 0.10f), 0.017f, 0.012f, 6, 255, 255, 255, 2.f, 1);
        mesh_ellipsoid(p->mesh, v3(0, 0.03f, 0.055f), v3(0.07f, 0.035f, 0.055f), 8, 4, 255, 255, 255, 1.6f);
    }
    // hair
    p = add_part(cm, MAT_HAIR, B_HEAD, 300);
    if (p) {
        mesh_ellipsoid(p->mesh, v3(0, 0.135f, -0.012f), v3(0.098f, 0.075f, 0.104f), detail, 6, 255, 255, 255, 1.4f);
    }
    // headlamp + goggles for a more "prepared survivor" read
    p = add_part(cm, MAT_EMISSIVE_WARM, B_HEAD, 120);
    if (p) {
        mesh_cyl_axis(p->mesh, v3(0, 0.145f, 0.09f), v3(0, 0.145f, 0.115f), 0.02f, 0.022f, 8, 255, 255, 255, 2.f, 1);
    }
    for (int side = 0; side < 2; side++) {
        int armBone = side == 0 ? B_ARM_L : B_ARM_R;
        int foreBone = side == 0 ? B_FORE_L : B_FORE_R;
        int handBone = side == 0 ? B_HAND_L : B_HAND_R;
        float sx = side == 0 ? 1.f : -1.f;
        p = add_part(cm, MAT_LEATHER, armBone, 400);
        if (p) {
            mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, -0.27f, 0), 0.055f, 0.045f, detail, 255, 255, 255, 1.6f, 0);
            mesh_ellipsoid(p->mesh, v3(sx * 0.01f, 0.01f, 0), v3(0.065f, 0.068f, 0.065f), 10, 5, 255, 255, 255, 1.6f);
            mesh_add_boxf(p->mesh, v3(-0.055f, -0.09f, -0.05f), v3(0.055f, -0.02f, 0.05f), 255, 255, 255, 2.f);  // shoulder pad
        }
        p = add_part(cm, MAT_LEATHER, foreBone, 400);
        if (p) {
            mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, -0.25f, 0), 0.045f, 0.036f, detail, 255, 255, 255, 1.8f, 0);
            mesh_cyl_axis(p->mesh, v3(0, -0.16f, 0), v3(0, -0.22f, 0), 0.05f, 0.05f, detail, 220, 220, 220, 2.f, 0);  // glove cuff
        }
        p = add_part(cm, MAT_SKIN_PALE, handBone, 250);
        if (p) {
            mesh_ellipsoid(p->mesh, v3(0, -0.04f, 0.01f), v3(0.038f, 0.055f, 0.024f), 8, 5, 255, 255, 255, 2.f);
        }
        int thighBone = side == 0 ? B_THIGH_L : B_THIGH_R;
        int shinBone = side == 0 ? B_SHIN_L : B_SHIN_R;
        int footBone = side == 0 ? B_FOOT_L : B_FOOT_R;
        p = add_part(cm, MAT_DENIM, thighBone, 400);
        if (p) mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, -0.42f, 0), 0.08f, 0.062f, detail, 255, 255, 255, 1.4f, 0);
        p = add_part(cm, MAT_DENIM, shinBone, 400);
        if (p) {
            mesh_cyl_axis(p->mesh, v3(0, 0, 0), v3(0, -0.40f, 0), 0.062f, 0.045f, detail, 255, 255, 255, 1.6f, 0);
            mesh_ellipsoid(p->mesh, v3(0, -0.01f, 0), v3(0.06f, 0.05f, 0.06f), 8, 4, 255, 255, 255, 1.8f);
        }
        p = add_part(cm, MAT_RUBBER, footBone, 300);
        if (p) {
            mesh_add_boxf(p->mesh, v3(-0.055f, -0.07f, -0.09f), v3(0.055f, 0.02f, 0.14f), 255, 255, 255, 2.f);
        }
    }
    make_zombie_clips(cm, 0.0f, 0.25f, 0);
    cm->height = 1.8f;
    cm->bbmin = v3(-0.35f, 0, -0.35f);
    cm->bbmax = v3(0.35f, 1.8f, 0.35f);
    (void)variant;
    return cm;
}
