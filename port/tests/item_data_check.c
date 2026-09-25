/* Checks, through the game's own C types, the itPublicData the host archive
 * API translated from the archive hsd_host_archive_test.cpp builds, which
 * C++ cannot include the item headers to read. */

#include <melee/it/itCommonItems.h>
#include <melee_host/boot.h>

#include <melee/it/forward.h>
#include <melee/it/it_3F14.h>
#include <melee/it/types.h>
#include <melee/lb/types.h>

#include <stddef.h>
#include <stdio.h>

int melee_host_test_check_item_public_data(void* translated, char* message,
                                           size_t size);

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            snprintf(message, size, "%s", #condition);                        \
            return 0;                                                         \
        }                                                                     \
    } while (0)

int melee_host_test_check_item_public_data(void* translated, char* message,
                                           size_t size)
{
    const it_804D6D20_t* const data = translated;
    const Article* first;
    const Article* character;
    const ItemAttr* attr;
    const struct ItemStateDesc* states;
    int i;

    CHECK(data != NULL);

    /* ItemCommonData: words, and the bytes that are bytes. */
    CHECK(data->x0 != NULL);
    CHECK(data->x0->x0 == 7);
    CHECK(data->x0->x48_byte == 0x5A);
    CHECK(data->x0->filler_1a[0] == 1 && data->x0->filler_1a[3] == 4);
    CHECK(data->x0->x15C == 2.5f);

    /* Two common entries share one article; a character entry shares its
     * attributes and states. */
    first = data->x4[0];
    character = data->x8[117];
    CHECK(first != NULL);
    CHECK(data->x4[1] == first);
    CHECK(data->x4[2] == NULL);
    CHECK(character != NULL && character != first);
    CHECK(character->x0_common_attr == first->x0_common_attr);
    CHECK(character->xC_itemStates == first->xC_itemStates);
    for (i = 0; i < It_Kind_Old_Kuri - It_PKind_Start; i++) {
        CHECK(data->xC[i] == NULL);
    }

    /* The flag bits MWCC packs from the most significant bit. */
    attr = first->x0_common_attr;
    CHECK(attr != NULL);
    CHECK(attr->x0_is_heavy == 1);
    CHECK(attr->x0_78 == 1);
    CHECK(attr->x0_hold_kind == 3);
    CHECK(attr->x1_1 == 1);
    CHECK(attr->x1_3 == 1);
    CHECK(attr->x1_4 == 0);
    CHECK(attr->x1_5 == 1);
    CHECK(attr->x1_67_cam_kind == 2);
    CHECK(attr->x1_8 == 1);
    CHECK(attr->x60_scale == 1.25f);

    /* What the host leaves out points at the marker. */
    CHECK(first->x4_specialAttributes == &melee_host_item_data_left_out);
    /* The Bob-omb's attributes are all floats: the disc block, and the z of
     * its last Vec3, which the block does not hold, left at zero. */
    CHECK(data->x4[6] != NULL);
    {
        const itBombHeiAttributes* const bomb =
            data->x4[6]->x4_specialAttributes;
        CHECK(bomb != NULL &&
              (void*) bomb != (void*) &melee_host_item_data_left_out);
        CHECK(bomb->x0 == 3.0f);
        CHECK(bomb->x20.x == 0.8f && bomb->x20.y == 0.7f);
        CHECK(bomb->x20.z == 0.0f);
    }
    /* Food: each food's model is widened, and the fields after it keep
     * their values rather than their disc offsets. */
    CHECK(data->x4[It_Kind_Foods] != NULL);
    {
        const itFoodsAttributes* const foods =
            data->x4[It_Kind_Foods]->x4_specialAttributes;
        CHECK(foods != NULL &&
              (void*) foods != (void*) &melee_host_item_data_left_out);
        CHECK(foods->count == 2);
        CHECK(foods->foods[0].joint != NULL && foods->foods[1].joint != NULL);
        CHECK(foods->foods[0].joint != foods->foods[1].joint);
        CHECK(foods->foods[0].heal_amount == 5);
        CHECK(foods->foods[0].x_offset == 5.0f);
        CHECK(foods->foods[0].y_offset == -11.0f);
        CHECK(foods->foods[1].heal_amount == 9);
        CHECK(foods->foods[1].x_offset == 3.5f);
        CHECK(foods->foods[1].y_offset == -8.5f);
    }
    CHECK(first->x14_dynamics == NULL);
    CHECK(character->x4_specialAttributes == NULL);
    CHECK((void*) character->x14_dynamics ==
          (void*) &melee_host_item_data_left_out);

    CHECK(first->x8_hurtbones != NULL);
    CHECK(first->x8_hurtbones->count == 1);
    CHECK(first->x8_hurtbones->descs[0].bone_id == 4);
    CHECK(first->x8_hurtbones->descs[0].a_offset.x == 1.0f);
    CHECK(first->x8_hurtbones->descs[0].scale == 2.0f);

    /* Two states up to the model that follows them; the first runs a script
     * converted to native words. */
    states = first->xC_itemStates->x0_itemStateDesc;
    CHECK(states[0].x0_anim_joint == NULL);
    CHECK(states[0].xC_script != NULL);
    CHECK(((const u32*) states[0].xC_script)[0] == ((1U << 26) | 5U));
    CHECK(((const u32*) states[0].xC_script)[1] == 0);
    CHECK(states[1].xC_script == NULL);

    CHECK(first->x10_modelDesc != NULL);
    CHECK(first->x10_modelDesc->x0_joint == NULL);
    CHECK(first->x10_modelDesc->x4_bone_count == 5);
    CHECK(first->x10_modelDesc->x8_bone_attach_id == -1);
    CHECK(first->x10_modelDesc->xC_bit_field == 0x80);

    CHECK(data->x10 != NULL);
    CHECK(data->x10->x0 == 3);
    CHECK(data->x10->x4 == 1.5f);

    /* The color animations reach the same converted script. */
    CHECK(data->x14 != NULL);
    CHECK(data->x14[0].unk == NULL);
    CHECK(data->x14[1].unk == states[0].xC_script);
    CHECK(data->x14[1].unk4 == 30);
    CHECK(data->x14[1].unk5 == 2);
    return 1;
}
