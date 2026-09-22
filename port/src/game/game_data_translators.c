/* Game data whose layout only the game's C headers describe, translated for
 * the host's archive API (melee_host_hsd_register_translator).
 *
 * Each translator reads the file big-endian through the reader and fills the
 * game's own types field by field, so the host compiler lays the result out
 * and the only offsets written by hand are the PowerPC ones on disk.  What a
 * translator cannot give a host meaning to is left out on purpose and said so
 * at the field, rather than handed to the game as bytes it would misread. */

#include <melee_host/boot.h>
#include <melee_host/hsd_archive.h>

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include <melee/ft/fighter.h>
#include <melee/ft/dobjlist.h>
#include <melee/ft/ftdata.h>
#include <melee/ft/ftwaitanim.h>
#include <melee/ft/kinds/ftCaptain/types.h>
#include <melee/ft/kinds/ftCommon/types.h>
#include <melee/ft/kinds/ftDonkey/types.h>
#include <melee/ft/kinds/ftFox/types.h>
#include <melee/ft/kinds/ftDrMario/types.h>
#include <melee/ft/kinds/ftGameWatch/types.h>
#include <melee/ft/kinds/ftKirby/types.h>
#include <melee/ft/kinds/ftKoopa/types.h>
#include <melee/ft/kinds/ftLuigi/types.h>
#include <melee/ft/kinds/ftMars/types.h>
#include <melee/ft/kinds/ftMewtwo/types.h>
#include <melee/ft/kinds/ftPopo/types.h>
#include <melee/ft/kinds/ftNess/types.h>
#include <melee/ft/kinds/ftPeach/types.h>
#include <melee/ft/kinds/ftPichu/types.h>
#include <melee/ft/kinds/ftPikachu/types.h>
#include <melee/ft/kinds/ftPurin/types.h>
#include <melee/ft/kinds/ftSamus/types.h>
#include <melee/ft/kinds/ftSeak/types.h>
#include <melee/ft/kinds/ftYoshi/types.h>
#include <melee/ft/kinds/ftZelda/types.h>
#include <melee/ft/kinds/ftLink/types.h>
#include <melee/ft/kinds/ftMario/types.h>
#include <melee/ft/types.h>
#include <melee/gm/gmevent.h>
#include <stdio.h>

#include <melee/gr/ground.h>
#include <melee/gr/types.h>
#include <melee/it/forward.h>
#include <melee/it/it_3F14.h>
#include <melee/it/itCharItems.h>
#include <melee/it/itCommonItems.h>
#include <melee/it/types.h>
#include <melee/it/itYoyo.h>
#include <melee/lb/types.h>
#include <melee/mp/types.h>
#include <melee/pl/types.h>
#include <melee/sc/types.h>
#include <melee/sfx/crowdsfx.h>
#include <melee/ty/types.h>

#include <dolphin/os.h>

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* A pointer field of the record at `at`: its target, or 0 with *present
 * false when the field is NULL. */
static mh_u32 target_of(MeleeHostHsdReader* reader, mh_u32 field,
                        bool* present)
{
    mh_u32 target = 0;
    *present = melee_host_hsd_reader_pointer(reader, field, &target);
    return target;
}

static gm_801BAB40_src* event_player(MeleeHostHsdReader* reader, mh_u32 at)
{
    gm_801BAB40_src* const player = melee_host_hsd_reader_allocate(
        reader, sizeof(*player), alignof(gm_801BAB40_src));
    if (player == NULL) {
        return NULL;
    }
    player->c_kind = (s8) melee_host_hsd_reader_u8(reader, at + 0x00);
    player->slot_type = melee_host_hsd_reader_u8(reader, at + 0x01);
    player->stocks = melee_host_hsd_reader_u8(reader, at + 0x02);
    player->color = melee_host_hsd_reader_u8(reader, at + 0x03);
    player->x5 = melee_host_hsd_reader_u8(reader, at + 0x04);
    player->sub_color = melee_host_hsd_reader_u8(reader, at + 0x05);
    player->team = melee_host_hsd_reader_u8(reader, at + 0x06);
    player->xB = melee_host_hsd_reader_u8(reader, at + 0x07);
    player->flags = melee_host_hsd_reader_u8(reader, at + 0x08);
    player->xE = melee_host_hsd_reader_u8(reader, at + 0x09);
    player->cpu_level = melee_host_hsd_reader_u8(reader, at + 0x0A);
    player->pad = melee_host_hsd_reader_u8(reader, at + 0x0B);
    player->x12 = melee_host_hsd_reader_u16(reader, at + 0x0C);
    player->hp = melee_host_hsd_reader_u16(reader, at + 0x0E);
    player->x18 = melee_host_hsd_reader_f32(reader, at + 0x10);
    player->x1C = melee_host_hsd_reader_f32(reader, at + 0x14);
    player->x20 = melee_host_hsd_reader_f32(reader, at + 0x18);
    return player;
}

static struct gm_evinit* event_init(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct gm_evinit* const init = melee_host_hsd_reader_allocate(
        reader, sizeof(*init), alignof(struct gm_evinit));
    const mh_u8 byte0 = melee_host_hsd_reader_u8(reader, at + 0x00);
    const mh_u8 byte1 = melee_host_hsd_reader_u8(reader, at + 0x01);
    int i;

    if (init == NULL) {
        return NULL;
    }
    /* MWCC allocates bit-fields from the most significant bit down, so the
     * first field is the top of the byte.  The host compiler allocates them
     * its own way, which is why they are assigned by name. */
    init->x0_0 = (u32) (byte0 >> 5) & 7U;
    init->x0_3 = (u32) (byte0 >> 2) & 7U;
    init->x0_6 = (u32) (byte0 >> 1) & 1U;
    init->x0_7 = (u32) byte0 & 1U;
    init->x1_0 = (u32) (byte1 >> 7) & 1U;
    init->x1_1 = (u32) (byte1 >> 6) & 1U;
    init->x1_2 = (u32) (byte1 >> 5) & 1U;
    init->x1_3 = (u32) (byte1 >> 4) & 1U;
    init->x1_4 = (u32) (byte1 >> 3) & 1U;
    init->x1_5 = (u32) byte1 & 7U;
    init->is_teams = melee_host_hsd_reader_u8(reader, at + 0x02);
    init->item_freq = (s8) melee_host_hsd_reader_u8(reader, at + 0x03);
    init->sd_penalty = (s8) melee_host_hsd_reader_u8(reader, at + 0x04);
    init->unk5 = melee_host_hsd_reader_u8(reader, at + 0x05);
    init->stkind = melee_host_hsd_reader_u16(reader, at + 0x06);
    init->time_limit = melee_host_hsd_reader_u32(reader, at + 0x08);
    for (i = 0; i < 4; i++) {
        init->padC[i] = melee_host_hsd_reader_u8(reader, at + 0x0C + i);
    }
    init->x10 = ((u64) melee_host_hsd_reader_u32(reader, at + 0x10) << 32) |
                melee_host_hsd_reader_u32(reader, at + 0x14);
    init->x18 = (s32) melee_host_hsd_reader_u32(reader, at + 0x18);
    init->x1C = melee_host_hsd_reader_f32(reader, at + 0x1C);
    init->game_speed = melee_host_hsd_reader_f32(reader, at + 0x20);
    init->unk24 = melee_host_hsd_reader_f32(reader, at + 0x24);
    return init;
}

static struct gm_evbonus* event_bonus(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct gm_evbonus* const bonus = melee_host_hsd_reader_allocate(
        reader, sizeof(*bonus), alignof(struct gm_evbonus));
    if (bonus == NULL) {
        return NULL;
    }
    bonus->c_kind = (s8) melee_host_hsd_reader_u8(reader, at + 0x00);
    bonus->x1 = melee_host_hsd_reader_u8(reader, at + 0x01);
    bonus->x2 = melee_host_hsd_reader_u8(reader, at + 0x02);
    bonus->x3 = melee_host_hsd_reader_u8(reader, at + 0x03);
    bonus->x4 = melee_host_hsd_reader_u8(reader, at + 0x04);
    bonus->x5 = melee_host_hsd_reader_u8(reader, at + 0x05);
    bonus->color = melee_host_hsd_reader_u8(reader, at + 0x06);
    bonus->pad7 = melee_host_hsd_reader_u8(reader, at + 0x07);
    bonus->x8 = melee_host_hsd_reader_f32(reader, at + 0x08);
    bonus->xC = melee_host_hsd_reader_f32(reader, at + 0x0C);
    bonus->x10 = melee_host_hsd_reader_f32(reader, at + 0x10);
    bonus->flags = melee_host_hsd_reader_u8(reader, at + 0x14);
    bonus->x15 = melee_host_hsd_reader_u8(reader, at + 0x15);
    bonus->x16 = melee_host_hsd_reader_u8(reader, at + 0x16);
    bonus->x17 = melee_host_hsd_reader_u8(reader, at + 0x17);
    return bonus;
}

static struct gm_evstage_table* event_stages(MeleeHostHsdReader* reader,
                                             mh_u32 at)
{
    struct gm_evstage_table* const stages = melee_host_hsd_reader_allocate(
        reader, sizeof(*stages), alignof(struct gm_evstage_table));
    int i;

    if (stages == NULL) {
        return NULL;
    }
    stages->count = melee_host_hsd_reader_u8(reader, at + 0x00);
    stages->pad1 = melee_host_hsd_reader_u8(reader, at + 0x01);
    for (i = 0; i < 7; i++) {
        stages->stage[i] =
            melee_host_hsd_reader_u16(reader, at + 0x02 + (mh_u32) i * 2);
    }
    for (i = 0; i < GM_MAX_PLAYERS; i++) {
        bool present;
        const mh_u32 player =
            target_of(reader, at + 0x10 + (mh_u32) i * 4, &present);
        stages->entries[i] = present ? event_player(reader, player) : NULL;
    }
    return stages;
}

static struct gm_804D6900_t* event_level(MeleeHostHsdReader* reader,
                                         mh_u32 at)
{
    struct gm_804D6900_t* const level = melee_host_hsd_reader_allocate(
        reader, sizeof(*level), alignof(struct gm_804D6900_t));
    bool present;
    mh_u32 target;
    int i;

    if (level == NULL) {
        return NULL;
    }
    level->kind = melee_host_hsd_reader_u8(reader, at + 0x00);
    level->flags = melee_host_hsd_reader_u8(reader, at + 0x01);
    level->pad2[0] = melee_host_hsd_reader_u8(reader, at + 0x02);
    level->pad2[1] = melee_host_hsd_reader_u8(reader, at + 0x03);

    /* x4 is each event's own parameters, and their shape changes with the
     * event: two ints, a coin count, three floats, a character list ending in
     * ChKind_Max, a list of ints, and in one level an int and a pointer.  Only
     * that event's code knows which, and it reads the bytes in place, so no
     * layout given here would be right for all of them.  The field is checked
     * and left NULL; everything that reads it is event mode code, which the
     * host's mode table does not have yet. */
    (void) target_of(reader, at + 0x04, &present);
    level->x4 = NULL;

    target = target_of(reader, at + 0x08, &present);
    level->evinit = present ? event_init(reader, target) : NULL;
    target = target_of(reader, at + 0x0C, &present);
    level->evbonus = present ? event_bonus(reader, target) : NULL;
    target = target_of(reader, at + 0x10, &present);
    level->evstage_table = present ? event_stages(reader, target) : NULL;
    for (i = 0; i < 5; i++) {
        target = target_of(reader, at + 0x14 + (mh_u32) i * 4, &present);
        level->player_init[i] = present ? event_player(reader, target) : NULL;
    }
    return level;
}

/* sqEventInitDataLevelTbl: one pointer per event, as many as are relocated in
 * a row.  GmEvent.dat has 51, the number gm_801BEBC0 searches. */
static void* event_level_table(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 data_size = melee_host_hsd_reader_data_size(reader);
    struct gm_804D6900_t** table;
    mh_u32 count = 0;
    mh_u32 i;

    while (root + (count + 1) * 4 <= data_size &&
           melee_host_hsd_reader_has_pointer(reader, root + count * 4))
    {
        count++;
    }
    if (count == 0) {
        melee_host_hsd_reader_fail(reader, "the event level table is empty");
        return NULL;
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(*table) * count,
                                           alignof(struct gm_804D6900_t*));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        bool present;
        const mh_u32 level = target_of(reader, root + i * 4, &present);
        table[i] = present ? event_level(reader, level) : NULL;
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

/* lbAudioLoadData in LbAd.dat: four tables, one per pairing of the language
 * setting and the saved language, each with the 30 lists lbAudioAx_80023968
 * accepts.  A list holds sound bank ids and ends in 0x83D60; the game reads
 * the ids in place, so the host converts each list to its own byte order,
 * terminator included.  Some lists start inside others, and each one is
 * walked to its own terminator as the game walks it. */
enum {
    AUDIO_LOAD_TABLES = 4,
    AUDIO_LOAD_LISTS = 30,
    AUDIO_LOAD_END = 0x83D60,
};

static int* audio_load_list(MeleeHostHsdReader* reader, mh_u32 at)
{
    const mh_u32 data_size = melee_host_hsd_reader_data_size(reader);
    mh_u32 count = 0;
    int* list;
    mh_u32 i;

    for (;;) {
        if (at + (count + 1) * 4 > data_size) {
            melee_host_hsd_reader_fail(
                reader, "a sound bank list runs past the data section");
            return NULL;
        }
        count++;
        if (melee_host_hsd_reader_u32(reader, at + (count - 1) * 4) ==
            AUDIO_LOAD_END)
        {
            break;
        }
    }
    list = melee_host_hsd_reader_allocate(reader, sizeof(*list) * count,
                                          alignof(int));
    if (list == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        list[i] = (int) melee_host_hsd_reader_u32(reader, at + i * 4);
    }
    return list;
}

static void* audio_load_data(MeleeHostHsdReader* reader, mh_u32 root)
{
    /* Laid out as the struct lbaudio_ax.c declares: four int** in a row. */
    int*** tables;
    int t;

    tables = melee_host_hsd_reader_allocate(
        reader, sizeof(*tables) * AUDIO_LOAD_TABLES, alignof(int**));
    if (tables == NULL) {
        return NULL;
    }
    for (t = 0; t < AUDIO_LOAD_TABLES; t++) {
        bool present;
        const mh_u32 table = target_of(reader, root + (mh_u32) t * 4, &present);
        int i;

        if (!present) {
            melee_host_hsd_reader_fail(reader, "a sound bank table is NULL");
            return NULL;
        }
        tables[t] = melee_host_hsd_reader_allocate(
            reader, sizeof(*tables[t]) * AUDIO_LOAD_LISTS, alignof(int*));
        if (tables[t] == NULL) {
            return NULL;
        }
        for (i = 0; i < AUDIO_LOAD_LISTS; i++) {
            const mh_u32 list =
                target_of(reader, table + (mh_u32) i * 4, &present);
            tables[t][i] = present ? audio_load_list(reader, list) : NULL;
        }
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : tables;
}

/* MemCardIconData in LbMcGame and MemSnapIconData in LbMcSnap: the banners and
 * the icon a save file carries, as the addresses of image bytes the card layer
 * copies into the file unchanged, so the bytes stay verbatim.  The game reads
 * the entries as ints (lbcardgame.static.h, the union in lbsnap.c), which on
 * the host are pointer-wide; the card write path narrows them again, and only
 * runs with a card in the slot.  The table runs as far as its fields are
 * relocated, and the NULL word after it is kept. */
static void* card_icon_table(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 data_size = melee_host_hsd_reader_data_size(reader);
    intptr_t* table;
    mh_u32 count = 0;
    mh_u32 i;

    while (root + (count + 1) * 4 <= data_size &&
           melee_host_hsd_reader_has_pointer(reader, root + count * 4))
    {
        count++;
    }
    if (count == 0) {
        melee_host_hsd_reader_fail(reader, "the icon table is empty");
        return NULL;
    }
    table = melee_host_hsd_reader_allocate(
        reader, sizeof(*table) * (count + 1), alignof(intptr_t));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        bool present;
        const mh_u32 image = target_of(reader, root + i * 4, &present);
        table[i] = present ? (intptr_t) melee_host_hsd_reader_payload(
                                 reader, image, 1)
                           : 0;
    }
    table[count] = 0;
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

/* lbRefData (LbRf.dat): two floats per refraction kind, the period and the
 * strength lbRefract_80021CE8 bends the texture behind a refracting object
 * with.  lbrefract.c declares the record privately as a count byte and a
 * pointer to the floats; this is the same layout, with the floats converted
 * from big-endian.  On disk the count is 3 and the six floats sit before the
 * record. */
struct RefractData {
    u8 count;
    f32* params;
};

static void* refract_data(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 count = melee_host_hsd_reader_u8(reader, root + 0x0);
    bool present;
    const mh_u32 params = target_of(reader, root + 0x4, &present);
    struct RefractData* data;
    mh_u32 i;

    if (!present || count == 0) {
        melee_host_hsd_reader_fail(reader, "the refraction table is empty");
        return NULL;
    }
    data = melee_host_hsd_reader_allocate(reader, sizeof(*data),
                                          alignof(struct RefractData));
    if (data == NULL) {
        return NULL;
    }
    data->params = melee_host_hsd_reader_allocate(
        reader, sizeof(f32) * count * 2, alignof(f32));
    if (data->params == NULL) {
        return NULL;
    }
    data->count = (u8) count;
    for (i = 0; i < count * 2; i++) {
        data->params[i] = melee_host_hsd_reader_f32(reader, params + i * 4);
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : data;
}

/* plLoadCommonData (PdPm.dat): the thresholds the bonus and trick code
 * compares a match's stats against (plbonus.c, pltrick.c, pl_040D.c,
 * gm_16F1.c).  The symbol is a pointer to the table, which Player_80036DD8
 * dereferences into pl_804D6470.  Every field of pl_804D6470_t is a 4-byte
 * float or integer, so the table has the same offsets on the host and each
 * word is converted from big-endian in place.  The exception is xC0, which the
 * decomp types as four bytes and nothing reads: bytes keep their order.  On
 * disk the table sits at data+0 and the pointer after it. */
_Static_assert(sizeof(pl_804D6470_t) == 0x184,
               "pl_804D6470_t must keep the PowerPC offsets on the host");

static void* player_common_data(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 bytes_field = offsetof(pl_804D6470_t, xC0);
    bool present;
    const mh_u32 source = target_of(reader, root + 0x0, &present);
    pl_804D6470_t** record;
    pl_804D6470_t* table;
    mh_u32 at;
    mh_u32 i;

    if (!present) {
        melee_host_hsd_reader_fail(reader,
                                   "the player common data pointer is NULL");
        return NULL;
    }
    record = melee_host_hsd_reader_allocate(reader, sizeof(*record),
                                            alignof(pl_804D6470_t*));
    table = melee_host_hsd_reader_allocate(reader, sizeof(*table),
                                           alignof(pl_804D6470_t));
    if (record == NULL || table == NULL) {
        return NULL;
    }
    for (at = 0; at < sizeof(*table); at += 4) {
        if (at == bytes_field) {
            for (i = 0; i < sizeof(table->xC0); i++) {
                table->xC0[i] = melee_host_hsd_reader_u8(reader, source + at + i);
            }
        } else {
            const mh_u32 word = melee_host_hsd_reader_u32(reader, source + at);
            memcpy((unsigned char*) table + at, &word, sizeof(word));
        }
    }
    *record = table;
    return melee_host_hsd_reader_failed(reader) ? NULL : record;
}

/* coll_data (Gr*.dat): the stage's collision map, which mpLibLoad copies into
 * its line and joint tables.  Without it the game falls back to an empty map
 * and the fighters fall through the stage.  MapCollData holds three pointers,
 * to the vertices, the lines and the joints, and moves on the host; the three
 * records hold none and keep their PowerPC layout.  On the disc (the 71
 * Gr*.dat) the record is 0x2C bytes, each array fills exactly its count of
 * records up to the next address, no relocation falls inside one, every line
 * names vertices within the count and every joint's vertex range stays within
 * it.  x2C, which nothing reads, would be the bytes after the record and is
 * left zero. */
_Static_assert(sizeof(Vec2) == 0x8 && sizeof(MapLine) == 0x10 &&
                   sizeof(MapJoint) == 0x28,
               "the collision records must keep the PowerPC layout");

enum {
    /* Lines and joints name each other and the vertices by 16-bit index. */
    COLL_DATA_MAX_RECORDS = 0x10000,
};

/* The array a MapCollData pointer field names, checked against its count and
 * against the bytes up to the next address. */
static bool coll_array(MeleeHostHsdReader* reader, mh_u32 field, s32 count,
                       mh_u32 record_size, mh_u32* out_target)
{
    bool present;

    *out_target = target_of(reader, field, &present);
    if (count < 0 || count > COLL_DATA_MAX_RECORDS ||
        (count != 0 && !present) ||
        (present && melee_host_hsd_reader_extent(reader, *out_target) <
                        (mh_u32) count * record_size))
    {
        melee_host_hsd_reader_fail(reader,
                                   "the collision map counts an array its "
                                   "pointer does not hold");
        return false;
    }
    return true;
}

static void* stage_coll_data(MeleeHostHsdReader* reader, mh_u32 root)
{
    const s32 vert_count = (s32) melee_host_hsd_reader_u32(reader, root + 0x4);
    const s32 line_count = (s32) melee_host_hsd_reader_u32(reader, root + 0xC);
    const s32 joint_count =
        (s32) melee_host_hsd_reader_u32(reader, root + 0x28);
    mh_u32 verts_at;
    mh_u32 lines_at;
    mh_u32 joints_at;
    MapCollData* coll;
    s32 i;

    if (!coll_array(reader, root + 0x0, vert_count, 0x8, &verts_at) ||
        !coll_array(reader, root + 0x8, line_count, 0x10, &lines_at) ||
        !coll_array(reader, root + 0x24, joint_count, 0x28, &joints_at))
    {
        return NULL;
    }
    coll = melee_host_hsd_reader_allocate(reader, sizeof(*coll),
                                          alignof(MapCollData));
    if (coll == NULL) {
        return NULL;
    }
    coll->vert_count = vert_count;
    coll->line_count = line_count;
    coll->joint_count = joint_count;
    coll->floor_start = (s16) melee_host_hsd_reader_u16(reader, root + 0x10);
    coll->floor_count = (s16) melee_host_hsd_reader_u16(reader, root + 0x12);
    coll->ceiling_start = (s16) melee_host_hsd_reader_u16(reader, root + 0x14);
    coll->ceiling_count = (s16) melee_host_hsd_reader_u16(reader, root + 0x16);
    coll->right_wall_start =
        (s16) melee_host_hsd_reader_u16(reader, root + 0x18);
    coll->right_wall_count =
        (s16) melee_host_hsd_reader_u16(reader, root + 0x1A);
    coll->left_wall_start =
        (s16) melee_host_hsd_reader_u16(reader, root + 0x1C);
    coll->left_wall_count =
        (s16) melee_host_hsd_reader_u16(reader, root + 0x1E);
    coll->dynamic_start = (s16) melee_host_hsd_reader_u16(reader, root + 0x20);
    coll->dynamic_count = (s16) melee_host_hsd_reader_u16(reader, root + 0x22);
    coll->x2C = 0;

    if (vert_count != 0) {
        coll->verts = melee_host_hsd_reader_allocate(
            reader, sizeof(Vec2) * (size_t) vert_count, alignof(Vec2));
        if (coll->verts == NULL) {
            return NULL;
        }
        for (i = 0; i < vert_count; i++) {
            const mh_u32 at = verts_at + (mh_u32) i * 0x8;
            coll->verts[i].x = melee_host_hsd_reader_f32(reader, at + 0x0);
            coll->verts[i].y = melee_host_hsd_reader_f32(reader, at + 0x4);
        }
    }
    if (line_count != 0) {
        coll->lines = melee_host_hsd_reader_allocate(
            reader, sizeof(MapLine) * (size_t) line_count, alignof(MapLine));
        if (coll->lines == NULL) {
            return NULL;
        }
        for (i = 0; i < line_count; i++) {
            const mh_u32 at = lines_at + (mh_u32) i * 0x10;
            MapLine* const line = &coll->lines[i];
            line->v0_idx = melee_host_hsd_reader_u16(reader, at + 0x0);
            line->v1_idx = melee_host_hsd_reader_u16(reader, at + 0x2);
            line->prev_id0 = (s16) melee_host_hsd_reader_u16(reader, at + 0x4);
            line->next_id0 = (s16) melee_host_hsd_reader_u16(reader, at + 0x6);
            line->prev_id1 = (s16) melee_host_hsd_reader_u16(reader, at + 0x8);
            line->next_id1 = (s16) melee_host_hsd_reader_u16(reader, at + 0xA);
            line->hi_flags = melee_host_hsd_reader_u16(reader, at + 0xC);
            line->lo_flags = melee_host_hsd_reader_u16(reader, at + 0xE);
            if (line->v0_idx >= vert_count || line->v1_idx >= vert_count) {
                melee_host_hsd_reader_fail(reader,
                                           "a collision line names a vertex "
                                           "past the map's vertices");
                return NULL;
            }
        }
    }
    if (joint_count != 0) {
        coll->joints = melee_host_hsd_reader_allocate(
            reader, sizeof(MapJoint) * (size_t) joint_count,
            alignof(MapJoint));
        if (coll->joints == NULL) {
            return NULL;
        }
        for (i = 0; i < joint_count; i++) {
            const mh_u32 at = joints_at + (mh_u32) i * 0x28;
            MapJoint* const joint = &coll->joints[i];
            joint->floor_start =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x00);
            joint->floor_count =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x02);
            joint->ceiling_start =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x04);
            joint->ceiling_count =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x06);
            joint->right_wall_start =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x08);
            joint->right_wall_count =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x0A);
            joint->left_wall_start =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x0C);
            joint->left_wall_count =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x0E);
            joint->dynamic_start =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x10);
            joint->dynamic_count =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x12);
            joint->left_bound = melee_host_hsd_reader_f32(reader, at + 0x14);
            joint->bottom_bound = melee_host_hsd_reader_f32(reader, at + 0x18);
            joint->right_bound = melee_host_hsd_reader_f32(reader, at + 0x1C);
            joint->top_bound = melee_host_hsd_reader_f32(reader, at + 0x20);
            joint->vtx_start =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x24);
            joint->vtx_count =
                (s16) melee_host_hsd_reader_u16(reader, at + 0x26);
        }
    }
    return coll;
}

