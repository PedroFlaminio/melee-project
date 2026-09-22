#ifndef MELEE_HOST_HSD_MATERIALIZE_HPP
#define MELEE_HOST_HSD_MATERIALIZE_HPP

#include "assets/hsd_runtime_archive.hpp"
#include "hsd_graphics_types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* Particle banks as psInitDataBankLoad reads them on the host, defined in
 * sysdolphin/baselib/psstructs.h.  They are the game's C types, so they are
 * declared outside the namespace. */
struct MeleeHostParticleCmdBank;
struct MeleeHostParticleTexBank;

namespace melee::assets {

struct HsdMaterializeStats {
    std::size_t joints;
    std::size_t dobj_descs;
    std::size_t mobj_descs;
    std::size_t tobj_descs;
    std::size_t pobj_descs;
    std::size_t vertex_descriptors;
    std::size_t image_descs;
    std::size_t tlut_descs;
    std::size_t robj_descs;
    std::size_t camera_descs;
    std::size_t anim_joints;
    std::size_t mat_anim_joints;
    std::size_t shape_anim_joints;
    std::size_t aobj_descs;
    std::size_t fobj_descs;
    std::size_t anim_data_bytes;
    std::size_t figa_trees;
    std::size_t figa_tracks;
    std::size_t shape_set_descs;
    std::size_t envelope_descs;
    std::size_t light_descs;
    std::size_t fog_descs;
    std::size_t sobj_descs;
    std::size_t descriptor_bytes;
    std::size_t payload_bytes;
};

/* The `LightList` src/melee/sc/types.h declares inside SceneDesc.  C gives a
 * nested struct file scope and C++ does not, so under C++ that name would be a
 * different type from the one sc/forward.h declares.  The host names its own
 * copy of the two pointers instead, which is the same layout. */
struct MaterializedLightList {
    HSD_LightDesc* desc;
    HSD_LightAnim** anims;
};

/* One entry of lbRumbleData, which lb_013B.c reads as
 * struct Fighter_804D653C_t: a rumble command list and the priority
 * HSD_PadRumbleAdd is given with it. */
struct MaterializedRumbleEntry {
    void* commands;
    u8 priority;
    u8 unk5;
};

/* The host-layout form of MnSelectChrDataTable.  The game keeps its matching
 * declaration private to mncharsel.c; its fields are intentionally identical
 * so the C code can keep using its original type. */
struct MaterializedStaticModel {
    HSD_Joint* joint;
    HSD_AnimJoint* animjoint;
    HSD_MatAnimJoint* matanim_joint;
    HSD_ShapeAnimJoint* shapeanim_joint;
};

struct MaterializedCharacterSelectData {
    HSD_CObjDesc* cam;
    HSD_LightDesc* light0;
    HSD_LightDesc* light1;
    HSD_FogDesc* fog;
    MaterializedStaticModel models[9];
};

struct MaterializedStageSelectData {
    HSD_CObjDesc* cam;
    HSD_LightDesc* light0;
    HSD_LightDesc* light1;
    HSD_FogDesc* fog;
    MaterializedStaticModel models[11];
    MaterializedStaticModel random_stage;
};

/* An effect archive's `eff*DataTable` in host layout.  It is laid out like
 * EF_DAT_Entry, whose `data` is the address of the first effect, and each
 * effect like EF_EffectDesc. */
struct MaterializedEffectDesc {
    f32 lifetime;
    MaterializedStaticModel model;
};

struct MaterializedEffectTable {
    MeleeHostParticleCmdBank* command_bank;
    MeleeHostParticleTexBank* texture_bank;
    MaterializedEffectDesc effects[1];
};

/* SceneDesc and the records it names, laid out as src/melee/sc/types.h
 * declares them.  The game types a fog's animations as HSD_CameraAnim and
 * reads only the AObjDesc they start with. */
struct MaterializedDynamicModel {
    HSD_Joint* joint;
    HSD_AnimJoint** anims;
    HSD_MatAnimJoint** matanims;
    HSD_ShapeAnimJoint** shapeanims;
};

struct MaterializedSceneCamera {
    HSD_CObjDesc* desc;
    HSD_CameraAnim** anims;
};

struct MaterializedSceneFog {
    HSD_FogDesc* desc;
    HSD_CameraAnim** anims;
};

struct MaterializedSceneDesc {
    MaterializedDynamicModel** models;
    MaterializedSceneCamera* cameras;
    MaterializedLightList** lights;
    MaterializedSceneFog* fogs;
};

/*
 * Rebuilds HSD descriptors in host layout so the original object loaders can
 * walk them unchanged.
 *
 * The archive stores descriptors exactly as the PowerPC saw them: big-endian
 * scalars, and pointer fields holding a 32-bit offset that archive.c adds the
 * data base to, in place.  A 64-bit host cannot do that.  Every pointer field
 * would need eight bytes where the file has four, so relocating in place would
 * overwrite the field that follows.  Instead each descriptor is allocated
 * again in host layout and its pointers filled with real host addresses.
 *
 * GX payloads are not translated.  Display lists, vertex arrays, image data
 * and palettes keep their big-endian bytes, because the display-list
 * interpreter and the texture decoders read them in that order.  They live in
 * one verbatim copy of the data section, which is why pointers into them need
 * no length.
 *
 * Descriptors come out of a single contiguous block.  That is what keeps
 * `jobj->id = (u32) joint` usable: the original keys its ID table on the
 * truncated joint address, and truncation stays injective as long as every
 * joint shares the same high 32 bits.
 */
class HsdMaterializedArchive final {
public:
    explicit HsdMaterializedArchive(const HsdRuntimeArchive& archive);
    ~HsdMaterializedArchive();

