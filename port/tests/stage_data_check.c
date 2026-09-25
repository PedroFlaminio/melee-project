/* Checks, through the game's own C types, the map_head the host archive API
 * translated from the archive hsd_host_archive_test.cpp builds, which C++
 * cannot include the stage headers to read. */

#include <melee/gr/ground.h>
#include <melee/gr/types.h>
#include <melee/it/itCommonItems.h>
#include <melee/it/types.h>
#include <melee/lb/forward.h>
#include <melee/mp/types.h>
#include <melee/sc/types.h>
#include <sysdolphin/baselib/lobj.h>

#include <stddef.h>
#include <stdio.h>

#include "game/yakumono_param.h"

int melee_host_test_check_stage_map_head(void* translated, char* message,
                                         size_t size);
int melee_host_test_check_stage_coll_data(void* translated, char* message,
                                          size_t size);
int melee_host_test_check_stage_extras(void* plit, void* quake, void* items,
                                       char* message, size_t size);
void melee_host_test_set_stage_grkind(int grkind);
int melee_host_test_check_icemt_yakumono(void* translated, char* message,
                                         size_t size);
int melee_host_test_grkind_mutecity(void);
int melee_host_test_check_mutecity_yakumono(void* translated, char* message,
                                            size_t size);
int melee_host_test_check_stage_item_specials(void* items, char* message,
                                              size_t size);

/* ground.c declares these records locally; game_data_translators.c keeps the
 * same declarations. */
typedef struct CheckLightOverrideEntry {
    HSD_LightDesc* desc;
    u8 a : 1;
    u8 b : 1;
    u8 c : 1;
    u8 _ : 5;
    u8 _pad[3];
} CheckLightOverrideEntry;

typedef struct CheckStagePairs {
    void* joint;
    struct {
        s16 a, b;
    }* unk4;
    s32 unk8;
} CheckStagePairs;

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            snprintf(message, size, "%s", #condition);                        \
            return 0;                                                         \
        }                                                                     \
    } while (0)

void melee_host_test_set_stage_grkind(int grkind)
{
    stage_info.grkind = grkind;
}

int melee_host_test_check_icemt_yakumono(void* translated, char* message,
                                         size_t size)
{
    const struct melee_host_yakumono_icemt* const params = translated;

    CHECK(params != NULL);
    CHECK(params->field_ixs != NULL);
    CHECK(params->field_ixs[0] == 0 && params->field_ixs[1] == 1);
    CHECK(params->field_ixs[2] == 2 && params->field_ixs[4] == 4);
    CHECK(params->xB0 != NULL && params->xB0[0] == -3 &&
          params->xB0[1] == 0x1234);
    CHECK(params->xB4 != NULL && params->xB4[0] == 200 &&
          params->xB4[1] == -200);
    return 1;
}

int melee_host_test_grkind_mutecity(void)
{
    return Gr_Kind_MuteCity;
}

/* The track's hit: nine words, reached as the game reaches it, through the
 * record's own type rather than DynamicsDesc, whose count only sat at +4
 * while a pointer was four bytes. */
int melee_host_test_check_mutecity_yakumono(void* translated, char* message,
                                            size_t size)
{
    const struct melee_host_yakumono_mutecity* const params = translated;

    CHECK(params != NULL);
    CHECK(params->x8 != NULL && params->xC == NULL);
    CHECK(params->x8->state == 1 && params->x8->damage == 8);
    CHECK(params->x8->kb_angle == 90 && params->x8->unkC == 50);
    CHECK(params->x8->element == 2 && params->x8->sfx_kind == 7);
    CHECK(params->x2C == 1.5F);
    return 1;
}

/* A Shy Guy and a Tingle.  The Shy Guy's first word is a record whose hit
 * points itheiho.c reads, then its walking speeds; the Tingle's first word
 * is never read, and its two trailing bytes keep their order. */
