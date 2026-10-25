// ============================================================================
// THE LAST NIGHT — skeletal animation (rigid-part, software-renderer friendly)
// ============================================================================
#ifndef TLN_ANIM_H
#define TLN_ANIM_H

#include "core.h"
#include "render.h"

#define MAX_BONES 48
#define MAX_CLIP_KEYS 48

typedef struct Skeleton {
    int count;
    int parent[MAX_BONES];
    Vec3 rest[MAX_BONES];      // rest-pose offset from parent
    char name[MAX_BONES][24];
} Skeleton;

// one pose = euler rotation (radians) + optional scale per bone
typedef struct Pose {
    Vec3 euler[MAX_BONES];
    Vec3 extra[MAX_BONES];     // additive offsets (root motion, lean, etc.)
} Pose;

typedef struct ClipKey {
    float t;
    Vec3 euler[MAX_BONES];
} ClipKey;

typedef struct AnimClip {
    char name[28];
    float duration;
    int loop;
    int nkeys;
    int bone_mask;             // bones this clip animates (bitmask, informational)
    ClipKey keys[8];           // up to 8 keyframes per clip
} AnimClip;

typedef struct Animator {
    const Skeleton *sk;
    const AnimClip *clip;
    const AnimClip *next;
    float time;
    float blend;               // 0..1 blend from next->clip
    float speed;
    int finished;              // one-shot clips
} Animator;

Skeleton *skeleton_new(int count);
void skeleton_bone(Skeleton *s, int i, int parent, Vec3 rest, const char *name);
void anim_sample(const Animator *a, Pose *out);
void anim_world_mats(const Skeleton *sk, const Pose *pose, Mat4 out[MAX_BONES]);
void anim_play(Animator *a, const AnimClip *clip, float blend_time);
void anim_update(Animator *a, float dt);
AnimClip *clip_make(const char *name, float duration, int loop);
void clip_free(AnimClip *c);
void clip_key(AnimClip *c, float t, const Vec3 *euler);  // appends key

// euler helpers (degrees)
static inline Vec3 e3(float x, float y, float z) { return v3(DEG2RAD(x), DEG2RAD(y), DEG2RAD(z)); }

// build a bone matrix from pose euler (YXZ order)
Mat4 bone_matrix(Vec3 euler, Vec3 rest_offset, Vec3 scale);

#endif