/* grGroundParam (Gr*.dat): a stage's scalars, the StageParam rows
 * Ground_801C28CC looks up by StKind, and nine colors.  The rows hold no
 * pointer and keep their 0x64 bytes on the host.  GroundParam holds one
 * pointer, to the rows, so its fields keep their offsets up to it and move
 * after it; every field is read at its PowerPC offset.  On disk (GrSh.dat)
 * the 18 rows end where the parameters begin. */
_Static_assert(offsetof(GroundParam, stage_params) == 0xB0,
               "GroundParam must keep the PowerPC offsets up to its rows");
_Static_assert(sizeof(StageParam) == 0x64 && offsetof(StageParam, x1A) == 0x1A,
               "StageParam must keep the PowerPC layout on the host");

enum {
    /* No stage lists more rows than there are stage kinds. */
    GROUND_PARAM_MAX_STAGE_ROWS = 0x100,
};

static s16 read_s16(MeleeHostHsdReader* reader, mh_u32 at)
{
    return (s16) melee_host_hsd_reader_u16(reader, at);
}

static void stage_param_row(MeleeHostHsdReader* reader, mh_u32 at,
                            StageParam* row)
{
    mh_u32 i;

    row->stkind = (StKind) (s32) melee_host_hsd_reader_u32(reader, at + 0x0);
    row->x4 = (s32) melee_host_hsd_reader_u32(reader, at + 0x4);
    row->x8 = (s32) melee_host_hsd_reader_u32(reader, at + 0x8);
    row->xC = melee_host_hsd_reader_u32(reader, at + 0xC);
    row->x10 = melee_host_hsd_reader_u32(reader, at + 0x10);
    row->x14 = read_s16(reader, at + 0x14);
    row->x16 = read_s16(reader, at + 0x16);
    row->x18 = read_s16(reader, at + 0x18);
    for (i = 0; i < ARRAY_SIZE(row->x1A); i++) {
        row->x1A[i] = read_s16(reader, at + 0x1A + i * 2);
    }
}