int melee_host_test_check_stage_item_specials(void* items, char* message,
                                              size_t size)
{
    struct GroundItemData** const entries = items;
    const itHeihoAttributes* heiho;
    const itTincleAttributes* tincle;

    CHECK(entries != NULL && entries[0] != NULL && entries[1] != NULL);
    CHECK(entries[2] == NULL);
    CHECK(entries[0]->unk0 == It_Kind_Heiho && entries[0]->unk4 != NULL);
    heiho = entries[0]->unk4->x4_specialAttributes;
    CHECK(heiho != NULL);
    CHECK(heiho->x0 != NULL && heiho->x0[0] == 15 && heiho->x0[4] == 300);
    CHECK(heiho->walk_vel[0] == 0.3F && heiho->walk_vel[2] == 0.75F);
    CHECK(heiho->walk_vel[3] == 1.5F);
    CHECK(heiho->knock_vel_x == 2.0F && heiho->x18 == 30.0F);

    CHECK(entries[1]->unk0 == It_Kind_Tincle && entries[1]->unk4 != NULL);
    tincle = entries[1]->unk4->x4_specialAttributes;
    CHECK(tincle != NULL);
    CHECK(tincle->x0 == 0.0F);
    CHECK(tincle->x4 == 600 && tincle->xC == -60.0F);
    CHECK(tincle->x50 == 3.0F);
    CHECK(tincle->x54 == 3 && tincle->x55 == 6);
    return 1;
}

int melee_host_test_check_stage_coll_data(void* translated, char* message,
                                          size_t size)
{
    const MapCollData* const coll = translated;
    const MapJoint* joint;

    CHECK(coll != NULL);
    CHECK(coll->vert_count == 3 && coll->verts != NULL);
    CHECK(coll->verts[0].x == 1.5F && coll->verts[0].y == -2.0F);
    CHECK(coll->verts[2].x == -8.0F && coll->verts[2].y == 0.5F);

    CHECK(coll->line_count == 2 && coll->lines != NULL);
    CHECK(coll->lines[0].v0_idx == 0 && coll->lines[0].v1_idx == 1);
    CHECK(coll->lines[0].prev_id0 == -1 && coll->lines[0].next_id0 == 1);
    CHECK(coll->lines[0].hi_flags == 0x0100 && coll->lines[0].lo_flags == 1);
    CHECK(coll->lines[1].v0_idx == 1 && coll->lines[1].v1_idx == 2);
    CHECK(coll->lines[1].prev_id0 == 0 && coll->lines[1].next_id1 == -1);
    CHECK(coll->lines[1].lo_flags == 4);

    CHECK(coll->floor_start == 0 && coll->floor_count == 2);
    CHECK(coll->ceiling_start == -1 && coll->dynamic_count == 0);

    CHECK(coll->joint_count == 1 && coll->joints != NULL);
    joint = &coll->joints[0];
    CHECK(joint->floor_count == 2 && joint->ceiling_start == -1);
    CHECK(joint->left_bound == -8.0F && joint->bottom_bound == -2.0F);
    CHECK(joint->right_bound == 3.0F && joint->top_bound == 4.25F);
    CHECK(joint->vtx_start == 0 && joint->vtx_count == 3);

    /* The word after the 0x2C-byte record is not part of it. */
    CHECK(coll->x2C == 0);
    return 1;
}