    HsdMaterializedArchive(const HsdMaterializedArchive&) = delete;
    HsdMaterializedArchive& operator=(const HsdMaterializedArchive&) = delete;

    /* Materializes the joint tree rooted at a public symbol naming a joint. */
    [[nodiscard]] HSD_Joint* joint(std::string_view public_symbol);

    /* Materializes a FigaTree, the game's own animation container.  Unlike the
     * HSD trees this one is flat: a list saying how many tracks each bone
     * takes, and the tracks laid end to end.  A character keeps one per
     * action. */
    [[nodiscard]] FigaTree* figa_tree(std::string_view public_symbol);

    /* Materializes an animation tree rooted at a public symbol.  The three
     * kinds travel together: one drives the joints' transforms, one the
     * materials, one the shape blends.  Any of them can be absent. */
    [[nodiscard]] HSD_AnimJoint* anim_joint(std::string_view public_symbol);
    [[nodiscard]] HSD_MatAnimJoint* mat_anim_joint(
        std::string_view public_symbol);
    [[nodiscard]] HSD_ShapeAnimJoint* shape_anim_joint(
        std::string_view public_symbol);

    /* The animation tables a scene model carries, which is how a scene names
     * several animations for one model.  Index selects within the table. */
    [[nodiscard]] HSD_AnimJoint* scene_model_anim(
        std::string_view public_symbol, std::size_t model_index,
        std::size_t anim_index);
    [[nodiscard]] HSD_MatAnimJoint* scene_model_mat_anim(
        std::string_view public_symbol, std::size_t model_index,
        std::size_t anim_index);
    [[nodiscard]] HSD_ShapeAnimJoint* scene_model_shape_anim(
        std::string_view public_symbol, std::size_t model_index,
        std::size_t anim_index);
    [[nodiscard]] std::size_t scene_model_anim_count(
        std::string_view public_symbol, std::size_t model_index);

    /* Materializes SceneDesc.cameras[index].desc, the camera the scene
     * carries.  Returns nullptr when the scene names no camera there. */
    [[nodiscard]] HSD_CObjDesc* scene_camera(std::string_view public_symbol,
                                             std::size_t camera_index);

    /* Materializes SceneDesc.models[index]->joint, the indirection the game's
     * own scene entry points walk. */
    [[nodiscard]] HSD_Joint* scene_model_joint(std::string_view public_symbol,
                                               std::size_t model_index);
    [[nodiscard]] std::size_t
    scene_model_count(std::string_view public_symbol) const;

    /* A `*_scene_data` symbol whole: the model table, the cameras, the light
     * lists and the fogs, each with its animation tables. */
    [[nodiscard]] MaterializedSceneDesc*
    scene_desc(std::string_view public_symbol);

    /* A `*_scene_models` symbol: a NULL-terminated table of models laid out
     * as DynamicModelDesc, each a joint and its animation tables. */
    [[nodiscard]] MaterializedDynamicModel**
    scene_models(std::string_view public_symbol);