static void* ground_param(MeleeHostHsdReader* reader, mh_u32 root)
{
    const s32 count = (s32) melee_host_hsd_reader_u32(reader, root + 0xB4);
    bool present;
    const mh_u32 rows = target_of(reader, root + 0xB0, &present);
    GroundParam* param;
    mh_u32 i;

    if (count < 0 || count > GROUND_PARAM_MAX_STAGE_ROWS ||
        (count != 0 && !present))
    {
        melee_host_hsd_reader_fail(reader,
                                   "the ground parameters list their stage "
                                   "rows wrongly");
        return NULL;
    }
    param = melee_host_hsd_reader_allocate(reader, sizeof(*param),
                                           alignof(GroundParam));
    if (param == NULL) {
        return NULL;
    }
    param->y = melee_host_hsd_reader_f32(reader, root + 0x0);
    param->x4 = read_s16(reader, root + 0x4);
    param->x6_pad[0] = melee_host_hsd_reader_u8(reader, root + 0x6);
    param->x6_pad[1] = melee_host_hsd_reader_u8(reader, root + 0x7);
    param->x8 = read_s16(reader, root + 0x8);
    param->xA = read_s16(reader, root + 0xA);
    param->xC = (s32) melee_host_hsd_reader_u32(reader, root + 0xC);
    param->x10 = (s32) melee_host_hsd_reader_u32(reader, root + 0x10);
    param->x14 = (s32) melee_host_hsd_reader_u32(reader, root + 0x14);
    param->x18 = melee_host_hsd_reader_f32(reader, root + 0x18);
    param->x1C = melee_host_hsd_reader_f32(reader, root + 0x1C);
    param->x20 = melee_host_hsd_reader_f32(reader, root + 0x20);
    param->x24 = melee_host_hsd_reader_f32(reader, root + 0x24);
    param->x28 = melee_host_hsd_reader_f32(reader, root + 0x28);
    param->x2C_pad[0] = melee_host_hsd_reader_u8(reader, root + 0x2C);
    param->x2C_pad[1] = melee_host_hsd_reader_u8(reader, root + 0x2D);
    param->x2E = read_s16(reader, root + 0x2E);
    param->x30 = (s32) melee_host_hsd_reader_u32(reader, root + 0x30);
    param->x34 = (s32) melee_host_hsd_reader_u32(reader, root + 0x34);
    param->x38 = (s32) melee_host_hsd_reader_u32(reader, root + 0x38);
    param->x3C = melee_host_hsd_reader_f32(reader, root + 0x3C);
    param->x40 = melee_host_hsd_reader_f32(reader, root + 0x40);
    param->x44 = melee_host_hsd_reader_f32(reader, root + 0x44);
    param->x48 = melee_host_hsd_reader_f32(reader, root + 0x48);
    /* A bool is one byte on both, followed by padding up to the float. */
    param->x4C_fixed_cam = melee_host_hsd_reader_u8(reader, root + 0x4C) != 0;
    param->x50 = melee_host_hsd_reader_f32(reader, root + 0x50);
    param->x54 = melee_host_hsd_reader_f32(reader, root + 0x54);
    param->x58 = melee_host_hsd_reader_f32(reader, root + 0x58);
    param->x5C = melee_host_hsd_reader_f32(reader, root + 0x5C);
    param->x60 = melee_host_hsd_reader_f32(reader, root + 0x60);
    param->x64 = melee_host_hsd_reader_f32(reader, root + 0x64);
    param->x68 = read_s16(reader, root + 0x68);
    for (i = 0; i < ARRAY_SIZE(param->x6A); i++) {
        param->x6A[i] = read_s16(reader, root + 0x6A + i * 2);
    }
    param->stage_params = NULL;
    if (present) {
        param->stage_params = melee_host_hsd_reader_allocate(
            reader, sizeof(StageParam) * (size_t) (count != 0 ? count : 1),
            alignof(StageParam));
        if (param->stage_params == NULL) {
            return NULL;
        }
        for (i = 0; i < (mh_u32) count; i++) {
            stage_param_row(reader, rows + i * 0x64, &param->stage_params[i]);
        }
    }
    param->stage_param_count = count;
    {
        GXColor* const colors[] = {
            &param->xB8, &param->xBC, &param->xC0, &param->xC4, &param->xC8,
            &param->xCC, &param->xD0, &param->xD4, &param->xD8,
        };
        for (i = 0; i < ARRAY_SIZE(colors); i++) {
            const mh_u32 at = root + 0xB8 + i * 4;
            colors[i]->r = melee_host_hsd_reader_u8(reader, at + 0);
            colors[i]->g = melee_host_hsd_reader_u8(reader, at + 1);
            colors[i]->b = melee_host_hsd_reader_u8(reader, at + 2);
            colors[i]->a = melee_host_hsd_reader_u8(reader, at + 3);
        }
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : param;
}

/* The trophy tables of TyDatai.usd, which Toy_803124BC loads for the trophy
 * display.  None holds a pointer, so each keeps its PowerPC layout, and none
 * records its length: every one ends with a row whose first field is -1, which
 * the game walks to and, when a search finds nothing, reads.  The row is copied
 * whole.  The model and sort tables are also indexed by trophy up to
 * TY_TROPHY_COUNT, so they must hold that many rows before the end. */
_Static_assert(sizeof(TrophyData) == 0x24, "TrophyData keeps its layout");
_Static_assert(sizeof(TyDspEntry) == 0x10, "TyDspEntry keeps its layout");
_Static_assert(sizeof(ToyNameData) == 0xC, "ToyNameData keeps its layout");

enum {
    TROPHY_TABLE_MAX_ROWS = 0x1000,
};

/* The rows before the one whose first word, `width` bytes wide, is -1. */
static mh_u32 rows_before_end(MeleeHostHsdReader* reader, mh_u32 root,
                              mh_u32 row_size, mh_u32 width,
                              mh_u32 required)
{
    mh_u32 rows;

    for (rows = 0; rows < TROPHY_TABLE_MAX_ROWS; rows++) {
        const mh_u32 at = root + rows * row_size;
        const bool end = width == 2
            ? melee_host_hsd_reader_u16(reader, at) == 0xFFFF
            : melee_host_hsd_reader_u32(reader, at) == 0xFFFFFFFF;
        if (melee_host_hsd_reader_failed(reader)) {
            return 0;
        }
        if (end) {
            break;
        }
    }
    if (rows == TROPHY_TABLE_MAX_ROWS) {
        melee_host_hsd_reader_fail(reader, "the trophy table has no end row");
        return 0;
    }
    if (rows < required) {
        melee_host_hsd_reader_fail(reader,
                                   "the trophy table has fewer rows than "
                                   "trophies");
        return 0;
    }
    return rows;
}

static void* trophy_model_rows(MeleeHostHsdReader* reader, mh_u32 root,
                               mh_u32 required)
{
    const mh_u32 rows = rows_before_end(reader, root, 0x24, 4, required);
    TrophyData* table;
    mh_u32 i;

    if (melee_host_hsd_reader_failed(reader)) {
        return NULL;
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(*table) * (rows + 1),
                                           alignof(TrophyData));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i <= rows; i++) {
        const mh_u32 at = root + i * 0x24;
        TrophyData* const row = &table[i];
        row->id = (s32) melee_host_hsd_reader_u32(reader, at + 0x00);
        row->x04 = (s32) melee_host_hsd_reader_u32(reader, at + 0x04);
        row->x08 = melee_host_hsd_reader_f32(reader, at + 0x08);
        row->x0C = melee_host_hsd_reader_f32(reader, at + 0x0C);
        row->x10 = melee_host_hsd_reader_f32(reader, at + 0x10);
        row->x14 = melee_host_hsd_reader_f32(reader, at + 0x14);
        row->x18 = melee_host_hsd_reader_f32(reader, at + 0x18);
        row->x1C = melee_host_hsd_reader_f32(reader, at + 0x1C);
        row->x20 = (s8) melee_host_hsd_reader_u8(reader, at + 0x20);
        row->x21 = (s8) melee_host_hsd_reader_u8(reader, at + 0x21);
        row->x22 = (s8) melee_host_hsd_reader_u8(reader, at + 0x22);
        row->x23 = (s8) melee_host_hsd_reader_u8(reader, at + 0x23);
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

/* tyInitModelTbl: one model row per trophy.  toy.c also counts through it by
 * TY_TROPHY_COUNT. */
static void* trophy_init_models(MeleeHostHsdReader* reader, mh_u32 root)
{
    return trophy_model_rows(reader, root, TY_TROPHY_COUNT);
}

/* tyInitModelDTbl: the few rows that differ in another language, only ever
 * searched to the end row. */
static void* trophy_init_models_other(MeleeHostHsdReader* reader, mh_u32 root)
{
    return trophy_model_rows(reader, root, 0);
}

/* tyModelSortTbl: six sort keys per trophy, read by trophy index. */
static void* trophy_sort_keys(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 rows = rows_before_end(reader, root, 0xC, 2, TY_TROPHY_COUNT);
    ToyNameData* table;
    mh_u32 i;

    if (melee_host_hsd_reader_failed(reader)) {
        return NULL;
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(*table) * (rows + 1),
                                           alignof(ToyNameData));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i <= rows; i++) {
        const mh_u32 at = root + i * 0xC;
        table[i].x0 = read_s16(reader, at + 0x0);
        table[i].x2 = read_s16(reader, at + 0x2);
        table[i].x4 = read_s16(reader, at + 0x4);
        table[i].x6 = read_s16(reader, at + 0x6);
        table[i].x8 = read_s16(reader, at + 0x8);
        table[i].xA = read_s16(reader, at + 0xA);
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

/* tyExpDifferentTbl and tyNoGetUsTbl: trophy numbers ending in -1. */
static void* trophy_number_list(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 rows = rows_before_end(reader, root, 2, 2, 0);
    s16* list;
    mh_u32 i;

    if (melee_host_hsd_reader_failed(reader)) {
        return NULL;
    }
    list = melee_host_hsd_reader_allocate(reader, sizeof(*list) * (rows + 1),
                                          alignof(s16));
    if (list == NULL) {
        return NULL;
    }
    for (i = 0; i <= rows; i++) {
        list[i] = read_s16(reader, root + i * 2);
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : list;
}

/* tyDisplayModelTbl and tyDisplayModelUsTbl: how each trophy stands in the
 * display, searched by trophy number to the end row. */
static void* trophy_display_rows(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 rows = rows_before_end(reader, root, 0x10, 4, 0);
    TyDspEntry* table;
    mh_u32 i;

    if (melee_host_hsd_reader_failed(reader)) {
        return NULL;
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(*table) * (rows + 1),
                                           alignof(TyDspEntry));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i <= rows; i++) {
        const mh_u32 at = root + i * 0x10;
        table[i].x00 = (s32) melee_host_hsd_reader_u32(reader, at + 0x0);
        table[i].x04 = melee_host_hsd_reader_u8(reader, at + 0x4);
        table[i].x05 = melee_host_hsd_reader_u8(reader, at + 0x5);
        table[i].pad_06[0] = melee_host_hsd_reader_u8(reader, at + 0x6);
        table[i].pad_06[1] = melee_host_hsd_reader_u8(reader, at + 0x7);
        table[i].x08 = melee_host_hsd_reader_f32(reader, at + 0x8);
        table[i].x0C = melee_host_hsd_reader_f32(reader, at + 0xC);
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

/* itPublicData (ItCo.usd): the common item data it_8027870C copies into six
 * globals: the shared ItemCommonData, the Article tables of the common items,
 * the character items and the Pokemon (indexed by kind from It_Kind_Capsule,
 * It_Kind_Kuriboh and It_PKind_Start), it_804D6D40_t and the item color
 * animations.
 *
 * An Article's common attributes, hurtboxes, states and model are translated;
 * a state's animations come from the host's materializer and its script is
 * converted for the host command layouts.  Objects several articles share on
 * disk stay shared.  Two parts are left out on purpose: the special
 * attributes, whose layout differs for every item kind, and the dynamics,
 * which item.c reads as ItemDynamics and itcoll.c as ItCollDynamics over the
 * same bytes, two views pointer-wide fields cannot both keep.  Such a field
 * points at melee_host_item_data_left_out, and Item_80267978 stops by name
 * when an item that has one is created. */
char melee_host_item_data_left_out;

_Static_assert(sizeof(ItemCommonData) == 0x160 &&
                   offsetof(ItemCommonData, x48_byte) == 0x48 &&
                   offsetof(ItemCommonData, filler_1a) == 0xE4 &&
                   offsetof(ItemCommonData, filler_1a_2) == 0xEC,
               "ItemCommonData keeps its PowerPC offsets");
_Static_assert(sizeof(ItemAttr) == 0x84 && offsetof(ItemAttr, x3) == 0x2 &&
                   offsetof(ItemAttr, x4_throw_speed_mul) == 0x4,
               "ItemAttr keeps its PowerPC offsets after its flags");
_Static_assert(sizeof(ItHurtBoneDesc) == 0x20,
               "ItHurtBoneDesc keeps its PowerPC layout");
_Static_assert(sizeof(it_804D6D40_t) == 0x1C,
               "it_804D6D40_t keeps its PowerPC layout");

enum {
    ITEM_MEMO_MAX = 0x800,
    ITEM_HURTBOXES_MAX = 0x100,
    ITEM_STATE_SIZE = 0x10,
    ITEM_COLOR_ANIM_SIZE = 0x8,
};

/* What this translation has built, by disk offset. */
static struct {
    mh_u32 offsets[ITEM_MEMO_MAX];
    void* objects[ITEM_MEMO_MAX];
    mh_u32 count;
} item_memo;

static void* item_memo_find(mh_u32 offset)
{
    mh_u32 i;

    for (i = 0; i < item_memo.count; i++) {
        if (item_memo.offsets[i] == offset) {
            return item_memo.objects[i];
        }
    }
    return NULL;
}

static void item_memo_add(MeleeHostHsdReader* reader, mh_u32 offset,
                          void* object)
{
    if (item_memo.count == ITEM_MEMO_MAX) {
        melee_host_hsd_reader_fail(reader,
                                   "the item data holds more objects than the "
                                   "host expects");
        return;
    }
    item_memo.offsets[item_memo.count] = offset;
    item_memo.objects[item_memo.count] = object;
    item_memo.count += 1;
}

/* Words from `begin` to `end` of the record at `source`, converted into the
 * same offsets of `dest`. */
static void copy_words(MeleeHostHsdReader* reader, mh_u32 source, void* dest,
                       mh_u32 begin, mh_u32 end)
{
    mh_u32 at;

    for (at = begin; at < end; at += 4) {
        const mh_u32 word = melee_host_hsd_reader_u32(reader, source + at);
        memcpy((unsigned char*) dest + at, &word, sizeof(word));
    }
}

static void copy_bytes(MeleeHostHsdReader* reader, mh_u32 source, void* dest,
                       mh_u32 begin, mh_u32 end)
{
    mh_u32 at;

    for (at = begin; at < end; at++) {
        ((unsigned char*) dest)[at] =
            melee_host_hsd_reader_u8(reader, source + at);
    }
}

static ItemCommonData* item_common_data(MeleeHostHsdReader* reader, mh_u32 at)
{
    ItemCommonData* const data = melee_host_hsd_reader_allocate(
        reader, sizeof(ItemCommonData), alignof(ItemCommonData));

    if (data == NULL) {
        return NULL;
    }
    copy_words(reader, at, data, 0x0, 0x48);
    data->x48_byte = melee_host_hsd_reader_u8(reader, at + 0x48);
    copy_words(reader, at, data, 0x4C, 0xE4);
    copy_bytes(reader, at, data, 0xE4, 0xE8);
    copy_words(reader, at, data, 0xE8, 0xEC);
    copy_bytes(reader, at, data, 0xEC, 0xF0);
    copy_words(reader, at, data, 0xF0, sizeof(ItemCommonData));
    return data;
}

static ItemAttr* item_attr(MeleeHostHsdReader* reader, mh_u32 at)
{
    ItemAttr* attr = item_memo_find(at);
    mh_u8 flags0;
    mh_u8 flags1;

    if (attr != NULL) {
        return attr;
    }
    attr = melee_host_hsd_reader_allocate(reader, sizeof(*attr),
                                          alignof(ItemAttr));
    if (attr == NULL) {
        return NULL;
    }
    /* MWCC allocates the flag bits from the most significant bit. */
    flags0 = melee_host_hsd_reader_u8(reader, at + 0x0);
    flags1 = melee_host_hsd_reader_u8(reader, at + 0x1);
    attr->x0_is_heavy = (flags0 >> 7) & 1;
    attr->x0_78 = (flags0 >> 3) & 0xF;
    attr->x0_hold_kind = flags0 & 7;
    attr->x1_1 = (flags1 >> 6) & 3;
    attr->x1_3 = (flags1 >> 5) & 1;
    attr->x1_4 = (flags1 >> 4) & 1;
    attr->x1_5 = (flags1 >> 3) & 1;
    attr->x1_67_cam_kind = (flags1 >> 1) & 3;
    attr->x1_8 = flags1 & 1;
    attr->x3 = melee_host_hsd_reader_u8(reader, at + 0x2);
    copy_words(reader, at, attr, 0x4, sizeof(ItemAttr));
    item_memo_add(reader, at, attr);
    return attr;
}

static ItHurtBoneList* item_hurtbones(MeleeHostHsdReader* reader, mh_u32 at)
{
    ItHurtBoneList* list = item_memo_find(at);
    const s32 count = (s32) melee_host_hsd_reader_u32(reader, at + 0x0);
    bool present;
    const mh_u32 descs = target_of(reader, at + 0x4, &present);
    s32 i;

    if (list != NULL) {
        return list;
    }
    if (count < 0 || count > ITEM_HURTBOXES_MAX || (count != 0 && !present)) {
        melee_host_hsd_reader_fail(reader,
                                   "an item's hurtboxes are listed wrongly");
        return NULL;
    }
    list = melee_host_hsd_reader_allocate(reader, sizeof(*list),
                                          alignof(ItHurtBoneList));
    if (list == NULL) {
        return NULL;
    }
    list->count = count;
    list->descs = NULL;
    if (count != 0) {
        list->descs = melee_host_hsd_reader_allocate(
            reader, sizeof(ItHurtBoneDesc) * (size_t) count,
            alignof(ItHurtBoneDesc));
        if (list->descs == NULL) {
            return NULL;
        }
        for (i = 0; i < count; i++) {
            copy_words(reader, descs + (mh_u32) i * 0x20, &list->descs[i], 0,
                       sizeof(ItHurtBoneDesc));
        }
    }
    item_memo_add(reader, at, list);
    return list;
}

/* An item's states: animations, and the script the state runs.  The array
 * records no length; it runs to the next address anything points at, which
 * on the disc is where the next object starts. */
static struct ItemStateDesc* item_states(MeleeHostHsdReader* reader,
                                         mh_u32 at)
{
    struct ItemStateDesc* states = item_memo_find(at);
    const mh_u32 count =
        melee_host_hsd_reader_extent(reader, at) / ITEM_STATE_SIZE;
    mh_u32 i;

    if (states != NULL) {
        return states;
    }
    if (count == 0) {
        melee_host_hsd_reader_fail(reader, "an item has no room for a state");
        return NULL;
    }
    states = melee_host_hsd_reader_allocate(
        reader, sizeof(*states) * count, alignof(struct ItemStateDesc));
    if (states == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        const mh_u32 desc = at + i * ITEM_STATE_SIZE;
        bool present;
        mh_u32 target;

        target = target_of(reader, desc + 0x0, &present);
        states[i].x0_anim_joint =
            present ? melee_host_hsd_reader_anim_joint(reader, target) : NULL;
        target = target_of(reader, desc + 0x4, &present);
        states[i].x4_matanim_joint =
            present ? melee_host_hsd_reader_mat_anim_joint(reader, target)
                    : NULL;
        target = target_of(reader, desc + 0x8, &present);
        states[i].x8_parameters =
            present ? melee_host_hsd_reader_shape_anim_joint(reader, target)
                    : NULL;
        target = target_of(reader, desc + 0xC, &present);
        states[i].xC_script =
            present ? melee_host_hsd_reader_command_stream(reader, target)
                    : NULL;
        if (melee_host_hsd_reader_failed(reader)) {
            return NULL;
        }
    }
    item_memo_add(reader, at, states);
    return states;
}

static ItemModelDesc* item_model(MeleeHostHsdReader* reader, mh_u32 at)
{
    ItemModelDesc* model = item_memo_find(at);
    bool present;
    mh_u32 joint;

    if (model != NULL) {
        return model;
    }
    model = melee_host_hsd_reader_allocate(reader, sizeof(*model),
                                           alignof(ItemModelDesc));
    if (model == NULL) {
        return NULL;
    }
    joint = target_of(reader, at + 0x0, &present);
    model->x0_joint = present ? melee_host_hsd_reader_joint(reader, joint)
                              : NULL;
    model->x4_bone_count = melee_host_hsd_reader_u32(reader, at + 0x4);
    model->x8_bone_attach_id =
        (s32) melee_host_hsd_reader_u32(reader, at + 0x8);
    model->xC_bit_field = melee_host_hsd_reader_u8(reader, at + 0xC);
    item_memo_add(reader, at, model);
    return model;
}

static Article* item_article(MeleeHostHsdReader* reader, mh_u32 at)
{
    Article* article = item_memo_find(at);
    bool present;
    mh_u32 target;

    if (article != NULL) {
        return article;
    }
    article = melee_host_hsd_reader_allocate(reader, sizeof(*article),
                                             alignof(Article));
    if (article == NULL) {
        return NULL;
    }
    target = target_of(reader, at + 0x00, &present);
    article->x0_common_attr = present ? item_attr(reader, target) : NULL;
    (void) target_of(reader, at + 0x04, &present);
    article->x4_specialAttributes =
        present ? &melee_host_item_data_left_out : NULL;
    target = target_of(reader, at + 0x08, &present);
    article->x8_hurtbones = present ? item_hurtbones(reader, target) : NULL;
    target = target_of(reader, at + 0x0C, &present);
    article->xC_itemStates =
        present ? (ItemStateArray*) item_states(reader, target) : NULL;
    target = target_of(reader, at + 0x10, &present);
    article->x10_modelDesc = present ? item_model(reader, target) : NULL;
    (void) target_of(reader, at + 0x14, &present);
    article->x14_dynamics =
        present ? (ItemDynamics*) (void*) &melee_host_item_data_left_out
                : NULL;
    item_memo_add(reader, at, article);
    return melee_host_hsd_reader_failed(reader) ? NULL : article;
}

/* The special attributes of common items whose game struct holds only 4-byte
 * scalars, by kind: the bytes the disc block holds and the struct's size.
 * item_article leaves every item's special attributes out because the layout
 * depends on the kind; a kind listed here gets its block, and any other keeps
 * the stop by name in Item_80267978.  The Bob-omb's block ends before the z of
 * its last Vec3, which itbombhei.c never reads; the struct keeps the room. */
static const struct {
    mh_u32 kind;
    mh_u32 disc_size;
    mh_u32 host_size;
} item_scalar_attributes[] = {
    { It_Kind_BombHei, 0x28, sizeof(itBombHeiAttributes) },
};

static void item_common_scalar_attributes(MeleeHostHsdReader* reader,
                                          mh_u32 table, Article** articles)
{
    mh_u32 i;

    for (i = 0; i < sizeof(item_scalar_attributes) /
                        sizeof(item_scalar_attributes[0]);
         i++)
    {
        const mh_u32 kind = item_scalar_attributes[i].kind;
        const mh_u32 disc_size = item_scalar_attributes[i].disc_size;
        bool present;
        const mh_u32 article = target_of(reader, table + kind * 4, &present);
        mh_u32 special;
        mh_u32 word;
        void* block;

        if (!present || articles[kind] == NULL) {
            continue;
        }
        special = target_of(reader, article + 0x04, &present);
        if (!present) {
            continue;
        }
        if (melee_host_hsd_reader_extent(reader, special) < disc_size) {
            melee_host_hsd_reader_fail(
                reader, "an item's special attributes are shorter than their "
                        "host layout");
            return;
        }
        for (word = 0; word < disc_size; word += 4) {
            if (melee_host_hsd_reader_has_pointer(reader, special + word)) {
                melee_host_hsd_reader_fail(
                    reader, "an item's special attributes hold a pointer");
                return;
            }
        }
        block = melee_host_hsd_reader_allocate(
            reader, item_scalar_attributes[i].host_size, alignof(f32));
        if (block == NULL) {
            return;
        }
        copy_words(reader, special, block, 0, disc_size);
        articles[kind]->x4_specialAttributes = block;
    }
}

static Article** item_article_table(MeleeHostHsdReader* reader, mh_u32 at,
                                    mh_u32 count)
{
    Article** const table = melee_host_hsd_reader_allocate(
        reader, sizeof(Article*) * count, alignof(Article*));
    mh_u32 i;

    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        bool present;
        const mh_u32 target = target_of(reader, at + i * 4, &present);
        table[i] = present ? item_article(reader, target) : NULL;
        if (melee_host_hsd_reader_failed(reader)) {
            return NULL;
        }
    }
    return table;
}

/* The color animations lb_800144C8 starts on an item: a script and the
 * priority a newer animation has to reach, indexed by animation number. */
static Fighter_804D653C_t* item_color_anims(MeleeHostHsdReader* reader,
                                           mh_u32 at)
{
    const mh_u32 count =
        melee_host_hsd_reader_extent(reader, at) / ITEM_COLOR_ANIM_SIZE;
    Fighter_804D653C_t* table;
    mh_u32 i;

    if (count == 0) {
        melee_host_hsd_reader_fail(reader, "the item color animations are "
                                           "empty");
        return NULL;
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(*table) * count,
                                           alignof(Fighter_804D653C_t));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        const mh_u32 entry = at + i * ITEM_COLOR_ANIM_SIZE;
        bool present;
        const mh_u32 script = target_of(reader, entry + 0x0, &present);
        table[i].unk =
            present ? melee_host_hsd_reader_command_stream(reader, script)
                    : NULL;
        table[i].unk4 = melee_host_hsd_reader_u8(reader, entry + 0x4);
        table[i].unk5 = melee_host_hsd_reader_u8(reader, entry + 0x5);
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

/* LbBf.dat's lbBgFlashColAnimData: the background flash's color animations,
 * which lb_0219.c hands to lb_800144C8 in the same layout. */
static void* bg_flash_color_anims(MeleeHostHsdReader* reader, mh_u32 root)
{
    return item_color_anims(reader, root);
}

static void* item_public_data(MeleeHostHsdReader* reader, mh_u32 root)
{
    mh_u32 tables[6];
    it_804D6D20_t* data;
    it_804D6D40_t* extra;
    mh_u32 i;

    item_memo.count = 0;
    for (i = 0; i < 6; i++) {
        bool present;
        tables[i] = target_of(reader, root + i * 4, &present);
        if (!present) {
            melee_host_hsd_reader_fail(reader,
                                       "itPublicData lacks one of its tables");
            return NULL;
        }
    }
    data = melee_host_hsd_reader_allocate(reader, sizeof(*data),
                                          alignof(it_804D6D20_t));
    extra = melee_host_hsd_reader_allocate(reader, sizeof(*extra),
                                           alignof(it_804D6D40_t));
    if (data == NULL || extra == NULL) {
        return NULL;
    }
    data->x0 = item_common_data(reader, tables[0]);
    data->x4 = item_article_table(reader, tables[1], It_Kind_Kuriboh);
    if (data->x4 != NULL) {
        item_common_scalar_attributes(reader, tables[1], data->x4);
    }
    data->x8 = item_article_table(reader, tables[2],
                                  It_PKind_Start - It_Kind_Kuriboh);
    data->xC = item_article_table(reader, tables[3],
                                  It_Kind_Old_Kuri - It_PKind_Start);
    copy_words(reader, tables[4], extra, 0, sizeof(it_804D6D40_t));
    data->x10 = extra;
    data->x14 = item_color_anims(reader, tables[5]);
    return melee_host_hsd_reader_failed(reader) ? NULL : data;
}

/* map_head (Gr*.dat): what a stage's models need beyond their joint trees
 * (UnkStageDat).  The models come first, because the light overrides compare
 * their descriptors with the models' lights by address:
 * - each model's joint, NULL-terminated animation tables, camera, lights, fog,
 *   GrJoint list, animation flag bytes (verbatim) and s16 list;
 * - unk0, entries of a model's joint and s16 pairs.  Ground_801C34AC finds a
 *   model's entry by the address of its joint descriptor and keeps the JObjs
 *   the pairs name, the players' start points among them; Ground_801C5940
 *   walks the pairs;
 * - the splines Ground_801C247C hands out;
 * - the light overrides: a descriptor and three flags from the most
 *   significant bit.  The entries also name materials and other records, which
 *   can never equal a light; those keep a unique address in the payload;
 * - the materials grDatFiles_801C6228 marks, the models' own HSD_MObjDesc.
 * Left out, because nothing in the game reads them: each model's x14 and the
 * unk20 table. */
_Static_assert(sizeof(GrJoint) == 6, "GrJoint keeps its PowerPC layout");

/* ground.c declares these records locally; the same declarations give the
 * host the same layout. */
typedef struct HostLightOverrideEntry {
    HSD_LightDesc* desc;
    u8 a : 1;
    u8 b : 1;
    u8 c : 1;
    u8 _ : 5;
    u8 _pad[3];
} HostLightOverrideEntry;

typedef struct HostStagePairs {
    struct HSD_Joint* joint;
    struct {
        s16 a, b;
    }* unk4;
    s32 unk8;
} HostStagePairs;

/* Ground_801C5940 declares the joint as four bytes of padding; the pointer
 * after it lands where Ground_801C34AC's does. */
_Static_assert(offsetof(HostStagePairs, unk4) ==
                   offsetof(struct { u8 x0_pad[0x4]; void* unk4; }, unk4),
               "both ground.c views of a pair entry agree on the host");

enum {
    STAGE_TABLE_MAX = 0x400,
    STAGE_MODEL_SIZE = 0x34,
};

static bool stage_count_ok(MeleeHostHsdReader* reader, s32 count,
                           bool present, const char* what)
{
    if (count < 0 || count > STAGE_TABLE_MAX || (count != 0 && !present)) {
        melee_host_hsd_reader_fail(reader, what);
        return false;
    }
    return true;
}

/* The stage's animation tables are indexed twice: the outer table picks an
 * animation set, and grAnime_801C7C1C then takes the entry for one joint of
 * the model out of it with `aj = &aj[joint]`.  So each entry is an array of
 * animation joints on disc, one per joint of the model, not a single tree -
 * building only the root leaves every index past the first reading whatever
 * follows it in the host's arena.
 *
 * The count comes from the model's own joint tree, which is materialized
 * before the tables and has one node per index the game can ask for.  Each
 * element is built as its own tree, which the reader shares by address, and
 * its root is copied into the array so the elements sit contiguously the way
 * the game indexes them. */
static mh_u32 stage_joint_count(const HSD_Joint* joint)
{
    mh_u32 count = 0;

    for (; joint != NULL; joint = joint->next) {
        count += 1 + stage_joint_count(joint->child);
    }
    return count;
}

/* Console strides; the host's structs are wider, which is the whole point. */
#define STAGE_ANIM_JOINT_STRIDE 0x14
#define STAGE_MAT_ANIM_JOINT_STRIDE 0x0C
#define STAGE_SHAPE_ANIM_JOINT_STRIDE 0x0C

static void* stage_anim_array(MeleeHostHsdReader* reader, mh_u32 target,
                              mh_u32 count, mh_u32 stride, size_t host_size,
                              void* (*build)(MeleeHostHsdReader*, mh_u32))
{
    unsigned char* array;
    mh_u32 i;

    if (count == 0) {
        count = 1;
    }
    array = melee_host_hsd_reader_allocate(reader, host_size * count,
                                           alignof(void*));
    if (array == NULL) {
        return NULL;
    }
    memset(array, 0, host_size * count);
    for (i = 0; i < count; i++) {
        void* const one = build(reader, target + i * stride);

        if (one == NULL) {
            break;
        }
        memcpy(array + (size_t) i * host_size, one, host_size);
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : array;
}

/* A NULL-terminated table of pointers, each entry an array of `count`
 * animation joints built by `build`. */
static void** stage_anim_table(MeleeHostHsdReader* reader, mh_u32 at,
                               mh_u32 count, mh_u32 stride, size_t host_size,
                               void* (*build)(MeleeHostHsdReader*, mh_u32))
{
    const mh_u32 limit = melee_host_hsd_reader_extent(reader, at) / 4;
    mh_u32 entries = 0;
    void** table;
    mh_u32 i;

    for (entries = 0; entries < limit; entries++) {
        bool present;
        (void) target_of(reader, at + entries * 4, &present);
        if (!present) {
            break;
        }
    }
    table = melee_host_hsd_reader_allocate(
        reader, sizeof(void*) * (entries + 1), alignof(void*));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < entries; i++) {
        bool present;
        const mh_u32 target = target_of(reader, at + i * 4, &present);
        table[i] = stage_anim_array(reader, target, count, stride, host_size,
                                    build);
    }
    table[entries] = NULL;
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

/* A NULL-terminated table of pointers, each entry built by `build`, bounded by
 * the block's extent; the terminator is kept. */
static void** stage_pointer_table(MeleeHostHsdReader* reader, mh_u32 at,
                                  void* (*build)(MeleeHostHsdReader*, mh_u32))
{
    const mh_u32 limit = melee_host_hsd_reader_extent(reader, at) / 4;
    mh_u32 count = 0;
    void** table;
    mh_u32 i;

    for (count = 0; count < limit; count++) {
        bool present;
        (void) target_of(reader, at + count * 4, &present);
        if (!present) {
            break;
        }
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(void*) * (count + 1),
                                           alignof(void*));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        bool present;
        const mh_u32 target = target_of(reader, at + i * 4, &present);
        table[i] = build(reader, target);
    }
    table[count] = NULL;
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

static s16* stage_s16_list(MeleeHostHsdReader* reader, mh_u32 at, s32 count)
{
    s16* const list = melee_host_hsd_reader_allocate(
        reader, sizeof(s16) * (size_t) count, alignof(s16));
    s32 i;

    if (list == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        list[i] = read_s16(reader, at + (mh_u32) i * 2);
    }
    return list;
}

static void stage_model(MeleeHostHsdReader* reader, mh_u32 at,
                        struct UnkStageDat_x8_t* model)
{
    bool present;
    mh_u32 target;
    mh_u32 joints;
    s32 count;
    s32 i;

    target = target_of(reader, at + 0x00, &present);
    model->unk0 = present ? melee_host_hsd_reader_joint(reader, target) : NULL;
    joints = stage_joint_count(model->unk0);
    target = target_of(reader, at + 0x04, &present);
    model->unk4 = present ? (HSD_AnimJoint**) stage_anim_table(
                                reader, target, joints,
                                STAGE_ANIM_JOINT_STRIDE, sizeof(HSD_AnimJoint),
                                melee_host_hsd_reader_anim_joint)
                          : NULL;
    target = target_of(reader, at + 0x08, &present);
    model->unk8 = present ? (HSD_MatAnimJoint**) stage_anim_table(
                                reader, target, joints,
                                STAGE_MAT_ANIM_JOINT_STRIDE,
                                sizeof(HSD_MatAnimJoint),
                                melee_host_hsd_reader_mat_anim_joint)
                          : NULL;
    target = target_of(reader, at + 0x0C, &present);
    model->unkC =
        present ? (HSD_ShapeAnimJoint**) stage_anim_table(
                      reader, target, joints, STAGE_SHAPE_ANIM_JOINT_STRIDE,
                      sizeof(HSD_ShapeAnimJoint),
                      melee_host_hsd_reader_shape_anim_joint)
                : NULL;
    target = target_of(reader, at + 0x10, &present);
    model->x10 = present ? melee_host_hsd_reader_camera(reader, target) : NULL;
    /* Nothing reads x14; in GrSh.dat it is a block of zeros. */
    (void) target_of(reader, at + 0x14, &present);
    model->x14 = NULL;
    target = target_of(reader, at + 0x18, &present);
    model->x18 =
        present ? melee_host_hsd_reader_light_lists(reader, target) : NULL;
    target = target_of(reader, at + 0x1C, &present);
    model->x1C = present ? melee_host_hsd_reader_fog(reader, target) : NULL;

    count = (s32) melee_host_hsd_reader_u32(reader, at + 0x24);
    target = target_of(reader, at + 0x20, &present);
    model->unk20 = NULL;
    model->unk24 = count;
    if (stage_count_ok(reader, count, present,
                       "a stage model lists its GrJoints wrongly") &&
        count != 0)
    {
        GrJoint* const joints = melee_host_hsd_reader_allocate(
            reader, sizeof(GrJoint) * (size_t) count, alignof(GrJoint));
        if (joints != NULL) {
            for (i = 0; i < count; i++) {
                const mh_u32 joint = target + (mh_u32) i * sizeof(GrJoint);
                joints[i].x = read_s16(reader, joint + 0);
                joints[i].y = read_s16(reader, joint + 2);
                joints[i].z = read_s16(reader, joint + 4);
            }
        }
        model->unk20 = joints;
    }

    target = target_of(reader, at + 0x28, &present);
    model->x28 =
        present ? melee_host_hsd_reader_payload(reader, target, 1) : NULL;

    count = (s32) melee_host_hsd_reader_u32(reader, at + 0x30);
    target = target_of(reader, at + 0x2C, &present);
    model->x2C = NULL;
    model->x30 = count;
    if (stage_count_ok(reader, count, present,
                       "a stage model lists its s16 values wrongly") &&
        count != 0)
    {
        model->x2C = stage_s16_list(reader, target, count);
    }
}

/* map_plit: the stage's NULL-terminated table of LightList, the lights
 * ftCo_09F4.c gives the fighters through Ground_801C49B4.  Light descriptors
 * are the ones map_head's overrides name, shared by address, which is how
 * Ground_801C20E0 finds them. */
static void* stage_light_lists(MeleeHostHsdReader* reader, mh_u32 root)
{
    return melee_host_hsd_reader_light_lists(reader, root);
}

/* quake_model_set: one DynamicModelDesc, the model grlib.c loads for a stage
 * quake, with its joint and three NULL-terminated animation tables. */
static void* stage_quake_model_set(MeleeHostHsdReader* reader, mh_u32 root)
{
    DynamicModelDesc* const model = melee_host_hsd_reader_allocate(
        reader, sizeof(*model), alignof(DynamicModelDesc));
    bool present;
    mh_u32 target;
    mh_u32 joints;

    if (model == NULL) {
        return NULL;
    }
    target = target_of(reader, root + 0x0, &present);
    model->joint = present ? melee_host_hsd_reader_joint(reader, target) : NULL;
    joints = stage_joint_count(model->joint);
    target = target_of(reader, root + 0x4, &present);
    model->anims = present ? (HSD_AnimJoint**) stage_anim_table(
                                 reader, target, joints,
                                 STAGE_ANIM_JOINT_STRIDE, sizeof(HSD_AnimJoint),
                                 melee_host_hsd_reader_anim_joint)
                           : NULL;
    target = target_of(reader, root + 0x8, &present);
    model->matanims = present ? (HSD_MatAnimJoint**) stage_anim_table(
                                    reader, target, joints,
                                    STAGE_MAT_ANIM_JOINT_STRIDE,
                                    sizeof(HSD_MatAnimJoint),
                                    melee_host_hsd_reader_mat_anim_joint)
                              : NULL;
    target = target_of(reader, root + 0xC, &present);
    model->shapeanims = present ? (HSD_ShapeAnimJoint**) stage_anim_table(
                                      reader, target, joints,
                                      STAGE_SHAPE_ANIM_JOINT_STRIDE,
                                      sizeof(HSD_ShapeAnimJoint),
                                      melee_host_hsd_reader_shape_anim_joint)
                                : NULL;
    return melee_host_hsd_reader_failed(reader) ? NULL : model;
}

/* One entry of itemdata: an item kind and the Article Ground_801C0754 creates
 * it from. */
static void* stage_item_entry(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct GroundItemData* const entry = melee_host_hsd_reader_allocate(
        reader, sizeof(*entry), alignof(struct GroundItemData));
    bool present;
    mh_u32 target;

    if (entry == NULL) {
        return NULL;
    }
    entry->unk0 = (s32) melee_host_hsd_reader_u32(reader, at + 0x0);
    target = target_of(reader, at + 0x4, &present);
    entry->unk4 = present ? item_article(reader, target) : NULL;
    return melee_host_hsd_reader_failed(reader) ? NULL : entry;
}

/* itemdata: the stage's NULL-terminated table of items, empty in Hyrule
 * Temple.  An Article leaves its per-kind attributes out, as itPublicData's
 * do, and creating such an item stops with a name. */
static void* stage_item_data(MeleeHostHsdReader* reader, mh_u32 root)
{
    item_memo.count = 0;
    return stage_pointer_table(reader, root, stage_item_entry);
}

/* ALDYakuAll: the state scripts the stage gives the random item.
 * Ground_801C0800 walks the table from index 1 until a NULL entry and writes
 * each script into it_804D6D38's state descriptors, so index 0 is never read
 * and is zero on every stage of the disc.  The scripts are item command
 * streams; in GrSh.dat the only one lives in the words right after
 * yakumono_param. */
static void* stage_yaku_scripts(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 limit = melee_host_hsd_reader_extent(reader, root) / 4;
    void** table;
    mh_u32 count;
    mh_u32 i;
    bool present;

    (void) target_of(reader, root, &present);
    if (limit < 2 || present) {
        melee_host_hsd_reader_fail(
            reader, "ALDYakuAll does not start with the entry the game skips");
        return NULL;
    }
    for (count = 1; count < limit; count++) {
        (void) target_of(reader, root + count * 4, &present);
        if (!present) {
            break;
        }
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(void*) * (count + 1),
                                           alignof(void*));
    if (table == NULL) {
        return NULL;
    }
    table[0] = NULL;
    for (i = 1; i < count; i++) {
        const mh_u32 target = target_of(reader, root + i * 4, &present);
        table[i] = melee_host_hsd_reader_command_stream(reader, target);
    }
    table[count] = NULL;
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

/* yakumono_param: the parameters a stage keeps for itself, read through the
 * struct its own grXXX.c declares.  Every stage has a different one - floats,
 * ints, pairs of u16 packed in a word, and pointers - so the layout is chosen
 * by the stage the game is loading, never by the block's size: several stages
 * share an extent with incompatible fields, and picking by size is what
 * findings R03-R05 of docs/review-fa257ed.md removed.
 *
 * Both files below are generated from the game's own structs by
 * port/tools/gen_yakumono_layout.py; rerun it when a stage's struct changes,
 * and the melee-host-yakumono-layout-generated test says when that is due. */
#include "yakumono_param.h"
#include "yakumono_param.c.inc"

static void* stage_yakumono_param(MeleeHostHsdReader* reader, mh_u32 root)
{
    /* Ground_801C0754 sets stage_info.grkind before grDatFiles_801C6038
     * reads the archive, and that read is what runs this. */
    return stage_yakumono_param_by_kind(reader, root,
                                        melee_host_stage_current_grkind());
}

static void* stage_map_head(MeleeHostHsdReader* reader, mh_u32 root)
{
    UnkStageDat* const dat = melee_host_hsd_reader_allocate(
        reader, sizeof(UnkStageDat), alignof(UnkStageDat));
    bool present;
    mh_u32 target;
    s32 count;
    s32 i;

    if (dat == NULL) {
        return NULL;
    }

    count = (s32) melee_host_hsd_reader_u32(reader, root + 0x0C);
    target = target_of(reader, root + 0x08, &present);
    dat->unkC = count;
    if (!stage_count_ok(reader, count, present,
                        "map_head lists its models wrongly"))
    {
        return NULL;
    }
    if (count != 0) {
        struct UnkStageDat_x8_t* const models = melee_host_hsd_reader_allocate(
            reader, sizeof(*models) * (size_t) count,
            alignof(struct UnkStageDat_x8_t));
        if (models == NULL) {
            return NULL;
        }
        for (i = 0; i < count; i++) {
            stage_model(reader, target + (mh_u32) i * STAGE_MODEL_SIZE,
                        &models[i]);
        }
        dat->unk8 = models;
    }

    count = (s32) melee_host_hsd_reader_u32(reader, root + 0x04);
    target = target_of(reader, root + 0x00, &present);
    dat->unk4 = count;
    if (!stage_count_ok(reader, count, present,
                        "map_head lists its pair tables wrongly"))
    {
        return NULL;
    }
    if (count != 0) {
        HostStagePairs* const entries = melee_host_hsd_reader_allocate(
            reader, sizeof(*entries) * (size_t) count, alignof(HostStagePairs));
        if (entries == NULL) {
            return NULL;
        }
        for (i = 0; i < count; i++) {
            const mh_u32 entry = target + (mh_u32) i * 0xC;
            const s32 pairs = (s32) melee_host_hsd_reader_u32(reader, entry + 0x8);
            bool listed;
            bool joined;
            const mh_u32 list = target_of(reader, entry + 0x4, &listed);
            const mh_u32 joint = target_of(reader, entry + 0x0, &joined);
            /* The model's joint, the same descriptor as the model's unk0. */
            entries[i].joint =
                joined ? melee_host_hsd_reader_joint(reader, joint) : NULL;
            entries[i].unk8 = pairs;
            entries[i].unk4 = NULL;
            if (stage_count_ok(reader, pairs, listed,
                               "map_head lists its s16 pairs wrongly") &&
                pairs != 0)
            {
                entries[i].unk4 = (void*) stage_s16_list(reader, list, pairs * 2);
            }
        }
        dat->unk0 = entries;
    }

    count = (s32) melee_host_hsd_reader_u32(reader, root + 0x14);
    target = target_of(reader, root + 0x10, &present);
    dat->unk14 = count;
    if (!stage_count_ok(reader, count, present,
                        "map_head lists its splines wrongly"))
    {
        return NULL;
    }
    if (count != 0) {
        HSD_Spline** const splines = melee_host_hsd_reader_allocate(
            reader, sizeof(HSD_Spline*) * (size_t) count, alignof(HSD_Spline*));
        if (splines == NULL) {
            return NULL;
        }
        for (i = 0; i < count; i++) {
            bool listed;
            const mh_u32 spline =
                target_of(reader, target + (mh_u32) i * 4, &listed);
            splines[i] =
                listed ? melee_host_hsd_reader_spline(reader, spline) : NULL;
        }
        dat->unk10 = splines;
    }

    count = (s32) melee_host_hsd_reader_u32(reader, root + 0x1C);
    target = target_of(reader, root + 0x18, &present);
    dat->unk1C = count;
    if (!stage_count_ok(reader, count, present,
                        "map_head lists its light overrides wrongly"))
    {
        return NULL;
    }
    if (count != 0) {
        HostLightOverrideEntry* const entries = melee_host_hsd_reader_allocate(
            reader, sizeof(*entries) * (size_t) count,
            alignof(HostLightOverrideEntry));
        if (entries == NULL) {
            return NULL;
        }
        /* unk1C counts twice the entries the table holds, in every stage file
         * on the disc, and find_light_override walks that many, reading the
         * records that follow (the next tables and map_head itself) as
         * entries.  None of those words can equal a light's address; here
         * they get a unique address in the payload that cannot either. */
        for (i = 0; i < count; i++) {
            const mh_u32 entry = target + (mh_u32) i * 8;
            const mh_u8 flags = melee_host_hsd_reader_u8(reader, entry + 0x4);
            HSD_LightDesc* light = NULL;
            if (melee_host_hsd_reader_has_pointer(reader, entry + 0x0)) {
                bool listed;
                const mh_u32 desc = target_of(reader, entry + 0x0, &listed);
                light = melee_host_hsd_reader_light_built_at(reader, desc);
                if (light == NULL) {
                    light = melee_host_hsd_reader_payload(reader, desc, 1);
                }
            } else if (melee_host_hsd_reader_u32(reader, entry + 0x0) != 0) {
                light = melee_host_hsd_reader_payload(reader, entry, 1);
            }
            entries[i].desc = light;
            entries[i].a = (flags >> 7) & 1;
            entries[i].b = (flags >> 6) & 1;
            entries[i].c = (flags >> 5) & 1;
            entries[i]._ = flags & 0x1F;
        }
        dat->unk18 = entries;
    }

    /* Nothing reads the unk20 table. */
    (void) target_of(reader, root + 0x20, &present);
    dat->unk20 = NULL;
    dat->unk24 = 0;

    count = (s32) melee_host_hsd_reader_u32(reader, root + 0x2C);
    target = target_of(reader, root + 0x28, &present);
    dat->unk2C = count;
    if (!stage_count_ok(reader, count, present,
                        "map_head lists its materials wrongly"))
    {
        return NULL;
    }
    if (count != 0) {
        UnkStageDatInternal** const materials = melee_host_hsd_reader_allocate(
            reader, sizeof(UnkStageDatInternal*) * (size_t) count,
            alignof(UnkStageDatInternal*));
        if (materials == NULL) {
            return NULL;
        }
        for (i = 0; i < count; i++) {
            bool listed;
            const mh_u32 mobj =
                target_of(reader, target + (mh_u32) i * 4, &listed);
            materials[i] =
                listed ? melee_host_hsd_reader_mobj(reader, mobj) : NULL;
        }
        dat->unk28 = materials;
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : dat;
}

/* ftLoadCommonData (PlCo.dat): 23 tables Fighter_LoadCommonData copies into
 * globals, in its order.  Most hold only 4-byte scalars and convert word by
 * word, sized by their struct or, for the arrays the game indexes without a
 * recorded length, by the room up to the next address the archive points at:
 * ftCommonData (its colors and x6EC are bytes), the item throw rows, the swing
 * rows, the staling multipliers, the scale, bunny hood, metal and gravity
 * modifiers, the crowd configuration, the CPU distance thresholds and weapon
 * reaches, and the CPU attack lists (ftCo_AttackEntry, 0x24 bytes of
 * scalars).  The rest point at other things:
 * - ftPartsTable and Fighter_804D6540, per fighter kind, point at byte
 *   arrays, which stay as they are;
 * - the two color animation tables are the item format;
 * - Fighter_804D6534 is the respawn platform's joint and animation;
 * - Fighter_804D6530 pairs a Vec2 list with its length, which the game reads
 *   back from the pointer-wide slot after it;
 * - the grab mash and smash charge shake tables are a Vec2 list and its
 *   length;
 * - Fighter_804D6514 and Fighter_804D6504 are joints;
 * - the CPU command scripts are bytes ftCo_800B4880 reads one at a time.
 * Fighter_804D6510 is left NULL: nothing in the game reads it. */
_Static_assert(sizeof(ftCommonData) == 0x818 &&
                   offsetof(ftCommonData, x6DC_colorsByPlayer) == 0x6DC &&
                   offsetof(ftCommonData, x6EC) == 0x6EC &&
                   offsetof(ftCommonData, x7D8) == 0x7D8,
               "ftCommonData keeps its PowerPC offsets");
_Static_assert(sizeof(struct Fighter_804D6524_t) == 0x9C &&
                   sizeof(struct Fighter_804D6520_t) == 0x3C &&
                   sizeof(struct Fighter_804D651C_t) == 0x24 &&
                   sizeof(struct Fighter_804D6518_t) == 0x8 &&
                   sizeof(CrowdConfig) == 0x44,
               "the fighter modifier tables keep their PowerPC layout");

enum {
    FT_COMMON_TABLES = 23,
    FT_VEC2_LIST_MAX = 0x100,
};

/* `size` bytes of 4-byte scalars, converted word by word. */
static void* scalar_block(MeleeHostHsdReader* reader, mh_u32 at, mh_u32 size)
{
    void* block;

    if (size == 0 || size % 4 != 0) {
        melee_host_hsd_reader_fail(reader,
                                   "a table of scalars is not whole words");
        return NULL;
    }
    block = melee_host_hsd_reader_allocate(reader, size, alignof(f32));
    if (block == NULL) {
        return NULL;
    }
    copy_words(reader, at, block, 0, size);
    return block;
}

/* A scalar array with no recorded length, up to the next boundary. */
static void* scalar_extent(MeleeHostHsdReader* reader, mh_u32 at)
{
    return scalar_block(reader, at,
                        melee_host_hsd_reader_extent(reader, at) / 4 * 4);
}

static ftCommonData* fighter_common_data(MeleeHostHsdReader* reader,
                                         mh_u32 at)
{
    ftCommonData* const data = melee_host_hsd_reader_allocate(
        reader, sizeof(ftCommonData), alignof(ftCommonData));

    if (data == NULL) {
        return NULL;
    }
    copy_words(reader, at, data, 0x000, 0x6DC);
    copy_bytes(reader, at, data, 0x6DC, 0x6F0);
    copy_words(reader, at, data, 0x6F0, 0x7D8);
    copy_bytes(reader, at, data, 0x7D8, 0x7DC);
    copy_words(reader, at, data, 0x7DC, sizeof(ftCommonData));
    return data;
}

static Vec2* vec2_list(MeleeHostHsdReader* reader, mh_u32 at, s32 count)
{
    Vec2* list;
    s32 i;

    if (count <= 0 || count > FT_VEC2_LIST_MAX) {
        melee_host_hsd_reader_fail(reader, "a Vec2 list has a bad length");
        return NULL;
    }
    list = melee_host_hsd_reader_allocate(reader, sizeof(Vec2) * (size_t) count,
                                          alignof(Vec2));
    if (list == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        list[i].x = melee_host_hsd_reader_f32(reader, at + (mh_u32) i * 8);
        list[i].y = melee_host_hsd_reader_f32(reader, at + (mh_u32) i * 8 + 4);
    }
    return list;
}

/* A per-kind table of pointers, each entry built by `build`. */
static void** per_entry_table(MeleeHostHsdReader* reader, mh_u32 at,
                              void* (*build)(MeleeHostHsdReader*, mh_u32))
{
    const mh_u32 count = melee_host_hsd_reader_extent(reader, at) / 4;
    void** table;
    mh_u32 i;

    if (count == 0) {
        melee_host_hsd_reader_fail(reader, "a fighter table is empty");
        return NULL;
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(void*) * count,
                                           alignof(void*));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        bool present;
        const mh_u32 target = target_of(reader, at + i * 4, &present);
        table[i] = present ? build(reader, target) : NULL;
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

static void* fighter_parts_entry(MeleeHostHsdReader* reader, mh_u32 at)
{
    FighterPartsTable* const entry = melee_host_hsd_reader_allocate(
        reader, sizeof(*entry), alignof(FighterPartsTable));
    bool present;
    mh_u32 target;

    if (entry == NULL) {
        return NULL;
    }
    target = target_of(reader, at + 0x0, &present);
    entry->joint_to_part =
        present ? melee_host_hsd_reader_payload(reader, target, 1) : NULL;
    target = target_of(reader, at + 0x4, &present);
    entry->part_to_joint =
        present ? melee_host_hsd_reader_payload(reader, target, 1) : NULL;
    entry->parts_num = melee_host_hsd_reader_u32(reader, at + 0x8);
    return entry;
}

static void* fighter_part_records_entry(MeleeHostHsdReader* reader, mh_u32 at)
{
    Fighter_804D6540_t* const entry = melee_host_hsd_reader_allocate(
        reader, sizeof(*entry), alignof(Fighter_804D6540_t));
    bool present;
    const mh_u32 records = target_of(reader, at + 0x0, &present);

    if (entry == NULL) {
        return NULL;
    }
    entry->x0 = present ? melee_host_hsd_reader_payload(reader, records, 1)
                        : NULL;
    entry->x4 = (int) melee_host_hsd_reader_u32(reader, at + 0x4);
    return entry;
}

static void* fighter_attack_list(MeleeHostHsdReader* reader, mh_u32 at)
{
    return scalar_extent(reader, at);
}

static void* fighter_script_bytes(MeleeHostHsdReader* reader, mh_u32 at)
{
    return melee_host_hsd_reader_payload(reader, at, 1);
}

static void** fighter_table_field(MeleeHostHsdReader* reader, mh_u32 field,
                                  void* (*build)(MeleeHostHsdReader*, mh_u32))
{
    bool present;
    const mh_u32 target = target_of(reader, field, &present);
    return present ? per_entry_table(reader, target, build) : NULL;
}

static struct Fighter_804D64FC_t* fighter_cpu_tables(MeleeHostHsdReader* reader,
                                                     mh_u32 at)
{
    struct Fighter_804D64FC_t* const cpu = melee_host_hsd_reader_allocate(
        reader, sizeof(*cpu), alignof(struct Fighter_804D64FC_t));
    bool present;
    mh_u32 target;

    if (cpu == NULL) {
        return NULL;
    }
    cpu->cmdscripts = (u8**) fighter_table_field(reader, at + 0x00,
                                                 fighter_script_bytes);
    cpu->x4 = fighter_table_field(reader, at + 0x04, fighter_attack_list);
    cpu->x8 = fighter_table_field(reader, at + 0x08, fighter_attack_list);
    cpu->xC = (UNK_T*) fighter_table_field(reader, at + 0x0C,
                                           fighter_attack_list);
    cpu->x10 = fighter_table_field(reader, at + 0x10, fighter_attack_list);
    cpu->x14 = fighter_table_field(reader, at + 0x14, fighter_attack_list);
    cpu->x18 = fighter_table_field(reader, at + 0x18, fighter_attack_list);
    cpu->x1C = fighter_table_field(reader, at + 0x1C, fighter_attack_list);
    target = target_of(reader, at + 0x20, &present);
    cpu->x20 = present ? scalar_extent(reader, target) : NULL;
    target = target_of(reader, at + 0x24, &present);
    cpu->x24 = present ? scalar_extent(reader, target) : NULL;
    return melee_host_hsd_reader_failed(reader) ? NULL : cpu;
}

/* Fighter_804D6530: a Vec2 list, then its length in the next slot, which
 * ftCo_DamageFall.c reads back as a pointer cast to an integer. */
static void** fighter_vec2_pairs(MeleeHostHsdReader* reader, mh_u32 at)
{
    const mh_u32 count = melee_host_hsd_reader_extent(reader, at) / 4;
    void** slots;
    mh_u32 i;

    if (count == 0 || count % 2 != 0) {
        melee_host_hsd_reader_fail(reader,
                                   "Fighter_804D6530 does not pair its lists");
        return NULL;
    }
    slots = melee_host_hsd_reader_allocate(reader, sizeof(void*) * count,
                                           alignof(void*));
    if (slots == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i += 2) {
        const s32 length =
            (s32) melee_host_hsd_reader_u32(reader, at + (i + 1) * 4);
        bool present;
        const mh_u32 list = target_of(reader, at + i * 4, &present);
        slots[i] = present ? vec2_list(reader, list, length) : NULL;
        slots[i + 1] = (void*) (intptr_t) length;
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : slots;
}

static struct Fighter_ShakeTable_t*
fighter_shake_table(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct Fighter_ShakeTable_t* const table = melee_host_hsd_reader_allocate(
        reader, sizeof(*table), alignof(struct Fighter_ShakeTable_t));
    const s32 length = (s32) melee_host_hsd_reader_u32(reader, at + 0x4);
    bool present;
    const mh_u32 list = target_of(reader, at + 0x0, &present);

    if (table == NULL) {
        return NULL;
    }
    table->x0 = present ? vec2_list(reader, list, length) : NULL;
    table->x4 = length;
    return table;
}

/* Fighter_804D6534: the respawn platform's joint and its animation. */
static void** fighter_respawn_platform(MeleeHostHsdReader* reader, mh_u32 at)
{
    void** const platform = melee_host_hsd_reader_allocate(
        reader, sizeof(void*) * 2, alignof(void*));
    bool present;
    mh_u32 target;

    if (platform == NULL) {
        return NULL;
    }
    target = target_of(reader, at + 0x0, &present);
    platform[0] = present ? melee_host_hsd_reader_joint(reader, target) : NULL;
    target = target_of(reader, at + 0x4, &present);
    platform[1] =
        present ? melee_host_hsd_reader_anim_joint(reader, target) : NULL;
    return platform;
}

static void* fighter_common_tables(MeleeHostHsdReader* reader, mh_u32 root)
{
    void** const tables = melee_host_hsd_reader_allocate(
        reader, sizeof(void*) * FT_COMMON_TABLES, alignof(void*));
    mh_u32 at[FT_COMMON_TABLES];
    mh_u32 i;

    if (tables == NULL) {
        return NULL;
    }
    for (i = 0; i < FT_COMMON_TABLES; i++) {
        bool present;
        at[i] = target_of(reader, root + i * 4, &present);
        if (!present) {
            melee_host_hsd_reader_fail(reader,
                                       "ftLoadCommonData lacks one of its "
                                       "tables");
            return NULL;
        }
    }
    tables[0] = fighter_common_data(reader, at[0]);
    tables[1] = scalar_extent(reader, at[1]);
    tables[2] = scalar_extent(reader, at[2]);
    tables[3] = scalar_extent(reader, at[3]);
    tables[4] = per_entry_table(reader, at[4], fighter_parts_entry);
    tables[5] = per_entry_table(reader, at[5], fighter_part_records_entry);
    /* The same format as the item color animations. */
    tables[6] = item_color_anims(reader, at[6]);
    tables[7] = item_color_anims(reader, at[7]);
    tables[8] = fighter_respawn_platform(reader, at[8]);
    tables[9] = fighter_vec2_pairs(reader, at[9]);
    tables[10] = fighter_shake_table(reader, at[10]);
    tables[11] = fighter_shake_table(reader, at[11]);
    tables[12] = scalar_block(reader, at[12], sizeof(struct Fighter_804D6524_t));
    tables[13] = scalar_block(reader, at[13], sizeof(struct Fighter_804D6520_t));
    tables[14] = scalar_block(reader, at[14], sizeof(struct Fighter_804D651C_t));
    tables[15] = scalar_block(reader, at[15], sizeof(struct Fighter_804D6518_t));
    tables[16] = melee_host_hsd_reader_joint(reader, at[16]);
    tables[17] = NULL;
    tables[18] = melee_host_hsd_reader_payload(reader, at[18], 1);
    tables[19] = melee_host_hsd_reader_payload(reader, at[19], 1);
    tables[20] = melee_host_hsd_reader_joint(reader, at[20]);
    tables[21] = scalar_block(reader, at[21], sizeof(CrowdConfig));
    tables[22] = fighter_cpu_tables(reader, at[22]);
    return melee_host_hsd_reader_failed(reader) ? NULL : tables;
}

/* ftData<Name> (Pl<Xx>.dat): a fighter's own data, which ftData_8008572C
 * loads into gFtDataList, translated field by field (struct ftData):
 * - scalar blocks: the common attributes (ftCo_DatAttrs, whose last field is
 *   a byte), x34, the x38 rows, the camera box, itPickup, the ledge values of
 *   x44 (six s16, then floats), the Vec2 of x50 and the int list of x54;
 * - the action tables (xC and x14): name, animation offset and size in the
 *   animation archive, converted command script and flags; x10 and x18, the
 *   byte pairs per action, stay as they are;
 * - the parts (x8): the per-costume visibility lookups down to their byte
 *   lists, and the per-costume u16 texture animation indices;
 * - x1C, per-part animation sets (a byte list of parts and animation joints);
 *   x20, whose joint table keeps small integers beside its joint;
 * - x24 and x28, WaitStruct pairs up to the -1 entry, widened to the host
 *   WaitStruct, whose union holds two pointers: getAnimID steps by that
 *   size, reads u.i and returns u.p.x as an enum, whose low half is u.i.x;
 * - the dynamics (bones, their DynamicsDesc and the 0x3C-byte records
 *   lb_80011710 copies), the hurtbox inits, the character's items (item
 *   articles), the sounds and the IK record;
 * - x5C, the metal joint.
 * The special attributes (x4) differ for every character; the translator
 * registered for each character lays them out.  Dynamics that name animation
 * trees are refused until a character needs them. */
_Static_assert(sizeof(ftCo_DatAttrs) == 0x184 &&
                   offsetof(ftCo_DatAttrs, weight_independent_throws_mask) ==
                       0x180,
               "ftCo_DatAttrs keeps its PowerPC layout");
_Static_assert(sizeof(ftHurtboxInit) == 0x28 && sizeof(ftData_x34) == 0x8 &&
                   sizeof(struct ftData_x38) == 0x14 &&
                   sizeof(struct UnkFloat6_Camera) == 0x18 &&
                   sizeof(itPickup) == 0x30 && sizeof(ftData_x44_t) == 0x1C,
               "the fighter scalar records keep their PowerPC layout");
_Static_assert(sizeof(struct lb_00F9_UnkDesc1Inner) == 0x3C,
               "the dynamics records keep their PowerPC layout");
/* ftData_80085FD4 hands out action entries as ftData_80085FD4_ret. */
_Static_assert(sizeof(struct ftData_80085FD4_ret) ==
                       sizeof(Fighter_WaitAnimData) &&
                   offsetof(struct ftData_80085FD4_ret, x8) ==
                       offsetof(Fighter_WaitAnimData, x8) &&
                   offsetof(struct ftData_80085FD4_ret, x14) ==
                       offsetof(Fighter_WaitAnimData, x14),
               "ftData_80085FD4_ret reads Fighter_WaitAnimData on the host");
_Static_assert(sizeof(struct ftFox_DatAttrs) == 0xD4 &&
                   offsetof(struct ftFox_DatAttrs,
                            xB0_FOX_REFLECTOR_REFLECTION) == 0xB0,
               "ftFox_DatAttrs keeps its PowerPC layout");

enum {
    FT_ACTION_SIZE = 0x18,
    FT_VIS_ENTRY_SIZE = 0x8,
    FT_BONE_DYNAMICS_SIZE = 0x18,
    FT_DYNAMICS_RECORD_SIZE = 0x3C,
    FT_HURTBOX_SIZE = 0x28,
    FT_TABLE_MAX = 0x800,
};

static bool fighter_count_ok(MeleeHostHsdReader* reader, s32 count,
                             const char* what)
{
    if (count < 0 || count > FT_TABLE_MAX) {
        melee_host_hsd_reader_fail(reader, what);
        return false;
    }
    return true;
}

static ftCo_DatAttrs* fighter_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftCo_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftCo_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x000, 0x180);
    copy_bytes(reader, at, attrs, 0x180, 0x184);
    return attrs;
}

/* Fox's special attributes: scalars, then a ReflectDesc whose last field is
 * a byte. */
static void* fighter_fox_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct ftFox_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(struct ftFox_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, 0xD0);
    copy_bytes(reader, at, attrs, 0xD0, 0xD4);
    return attrs;
}

/* Mario's disk record is 0x80 bytes.  The host's s32 and ReflectDesc have
 * wider alignment, so fill it member by member rather than copying the PPC
 * offsets into the host struct. */
static void* fighter_mario_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftMario_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftMario_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    memset(attrs, 0, sizeof(*attrs));
    attrs->specials.vel_x_decay = melee_host_hsd_reader_f32(reader, at + 0x00);
    attrs->specials.vel.x = melee_host_hsd_reader_f32(reader, at + 0x04);
    attrs->specials.vel.y = melee_host_hsd_reader_f32(reader, at + 0x08);
    attrs->specials.grav = melee_host_hsd_reader_f32(reader, at + 0x0C);
    attrs->specials.terminal_vel = melee_host_hsd_reader_f32(reader, at + 0x10);
    attrs->specials.cape_kind = melee_host_hsd_reader_u32(reader, at + 0x14);
    attrs->specialhi.freefall_mobility = melee_host_hsd_reader_f32(reader, at + 0x18);
    attrs->specialhi.landing_lag = melee_host_hsd_reader_f32(reader, at + 0x1C);
    attrs->specialhi.reverse_stick_range = melee_host_hsd_reader_f32(reader, at + 0x20);
    attrs->specialhi.momentum_stick_range = melee_host_hsd_reader_f32(reader, at + 0x24);
    attrs->specialhi.angle_diff = melee_host_hsd_reader_f32(reader, at + 0x28);
    attrs->specialhi.vel_x = melee_host_hsd_reader_f32(reader, at + 0x2C);
    attrs->specialhi.grav = melee_host_hsd_reader_f32(reader, at + 0x30);
    attrs->specialhi.vel_mul = melee_host_hsd_reader_f32(reader, at + 0x34);
    attrs->speciallw.vel_y = melee_host_hsd_reader_f32(reader, at + 0x38);
    attrs->speciallw.momentum_x = melee_host_hsd_reader_f32(reader, at + 0x3C);
    attrs->speciallw.air_momentum_x = melee_host_hsd_reader_f32(reader, at + 0x40);
    attrs->speciallw.momentum_x_mul = melee_host_hsd_reader_f32(reader, at + 0x44);
    attrs->speciallw.air_momentum_x_mul = melee_host_hsd_reader_f32(reader, at + 0x48);
    attrs->speciallw.friction_end = melee_host_hsd_reader_f32(reader, at + 0x4C);
    attrs->speciallw.unk0 = (s32) melee_host_hsd_reader_u32(reader, at + 0x50);
    attrs->speciallw.tap_y_vel_max = melee_host_hsd_reader_f32(reader, at + 0x54);
    attrs->speciallw.tap_grav = melee_host_hsd_reader_f32(reader, at + 0x58);
    attrs->speciallw.landing_lag = (s32) melee_host_hsd_reader_u32(reader, at + 0x5C);
    attrs->cape_reflection.x0_bone_id = melee_host_hsd_reader_u32(reader, at + 0x60);
    attrs->cape_reflection.x4_max_damage = (s32) melee_host_hsd_reader_u32(reader, at + 0x64);
    attrs->cape_reflection.x8_offset.x = melee_host_hsd_reader_f32(reader, at + 0x68);
    attrs->cape_reflection.x8_offset.y = melee_host_hsd_reader_f32(reader, at + 0x6C);
    attrs->cape_reflection.x8_offset.z = melee_host_hsd_reader_f32(reader, at + 0x70);
    attrs->cape_reflection.x14_size = melee_host_hsd_reader_f32(reader, at + 0x74);
    attrs->cape_reflection.x18_damage_mul = melee_host_hsd_reader_f32(reader, at + 0x78);
    attrs->cape_reflection.x1C_speed_mul = melee_host_hsd_reader_f32(reader, at + 0x7C);
    return attrs;
}

_Static_assert(sizeof(struct ftLk_DatAttrs) == 0xDC &&
                   offsetof(struct ftLk_DatAttrs, x64) == 0x64 &&
                   offsetof(struct ftLk_DatAttrs, x84) == 0x84 &&
                   offsetof(struct ftLk_DatAttrs, xBC) == 0xBC &&
                   offsetof(struct ftLk_DatAttrs, xC4) == 0xC4,
               "Link's attributes keep their PowerPC offsets");

/* Link's special attributes.  ftCo_0D8E.c reads the same record as
 * ftCo_LinkCatchAttrs by offset, so the host struct keeps the PowerPC layout:
 * words, then the sword trail's colour bytes inside SwordAttrs and the four
 * bytes before the absorb description. */
static void* fighter_link_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct ftLk_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(struct ftLk_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, 0x6C);
    copy_bytes(reader, at, attrs, 0x6C, 0x78);
    copy_words(reader, at, attrs, 0x78, 0xC0);
    copy_bytes(reader, at, attrs, 0xC0, 0xC4);
    copy_words(reader, at, attrs, 0xC4, 0xDC);
    return attrs;
}

static void* fighter_captain_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftCaptain_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftCaptain_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, 0x8C);
    return attrs;
}

static void* fighter_donkey_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftDonkeyAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftDonkeyAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, 0x74);
    return attrs;
}



static void* fighter_drmario_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftDrMarioAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftDrMarioAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_falco_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct ftFox_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(struct ftFox_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_gamewatch_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftGameWatchAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftGameWatchAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_ganon_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftCaptain_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftCaptain_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_kirby_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct ftKb_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(struct ftKb_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_koopa_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftKoopaAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftKoopaAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_luigi_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftLuigiAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftLuigiAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_mars_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    MarsAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(MarsAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_mewtwo_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftMewtwoAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftMewtwoAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_nana_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftIceClimberAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftIceClimberAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_ness_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftNessAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftNessAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_peach_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftPe_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftPe_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_pichu_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftPichuAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftPichuAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_pikachu_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftPikachuAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftPikachuAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_popo_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftIceClimberAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftIceClimberAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_purin_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftPurinAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftPurinAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_samus_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftSs_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftSs_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_seak_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftSeakAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftSeakAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_yoshi_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftYoshiAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftYoshiAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    memset(attrs, 0, sizeof(*attrs));
    
    mh_u32 extent = melee_host_hsd_reader_extent(reader, at);
    mh_u32 copy_size = extent < sizeof(*attrs) ? extent : sizeof(*attrs);
    
    copy_words(reader, at, attrs, 0x00, copy_size & ~3);
    return attrs;
}

static void* fighter_zelda_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftZelda_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftZelda_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_clink_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct ftLk_DatAttrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(struct ftLk_DatAttrs));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_emblem_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    MarsAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(MarsAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static void* fighter_gigakoopa_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftKoopaAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(ftKoopaAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    copy_words(reader, at, attrs, 0x00, sizeof(*attrs) & ~3);
    return attrs;
}

static Fighter_WaitAnimData* fighter_actions(MeleeHostHsdReader* reader,
                                             mh_u32 at)
{
    const mh_u32 count = melee_host_hsd_reader_extent(reader, at) /
                         FT_ACTION_SIZE;
    Fighter_WaitAnimData* actions;
    mh_u32 i;

    if (count == 0 || count > FT_TABLE_MAX) {
        melee_host_hsd_reader_fail(reader, "a fighter action table is empty");
        return NULL;
    }
    actions = melee_host_hsd_reader_allocate(
        reader, sizeof(*actions) * count, alignof(Fighter_WaitAnimData));
    if (actions == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        const mh_u32 entry = at + i * FT_ACTION_SIZE;
        bool present;
        mh_u32 target;

        target = target_of(reader, entry + 0x00, &present);
        actions[i].x0 =
            present ? melee_host_hsd_reader_payload(reader, target, 1) : NULL;
        actions[i].x4 = (s32) melee_host_hsd_reader_u32(reader, entry + 0x04);
        actions[i].x8 = (s32) melee_host_hsd_reader_u32(reader, entry + 0x08);
        target = target_of(reader, entry + 0x0C, &present);
        actions[i].xC = present
                            ? melee_host_hsd_reader_command_stream(reader,
                                                                   target)
                            : NULL;
        actions[i].x10_animCurrFlags =
            (s32) melee_host_hsd_reader_u32(reader, entry + 0x10);
        actions[i].x14 = melee_host_hsd_reader_u32(reader, entry + 0x14);
        if (melee_host_hsd_reader_failed(reader)) {
            return NULL;
        }
    }
    return actions;
}

/* One visibility lookup per model: a count of TempS and the TempS, each a
 * count and a list of DObj indices. */
static FtPartsVisLookup* fighter_vis_lookup(MeleeHostHsdReader* reader,
                                            mh_u32 at, u32 model_num)
{
    FtPartsVisLookup* const lookups = melee_host_hsd_reader_allocate(
        reader, sizeof(*lookups) * model_num, alignof(FtPartsVisLookup));
    u32 i;

    if (lookups == NULL) {
        return NULL;
    }
    for (i = 0; i < model_num; i++) {
        const mh_u32 entry = at + i * FT_VIS_ENTRY_SIZE;
        const s32 count = (s32) melee_host_hsd_reader_u32(reader, entry);
        bool present;
        const mh_u32 list = target_of(reader, entry + 4, &present);
        s32 j;

        lookups[i].x0 = count;
        lookups[i].x4 = NULL;
        if (!fighter_count_ok(reader, count,
                              "a fighter visibility lookup has a bad count") ||
            !present || count == 0)
        {
            continue;
        }
        lookups[i].x4 = melee_host_hsd_reader_allocate(
            reader, sizeof(TempS) * (size_t) count, alignof(TempS));
        if (lookups[i].x4 == NULL) {
            return NULL;
        }
        for (j = 0; j < count; j++) {
            const mh_u32 temp = list + (mh_u32) j * FT_VIS_ENTRY_SIZE;
            bool listed;
            const mh_u32 bytes = target_of(reader, temp + 4, &listed);
            lookups[i].x4[j].x0 = (int) melee_host_hsd_reader_u32(reader, temp);
            lookups[i].x4[j].x4 =
                listed ? melee_host_hsd_reader_payload(reader, bytes, 1) : NULL;
        }
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : lookups;
}

static struct ftData_x8* fighter_parts(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct ftData_x8* const parts = melee_host_hsd_reader_allocate(
        reader, sizeof(*parts), alignof(struct ftData_x8));
    const u32 model_num = melee_host_hsd_reader_u32(reader, at + 0x0);
    const u32 tobjs = melee_host_hsd_reader_u32(reader, at + 0x8);
    bool present;
    mh_u32 target;

    if (parts == NULL) {
        return NULL;
    }
    if (model_num > 11 || tobjs > FT_TABLE_MAX) {
        melee_host_hsd_reader_fail(reader, "fighter parts have bad counts");
        return NULL;
    }
    parts->x0.model_num = model_num;
    target = target_of(reader, at + 0x4, &present);
    if (present) {
        const mh_u32 rows = melee_host_hsd_reader_extent(reader, target) / 16;
        void* (*const table)[4] = melee_host_hsd_reader_allocate(
            reader, sizeof(void* [4]) * rows, alignof(void*));
        mh_u32 row;
        if (table == NULL) {
            return NULL;
        }
        for (row = 0; row < rows; row++) {
            mh_u32 column;
            for (column = 0; column < 4; column++) {
                bool listed;
                const mh_u32 lookup =
                    target_of(reader, target + row * 16 + column * 4, &listed);
                table[row][column] =
                    listed ? fighter_vis_lookup(reader, lookup, model_num)
                           : NULL;
            }
        }
        parts->x0.vis_table = table;
    }
    parts->x8.x8 = tobjs;
    target = target_of(reader, at + 0xC, &present);
    if (present) {
        const mh_u32 slots = melee_host_hsd_reader_extent(reader, target) / 4;
        u16** const rows = melee_host_hsd_reader_allocate(
            reader, sizeof(u16*) * slots, alignof(u16*));
        mh_u32 slot;
        if (rows == NULL) {
            return NULL;
        }
        for (slot = 0; slot < slots; slot++) {
            bool listed;
            const mh_u32 list = target_of(reader, target + slot * 4, &listed);
            rows[slot] = NULL;
            if (listed && tobjs != 0) {
                u32 k;
                rows[slot] = melee_host_hsd_reader_allocate(
                    reader, sizeof(u16) * tobjs, alignof(u16));
                if (rows[slot] == NULL) {
                    return NULL;
                }
                for (k = 0; k < tobjs; k++) {
                    rows[slot][k] =
                        melee_host_hsd_reader_u16(reader, list + k * 2);
                }
            }
        }
        parts->x8.xC = rows;
    }
    parts->x10 = melee_host_hsd_reader_u8(reader, at + 0x10);
    parts->x11 = melee_host_hsd_reader_u8(reader, at + 0x11);
    parts->x12 = melee_host_hsd_reader_u8(reader, at + 0x12);
    parts->x13 = melee_host_hsd_reader_u8(reader, at + 0x13);
    parts->x14 = melee_host_hsd_reader_u8(reader, at + 0x14);
    return melee_host_hsd_reader_failed(reader) ? NULL : parts;
}

static void* fighter_part_anim_set(MeleeHostHsdReader* reader, mh_u32 at)
{
    if (at == 0) return NULL;
    ftData_x1C* const set = melee_host_hsd_reader_allocate(
        reader, sizeof(*set), alignof(ftData_x1C));
    bool present;
    mh_u32 target;

    if (set == NULL) {
        return NULL;
    }
    set->x0 = melee_host_hsd_reader_u16(reader, at + 0x0);
    set->x2 = melee_host_hsd_reader_u16(reader, at + 0x2);
    target = target_of(reader, at + 0x4, &present);
    if (present) {
        mh_u32 extent = melee_host_hsd_reader_extent(reader, target);
        mh_u32 alloc_size = extent < 16 ? 16 : extent;
        set->x4 = melee_host_hsd_reader_allocate(reader, alloc_size, 4);
        if (set->x4) {
            memset(set->x4, 0, alloc_size);
            copy_bytes(reader, target, set->x4, 0, extent);
        }
    } else {
        set->x4 = NULL;
    }
    
    target = target_of(reader, at + 0x8, &present);
    set->x8 = present ? (HSD_AnimJoint**) per_entry_table(
                            reader, target, melee_host_hsd_reader_anim_joint)
                      : NULL;
    return set;
}

static void* fighter_parts_vis_lookup_set(MeleeHostHsdReader* reader, mh_u32 at)
{
    if (at == 0) return NULL;
    ftData_x1C* const set = melee_host_hsd_reader_allocate(
        reader, sizeof(*set), alignof(ftData_x1C));
    bool present;
    mh_u32 target;

    if (set == NULL) {
        return NULL;
    }
    set->x0 = melee_host_hsd_reader_u16(reader, at + 0x0);
    set->x2 = melee_host_hsd_reader_u16(reader, at + 0x2);
    target = target_of(reader, at + 0x4, &present);
    if (present) {
        mh_u32 extent = melee_host_hsd_reader_extent(reader, target);
        // S_UNK_YOSHI2 has 3 s32s (12 bytes) followed by u8 array
        struct S_UNK_YOSHI2 {
            mh_u32 x0, x4, x8;
            mh_u8 xC[0];
        }* yoshi2 = melee_host_hsd_reader_allocate(reader, extent, 4);
        if (yoshi2) {
            if (extent >= 4) yoshi2->x0 = melee_host_hsd_reader_u32(reader, target + 0x0);
            if (extent >= 8) yoshi2->x4 = melee_host_hsd_reader_u32(reader, target + 0x4);
            if (extent >= 12) yoshi2->x8 = melee_host_hsd_reader_u32(reader, target + 0x8);
            if (extent > 12) {
                /* copy_bytes applies its offset to both addresses.  Pass the
                 * allocation base so byte 12 of the disc reaches byte 12 of
                 * this record, rather than byte 24. */
                copy_bytes(reader, target, yoshi2, 12, extent);
            }
        }
        set->x4 = (mh_u8*) yoshi2;
    } else {
        set->x4 = NULL;
    }
    
    target = target_of(reader, at + 0x8, &present);
    set->x8 = present ? (HSD_AnimJoint**) per_entry_table(
                            reader, target, melee_host_hsd_reader_anim_joint)
                      : NULL;
    return set;
}

/* x20: a joint table in which only some slots are joints; ftCo_Guard.c reads
 * slot 2, and the others hold small integers, kept as they are. */
static ftData_x20* fighter_guard_joints(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftData_x20* const data = melee_host_hsd_reader_allocate(
        reader, sizeof(*data), alignof(ftData_x20));
    bool present;
    const mh_u32 table = target_of(reader, at + 0x0, &present);

    if (data == NULL) {
        return NULL;
    }
    data->x8 = melee_host_hsd_reader_f32(reader, at + 0x4);
    data->x0 = NULL;
    if (present) {
        const mh_u32 slots = melee_host_hsd_reader_extent(reader, table) / 4;
        HSD_Joint** const joints = melee_host_hsd_reader_allocate(
            reader, sizeof(HSD_Joint*) * slots, alignof(HSD_Joint*));
        mh_u32 i;
        if (joints == NULL) {
            return NULL;
        }
        for (i = 0; i < slots; i++) {
            const mh_u32 slot = table + i * 4;
            if (melee_host_hsd_reader_has_pointer(reader, slot)) {
                bool listed;
                const mh_u32 joint = target_of(reader, slot, &listed);
                joints[i] = melee_host_hsd_reader_joint(reader, joint);
            } else {
                joints[i] = (HSD_Joint*) (intptr_t) (s32)
                    melee_host_hsd_reader_u32(reader, slot);
            }
        }
        data->x0 = joints;
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : data;
}

static WaitStruct* fighter_wait_pairs(MeleeHostHsdReader* reader, mh_u32 at)
{
    const mh_u32 limit = melee_host_hsd_reader_extent(reader, at) / 8;
    mh_u32 count = 0;
    WaitStruct* pairs;
    mh_u32 i;

    while (count < limit &&
           (s32) melee_host_hsd_reader_u32(reader, at + count * 8) != -1)
    {
        count++;
    }
    if (count == limit) {
        melee_host_hsd_reader_fail(reader,
                                   "a fighter wait table has no end entry");
        return NULL;
    }
    pairs = melee_host_hsd_reader_allocate(
        reader, sizeof(WaitStruct) * (count + 1), alignof(WaitStruct));
    if (pairs == NULL) {
        return NULL;
    }
    for (i = 0; i <= count; i++) {
        pairs[i].u.i.x = (int) melee_host_hsd_reader_u32(reader, at + i * 8);
        pairs[i].u.i.y = (int) melee_host_hsd_reader_u32(reader, at + i * 8 + 4);
    }
    return pairs;
}

static struct ftDynamics* fighter_dynamics(MeleeHostHsdReader* reader,
                                           mh_u32 at)
{
    struct ftDynamics* const dynamics = melee_host_hsd_reader_allocate(
        reader, sizeof(*dynamics), alignof(struct ftDynamics));
    const s32 count = (s32) melee_host_hsd_reader_u32(reader, at + 0x0);
    bool present;
    mh_u32 target;

    if (dynamics == NULL) {
        return NULL;
    }
    if (count < 0 || count > Ft_Dynamics_NumMax) {
        melee_host_hsd_reader_fail(reader, "fighter dynamics have a bad count");
        return NULL;
    }
    dynamics->dynamicsNum = count;
    target = target_of(reader, at + 0x4, &present);
    if (present && count != 0) {
        ArticleDynamicBones* const bones = melee_host_hsd_reader_allocate(
            reader, sizeof(*bones), alignof(ArticleDynamicBones));
        s32 i;
        if (bones == NULL) {
            return NULL;
        }
        for (i = 0; i < count; i++) {
            const mh_u32 entry = target + (mh_u32) i * FT_BONE_DYNAMICS_SIZE;
            const u32 records = melee_host_hsd_reader_u32(reader, entry + 0x8);
            bool listed;
            const mh_u32 data = target_of(reader, entry + 0x4, &listed);
            BoneDynamicsDesc* const bone = &bones->array[i];
            bone->bone_id = (enum_t) melee_host_hsd_reader_u32(reader, entry);
            if (records > FT_TABLE_MAX) {
                melee_host_hsd_reader_fail(reader,
                                           "fighter dynamics list too many "
                                           "records");
                return NULL;
            }
            bone->dyn_desc.data =
                listed && records != 0
                    ? scalar_block(reader, data,
                                   records * FT_DYNAMICS_RECORD_SIZE)
                    : NULL;
            bone->dyn_desc.count = records;
            bone->dyn_desc.pos.x = melee_host_hsd_reader_f32(reader, entry + 0xC);
            bone->dyn_desc.pos.y =
                melee_host_hsd_reader_f32(reader, entry + 0x10);
            bone->dyn_desc.pos.z =
                melee_host_hsd_reader_f32(reader, entry + 0x14);
        }
        dynamics->ftDynamicBones = bones;
    }
    dynamics->x4 = (int) melee_host_hsd_reader_u32(reader, at + 0x8);
    target = target_of(reader, at + 0xC, &present);
    dynamics->x8 = present ? scalar_extent(reader, target) : NULL;
    (void) target_of(reader, at + 0x10, &present);
    if (present) {
        melee_host_hsd_reader_fail(reader,
                                   "fighter dynamics that name animation trees "
                                   "are not translated yet");
        return NULL;
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : dynamics;
}

static ftData_x30* fighter_hurtboxes(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftData_x30* const hurtboxes = melee_host_hsd_reader_allocate(
        reader, sizeof(*hurtboxes), alignof(ftData_x30));
    const s32 count = (s32) melee_host_hsd_reader_u32(reader, at + 0x0);
    bool present;
    const mh_u32 inits = target_of(reader, at + 0x4, &present);

    if (hurtboxes == NULL ||
        !fighter_count_ok(reader, count, "fighter hurtboxes have a bad count"))
    {
        return NULL;
    }
    hurtboxes->count = count;
    hurtboxes->inits =
        present && count != 0
            ? scalar_block(reader, inits, (mh_u32) count * FT_HURTBOX_SIZE)
            : NULL;
    return hurtboxes;
}

static ftData_x44_t* fighter_ledge(MeleeHostHsdReader* reader, mh_u32 at)
{
    ftData_x44_t* const ledge = melee_host_hsd_reader_allocate(
        reader, sizeof(*ledge), alignof(ftData_x44_t));

    if (ledge == NULL) {
        return NULL;
    }
    ledge->unk0 = read_s16(reader, at + 0x0);
    ledge->unk2 = read_s16(reader, at + 0x2);
    ledge->unk4 = read_s16(reader, at + 0x4);
    ledge->unk6 = read_s16(reader, at + 0x6);
    ledge->unk8 = read_s16(reader, at + 0x8);
    ledge->unkA = read_s16(reader, at + 0xA);
    copy_words(reader, at, ledge, 0xC, sizeof(ftData_x44_t));
    return ledge;
}

/* A character's item list names item articles, whose first field, the common
 * attributes, is always relocated.  Not every slot is one: Fox's fifth slot
 * names int pairs ending in -1, which nothing reads, and keeps its bytes. */
static void* fighter_item_article(MeleeHostHsdReader* reader, mh_u32 at)
{
    if (!melee_host_hsd_reader_has_pointer(reader, at)) {
        return melee_host_hsd_reader_payload(reader, at, 1);
    }
    return item_article(reader, at);
}

/* The special attributes of the items a character's list names, one
 * translator per item.  A block of 4-byte scalars keeps its offsets; a block
 * with models or animations is filled field by field, because the host's
 * pointers are wider. */
typedef void* (*FighterItemSpecial)(MeleeHostHsdReader* reader, mh_u32 at);

/* `disc_size` bytes of scalars in a block of `host_size`.  A struct can run
 * past what its disc block holds when the code never reads the rest. */

static void* item_generic_scalar_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    if (!melee_host_hsd_reader_has_pointer(reader, at)) {
        return scalar_extent(reader, at);
    }
    return NULL; // Has pointers, generic scalar fails!
}

static void* item_special_scalars(MeleeHostHsdReader* reader, mh_u32 at,
                                  mh_u32 disc_size, mh_u32 host_size)
{
    void* block;
    mh_u32 word;

    if (melee_host_hsd_reader_extent(reader, at) < disc_size) {
        melee_host_hsd_reader_fail(reader, "an item's special attributes are "
                                           "shorter than their host layout");
        return NULL;
    }
    for (word = 0; word < disc_size; word += 4) {
        if (melee_host_hsd_reader_has_pointer(reader, at + word)) {
            melee_host_hsd_reader_fail(
                reader, "an item's special attributes hold a pointer");
            return NULL;
        }
    }
    block = melee_host_hsd_reader_allocate(reader, host_size, alignof(f32));
    if (block == NULL) {
        return NULL;
    }
    memset(block, 0, host_size);
    copy_words(reader, at, block, 0, disc_size);
    return block;
}

static HSD_Joint* item_special_joint(MeleeHostHsdReader* reader,
                                     mh_u32 field)
{
    bool present;
    const mh_u32 target = target_of(reader, field, &present);

    return present ? melee_host_hsd_reader_joint(reader, target) : NULL;
}

static void item_special_anims(MeleeHostHsdReader* reader, mh_u32 at,
                               AnimBundle* bundle)
{
    bool present;
    mh_u32 target;

    target = target_of(reader, at + 0x0, &present);
    bundle->anim =
        present ? melee_host_hsd_reader_anim_joint(reader, target) : NULL;
    target = target_of(reader, at + 0x4, &present);
    bundle->matanim =
        present ? melee_host_hsd_reader_mat_anim_joint(reader, target) : NULL;
    target = target_of(reader, at + 0x8, &present);
    bundle->shapeanim =
        present ? melee_host_hsd_reader_shape_anim_joint(reader, target)
                : NULL;
}

_Static_assert(sizeof(itUnkAttributes) == 0x14 &&
                   sizeof(itLinkBombAttributes) == 0x40 &&
                   offsetof(itLinkBoomerangAttributes, x40) == 0x40 &&
                   offsetof(itLinkHookshotAttributes, x50) == 0x50 &&
                   offsetof(itLinkArrowAttributes, x20) == 0x20,
               "the scalar heads of Mario's and Link's item attributes keep "
               "their PowerPC offsets");

/* The fireball: speed, angle, life and the two bounce factors. */
static void* mario_fireball_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    return item_special_scalars(reader, at, 0x14, sizeof(itUnkAttributes));
}

/* The cape and the bow have zero-filled blocks their code never reads; the
 * Article still names one, and the host keeps its bytes. */
static void* mario_cape_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    return item_special_scalars(reader, at, 0x4, 0x4);
}

static void* link_bow_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    return item_special_scalars(reader, at, 0x8, 0x8);
}

/* The bomb's block ends before `vel`, which itlinkbomb.c never reads. */

static void* samus_grapple_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    itSamusGrappleAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(itSamusGrappleAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    memset(attrs, 0, sizeof(*attrs));
    copy_words(reader, at, attrs, 0x00, 0x64);
    attrs->x64 = item_special_joint(reader, at + 0x64);
    attrs->x68 = item_special_joint(reader, at + 0x68);
    attrs->x6C = item_special_joint(reader, at + 0x6C);
    attrs->x70 = item_special_joint(reader, at + 0x70);
    
    item_special_anims(reader, at + 0x74, (AnimBundle*)&attrs->x74);
    item_special_anims(reader, at + 0x80, (AnimBundle*)&attrs->x80);
    item_special_anims(reader, at + 0x8C, (AnimBundle*)&attrs->x8C);
    item_special_anims(reader, at + 0x98, (AnimBundle*)&attrs->x98);
    item_special_anims(reader, at + 0xA4, (AnimBundle*)&attrs->xA4);

    return melee_host_hsd_reader_failed(reader) ? NULL : attrs;
}

static void* seak_chain_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    itSeakChain_Attrs* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(itSeakChain_Attrs));

    if (attrs == NULL) {
        return NULL;
    }
    memset(attrs, 0, sizeof(*attrs));
    copy_words(reader, at, attrs, 0x00, 0x64);
    attrs->x64_joint = item_special_joint(reader, at + 0x64);
    attrs->x68_joint = item_special_joint(reader, at + 0x68);

    return melee_host_hsd_reader_failed(reader) ? NULL : attrs;
}