int melee_host_test_check_stage_map_head(void* translated, char* message,
                                         size_t size)
{
    const UnkStageDat* const dat = translated;
    const struct UnkStageDat_x8_t* model;
    const HSD_LightDesc* light;
    const CheckLightOverrideEntry* overrides;
    const CheckStagePairs* pairs;

    CHECK(dat != NULL);

    /* One model: its joint, an animation table that is only its terminator,
     * the left-out x14, one ambient light, two GrJoints, flag bytes and three
     * s16. */
    CHECK(dat->unkC == 1);
    model = dat->unk8;
    CHECK(model != NULL);
    CHECK(model->unk0 != NULL);
    CHECK(model->unk4 != NULL && model->unk4[0] == NULL);
    CHECK(model->unk8 == NULL && model->unkC == NULL);
    CHECK(model->x10 == NULL);
    CHECK(model->x14 == NULL);
    CHECK(model->x18 != NULL && model->x18[0] != NULL && model->x18[1] == NULL);
    light = model->x18[0]->desc;
    CHECK(light != NULL);
    CHECK(light->color.r == 0x40 && light->color.a == 0xFF);
    CHECK(model->x1C == NULL);
    CHECK(model->unk24 == 2);
    CHECK(model->unk20 != NULL);
    CHECK(model->unk20[0].x == 1 && model->unk20[1].y == -3 &&
          model->unk20[1].z == 6);
    CHECK(model->x28 != NULL && ((const u8*) model->x28)[0] == 1 &&
          ((const u8*) model->x28)[1] == 0);
    CHECK(model->x30 == 3);
    CHECK(model->x2C != NULL && model->x2C[0] == 10 && model->x2C[2] == -7);

    CHECK(dat->unk4 == 1);
    pairs = dat->unk0;
    CHECK(pairs != NULL && pairs[0].unk8 == 2);
    /* Ground_801C34AC finds the entry by the model's joint descriptor. */
    CHECK(pairs[0].joint == model->unk0);
    CHECK(pairs[0].unk4 != NULL);
    CHECK(pairs[0].unk4[0].a == 1 && pairs[0].unk4[0].b == 0x94);
    CHECK(pairs[0].unk4[1].a == 2 && pairs[0].unk4[1].b == 0x95);

    CHECK(dat->unk10 == NULL && dat->unk14 == 0);

    /* Four overrides counted for the two the table holds.  The first names
     * the model's light, flags from the most significant bit; the rest never
     * equal it. */
    CHECK(dat->unk1C == 4);
    overrides = dat->unk18;
    CHECK(overrides != NULL);
    CHECK(overrides[0].desc == light);
    CHECK(overrides[0].a == 1 && overrides[0].b == 1 && overrides[0].c == 0);
    CHECK(overrides[1].desc != NULL && overrides[1].desc != light);
    CHECK(overrides[1].a == 0 && overrides[1].c == 1);
    CHECK(overrides[2].desc != NULL && overrides[2].desc != light);
    CHECK(overrides[3].desc != NULL && overrides[3].desc != light);
    CHECK(overrides[2].desc != overrides[3].desc);

    CHECK(dat->unk20 == NULL && dat->unk24 == 0);

    /* The material, read through the host layout of UnkStageDatInternal. */
    CHECK(dat->unk2C == 1);
    CHECK(dat->unk28 != NULL && dat->unk28[0] != NULL);
    CHECK(dat->unk28[0]->unk4 == 0x12);
    return 1;
}

int melee_host_test_check_stage_extras(void* plit, void* quake, void* items,
                                       char* message, size_t size)
{
    LightList** const lists = plit;
    const DynamicModelDesc* const model = quake;
    struct GroundItemData** const entries = items;

    /* One light list naming an ambient light, then the terminator. */
    CHECK(lists != NULL && lists[0] != NULL && lists[1] == NULL);
    CHECK(lists[0]->desc != NULL);
    CHECK(lists[0]->desc->color.r == 0x11 && lists[0]->desc->color.g == 0x22 &&
          lists[0]->desc->color.b == 0x33);
    CHECK(lists[0]->anims == NULL);

    /* The quake model: a bare joint and one animation. */
    CHECK(model != NULL && model->joint != NULL);
    CHECK(model->anims != NULL && model->anims[0] != NULL &&
          model->anims[1] == NULL);
    CHECK(model->matanims == NULL && model->shapeanims == NULL);

    /* No stage items, as in Hyrule Temple. */
    CHECK(entries != NULL && entries[0] == NULL);
    return 1;
}