    /* A camera named on its own, which is how a menu or the title keeps one
     * when there is no SceneDesc around it. */
    [[nodiscard]] HSD_CObjDesc* camera(std::string_view public_symbol);

    /* The NULL-terminated table a `*_scene_lights` symbol names, one light
     * list per entry, which is what lb_80011AC4 walks. */
    [[nodiscard]] MaterializedLightList**
    scene_lights(std::string_view public_symbol);

    [[nodiscard]] HSD_FogDesc* fog(std::string_view public_symbol);

    [[nodiscard]] MaterializedCharacterSelectData*
    character_select_data(std::string_view public_symbol);
    [[nodiscard]] MaterializedStageSelectData*
    stage_select_data(std::string_view public_symbol);

    /* An effect archive's table: its two particle banks, both NULL when the
     * archive has no particles, then the effects efLib_Create indexes. */
    [[nodiscard]] MaterializedEffectTable*
    effect_data_table(std::string_view public_symbol);
    /* A stage's `map_ptcl` and `map_texg`: the same banks, named on their
     * own. */
    [[nodiscard]] MeleeHostParticleCmdBank*
    particle_command_bank(std::string_view public_symbol);
    [[nodiscard]] MeleeHostParticleTexBank*
    particle_texture_bank(std::string_view public_symbol);

    /* An image and optional palette drawn as a screen sprite. */
    [[nodiscard]] HSD_SObjDesc* sobj_desc(std::string_view public_symbol);
    /* A loose image a stage names directly, rather than one reached
     * through a TObj or a sprite. */
    [[nodiscard]] HSD_ImageDesc* image_desc(std::string_view public_symbol);

    /* lbRumbleData: a table with neither count nor terminator, which runs as
     * long as each record's command pointer is relocated. */
    [[nodiscard]] MaterializedRumbleEntry*
    rumble_table(std::string_view public_symbol);

    /* A `SIS_*` text table: one pointer per string, as long as the fields are
     * relocated.  The text archives keep the table at the start of the data
     * and relocate nothing else.  The strings stay verbatim, byte order
     * included, which is how the text interpreter reads them. */
    [[nodiscard]] u8** sis_table(std::string_view public_symbol);

    /* Declares a pointer field that HSD_ArchiveLocateExtern resolved to NULL.
     * The file threads the extern's chain through those fields, so until it
     * is declared a field holds the next link: a non-zero value with no
     * relocation, which would be refused. */
    void declare_null_field(std::uint32_t data_offset);

    [[nodiscard]] const HsdMaterializeStats& stats() const noexcept;

    /* What a translator written in C reaches through the reader in
     * hsd_host_archive.cpp: the file, the descriptor arena, pointer fields
     * with the same NULL and relocation rules as the schemas here, and the
     * verbatim payload. */
    [[nodiscard]] const HsdRuntimeArchive& runtime() const noexcept;
    void* translator_allocate(std::size_t size, std::size_t alignment);
    [[nodiscard]] std::optional<HsdRuntimeNode>
    translator_pointer(std::uint32_t field_offset) const;
    [[nodiscard]] void* translator_payload(std::uint32_t data_offset,
                                           std::size_t length) const;
    /* The command stream at `data_offset`, its words converted in place to
     * the native order host_command_layout.h lays out.  Converting it again
     * returns the same words. */
    [[nodiscard]] void* translator_command_stream(std::uint32_t data_offset);
    /* The joint tree and the animations at `data_offset`, built as a joint or
     * animation symbol is and shared with every other reference to it. */
    [[nodiscard]] HSD_Joint* translator_joint(std::uint32_t data_offset);
    [[nodiscard]] HSD_AnimJoint*
    translator_anim_joint(std::uint32_t data_offset);
    [[nodiscard]] HSD_MatAnimJoint*
    translator_mat_anim_joint(std::uint32_t data_offset);
    [[nodiscard]] HSD_ShapeAnimJoint*
    translator_shape_anim_joint(std::uint32_t data_offset);
    /* The bytes from `data_offset` to the next address a relocation targets or
     * a public symbol names: the most that a block recording no length of its
     * own can hold. */
    [[nodiscard]] std::uint32_t translator_extent(std::uint32_t data_offset);
    /* Descriptors a stage's map_head points at, built as the scene symbols
     * build them.  A light, a material and a joint are shared with every
     * other reference to the same address; translator_light_built_at returns
     * a light only once one was built there. */
    [[nodiscard]] HSD_CObjDesc* translator_camera(std::uint32_t data_offset);
    [[nodiscard]] HSD_FogDesc* translator_fog(std::uint32_t data_offset);
    [[nodiscard]] MaterializedLightList**
    translator_light_lists(std::uint32_t data_offset);
    [[nodiscard]] HSD_LightDesc*
    translator_light_built_at(std::uint32_t data_offset) const;
    [[nodiscard]] HSD_MObjDesc* translator_mobj(std::uint32_t data_offset);
    [[nodiscard]] HSD_Spline* translator_spline(std::uint32_t data_offset);

private:
    MaterializedLightList** light_list_table(HsdRuntimeNode table);
    /* Every light descriptor built, by the address of its record. */
    std::unordered_map<std::uint32_t, HSD_LightDesc*> light_descs_;

