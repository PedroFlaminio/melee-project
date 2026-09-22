#ifndef MELEE_HOST_HSD_ARCHIVE_H
#define MELEE_HOST_HSD_ARCHIVE_H

#include <melee_host/host.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The host implements sysdolphin's archive API (HSD_ArchiveParse,
 * HSD_ArchiveGetPublicAddress, HSD_ArchiveGetExtern and
 * HSD_ArchiveLocateExtern) in place of archive.c.
 *
 * The original relocates the file in place, turning every 32-bit offset into
 * a pointer, which a 64-bit host cannot do: each pointer needs eight bytes
 * where the file has four.  Here the parse keeps the file's buffer as the
 * archive's identity, and a public symbol is rebuilt in host layout when it is
 * first asked for.  The symbol's suffix says what it is, which is the same
 * naming the game's own lookups rely on.
 *
 * The descriptors live until the same buffer is parsed again, which is when
 * the console would have overwritten the bytes they came from, or until they
 * are released explicitly. */

typedef enum MeleeHostHsdSymbolKind {
    MELEE_HOST_HSD_SYMBOL_UNSUPPORTED = 0,
    MELEE_HOST_HSD_SYMBOL_JOINT,
    MELEE_HOST_HSD_SYMBOL_ANIM_JOINT,
    MELEE_HOST_HSD_SYMBOL_MAT_ANIM_JOINT,
    MELEE_HOST_HSD_SYMBOL_SHAPE_ANIM_JOINT,
    MELEE_HOST_HSD_SYMBOL_CAMERA,
    MELEE_HOST_HSD_SYMBOL_SCENE_LIGHTS,
    MELEE_HOST_HSD_SYMBOL_FOG,
    MELEE_HOST_HSD_SYMBOL_SOBJ_DESC,
    MELEE_HOST_HSD_SYMBOL_FIGATREE,
    MELEE_HOST_HSD_SYMBOL_IMAGE_DESC,
    /* SceneDesc: models, cameras, lights and fogs. */
    MELEE_HOST_HSD_SYMBOL_SCENE_DATA,
    /* A NULL-terminated table of DynamicModelDesc pointers. */
    MELEE_HOST_HSD_SYMBOL_SCENE_MODELS,
    /* A symbol whose name carries no kind, recognised by its whole name. */
    MELEE_HOST_HSD_SYMBOL_RUMBLE_TABLE,
    /* A text table, recognised by the SIS_ its name starts with. */
    MELEE_HOST_HSD_SYMBOL_SIS_TABLE,
    /* Game data a translator is registered for by name; see
     * melee_host_hsd_register_translator. */
    MELEE_HOST_HSD_SYMBOL_GAME_DATA,
    MELEE_HOST_HSD_SYMBOL_KIND_COUNT
} MeleeHostHsdSymbolKind;

typedef struct MeleeHostHsdArchiveStats {
    mh_u32 archives_live;
    /* Public symbols rebuilt in host layout, and those refused: an unknown
     * kind, or a record the materializer would not guess at. */
    mh_u32 symbols_translated;
    mh_u32 symbols_refused;
    /* Pointer fields an extern resolved to NULL. */
    mh_u32 extern_fields_nulled;
    /* Externs the game asked to bind to an address, which the host cannot
     * do yet. */
    mh_u32 externs_refused;
    mh_u64 descriptor_bytes;
    mh_u64 payload_bytes;
} MeleeHostHsdArchiveStats;

/* The kind a public symbol's name selects, longest suffix first, so
 * `_matanim_joint` is not taken for `_joint`. */
MeleeHostHsdSymbolKind melee_host_hsd_symbol_kind(const char* symbol);
const char* melee_host_hsd_symbol_kind_name(MeleeHostHsdSymbolKind kind);

MeleeHostStatus
melee_host_hsd_archive_stats(MeleeHostHsdArchiveStats* out_stats);

/* Drops what was parsed from `bytes`.  Objects the original loaders built
 * still point into those descriptors, so this is only safe once they are
 * gone. */
MeleeHostStatus melee_host_hsd_archive_release(const void* bytes);
void melee_host_hsd_archive_release_all(void);

/* Why the last parse or lookup failed, including a refused symbol. */
const char* melee_host_hsd_archive_last_error(void);

/* The stage's own symbols, around the block of lookups grDatFiles_801C6038
 * makes.  It keeps whatever comes back, NULL included, and each stage's code
 * then reads the ones it needs without checking, so a symbol the host refused
 * would arrive as a NULL the stage follows.  Marking before the lookups and
 * checking after them stops with the reason instead, in the frame that asked
 * for the stage. */
void melee_host_stage_symbols_mark(void);
void melee_host_stage_symbols_check(void);

