// ============================================================================
// THE LAST NIGHT — procedural model library
// ============================================================================
#ifndef TLN_MODELS_H
#define TLN_MODELS_H

#include "world.h"

struct Model {
    Mesh *part[12];
    int nparts;
    Vec3 bbmin, bbmax;
    float radius, height;
};

typedef struct CharPart {
    Mesh *mesh;
    int bone;
    float jiggle;     // secondary motion amount
} CharPart;

typedef struct CharModel {
    CharPart parts[26];
    int nparts;
    Skeleton *sk;
    AnimClip *clip_idle, *clip_walk, *clip_run, *clip_attack, *clip_hit, *clip_death, *clip_scream, *clip_special, *clip_crawl;
    int type;
    float height;
    Vec3 muzzle;      // local muzzle offset (unused for zombies)
    Vec3 bbmin, bbmax;
} CharModel;

Model *model_new(void);
Mesh *model_part(Model *m, int mat, int cap_hint);
void   model_final(Model *m);
void   model_draw(Renderer *r, const Model *m, const Material *mats, Mat4 xf);
void   model_free(Model *m);

// ---- props -----------------------------------------------------------------
Model *prop_shelf(int width_units, int two_sided, int fill);
Model *prop_checkout(void);
Model *prop_cart(void);
Model *prop_cart_nest(void);
Model *prop_freezer(int len);
Model *prop_fridge(int len);
Model *prop_pallet(int with_boxes);
Model *prop_boxes(int count);
Model *prop_barrel(int rusty);
Model *prop_table(void);
Model *prop_chair(void);
Model *prop_locker(int count);
Model *prop_vending(void);
Model *prop_atm(void);
Model *prop_dumpster(void);
Model *prop_streetlight(void);
Model *prop_fence_segment(float len);
Model *prop_car(int variant);
Model *prop_truck(int box_truck);
Model *prop_door(int kind);
Model *prop_barricade(int level);
Model *prop_trap(int kind);
Model *prop_debris(int seed);
Model *prop_generator(void);
Model *prop_ceiling_light(int lit);
Model *prop_wall_sign(int mat, float w, float h);
Model *prop_pipe_run(float len);
Model *prop_mannequin(void);
Model *prop_shelf_broken(void);
Model *prop_pallet_jack(void);
Model *prop_register_stand(void);
Model *prop_wet_sign(void);
Model *prop_extinguisher(void);
Model *prop_crate_stack(void);
Model *prop_fire_barrel(int lit);
Model *prop_radio_tower(void);
Model *prop_boss_core(void);
Model *prop_light_pole(int on);
Model *prop_traffic_cone(void);

// ---- characters ------------------------------------------------------------
CharModel *char_zombie(int type, int variant, Assets *as);
CharModel *char_player(int variant, Assets *as);
void char_free(CharModel *c);
void char_draw(Renderer *r, CharModel *cm, const Material *mats, const Pose *pose, Mat4 root, float scale);

#endif