static void* link_bomb_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    return item_special_scalars(reader, at, 0x34,
                                sizeof(itLinkBombAttributes));
}

/* The boomerang: scalars, the models of its two flights and their
 * animations. */
static void* link_boomerang_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    itLinkBoomerangAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(itLinkBoomerangAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    memset(attrs, 0, sizeof(*attrs));
    copy_words(reader, at, attrs, 0x00, 0x44);
    attrs->x44 = item_special_joint(reader, at + 0x44);
    attrs->x48 = item_special_joint(reader, at + 0x48);
    item_special_anims(reader, at + 0x4C, &attrs->x4C_anim);
    item_special_anims(reader, at + 0x58, &attrs->x58_anim);
    return melee_host_hsd_reader_failed(reader) ? NULL : attrs;
}

/* The hookshot: chain scalars (it_link_attr_math writes the derived ones
 * back into the block) and the models of the chain links and the tip.  The
 * disc block goes on past the struct with a pointer and two words nothing
 * reads. */
static void* link_hookshot_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    itLinkHookshotAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(itLinkHookshotAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    memset(attrs, 0, sizeof(*attrs));
    copy_words(reader, at, attrs, 0x00, 0x54);
    attrs->x54 = item_special_joint(reader, at + 0x54);
    attrs->x58 = item_special_joint(reader, at + 0x58);
    attrs->x5C = item_special_joint(reader, at + 0x5C);
    return melee_host_hsd_reader_failed(reader) ? NULL : attrs;
}

