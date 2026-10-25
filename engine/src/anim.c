#include "anim.h"
#include <string.h>

Skeleton *skeleton_new(int count) {
    Skeleton *s = (Skeleton *)calloc(1, sizeof(Skeleton));
    s->count = clampi(count, 1, MAX_BONES);
    return s;
}
void skeleton_bone(Skeleton *s, int i, int parent, Vec3 rest, const char *name) {
    if (i < 0 || i >= MAX_BONES) return;
    s->parent[i] = parent;
    s->rest[i] = rest;
    snprintf(s->name[i], sizeof s->name[i], "%s", name ? name : "");
    if (i >= s->count) s->count = i + 1;
}
Mat4 bone_matrix(Vec3 euler, Vec3 rest_offset, Vec3 scale) {
    Mat4 t = mat4_translate(rest_offset);
    Mat4 r = mat4_mul(mat4_rot_y(euler.y), mat4_mul(mat4_rot_x(euler.x), mat4_rot_z(euler.z)));
    Mat4 s = mat4_scale(scale);
    return mat4_mul(t, mat4_mul(r, s));
}

static void lerp_euler(const Vec3 *a, const Vec3 *b, float t, Vec3 *out, int n) {
    for (int i = 0; i < n; i++) {
        out[i].x = lerpf(a[i].x, b[i].x, t);
        out[i].y = lerpf(a[i].y, b[i].y, t);
        out[i].z = lerpf(a[i].z, b[i].z, t);
    }
}

void anim_sample(const Animator *a, Pose *out) {
    const Skeleton *sk = a->sk;
    int n = sk->count;
    memset(out, 0, sizeof(*out));
    Vec3 e1[MAX_BONES], e2[MAX_BONES];
    memset(e1, 0, sizeof e1);
    memset(e2, 0, sizeof e2);
    if (a->clip) {
        float t = a->time;
        if (a->clip->loop && a->clip->duration > 0.001f) {
            t = t - floorf(t / a->clip->duration) * a->clip->duration;
        }
        const AnimClip *c = a->clip;
        if (c->nkeys == 0) {
            /* nothing */
        } else if (c->nkeys == 1) {
            for (int i = 0; i < n; i++) e1[i] = c->keys[0].euler[i];
        } else {
            int k = 0;
            while (k < c->nkeys - 2 && c->keys[k + 1].t < t) k++;
            float t0 = c->keys[k].t, t1 = c->keys[k + 1].t;
            float f = (t1 - t0) > 1e-6f ? (t - t0) / (t1 - t0) : 0.f;
            f = saturatef(f);
            f = f * f * (3.f - 2.f * f);  // smoothstep for softer poses
            lerp_euler(c->keys[k].euler, c->keys[k + 1].euler, f, e1, n);
        }
    }
    if (a->next && a->blend < 1.f) {
        const AnimClip *c = a->next;
        if (c->nkeys > 0) {
            float t = a->time;
            if (c->loop && c->duration > 0.001f) t = t - floorf(t / c->duration) * c->duration;
            if (c->nkeys == 1) {
                for (int i = 0; i < n; i++) e2[i] = c->keys[0].euler[i];
            } else {
                int k = 0;
                while (k < c->nkeys - 2 && c->keys[k + 1].t < t) k++;
                float t0 = c->keys[k].t, t1 = c->keys[k + 1].t;
                float f = (t1 - t0) > 1e-6f ? (t - t0) / (t1 - t0) : 0.f;
                f = smoothstepf(0.f, 1.f, saturatef(f));
                lerp_euler(c->keys[k].euler, c->keys[k + 1].euler, f, e2, n);
            }
        }
        float bl = saturatef(a->blend);
        lerp_euler(e2, e1, bl, out->euler, n);
    } else {
        for (int i = 0; i < n; i++) out->euler[i] = e1[i];
    }
}

void anim_world_mats(const Skeleton *sk, const Pose *pose, Mat4 out[MAX_BONES]) {
    Mat4 world[MAX_BONES];
    for (int i = 0; i < sk->count; i++) {
        // rig space: bone rotation applied around its origin; rest offset translation
        Mat4 local = bone_matrix(pose->euler[i], sk->rest[i], v3s(1.f));
        int p = sk->parent[i];
        if (p >= 0 && p < i) {
            world[i] = mat4_mul(world[p], local);
        } else {
            world[i] = local;
        }
        out[i] = world[i];
    }
}

void anim_play(Animator *a, const AnimClip *clip, float blend_time) {
    if (a->clip == clip) return;
    if (blend_time <= 0.f || !a->clip) {
        a->clip = clip;
        a->next = NULL;
        a->blend = 1.f;
        a->time = 0.f;
        a->finished = 0;
        return;
    }
    a->next = a->clip;         // old clip becomes the source we blend from
    a->clip = clip;
    a->blend = 0.f;
    a->time = 0.f;
    a->finished = 0;
}

void anim_update(Animator *a, float dt) {
    a->time += dt * a->speed;
    if (a->blend < 1.f) {
        a->blend += dt / 0.16f;
        if (a->blend >= 1.f) { a->blend = 1.f; a->next = NULL; }
    }
    if (a->clip && !a->clip->loop && a->clip->duration > 0.f && a->time >= a->clip->duration) {
        a->time = a->clip->duration - 0.0001f;
        a->finished = 1;
    }
}

AnimClip *clip_make(const char *name, float duration, int loop) {
    AnimClip *c = (AnimClip *)calloc(1, sizeof(AnimClip));
    snprintf(c->name, sizeof c->name, "%s", name ? name : "clip");
    c->duration = duration;
    c->loop = loop;
    c->nkeys = 0;
    return c;
}
void clip_free(AnimClip *c) {
    if (!c) return;
    free(c);
}

void clip_key(AnimClip *c, float t, const Vec3 *euler) {
    if (c->nkeys >= 8) return;
    ClipKey *k = &c->keys[c->nkeys++];
    k->t = t;
    memcpy(k->euler, euler, sizeof(Vec3) * MAX_BONES);
}