/* Game data whose layout only the game's C headers describe is translated by a
 * function registered under the symbol's name.  The translator reads the file
 * through the reader, which checks every offset against the data section and
 * every pointer field against the relocation table, and builds its result in
 * zeroed memory that lives as long as the archive's other descriptors.  A
 * translator that meets something it cannot translate calls
 * melee_host_hsd_reader_fail and returns NULL, and the lookup is refused with
 * that reason; a failed read or allocation does the same on its own. */
typedef struct MeleeHostHsdReader MeleeHostHsdReader;
typedef void* (*MeleeHostHsdTranslator)(MeleeHostHsdReader* reader,
                                        mh_u32 root_offset);

/* Registers `translator` for public symbols named `symbol`, replacing an
 * earlier one; NULL removes the registration. */
MeleeHostStatus melee_host_hsd_register_translator(
    const char* symbol, MeleeHostHsdTranslator translator);

mh_u32 melee_host_hsd_reader_data_size(MeleeHostHsdReader* reader);
/* Big-endian scalars at a data offset; zero once the reader has failed. */
mh_u8 melee_host_hsd_reader_u8(MeleeHostHsdReader* reader, mh_u32 offset);
mh_u16 melee_host_hsd_reader_u16(MeleeHostHsdReader* reader, mh_u32 offset);
mh_u32 melee_host_hsd_reader_u32(MeleeHostHsdReader* reader, mh_u32 offset);
mh_f32 melee_host_hsd_reader_f32(MeleeHostHsdReader* reader, mh_u32 offset);
/* Whether the field at `field` is relocated, without judging it. */
bool melee_host_hsd_reader_has_pointer(MeleeHostHsdReader* reader,
                                       mh_u32 field);
/* The pointer field at `field`: true with the target's data offset when it is
 * relocated, false when it is NULL.  A non-zero field without a relocation
 * fails the reader. */
bool melee_host_hsd_reader_pointer(MeleeHostHsdReader* reader, mh_u32 field,
                                   mh_u32* out_target);
/* `length` bytes of the host's verbatim copy of the data at `offset`. */
void* melee_host_hsd_reader_payload(MeleeHostHsdReader* reader, mh_u32 offset,
                                    size_t length);
/* The command stream at `offset` (a script or a color overlay), its words
 * converted in place to native order for the host's command layouts, with
 * every subroutine and goto target stored as its distance from the operand
 * word; the streams those reach are converted too.  Asking again returns the
 * same words. */
void* melee_host_hsd_reader_command_stream(MeleeHostHsdReader* reader,
                                           mh_u32 offset);
/* The HSD_Joint, HSD_AnimJoint, HSD_MatAnimJoint or HSD_ShapeAnimJoint tree at
 * `offset`, built as the joint and animation symbols are and shared with every
 * other reference to the same address. */
void* melee_host_hsd_reader_joint(MeleeHostHsdReader* reader, mh_u32 offset);
void* melee_host_hsd_reader_anim_joint(MeleeHostHsdReader* reader,
                                       mh_u32 offset);
void* melee_host_hsd_reader_mat_anim_joint(MeleeHostHsdReader* reader,
                                           mh_u32 offset);
void* melee_host_hsd_reader_shape_anim_joint(MeleeHostHsdReader* reader,
                                             mh_u32 offset);
/* The bytes from `offset` to the next address a relocation targets or a public
 * symbol names: the most that a block recording no length can hold. */
mh_u32 melee_host_hsd_reader_extent(MeleeHostHsdReader* reader, mh_u32 offset);
/* The HSD_CObjDesc, HSD_FogDesc, NULL-terminated LightList table,
 * HSD_MObjDesc or HSD_Spline at `offset`, built as the scene and joint symbols
 * build them; lights and materials are shared with every other reference to
 * the same address. */
void* melee_host_hsd_reader_camera(MeleeHostHsdReader* reader, mh_u32 offset);
void* melee_host_hsd_reader_fog(MeleeHostHsdReader* reader, mh_u32 offset);
void* melee_host_hsd_reader_light_lists(MeleeHostHsdReader* reader,
                                        mh_u32 offset);
void* melee_host_hsd_reader_mobj(MeleeHostHsdReader* reader, mh_u32 offset);
void* melee_host_hsd_reader_spline(MeleeHostHsdReader* reader, mh_u32 offset);
/* The HSD_LightDesc already built from the record at `offset`, or NULL when
 * none was; asking never builds one. */
void* melee_host_hsd_reader_light_built_at(MeleeHostHsdReader* reader,
                                           mh_u32 offset);
void* melee_host_hsd_reader_allocate(MeleeHostHsdReader* reader, size_t size,
                                     size_t alignment);
void melee_host_hsd_reader_fail(MeleeHostHsdReader* reader,
                                const char* reason);
bool melee_host_hsd_reader_failed(const MeleeHostHsdReader* reader);

#ifdef __cplusplus
}
#endif

#endif