/* The arrow: scalars and its two models.  The block ends before x2C, which
 * itlinkarrow.c never reads. */
static void* link_arrow_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    itLinkArrowAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(itLinkArrowAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    memset(attrs, 0, sizeof(*attrs));
    copy_words(reader, at, attrs, 0x00, 0x24);
    attrs->x24 = item_special_joint(reader, at + 0x24);
    attrs->x28 = item_special_joint(reader, at + 0x28);
    return melee_host_hsd_reader_failed(reader) ? NULL : attrs;
}

/* The special attributes of a character's items, by slot of its item list:
 * the size in bytes of each block the host translates, for blocks that hold
 * only 4-byte scalars.  item_article leaves every item's special attributes
 * out because their layout depends on the item; the fighter's own translator
 * knows which items its list names.  A slot past the list, or of size 0, keeps
 * the stop by name in Item_80267978. */
struct FighterItemAttrs {
    const mh_u32* sizes;
    mh_u32 count;
    /* Some fighter lists also carry a model joint rather than an item
     * Article.  Link's slot 6 is the sword model that ftParts installs while
     * the fighter is created. */
    const mh_u8* direct_joint_slots;
    mh_u32 direct_joint_count;
    /* Per slot, the translator of an item whose block is not only scalars
     * or whose struct is not the size of its disc block. */
    const FighterItemSpecial* specials;
    mh_u32 special_count;
    /* Custom item parsers for slots that are neither Articles nor direct joints
     * (e.g. Samus's grapple beam model). */
    const mh_u8* custom_slots;
    const FighterItemSpecial* custom_translators;
    mh_u32 custom_count;
};