    void index_stream_boundaries();
    void* command_stream(HsdRuntimeNode start);

    /* Where a command stream stops: every relocation target and public root,
     * sorted.  Indexed with the relocated fields the first time a stream is
     * converted, together with which payload words already are. */
    std::vector<std::uint32_t> stream_boundaries_;
    std::unordered_map<std::uint32_t, std::uint32_t> relocated_fields_;
    std::vector<bool> converted_words_;
    bool streams_indexed_ = false;

    template <typename T> T* allocate();
    void* allocate_bytes(std::size_t size, std::size_t alignment);
    /* A block owned by this archive but outside the descriptor arena, for
     * data that is large and holds no joint, so that joints keep sharing the
     * arena's upper 32 bits.  Zeroed, aligned to at most 32 bytes. */
    void* allocate_outside_arena(std::size_t size, std::size_t alignment);

    [[nodiscard]] std::optional<HsdRuntimeNode>
    reference(HsdRuntimeNode node, std::uint32_t relative_offset) const;
    [[nodiscard]] void* payload(HsdRuntimeNode node,
                                std::size_t length) const;
    [[nodiscard]] char* payload_string(HsdRuntimeNode node) const;
    void read_vec3(HsdRuntimeNode node, std::uint32_t relative_offset,
                   Vec3* out) const;
    void read_color(HsdRuntimeNode node, std::uint32_t relative_offset,
                    GXColor* out) const;