static bool fighter_item_is_direct_joint(const struct FighterItemAttrs* attrs,
                                         mh_u32 slot)
{
    mh_u32 i;

    for (i = 0; i < attrs->direct_joint_count; i++) {
        if (attrs->direct_joint_slots[i] == slot) {
            return true;
        }
    }
    return false;
}

static void** fighter_items(MeleeHostHsdReader* reader, mh_u32 at,
                            const struct FighterItemAttrs* attrs)
{
    const mh_u32 length = melee_host_hsd_reader_extent(reader, at) / 4;
    void** const table = melee_host_hsd_reader_allocate(
        reader, sizeof(*table) * length, alignof(void*));
    mh_u32 slot;

    if (table == NULL || length == 0) {
        return NULL;
    }
    for (slot = 0; slot < length; slot++) {
        bool present;
        const mh_u32 target = target_of(reader, at + slot * 4, &present);

        table[slot] = NULL;
        if (!present) {
            continue;
        }

        bool custom = false;
        mh_u32 i;
        for (i = 0; i < attrs->custom_count; i++) {
            if (attrs->custom_slots[i] == slot) {
                table[slot] = attrs->custom_translators[i](reader, target);
                custom = true;
                break;
            }
        }

        if (!custom) {
            table[slot] = fighter_item_is_direct_joint(attrs, slot)
                              ? melee_host_hsd_reader_joint(reader, target)
                              : fighter_item_article(reader, target);
        }

        if (melee_host_hsd_reader_failed(reader)) {
            return NULL;
        }
    }
    for (slot = 0; slot < attrs->count && slot < length; slot++) {
        bool present;
        const mh_u32 article = target_of(reader, at + slot * 4, &present);
        const mh_u32 size = attrs->sizes[slot];
        mh_u32 special;
        mh_u32 word;

        if (!present || size == 0 ||
            !melee_host_hsd_reader_has_pointer(reader, article))
        {
            continue;
        }
        special = target_of(reader, article + 0x04, &present);
        if (!present) {
            continue;
        }
        if (melee_host_hsd_reader_extent(reader, special) < size) {
            melee_host_hsd_reader_fail(
                reader, "an item's special attributes are shorter than their "
                        "host layout");
            return NULL;
        }
        for (word = 0; word < size; word += 4) {
            if (melee_host_hsd_reader_has_pointer(reader, special + word)) {
                melee_host_hsd_reader_fail(
                    reader, "an item's special attributes hold a pointer");
                return NULL;
            }
        }
        ((Article*) table[slot])->x4_specialAttributes =
            scalar_block(reader, special, size);
    }
    for (slot = 0; slot < attrs->special_count && slot < length; slot++) {
        bool present;
        const mh_u32 article = target_of(reader, at + slot * 4, &present);
        mh_u32 special;

        if (!present || attrs->specials[slot] == NULL ||
            !melee_host_hsd_reader_has_pointer(reader, article))
        {
            continue;
        }
        special = target_of(reader, article + 0x04, &present);
        if (!present) {
            continue;
        }
        ((Article*) table[slot])->x4_specialAttributes =
            attrs->specials[slot](reader, special);
        if (melee_host_hsd_reader_failed(reader)) {
            return NULL;
        }
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

static FtSFXArr* fighter_sound_list(MeleeHostHsdReader* reader, mh_u32 at)
{
    FtSFXArr* const list = melee_host_hsd_reader_allocate(
        reader, sizeof(*list), alignof(FtSFXArr));
    const s32 count = (s32) melee_host_hsd_reader_u32(reader, at + 0x0);
    bool present;
    const mh_u32 ids = target_of(reader, at + 0x4, &present);

    if (list == NULL ||
        !fighter_count_ok(reader, count, "a fighter sound list has a bad count"))
    {
        return NULL;
    }
    list->num = count;
    list->sfx_ids = present && count != 0
                        ? scalar_block(reader, ids, (mh_u32) count * 4)
                        : NULL;
    return list;
}

static FtSFX* fighter_sounds(MeleeHostHsdReader* reader, mh_u32 at)
{
    FtSFX* const sounds = melee_host_hsd_reader_allocate(
        reader, sizeof(*sounds), alignof(FtSFX));
    bool present;
    mh_u32 target;

    if (sounds == NULL) {
        return NULL;
    }
    target = target_of(reader, at + 0x00, &present);
    sounds->smash = present ? fighter_sound_list(reader, target) : NULL;
    sounds->x4 = (int) melee_host_hsd_reader_u32(reader, at + 0x04);
    sounds->x8 = (int) melee_host_hsd_reader_u32(reader, at + 0x08);
    sounds->xC = (int) melee_host_hsd_reader_u32(reader, at + 0x0C);
    sounds->x10 = (int) melee_host_hsd_reader_u32(reader, at + 0x10);
    sounds->x14 = (int) melee_host_hsd_reader_u32(reader, at + 0x14);
    sounds->x18 = (int) melee_host_hsd_reader_u32(reader, at + 0x18);
    target = target_of(reader, at + 0x1C, &present);
    sounds->x1C = present ? fighter_sound_list(reader, target) : NULL;
    target = target_of(reader, at + 0x20, &present);
    sounds->x20 = present ? fighter_sound_list(reader, target) : NULL;
    sounds->x24 = (int) melee_host_hsd_reader_u32(reader, at + 0x24);
    sounds->x28 = (int) melee_host_hsd_reader_u32(reader, at + 0x28);
    sounds->x2C = (int) melee_host_hsd_reader_u32(reader, at + 0x2C);
    sounds->x30 = (int) melee_host_hsd_reader_u32(reader, at + 0x30);
    sounds->x34 = (int) melee_host_hsd_reader_u32(reader, at + 0x34);
    return melee_host_hsd_reader_failed(reader) ? NULL : sounds;
}

static struct ftData_x58_t* fighter_ik(MeleeHostHsdReader* reader, mh_u32 at)
{
    struct ftData_x58_t* const ik = melee_host_hsd_reader_allocate(
        reader, sizeof(*ik), alignof(struct ftData_x58_t));

    if (ik == NULL) {
        return NULL;
    }
    ik->x0 = melee_host_hsd_reader_u8(reader, at + 0x00);
    ik->x1 = melee_host_hsd_reader_u8(reader, at + 0x01);
    ik->x4 = melee_host_hsd_reader_f32(reader, at + 0x04);
    ik->x8 = melee_host_hsd_reader_u8(reader, at + 0x08);
    ik->x9 = melee_host_hsd_reader_u8(reader, at + 0x09);
    ik->xC = melee_host_hsd_reader_f32(reader, at + 0x0C);
    ik->x10 = melee_host_hsd_reader_u8(reader, at + 0x10);
    ik->x11 = melee_host_hsd_reader_u8(reader, at + 0x11);
    ik->x18 = melee_host_hsd_reader_f32(reader, at + 0x18);
    return ik;
}

static void* fighter_data(MeleeHostHsdReader* reader, mh_u32 root,
                          void* (*special_attrs)(MeleeHostHsdReader*, mh_u32),
                          void* (*x1C_translator)(MeleeHostHsdReader*, mh_u32),
                          const struct FighterItemAttrs* items)
{
    struct ftData* const data = melee_host_hsd_reader_allocate(
        reader, sizeof(*data), alignof(struct ftData));
    bool present;
    mh_u32 target;

    if (data == NULL) {
        return NULL;
    }
    item_memo.count = 0;
#define FT_FIELD(offset)                                                      \
    (target = target_of(reader, root + (offset), &present), present)
    if (FT_FIELD(0x00)) data->x0 = fighter_attrs(reader, target);
    if (FT_FIELD(0x04)) data->ext_attr = special_attrs(reader, target);
    if (FT_FIELD(0x08)) data->x8 = fighter_parts(reader, target);
    if (FT_FIELD(0x0C)) data->xC = fighter_actions(reader, target);
    if (FT_FIELD(0x10)) data->x10 = melee_host_hsd_reader_payload(reader, target, 1);
    if (FT_FIELD(0x14)) data->x14 = fighter_actions(reader, target);
    if (FT_FIELD(0x18)) data->x18 = melee_host_hsd_reader_payload(reader, target, 1);
    if (FT_FIELD(0x1C))
        data->x1C = (ftData_x1C**) per_entry_table(reader, target,
                                                   x1C_translator);
    if (FT_FIELD(0x20)) data->x20 = fighter_guard_joints(reader, target);
    if (FT_FIELD(0x24)) data->x24 = fighter_wait_pairs(reader, target);
    if (FT_FIELD(0x28)) data->x28 = fighter_wait_pairs(reader, target);
    if (FT_FIELD(0x2C)) data->x2C = fighter_dynamics(reader, target);
    if (FT_FIELD(0x30)) data->x30 = fighter_hurtboxes(reader, target);
    if (FT_FIELD(0x34)) data->x34 = scalar_block(reader, target, sizeof(ftData_x34));
    if (FT_FIELD(0x38)) data->x38 = scalar_extent(reader, target);
    if (FT_FIELD(0x3C))
        data->x3C = scalar_block(reader, target, sizeof(struct UnkFloat6_Camera));
    if (FT_FIELD(0x40)) data->x40 = scalar_block(reader, target, sizeof(itPickup));
    if (FT_FIELD(0x44)) data->x44 = fighter_ledge(reader, target);
    if (FT_FIELD(0x48)) data->x48_items = fighter_items(reader, target, items);
    if (FT_FIELD(0x4C)) data->x4C_sfx = fighter_sounds(reader, target);
    if (FT_FIELD(0x50)) data->x50 = scalar_block(reader, target, sizeof(Vec2));
    if (FT_FIELD(0x54)) data->x54 = scalar_extent(reader, target);
    if (FT_FIELD(0x58)) data->x58 = fighter_ik(reader, target);
    if (FT_FIELD(0x5C)) data->x5C = melee_host_hsd_reader_joint(reader, target);
#undef FT_FIELD
    return melee_host_hsd_reader_failed(reader) ? NULL : data;
}

_Static_assert(sizeof(FoxLaserAttr) == 0x28 && sizeof(FoxBlasterAttr) == 0x28,
               "Fox's item attributes keep their PowerPC sizes");

/* ftFx_Init_OnLoad registers the list's first three slots as the blaster's
 * shot, the blaster and the illusion; all three blocks are floats in PlFx.dat
 * (the illusion's two: its speed and its life), with no relocation inside. */
static const mh_u32 fighter_fox_item_attr_sizes[] = {
    sizeof(FoxLaserAttr),
    sizeof(FoxBlasterAttr),
    2 * sizeof(f32),
};

static void* fighter_data_fox(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs items = {
        fighter_fox_item_attr_sizes,
        sizeof(fighter_fox_item_attr_sizes) /
            sizeof(fighter_fox_item_attr_sizes[0]),
        NULL,
        0,
        NULL,
        0,
    };

    return fighter_data(reader, root, fighter_fox_attrs, fighter_part_anim_set, &items);
}

static void* fighter_data_mario(MeleeHostHsdReader* reader, mh_u32 root)
{
    /* Slot 0 is the fireball, slot 2 the cape (ftMr_Init_OnLoad). */
    static const FighterItemSpecial mario_item_specials[] = {
        mario_fireball_attrs,
        NULL,
        mario_cape_attrs,
    };
    static const struct FighterItemAttrs mario_items = {
        NULL,
        0,
        NULL,
        0,
        mario_item_specials,
        sizeof(mario_item_specials) / sizeof(mario_item_specials[0]),
    };

    return fighter_data(reader, root, fighter_mario_attrs, fighter_part_anim_set, &mario_items);
}

static void* fighter_data_link(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const mh_u8 link_direct_joint_slots[] = { 6 };
    /* ftLk_Init_OnLoad: bomb, boomerang, hookshot, arrow and bow. */
    static const FighterItemSpecial link_item_specials[] = {
        link_bomb_attrs,     link_boomerang_attrs, link_hookshot_attrs,
        link_arrow_attrs,    link_bow_attrs,
    };
    static const struct FighterItemAttrs link_items = {
        NULL,
        0,
        link_direct_joint_slots,
        sizeof(link_direct_joint_slots) / sizeof(link_direct_joint_slots[0]),
        link_item_specials,
        sizeof(link_item_specials) / sizeof(link_item_specials[0]),
    };

    return fighter_data(reader, root, fighter_link_attrs, fighter_part_anim_set, &link_items);
}

static void* fighter_data_captain(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs captain_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_captain_attrs, fighter_part_anim_set, &captain_items);
}

static void* fighter_data_donkey(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs donkey_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_donkey_attrs, fighter_part_anim_set, &donkey_items);
}


/* A character's demo motions (ftDemoResultMotionFileFox in GmRstMFx.dat and
 * the intro, ending and wait files named next to it in ftData_803C2468): the
 * base of a block of nested archives, one per demo action.  ftData_80085B98
 * adds each action's offset to the base and ftData_80085CD8 parses the archive
 * there, as it does with an animation read from ARAM, so the host hands over
 * the bytes as they are, up to the next address the file names. */
static void* demo_motion_file(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 extent = melee_host_hsd_reader_extent(reader, root);

    if (extent == 0) {
        melee_host_hsd_reader_fail(reader, "the demo motion block is empty");
        return NULL;
    }
    return melee_host_hsd_reader_payload(reader, root, extent);
}

static void register_demo_motion_files(void)
{
    int kind;

    for (kind = 0; kind < Ft_Kind_Max; kind++) {
        const Fighter_DemoStrings* const names = ftData_803C2468[kind];

        if (names == NULL) {
            continue;
        }
        if (names->result_filename != NULL) {
            (void) melee_host_hsd_register_translator(names->result_filename,
                                                      demo_motion_file);
        }
        if (names->intro_filename != NULL) {
            (void) melee_host_hsd_register_translator(names->intro_filename,
                                                      demo_motion_file);
        }
        if (names->ending_filename != NULL) {
            (void) melee_host_hsd_register_translator(names->ending_filename,
                                                      demo_motion_file);
        }
        if (names->vi_wait_filename != NULL) {
            (void) melee_host_hsd_register_translator(
                names->vi_wait_filename, demo_motion_file);
        }
    }
}