    HSD_Joint* joint_chain(HsdRuntimeNode node);
    HSD_DObjDesc* dobj_chain(HsdRuntimeNode node);
    HSD_PObjDesc* pobj_chain(HsdRuntimeNode node);
    HSD_MObjDesc* mobj_desc(HsdRuntimeNode node);
    HSD_TObjDesc* tobj_chain(HsdRuntimeNode node);
    HSD_RObjDesc* robj_chain(HsdRuntimeNode node);
    HSD_RvalueList* rvalue_list(HsdRuntimeNode node);
    HSD_CObjDesc* camera_desc(HsdRuntimeNode node);
    HSD_FogDesc* fog_desc(HsdRuntimeNode node);
    HSD_Spline* spline_desc(HsdRuntimeNode node);
    MeleeHostParticleCmdBank* command_bank_at(HsdRuntimeNode bank);
    MeleeHostParticleTexBank* texture_bank_at(HsdRuntimeNode bank);
    HSD_LightDesc* light_desc_chain(HsdRuntimeNode node);
    HSD_LightAnim* light_anim_chain(HsdRuntimeNode node);
    HSD_WObjAnim* world_anim(HsdRuntimeNode node);
    HSD_FogAdjDesc* fog_adj_desc(HsdRuntimeNode node);
    MaterializedDynamicModel* dynamic_model(HsdRuntimeNode node);
    HSD_CameraAnim* camera_anim(HsdRuntimeNode node);
    HSD_CameraAnim* fog_anim(HsdRuntimeNode node);
    [[nodiscard]] std::size_t scene_entry_count(HsdRuntimeNode array);
    /* A NULL-terminated table of pointers, each entry built by `build`. */
    template <typename T>
    T** pointer_table(HsdRuntimeNode table,
                      T* (HsdMaterializedArchive::*build)(HsdRuntimeNode),
                      const char* what);
    [[nodiscard]] std::optional<HsdRuntimeNode> scene_model_anim_entry(
        std::string_view public_symbol, std::size_t model_index,
        std::uint32_t table_offset, std::size_t anim_index);
    HSD_AnimJoint* anim_joint_chain(HsdRuntimeNode node);
    HSD_MatAnimJoint* mat_anim_joint_chain(HsdRuntimeNode node);
    HSD_ShapeAnimJoint* shape_anim_joint_chain(HsdRuntimeNode node);
    HSD_MatAnim* mat_anim_chain(HsdRuntimeNode node);
    HSD_TexAnim* tex_anim_chain(HsdRuntimeNode node);
    HSD_RenderAnim* render_anim(HsdRuntimeNode node);
    HSD_ShapeAnimDObj* shape_anim_dobj_chain(HsdRuntimeNode node);
    HSD_ShapeAnim* shape_anim_chain(HsdRuntimeNode node);
    HSD_RObjAnimJoint* robj_anim_chain(HsdRuntimeNode node);
    template <typename T> T* anim_link_chain(HsdRuntimeNode node);
    HSD_AObjDesc* aobj_desc(HsdRuntimeNode node);
    FigaTree* figa_tree_at(HsdRuntimeNode node);
    HSD_FObjDesc* fobj_chain(HsdRuntimeNode node);
    HSD_WObjDesc* world_desc(HsdRuntimeNode node);
    HSD_VtxDescList* vertex_descriptors(HsdRuntimeNode node);
    HSD_ShapeSetDesc* shape_set_desc(HsdRuntimeNode node);
    HSD_EnvelopeDesc** envelope_array(HsdRuntimeNode node);
    HSD_EnvelopeDesc* envelope_list(HsdRuntimeNode node);
    u8** payload_pointer_array(HsdRuntimeNode node, std::size_t count);
    HSD_ImageDesc* image_desc(HsdRuntimeNode node);
    HSD_TlutDesc* tlut_desc(HsdRuntimeNode node);
    HSD_Material* material(HsdRuntimeNode node);
    HSD_PEDesc* pixel_engine_desc(HsdRuntimeNode node);
    HSD_TexLODDesc* lod_desc(HsdRuntimeNode node);
    HSD_TObjTevDesc* tev_desc(HsdRuntimeNode node);
    f32* matrix(HsdRuntimeNode node);

    const HsdRuntimeArchive& archive_;
    std::byte* payload_ = nullptr;
    std::size_t payload_size_ = 0;
    std::byte* descriptors_ = nullptr;
    std::size_t descriptor_capacity_ = 0;
    std::size_t descriptor_used_ = 0;
    std::vector<std::byte*> outside_arena_;
    std::size_t depth_ = 0;

    std::unordered_map<std::uint32_t, HSD_Joint*> joints_;
    std::unordered_map<std::uint32_t, HSD_DObjDesc*> dobjs_;
    std::unordered_map<std::uint32_t, HSD_MObjDesc*> mobjs_;
    std::unordered_map<std::uint32_t, HSD_TObjDesc*> tobjs_;
    std::unordered_map<std::uint32_t, HSD_PObjDesc*> pobjs_;
    std::unordered_map<std::uint32_t, HSD_VtxDescList*> vertex_lists_;
    std::unordered_map<std::uint32_t, HSD_ImageDesc*> images_;
    std::unordered_map<std::uint32_t, HSD_TlutDesc*> tluts_;
    std::unordered_map<std::uint32_t, HSD_RObjDesc*> robjs_;
    std::unordered_map<std::uint32_t, HSD_ShapeSetDesc*> shape_sets_;
    std::unordered_map<std::uint32_t, HSD_EnvelopeDesc**> envelope_arrays_;
    std::unordered_map<std::uint32_t, HSD_AnimJoint*> anim_joints_;
    std::unordered_map<std::uint32_t, HSD_MatAnimJoint*> mat_anim_joints_;
    std::unordered_map<std::uint32_t, HSD_ShapeAnimJoint*> shape_anim_joints_;
    std::unordered_map<std::uint32_t, HSD_AObjDesc*> aobj_descs_;
    std::unordered_set<std::uint32_t> null_fields_;
    HsdMaterializeStats stats_{};
};

} // namespace melee::assets

#endif