static mh_u32 stage_symbols_refused_mark;

void melee_host_stage_symbols_mark(void)
{
    MeleeHostHsdArchiveStats stats = { 0 };

    (void) melee_host_hsd_archive_stats(&stats);
    stage_symbols_refused_mark = stats.symbols_refused;
}

void melee_host_stage_symbols_check(void)
{
    MeleeHostHsdArchiveStats stats = { 0 };

    (void) melee_host_hsd_archive_stats(&stats);
    if (stats.symbols_refused != stage_symbols_refused_mark) {
        OSPanic(__FILE__, __LINE__, "the host cannot load this stage: %s",
                melee_host_hsd_archive_last_error());
    }
}


static void* fighter_data_drmario(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial drmario_item_specials[] = {
        NULL, item_generic_scalar_attrs, NULL, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs drmario_items = {
        NULL, 0, NULL, 0, drmario_item_specials,
        sizeof(drmario_item_specials) / sizeof(drmario_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_drmario_attrs, fighter_part_anim_set, &drmario_items);
}

static void* fighter_data_falco(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial falco_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, NULL, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs falco_items = {
        NULL, 0, NULL, 0, falco_item_specials,
        sizeof(falco_item_specials) / sizeof(falco_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_falco_attrs, fighter_part_anim_set, &falco_items);
}

static void* fighter_data_gamewatch(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial gamewatch_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs,
        fighter_parts_vis_lookup_set
    };
    static const struct FighterItemAttrs gamewatch_items = {
        NULL, 0, NULL, 0, gamewatch_item_specials,
        sizeof(gamewatch_item_specials) / sizeof(gamewatch_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_gamewatch_attrs, fighter_part_anim_set, &gamewatch_items);
}

static void* fighter_data_ganon(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs ganon_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_ganon_attrs, fighter_part_anim_set, &ganon_items);
}

static void* fighter_data_kirby(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs kirby_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_kirby_attrs, fighter_parts_vis_lookup_set, &kirby_items);
}

static void* fighter_data_koopa(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial koopa_item_specials[] = {
        item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs koopa_items = {
        NULL, 0, NULL, 0, koopa_item_specials,
        sizeof(koopa_item_specials) / sizeof(koopa_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_koopa_attrs, fighter_part_anim_set, &koopa_items);
}

static void* fighter_data_luigi(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial luigi_item_specials[] = {
        item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs luigi_items = {
        NULL, 0, NULL, 0, luigi_item_specials,
        sizeof(luigi_item_specials) / sizeof(luigi_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_luigi_attrs, fighter_part_anim_set, &luigi_items);
}

static void* fighter_data_mars(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs mars_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_mars_attrs, fighter_part_anim_set, &mars_items);
}

static void* fighter_data_mewtwo(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial mewtwo_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs mewtwo_items = {
        NULL, 0, NULL, 0, mewtwo_item_specials,
        sizeof(mewtwo_item_specials) / sizeof(mewtwo_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_mewtwo_attrs, fighter_part_anim_set, &mewtwo_items);
}

static void* fighter_data_nana(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs nana_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_nana_attrs, fighter_part_anim_set, &nana_items);
}

static void* ness_yoyo_attrs(MeleeHostHsdReader* reader, mh_u32 at)
{
    itYoyoAttributes* const attrs = melee_host_hsd_reader_allocate(
        reader, sizeof(*attrs), alignof(itYoyoAttributes));

    if (attrs == NULL) {
        return NULL;
    }
    memset(attrs, 0, sizeof(*attrs));
    copy_words(reader, at, attrs, 0x00, 0x50);
    attrs->x50_string_joint = item_special_joint(reader, at + 0x50);
    attrs->x54_yoyo_joint = item_special_joint(reader, at + 0x54);
    
    bool present;
    mh_u32 target = target_of(reader, at + 0x58, &present);
    attrs->x58_yoyo_matanim = present ? melee_host_hsd_reader_mat_anim_joint(reader, target) : NULL;
    attrs->x5C_UNK7 = melee_host_hsd_reader_u32(reader, at + 0x5C);

    return melee_host_hsd_reader_failed(reader) ? NULL : attrs;
}

static void* fighter_data_ness(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial ness_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, ness_yoyo_attrs
    };
    static const struct FighterItemAttrs ness_items = {
        NULL, 0, NULL, 0, ness_item_specials,
        sizeof(ness_item_specials) / sizeof(ness_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_ness_attrs, fighter_part_anim_set, &ness_items);
}

static void* fighter_data_peach(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial peach_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs peach_items = {
        NULL, 0, NULL, 0, peach_item_specials,
        sizeof(peach_item_specials) / sizeof(peach_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_peach_attrs, fighter_part_anim_set, &peach_items);
}

static void* fighter_data_pichu(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial pichu_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs pichu_items = {
        NULL, 0, NULL, 0, pichu_item_specials,
        sizeof(pichu_item_specials) / sizeof(pichu_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_pichu_attrs, fighter_part_anim_set, &pichu_items);
}

static void* fighter_data_pikachu(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial pikachu_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs pikachu_items = {
        NULL, 0, NULL, 0, pikachu_item_specials,
        sizeof(pikachu_item_specials) / sizeof(pikachu_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_pikachu_attrs, fighter_part_anim_set, &pikachu_items);
}

static void* fighter_data_popo(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial popo_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs popo_items = {
        NULL, 0, NULL, 0, popo_item_specials,
        sizeof(popo_item_specials) / sizeof(popo_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_popo_attrs, fighter_part_anim_set, &popo_items);
}

static void* fighter_data_purin(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs purin_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_purin_attrs, fighter_part_anim_set, &purin_items);
}

static void* samus_throw_beam_model(MeleeHostHsdReader* reader, mh_u32 at)
{
    /* struct UNK_SAMUS_S1 {
     *     HSD_Joint* x0_joint;
     *     HSD_AnimJoint** x4_anim_joints;
     *     HSD_AnimJoint* x8_anim_joint;
     *     HSD_MatAnimJoint* xC_matanim_joint;
     * }; */
    struct SamusThrowBeamModel {
        void* joint;
        void* anim_joints;
        void* anim_joint;
        void* matanim_joint;
    }* beam = melee_host_hsd_reader_allocate(reader, sizeof(*beam),
                                              alignof(struct SamusThrowBeamModel));
    bool present;
    mh_u32 target;

    if (beam == NULL) {
        return NULL;
    }

    target = target_of(reader, at + 0x00, &present);
    beam->joint = present ? melee_host_hsd_reader_joint(reader, target) : NULL;

    target = target_of(reader, at + 0x04, &present);
    if (present) {
        void** arr = melee_host_hsd_reader_allocate(reader, 4 * sizeof(void*), alignof(void*));
        if (arr != NULL) {
            int i;
            for (i = 0; i < 4; i++) {
                bool p;
                mh_u32 t = target_of(reader, target + (mh_u32) i * 4, &p);
                arr[i] = p ? melee_host_hsd_reader_anim_joint(reader, t) : NULL;
            }
        }
        beam->anim_joints = arr;
    } else {
        beam->anim_joints = NULL;
    }

    target = target_of(reader, at + 0x08, &present);
    beam->anim_joint = present ? melee_host_hsd_reader_anim_joint(reader, target) : NULL;

    target = target_of(reader, at + 0x0C, &present);
    beam->matanim_joint =
        present ? melee_host_hsd_reader_mat_anim_joint(reader, target) : NULL;

    return melee_host_hsd_reader_failed(reader) ? NULL : beam;
}

static void* fighter_data_samus(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial samus_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, samus_grapple_attrs
    };
    static const mh_u8 samus_custom_slots[] = { 4 };
    static const FighterItemSpecial samus_custom_translators[] = { samus_throw_beam_model };
    static const struct FighterItemAttrs samus_items = {
        NULL, 0,
        NULL, 0,
        samus_item_specials,
        sizeof(samus_item_specials) / sizeof(samus_item_specials[0]),
        samus_custom_slots,
        samus_custom_translators,
        sizeof(samus_custom_slots) / sizeof(samus_custom_slots[0]),
    };
    return fighter_data(reader, root, fighter_samus_attrs, fighter_part_anim_set, &samus_items);
}

static void* fighter_data_seak(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial seak_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs, seak_chain_attrs
    };
    static const struct FighterItemAttrs seak_items = {
        NULL, 0, NULL, 0, seak_item_specials,
        sizeof(seak_item_specials) / sizeof(seak_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_seak_attrs, fighter_part_anim_set, &seak_items);
}

static void* fighter_data_yoshi(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial yoshi_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs yoshi_items = {
        NULL, 0, NULL, 0, yoshi_item_specials,
        sizeof(yoshi_item_specials) / sizeof(yoshi_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_yoshi_attrs, fighter_parts_vis_lookup_set, &yoshi_items);
}

static void* fighter_data_zelda(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial zelda_item_specials[] = {
        item_generic_scalar_attrs, item_generic_scalar_attrs
    };
    static const struct FighterItemAttrs zelda_items = {
        NULL, 0, NULL, 0, zelda_item_specials,
        sizeof(zelda_item_specials) / sizeof(zelda_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_zelda_attrs, fighter_part_anim_set, &zelda_items);
}

static void* fighter_data_clink(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const FighterItemSpecial clink_item_specials[] = {
        link_bomb_attrs, link_boomerang_attrs, link_hookshot_attrs, link_arrow_attrs, link_bow_attrs, item_generic_scalar_attrs
    };
    static const mh_u8 clink_direct_joint_slots[] = { 6 };
    static const struct FighterItemAttrs clink_items = {
        NULL, 0, clink_direct_joint_slots,
        sizeof(clink_direct_joint_slots) / sizeof(clink_direct_joint_slots[0]),
        clink_item_specials,
        sizeof(clink_item_specials) / sizeof(clink_item_specials[0]),
    };
    return fighter_data(reader, root, fighter_clink_attrs, fighter_part_anim_set, &clink_items);
}

static void* fighter_data_emblem(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs emblem_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_emblem_attrs, fighter_part_anim_set, &emblem_items);
}

static void* fighter_data_gigakoopa(MeleeHostHsdReader* reader, mh_u32 root)
{
    static const struct FighterItemAttrs gigakoopa_items = {
        NULL, 0, NULL, 0, NULL, 0,
    };
    return fighter_data(reader, root, fighter_gigakoopa_attrs, fighter_part_anim_set, &gigakoopa_items);
}

static void* string_array(MeleeHostHsdReader* reader, mh_u32 root)
{
    const mh_u32 extent = melee_host_hsd_reader_extent(reader, root);
    const mh_u32 count = extent / 4;
    void** table;
    mh_u32 i;

    if (extent == 0) {
        return NULL;
    }
    table = melee_host_hsd_reader_allocate(reader, sizeof(*table) * count,
                                           alignof(void*));
    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        bool present;
        const mh_u32 target = target_of(reader, root + i * 4, &present);

        table[i] = NULL;
        if (present) {
            table[i] = melee_host_hsd_reader_payload(
                reader, target, melee_host_hsd_reader_extent(reader, target));
        }
    }
    return melee_host_hsd_reader_failed(reader) ? NULL : table;
}

void melee_host_game_register_data_translators(void)
{
    (void) melee_host_hsd_register_translator("ftDataFox", fighter_data_fox);
    (void) melee_host_hsd_register_translator("ftDataMario",
                                              fighter_data_mario);
    (void) melee_host_hsd_register_translator("ftDataLink", fighter_data_link);
    (void) melee_host_hsd_register_translator("ftDataCaptain", fighter_data_captain);
    (void) melee_host_hsd_register_translator("ftDataDonkey", fighter_data_donkey);
    (void) melee_host_hsd_register_translator("ftDataDrMario", fighter_data_drmario);
    (void) melee_host_hsd_register_translator("ftDataFalco", fighter_data_falco);
    (void) melee_host_hsd_register_translator("ftDataGameWatch", fighter_data_gamewatch);
    (void) melee_host_hsd_register_translator("ftDataGanon", fighter_data_ganon);
    (void) melee_host_hsd_register_translator("ftDataKirby", fighter_data_kirby);
    (void) melee_host_hsd_register_translator("ftDataKoopa", fighter_data_koopa);
    (void) melee_host_hsd_register_translator("ftDataLuigi", fighter_data_luigi);
    (void) melee_host_hsd_register_translator("ftDataMars", fighter_data_mars);
    (void) melee_host_hsd_register_translator("ftDataMewtwo", fighter_data_mewtwo);
    (void) melee_host_hsd_register_translator("ftDataNana", fighter_data_nana);
    (void) melee_host_hsd_register_translator("ftDataNess", fighter_data_ness);
    (void) melee_host_hsd_register_translator("ftDataPeach", fighter_data_peach);
    (void) melee_host_hsd_register_translator("ftDataPichu", fighter_data_pichu);
    (void) melee_host_hsd_register_translator("ftDataPikachu", fighter_data_pikachu);
    (void) melee_host_hsd_register_translator("ftDataPopo", fighter_data_popo);
    (void) melee_host_hsd_register_translator("ftDataPurin", fighter_data_purin);
    (void) melee_host_hsd_register_translator("ftDataSamus", fighter_data_samus);
    (void) melee_host_hsd_register_translator("ftDataSeak", fighter_data_seak);
    (void) melee_host_hsd_register_translator("ftDataYoshi", fighter_data_yoshi);
    (void) melee_host_hsd_register_translator("ftDataZelda", fighter_data_zelda);
    (void) melee_host_hsd_register_translator("ftDataCLink", fighter_data_clink);
    (void) melee_host_hsd_register_translator("ftDataEmblem", fighter_data_emblem);
    (void) melee_host_hsd_register_translator("ftDataGigaKoopa", fighter_data_gigakoopa);
    (void) melee_host_hsd_register_translator("lbBgFlashColAnimData",
                                              bg_flash_color_anims);
    (void) melee_host_hsd_register_translator("ftLoadCommonData",
                                              fighter_common_tables);
    (void) melee_host_hsd_register_translator("map_head", stage_map_head);
    (void) melee_host_hsd_register_translator("itPublicData",
                                              item_public_data);
    (void) melee_host_hsd_register_translator("tyInitModelTbl",
                                              trophy_init_models);
    (void) melee_host_hsd_register_translator("tyInitModelDTbl",
                                              trophy_init_models_other);
    (void) melee_host_hsd_register_translator("tyModelSortTbl",
                                              trophy_sort_keys);
    (void) melee_host_hsd_register_translator("tyExpDifferentTbl",
                                              trophy_number_list);
    (void) melee_host_hsd_register_translator("tyNoGetUsTbl",
                                              trophy_number_list);
    (void) melee_host_hsd_register_translator("tyDisplayModelTbl",
                                              trophy_display_rows);
    (void) melee_host_hsd_register_translator("tyDisplayModelUsTbl",
                                              trophy_display_rows);
    (void) melee_host_hsd_register_translator("grGroundParam", ground_param);
    (void) melee_host_hsd_register_translator("coll_data", stage_coll_data);
    (void) melee_host_hsd_register_translator("map_plit", stage_light_lists);
    (void) melee_host_hsd_register_translator("quake_model_set",
                                              stage_quake_model_set);
    (void) melee_host_hsd_register_translator("itemdata", stage_item_data);
    (void) melee_host_hsd_register_translator("ALDYakuAll", stage_yaku_scripts);
    (void) melee_host_hsd_register_translator("yakumono_param",
                                              stage_yakumono_param);
    (void) melee_host_hsd_register_translator("lbRefData", refract_data);
    (void) melee_host_hsd_register_translator("plLoadCommonData",
                                              player_common_data);
    (void) melee_host_hsd_register_translator("sqEventInitDataLevelTbl",
                                              event_level_table);
    (void) melee_host_hsd_register_translator("lbAudioLoadData",
                                              audio_load_data);
    (void) melee_host_hsd_register_translator("MemCardIconData",
                                              card_icon_table);
    (void) melee_host_hsd_register_translator("MemSnapIconData",
                                              card_icon_table);
    (void) melee_host_hsd_register_translator("mnNameAutoNameUs", string_array);
    (void) melee_host_hsd_register_translator("mnNameRefuseNameUs", string_array);
    (void) melee_host_hsd_register_translator("mnNameAutoName", string_array);
    (void) melee_host_hsd_register_translator("mnNameRefuseName", string_array);
    register_demo_motion_files();
}
