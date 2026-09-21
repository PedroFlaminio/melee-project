/* Host implementation of the GX state-setting API.
 *
 * The GameCube's GX library writes hardware register images; the host instead
 * keeps the state the API describes, because the renderer consumes GX state,
 * not Flipper registers.  Every entry point here is one the ported game code
 * actually calls, and each one records enough to reconstruct a draw.
 *
 * The opaque SDK objects (GXTexObj, GXTlutObj, GXLightObj) stay self-contained:
 * their payload is packed into the blob the game owns, with 64-bit pointers
 * split across two 32-bit words rather than truncated.  That keeps object
 * lifetime with the game, exactly as on hardware, and avoids a host-side
 * registry that a memcpy of the blob would desynchronize.
 */

#include <melee_host/gx.h>

#include <dolphin/gx/GXCull.h>
#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXGeometry.h>
#include <dolphin/gx/GXGet.h>
#include <dolphin/gx/GXManage.h>
#include <dolphin/vi/vitypes.h>
#include <dolphin/gx/GXFrameBuffer.h>
#include <dolphin/gx/GXLighting.h>
#include <dolphin/gx/GXPixel.h>
#include <dolphin/gx/GXStruct.h>
#include <dolphin/gx/GXTev.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTransform.h>
#include <dolphin/mtx.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <mutex>

namespace {

std::mutex state_mutex;

constexpr std::size_t kTexMaps = MELEE_HOST_GX_MAX_TEXMAP;
constexpr std::size_t kTexCoords = MELEE_HOST_GX_MAX_TEXCOORD;
constexpr std::size_t kTevStages = MELEE_HOST_GX_MAX_TEVSTAGE;
constexpr std::size_t kChannels = MELEE_HOST_GX_MAX_CHANNEL;
constexpr std::size_t kLights = MELEE_HOST_GX_MAX_LIGHT;
constexpr std::size_t kMatrixRows = MELEE_HOST_GX_MATRIX_ROWS;
constexpr std::size_t kTluts = MELEE_HOST_GX_MAX_TLUT;

/* Packed payloads for the SDK's opaque objects.  Sizes are asserted against
 * the structures the game allocates, so a layout mistake fails to compile
 * instead of corrupting neighbouring fields. */
struct PackedTexObj {
    std::uint32_t image_low;
    std::uint32_t image_high;
    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t format;
    std::uint8_t wrap_s;
    std::uint8_t wrap_t;
    std::uint8_t mipmap;
    std::uint32_t tlut_name;
    std::uint8_t min_filter;
    std::uint8_t mag_filter;
    std::uint8_t bias_clamp;
    std::uint8_t edge_lod;
    std::uint8_t max_anisotropy;
    std::uint8_t min_lod_q4;
    std::uint8_t max_lod_q4;
    std::uint8_t color_indexed;
    float lod_bias;
};
static_assert(sizeof(PackedTexObj) == sizeof(GXTexObj),
              "packed texture payload must fit the SDK object exactly");

struct PackedTlutObj {
    std::uint32_t entries_low;
    std::uint32_t entries_high;
    std::uint16_t entry_count;
    std::uint8_t format;
    std::uint8_t reserved;
};
static_assert(sizeof(PackedTlutObj) == sizeof(GXTlutObj),
              "packed TLUT payload must fit the SDK object exactly");

struct PackedLightObj {
    std::uint8_t color[4];
    float position[3];
    float direction[3];
    float angle_attenuation[3];
    float distance_attenuation[3];
    std::uint32_t reserved[3];
};
static_assert(sizeof(PackedLightObj) == sizeof(GXLightObj),
              "packed light payload must fit the SDK object exactly");

/* The swap tables GXInit installs, which tev.cpp evaluates with. */
constexpr std::array<std::array<mh_u32, 4>, 4> kInitSwapTables{ {
    { GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA },
    { GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA },
    { GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA },
    { GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA },
} };

MeleeHostGxPixelState pixel_state{};
MeleeHostGxTransformState transform_state{};
MeleeHostGxTevState tev_state{};
MeleeHostGxFogState fog_state{};
MeleeHostGxCopyState copy_state{};
MeleeHostGxDisplayCopyState display_copy_state{};
MeleeHostGxDrawSyncState draw_sync_state{};
GXDrawDoneCallback draw_done_callback = nullptr;
MeleeHostGxFrameSink frame_sink = nullptr;
void* frame_sink_user_data = nullptr;
std::array<MeleeHostGxChannelControl, kChannels> channel_controls{};
std::array<MeleeHostGxTextureDesc, kTexMaps> bound_textures{};
std::array<MeleeHostGxTlutDesc, kTluts> loaded_tluts{};
std::array<MeleeHostGxLightDesc, kLights> lights{};
std::array<std::array<float, 4>, kMatrixRows> matrix_memory{};
/* Matrix memory starts as zeros, not identity, so a consumer that transforms
 * by it has to know whether the game ever loaded the row it is about to use. */
std::array<bool, kMatrixRows> matrix_loaded{};
/* GX keeps normal matrices apart from position and texture matrices: a normal
 * matrix is 3x3, in its own region of transform memory, addressed by the same
 * GX_PNMTXn id as the position matrix it goes with.  HSD loads a lit PObj's
 * inverse transpose there right after its position matrix, and sharing the
 * rows dropped the view translation from every lit vertex.  Nothing reads them
 * back yet: the capture derives normals from the position matrix. */
std::array<std::array<float, 3>, kMatrixRows> normal_matrix_memory{};

/* Indirect texturing, as the GX calls leave it.  It is folded into each
 * captured draw state, and the presenter uses it to generate the TEV sample
 * coordinate. */
constexpr std::size_t kIndirectStages = MELEE_HOST_GX_MAX_INDIRECT_STAGE;
constexpr std::size_t kIndirectMatrices = MELEE_HOST_GX_MAX_INDIRECT_MATRIX;
MeleeHostGxIndirectState indirect_state{};

/* Per texture coordinate, whether lines and points take the texture offsets
 * GXEnableTexOffsets turns on.  Recorded only: the presenter does not draw
 * lines or points as textured sprites yet. */
struct TextureOffsets {
    bool lines = false;
    bool points = false;
};
std::array<TextureOffsets, kTexCoords> texture_offsets{};

/* GXSetTevOp is shorthand: the SDK expands it into the input and operation
 * calls, so the stage the hardware sees is the expanded one.  Recording only
 * the mode would leave every reader to repeat the expansion. */
void apply_tev_preset_locked(std::size_t stage, GXTevMode mode)
{
    MeleeHostGxTevStage& slot = tev_state.stages[stage];
    const mh_u32 color_prev = stage == 0 ? static_cast<mh_u32>(GX_CC_RASC)
                                         : static_cast<mh_u32>(GX_CC_CPREV);
    const mh_u32 alpha_prev = stage == 0 ? static_cast<mh_u32>(GX_CA_RASA)
                                         : static_cast<mh_u32>(GX_CA_APREV);
    const auto color = [&slot](mh_u32 a, mh_u32 b, mh_u32 c, mh_u32 d) {
        slot.color_input[0] = a;
        slot.color_input[1] = b;
        slot.color_input[2] = c;
        slot.color_input[3] = d;
    };
    const auto alpha = [&slot](mh_u32 a, mh_u32 b, mh_u32 c, mh_u32 d) {
        slot.alpha_input[0] = a;
        slot.alpha_input[1] = b;
        slot.alpha_input[2] = c;
        slot.alpha_input[3] = d;
    };
    constexpr mh_u32 kCZero = GX_CC_ZERO;
    constexpr mh_u32 kAZero = GX_CA_ZERO;
    switch (mode) {
    case GX_MODULATE:
        color(kCZero, GX_CC_TEXC, color_prev, kCZero);
        alpha(kAZero, GX_CA_TEXA, alpha_prev, kAZero);
        break;
    case GX_DECAL:
        color(color_prev, GX_CC_TEXC, GX_CC_TEXA, kCZero);
        alpha(kAZero, kAZero, kAZero, alpha_prev);
        break;
    case GX_BLEND:
        color(color_prev, GX_CC_ONE, GX_CC_TEXC, kCZero);
        alpha(kAZero, GX_CA_TEXA, alpha_prev, kAZero);
        break;
    case GX_REPLACE:
        color(kCZero, kCZero, kCZero, GX_CC_TEXC);
        alpha(kAZero, kAZero, kAZero, GX_CA_TEXA);
        break;
    case GX_PASSCLR:
        color(kCZero, kCZero, kCZero, color_prev);
        alpha(kAZero, kAZero, kAZero, alpha_prev);
        break;
    default:
        return;
    }
    slot.color_op = static_cast<mh_u32>(GX_TEV_ADD);
    slot.alpha_op = static_cast<mh_u32>(GX_TEV_ADD);
    slot.color_bias = static_cast<mh_u32>(GX_TB_ZERO);
    slot.alpha_bias = static_cast<mh_u32>(GX_TB_ZERO);
    slot.color_scale = static_cast<mh_u32>(GX_CS_SCALE_1);
    slot.alpha_scale = static_cast<mh_u32>(GX_CS_SCALE_1);
    slot.color_clamp = true;
    slot.alpha_clamp = true;
    slot.color_out_reg = static_cast<mh_u32>(GX_TEVPREV);
    slot.alpha_out_reg = static_cast<mh_u32>(GX_TEVPREV);
    slot.mode = static_cast<mh_u32>(mode);
}

/* The default state mirrors the SDK's post-GXInit configuration for the
 * fields the port models, so a reset leaves a usable pipeline rather than
 * zeroes that no real frame would ever set. */
void reset_locked()
{
    pixel_state = MeleeHostGxPixelState{};
    pixel_state.z_compare_enable = true;
    pixel_state.z_update_enable = true;
    pixel_state.z_func = static_cast<mh_u32>(GX_LEQUAL);
    pixel_state.blend_mode = static_cast<mh_u32>(GX_BM_NONE);
    pixel_state.blend_src_factor = static_cast<mh_u32>(GX_BL_SRCALPHA);
    pixel_state.blend_dst_factor = static_cast<mh_u32>(GX_BL_INVSRCALPHA);
    pixel_state.blend_logic_op = static_cast<mh_u32>(GX_LO_CLEAR);
    pixel_state.color_update_enable = true;
    pixel_state.alpha_update_enable = true;
    pixel_state.alpha_compare_0 = static_cast<mh_u32>(GX_ALWAYS);
    pixel_state.alpha_compare_1 = static_cast<mh_u32>(GX_ALWAYS);
    pixel_state.alpha_op = static_cast<mh_u32>(GX_AOP_AND);
    pixel_state.cull_mode = static_cast<mh_u32>(GX_CULL_BACK);

    transform_state = MeleeHostGxTransformState{};
    transform_state.projection_type = static_cast<mh_u32>(GX_PERSPECTIVE);
    transform_state.current_matrix = static_cast<mh_u32>(GX_PNMTX0);

    /* GXInit: one stage replacing with texture map 0 through coordinate 0,
     * orders for the first eight stages, a 2x4 identity texgen per
     * coordinate, and no lighting channels. */
    tev_state = MeleeHostGxTevState{};
    tev_state.stage_count = 1;
    tev_state.texcoord_gen_count = 1;
    tev_state.channel_count = 0;
    for (std::size_t stage = 0; stage < kTevStages; ++stage) {
        MeleeHostGxTevStage& slot = tev_state.stages[stage];
        const bool ordered = stage < kTexMaps;
        slot.mode = static_cast<mh_u32>(GX_REPLACE);
        slot.texcoord = ordered ? static_cast<mh_u32>(stage)
                                : static_cast<mh_u32>(GX_TEXCOORD_NULL);
        slot.texmap = ordered ? static_cast<mh_u32>(stage)
                              : static_cast<mh_u32>(GX_TEXMAP_NULL);
        slot.color_channel = ordered ? static_cast<mh_u32>(GX_COLOR0A0)
                                     : static_cast<mh_u32>(GX_COLOR_NULL);
        slot.color_op = static_cast<mh_u32>(GX_TEV_ADD);
        slot.alpha_op = static_cast<mh_u32>(GX_TEV_ADD);
        slot.color_bias = static_cast<mh_u32>(GX_TB_ZERO);
        slot.alpha_bias = static_cast<mh_u32>(GX_TB_ZERO);
        slot.color_scale = static_cast<mh_u32>(GX_CS_SCALE_1);
        slot.alpha_scale = static_cast<mh_u32>(GX_CS_SCALE_1);
        slot.color_out_reg = static_cast<mh_u32>(GX_TEVPREV);
        slot.alpha_out_reg = static_cast<mh_u32>(GX_TEVPREV);
        slot.color_clamp = true;
        slot.alpha_clamp = true;
        slot.konst_color_select = static_cast<mh_u32>(GX_TEV_KCSEL_1_4);
        slot.konst_alpha_select = static_cast<mh_u32>(GX_TEV_KASEL_1);
        slot.raster_swap = static_cast<mh_u32>(GX_TEV_SWAP0);
        slot.texture_swap = static_cast<mh_u32>(GX_TEV_SWAP0);
    }
    apply_tev_preset_locked(0, GX_REPLACE);
    for (std::size_t coord = 0; coord < kTexCoords; ++coord) {
        MeleeHostGxTexCoordGen& gen = tev_state.texcoord_gens[coord];
        gen.function = static_cast<mh_u32>(GX_TG_MTX2x4);
        gen.source = static_cast<mh_u32>(GX_TG_TEX0) + static_cast<mh_u32>(coord);
        gen.matrix = static_cast<mh_u32>(GX_IDENTITY);
        gen.normalize = false;
        gen.post_matrix = static_cast<mh_u32>(GX_PTIDENTITY);
    }
    /* GXInit leaves every stage direct, no indirect stage and no texture
     * offsets. */
    indirect_state = MeleeHostGxIndirectState{};
    texture_offsets.fill(TextureOffsets{});

    fog_state = MeleeHostGxFogState{};
    fog_state.type = static_cast<mh_u32>(GX_FOG_NONE);
    for (mh_u16& entry : fog_state.range_adjust_table) {
        entry = 256;
    }

    copy_state = MeleeHostGxCopyState{};
    display_copy_state = MeleeHostGxDisplayCopyState{};
    display_copy_state.vertical_scale = 1.0F;
    display_copy_state.gamma = static_cast<mh_u32>(GX_GM_1_0);
    display_copy_state.clamp = static_cast<mh_u32>(GX_CLAMP_TOP |
                                                   GX_CLAMP_BOTTOM);
    draw_sync_state = MeleeHostGxDrawSyncState{};
    /* GXInit leaves both pairs unlit, ambient from a black register and
     * material from the vertex, with a white register behind it. */
    MeleeHostGxChannelControl channel_default{};
    channel_default.ambient_source = static_cast<mh_u32>(GX_SRC_REG);
    channel_default.material_source = static_cast<mh_u32>(GX_SRC_VTX);
    channel_default.diffuse_function = static_cast<mh_u32>(GX_DF_NONE);
    channel_default.attenuation_function = static_cast<mh_u32>(GX_AF_NONE);
    for (mh_u8& component : channel_default.material_color) {
        component = 255;
    }
    channel_controls.fill(channel_default);
    bound_textures.fill(MeleeHostGxTextureDesc{});
    loaded_tluts.fill(MeleeHostGxTlutDesc{});
    lights.fill(MeleeHostGxLightDesc{});
    for (auto& row : matrix_memory) {
        row = { 0.0F, 0.0F, 0.0F, 0.0F };
    }
    matrix_loaded.fill(false);
    for (auto& row : normal_matrix_memory) {
        row = { 0.0F, 0.0F, 0.0F };
    }
}

bool state_initialized = false;

void ensure_initialized_locked()
{
    if (!state_initialized) {
        reset_locked();
        state_initialized = true;
    }
}

float dot3(const mh_f32 left[3], const mh_f32 right[3])
{
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

void normalize3(mh_f32 vector[3])
{
    const float length = std::sqrt(dot3(vector, vector));
    if (length > 0.0F) {
        vector[0] /= length;
        vector[1] /= length;
        vector[2] /= length;
    }
}

/* A lit value rounded to a channel.  The hardware lights in fixed point and has
 * no NaN, but a light or normal the game leaves degenerate can make the float
 * evaluation one (the results screen does), and converting NaN to an integer is
 * undefined: it stores 0. */
mh_u8 to_channel(float value)
{
    if (!(value > 0.0F)) {
        return 0;
    }
    return static_cast<mh_u8>(std::min(value, 255.0F) + 0.5F);
}

void evaluate_channel_locked(const MeleeHostGxCapturedVertex& vertex,
                             std::size_t color_channel,
                             std::size_t alpha_channel, mh_u8 out[4])
{
    constexpr mh_u32 kSourceVertex = static_cast<mh_u32>(GX_SRC_VTX);
    constexpr mh_u32 kDiffuseNone = static_cast<mh_u32>(GX_DF_NONE);
    constexpr mh_u32 kDiffuseClamp = static_cast<mh_u32>(GX_DF_CLAMP);
    constexpr mh_u32 kAttenuationSpot = static_cast<mh_u32>(GX_AF_SPOT);
    constexpr mh_u32 kAttenuationNone = static_cast<mh_u32>(GX_AF_NONE);
    const auto& color_control = channel_controls[color_channel];
    const auto& alpha_control = channel_controls[alpha_channel];
    const auto source = [&vertex](mh_u32 selection, const mh_u8 registered[4]) {
        return selection == kSourceVertex &&
                       (vertex.attributes & MELEE_HOST_GX_VERTEX_COLOR) != 0
                   ? vertex.color
                   : registered;
    };
    const mh_u8* const ambient = source(color_control.ambient_source,
                                        color_control.ambient_color);
    const mh_u8* const material = source(color_control.material_source,
                                         color_control.material_color);
    const mh_u8* const alpha_ambient = source(alpha_control.ambient_source,
                                              alpha_control.ambient_color);
    const mh_u8* const alpha_material = source(alpha_control.material_source,
                                               alpha_control.material_color);
    /* GX lights a channel only when GXSetChanCtrl enables it: with lighting
     * off the channel passes its material colour through, and neither the
     * ambient register nor any light takes part.  Colour and alpha decide
     * this separately. */
    const bool color_lit = color_control.lighting_enabled;
    const bool alpha_lit = alpha_control.lighting_enabled;
    float lit[4]{ color_lit ? static_cast<float>(ambient[0]) : 255.0F,
                  color_lit ? static_cast<float>(ambient[1]) : 255.0F,
                  color_lit ? static_cast<float>(ambient[2]) : 255.0F,
                  alpha_lit ? static_cast<float>(alpha_ambient[3]) : 255.0F };

    if (color_lit || alpha_lit) {
        mh_f32 normal[3]{ vertex.normal.x, vertex.normal.y, vertex.normal.z };
        normalize3(normal);
        for (std::size_t index = 0; index < kLights; ++index) {
            const auto& light = lights[index];
            const mh_u32 mask = 1U << index;
            if (!light.loaded ||
                ((color_control.light_mask & mask) == 0U &&
                 (alpha_control.light_mask & mask) == 0U)) {
                continue;
            }
            mh_f32 direction[]{ light.position[0] - vertex.position.x,
                                 light.position[1] - vertex.position.y,
                                 light.position[2] - vertex.position.z };
            const float distance = std::sqrt(dot3(direction, direction));
            normalize3(direction);
            float diffuse = dot3(normal, direction);
            if (color_control.diffuse_function == kDiffuseNone) {
                diffuse = 1.0F;
            } else if (color_control.diffuse_function == kDiffuseClamp) {
                diffuse = std::max(diffuse, 0.0F);
            }
            float attenuation = 1.0F;
            if (color_control.attenuation_function != kAttenuationNone) {
                const float denominator = light.distance_attenuation[0] +
                    distance * (light.distance_attenuation[1] +
                                distance * light.distance_attenuation[2]);
                attenuation = denominator > 0.0F ? 1.0F / denominator : 0.0F;
                if (color_control.attenuation_function == kAttenuationSpot) {
                    /* GXInitLightDir stores the negated authoring direction,
                     * which is already the vector from the shaded point to
                     * the light used by the hardware. */
                    const float cosine = dot3(light.direction, direction);
                    const float spot = light.angle_attenuation[0] +
                        cosine * (light.angle_attenuation[1] +
                                  cosine * light.angle_attenuation[2]);
                    attenuation *= std::max(spot, 0.0F);
                }
            }
            const float factor = diffuse * attenuation;
            for (std::size_t component = 0; component < 4; ++component) {
                if (component < 3 ? !color_lit : !alpha_lit) {
                    continue;
                }
                lit[component] +=
                    factor * static_cast<float>(light.color[component]);
            }
        }
    }
    for (std::size_t component = 0; component < 3; ++component) {
        const float value = static_cast<float>(material[component]) *
                            lit[component] / 255.0F;
        out[component] = to_channel(value);
    }
    const float alpha = static_cast<float>(alpha_material[3]) * lit[3] / 255.0F;
    out[3] = to_channel(alpha);
}

const void* combine_pointer(std::uint32_t low, std::uint32_t high)
{
    const std::uintptr_t value = (static_cast<std::uintptr_t>(high) << 32) |
                                 static_cast<std::uintptr_t>(low);
    return reinterpret_cast<const void*>(value);
}

void split_pointer(const void* pointer, std::uint32_t& low,
                   std::uint32_t& high)
{
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    low = static_cast<std::uint32_t>(value & 0xFFFFFFFFU);
    high = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(value) >> 32) & 0xFFFFFFFFU);
}

std::uint8_t to_lod_q4(float lod)
{
    const float clamped = std::clamp(lod, 0.0F, 15.9375F);
    return static_cast<std::uint8_t>(clamped * 16.0F + 0.5F);
}

float from_lod_q4(std::uint8_t value)
{
    return static_cast<float>(value) / 16.0F;
}

/* The payload is moved through std::memcpy rather than a reinterpret_cast so
 * the accesses stay inside the object's own bytes without relying on type
 * punning.  Both directions compile down to the same loads and stores. */
template <typename Packed, typename Object> Packed load_pack(const Object* o)
{
    static_assert(sizeof(Packed) == sizeof(Object),
                  "payload and SDK object must be the same size");
    Packed packed;
    std::memcpy(&packed, o, sizeof(packed));
    return packed;
}

template <typename Packed, typename Object>
void store_pack(Object* o, const Packed& packed)
{
    static_assert(sizeof(Packed) == sizeof(Object),
                  "payload and SDK object must be the same size");
    std::memcpy(o, &packed, sizeof(packed));
}

/* Position and normal matrices occupy three consecutive rows of matrix
 * memory; texture matrices may occupy two.  Writing past the end would
 * silently corrupt the next matrix, so out-of-range ids are dropped. */
void store_matrix_rows(std::uint32_t id, MtxPtr matrix, std::size_t rows)
{
    if (id + rows > kMatrixRows) {
        return;
    }
    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            matrix_memory[id + row][column] = matrix[row][column];
        }
        matrix_loaded[id + row] = true;
    }
}

/* A normal matrix keeps its 3x3 part in normal matrix memory, three rows per
 * id like the position matrix it accompanies, and leaves position rows alone. */
void store_normal_matrix(std::uint32_t id, MtxPtr matrix)
{
    if (id + 3 > kMatrixRows) {
        return;
    }
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            normal_matrix_memory[id + row][column] = matrix[row][column];
        }
    }
}

/* Resolves a GXChannelID to the hardware channels it writes.  GX_COLOR0A0 and
 * GX_COLOR1A1 each name a colour/alpha pair rather than a channel, and an id
 * outside the four real channels resolves to nothing. */
std::array<std::size_t, 2> channels_of(GXChannelID chan)
{
    switch (chan) {
    case GX_COLOR0:
        return { 0, kChannels };
    case GX_COLOR1:
        return { 1, kChannels };
    case GX_ALPHA0:
        return { 2, kChannels };
    case GX_ALPHA1:
        return { 3, kChannels };
    case GX_COLOR0A0:
        return { 0, 2 };
    case GX_COLOR1A1:
        return { 1, 3 };
    default:
        return { kChannels, kChannels };
    }
}

} // namespace

extern "C" {

void melee_host_gx_state_reset(void)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    reset_locked();
    state_initialized = true;
}

void GXSetZMode(GXBool compare_enable, GXCompare func, GXBool update_enable)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.z_compare_enable = compare_enable != GX_FALSE;
    pixel_state.z_func = static_cast<mh_u32>(func);
    pixel_state.z_update_enable = update_enable != GX_FALSE;
}

void GXSetZCompLoc(GXBool before_tex)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.z_before_texture = before_tex != GX_FALSE;
}

void GXSetZTexture(GXZTexOp op, GXTexFmt fmt, u32 bias)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.z_texture_op = static_cast<mh_u32>(op);
    pixel_state.z_texture_format = static_cast<mh_u32>(fmt);
    pixel_state.z_texture_bias = bias;
}

void GXSetBlendMode(GXBlendMode type, GXBlendFactor src_factor,
                    GXBlendFactor dst_factor, GXLogicOp op)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.blend_mode = static_cast<mh_u32>(type);
    pixel_state.blend_src_factor = static_cast<mh_u32>(src_factor);
    pixel_state.blend_dst_factor = static_cast<mh_u32>(dst_factor);
    pixel_state.blend_logic_op = static_cast<mh_u32>(op);
}

void GXSetColorUpdate(GXBool update_enable)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.color_update_enable = update_enable != GX_FALSE;
}

void GXSetAlphaUpdate(GXBool update_enable)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.alpha_update_enable = update_enable != GX_FALSE;
}

void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op,
                       GXCompare comp1, u8 ref1)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.alpha_compare_0 = static_cast<mh_u32>(comp0);
    pixel_state.alpha_ref_0 = ref0;
    pixel_state.alpha_op = static_cast<mh_u32>(op);
    pixel_state.alpha_compare_1 = static_cast<mh_u32>(comp1);
    pixel_state.alpha_ref_1 = ref1;
}

void GXSetCullMode(GXCullMode mode)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.cull_mode = static_cast<mh_u32>(mode);
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.scissor_left = left;
    pixel_state.scissor_top = top;
    pixel_state.scissor_width = wd;
    pixel_state.scissor_height = ht;
}

void GXPixModeSync(void)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.pixel_sync_count += 1;
}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz,
                         f32 farz, u32 field)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    transform_state.viewport_left = left;
    transform_state.viewport_top = top;
    transform_state.viewport_width = wd;
    transform_state.viewport_height = ht;
    transform_state.viewport_near = nearz;
    transform_state.viewport_far = farz;
    transform_state.viewport_field = field;
}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz)
{
    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1);
}

void GXGetViewportv(f32* vp)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    vp[0] = transform_state.viewport_left;
    vp[1] = transform_state.viewport_top;
    vp[2] = transform_state.viewport_width;
    vp[3] = transform_state.viewport_height;
    vp[4] = transform_state.viewport_near;
    vp[5] = transform_state.viewport_far;
}

/* GX stores a projection as the type tag followed by the five values that
 * vary between the perspective and orthographic forms; GXGetProjectionv
 * reports the same six-float record back. */
void GXSetProjection(f32 mtx[4][4], GXProjectionType type)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    transform_state.projection_type = static_cast<mh_u32>(type);
    transform_state.projection[0] = mtx[0][0];
    transform_state.projection[2] = mtx[1][1];
    transform_state.projection[4] = mtx[2][2];
    transform_state.projection[5] = mtx[2][3];
    if (type == GX_PERSPECTIVE) {
        transform_state.projection[1] = mtx[0][2];
        transform_state.projection[3] = mtx[1][2];
    } else {
        transform_state.projection[1] = mtx[0][3];
        transform_state.projection[3] = mtx[1][3];
    }
}

/* A point through a model-view matrix, a projection in GXGetProjectionv's
 * six-float form and a GXGetViewportv viewport, to window coordinates.  It
 * reads no GX state; the arithmetic keeps the SDK's order of operations, so
 * the result is the same float for float. */
void GXProject(f32 x, f32 y, f32 z, f32 mtx[3][4], f32* pm, f32* vp, f32* sx,
               f32* sy, f32* sz)
{
    const f32 eye_x =
        mtx[0][3] + ((mtx[0][2] * z) + ((mtx[0][0] * x) + (mtx[0][1] * y)));
    const f32 eye_y =
        mtx[1][3] + ((mtx[1][2] * z) + ((mtx[1][0] * x) + (mtx[1][1] * y)));
    const f32 eye_z =
        mtx[2][3] + ((mtx[2][2] * z) + ((mtx[2][0] * x) + (mtx[2][1] * y)));
    f32 clip_x;
    f32 clip_y;
    f32 clip_z;
    f32 inverse_w;

    /* The type tag is GX_PERSPECTIVE, which is 0, or GX_ORTHOGRAPHIC. */
    if (std::fpclassify(pm[0]) == FP_ZERO) {
        clip_x = (eye_x * pm[1]) + (eye_z * pm[2]);
        clip_y = (eye_y * pm[3]) + (eye_z * pm[4]);
        clip_z = pm[6] + (eye_z * pm[5]);
        inverse_w = 1.0F / -eye_z;
    } else {
        clip_x = pm[2] + (eye_x * pm[1]);
        clip_y = pm[4] + (eye_y * pm[3]);
        clip_z = pm[6] + (eye_z * pm[5]);
        inverse_w = 1.0F;
    }
    *sx = (vp[2] / 2.0F) + (vp[0] + (inverse_w * (clip_x * vp[2] / 2.0F)));
    *sy = (vp[3] / 2.0F) + (vp[1] + (inverse_w * (-clip_y * vp[3] / 2.0F)));
    *sz = vp[5] + (inverse_w * (clip_z * (vp[5] - vp[4])));
}

/* The SDK only asserts that the hardware has no TEV clamp mode.  The game
 * still calls it, with (0, 0), before drawing collision and debug lines. */
void GXSetTevClampMode(int stage, int mode)
{
    (void) stage;
    (void) mode;
}

void GXGetProjectionv(f32* ptr)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    ptr[0] = static_cast<f32>(transform_state.projection_type);
    for (std::size_t i = 0; i < 6; ++i) {
        ptr[i + 1] = transform_state.projection[i];
    }
}

void GXSetCurrentMtx(u32 id)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    transform_state.current_matrix = id;
}

void GXLoadPosMtxImm(f32 mtx[3][4], u32 id)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    store_matrix_rows(id, mtx, 3);
}

void GXLoadNrmMtxImm(f32 mtx[3][4], u32 id)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    store_normal_matrix(id, mtx);
}

void GXLoadTexMtxImm(f32 mtx[][4], u32 id, GXTexMtxType type)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    store_matrix_rows(id, mtx, type == GX_MTX2x4 ? 2U : 3U);
}

void GXSetNumTevStages(u8 nStages)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    tev_state.stage_count = nStages;
}

void GXSetNumTexGens(u8 nTexGens)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    tev_state.texcoord_gen_count = nTexGens;
}

void GXSetNumChans(u8 nChans)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    tev_state.channel_count = nChans;
}

void GXSetTevOp(GXTevStageID id, GXTevMode mode)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(id);
    if (stage >= kTevStages) {
        return;
    }
    apply_tev_preset_locked(stage, mode);
}

void GXSetTevOrder(GXTevStageID stage_id, GXTexCoordID coord, GXTexMapID map,
                   GXChannelID color)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(stage_id);
    if (stage >= kTevStages) {
        return;
    }
    tev_state.stages[stage].texcoord = static_cast<mh_u32>(coord);
    tev_state.stages[stage].texmap = static_cast<mh_u32>(map);
    tev_state.stages[stage].color_channel = static_cast<mh_u32>(color);
}

void GXSetNumIndStages(u8 nIndStages)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    indirect_state.stage_count = nIndStages;
}

void GXSetIndTexOrder(GXIndTexStageID ind_stage, GXTexCoordID tex_coord,
                      GXTexMapID tex_map)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(ind_stage);
    if (stage >= kIndirectStages) {
        return;
    }
    indirect_state.stages[stage].texcoord = static_cast<mh_u32>(tex_coord);
    indirect_state.stages[stage].texmap = static_cast<mh_u32>(tex_map);
}

void GXSetIndTexCoordScale(GXIndTexStageID ind_state, GXIndTexScale scale_s,
                           GXIndTexScale scale_t)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(ind_state);
    if (stage >= kIndirectStages) {
        return;
    }
    indirect_state.stages[stage].scale_s = static_cast<mh_u32>(scale_s);
    indirect_state.stages[stage].scale_t = static_cast<mh_u32>(scale_t);
}

/* GX_ITM_0 to GX_ITM_2 name the three loadable matrices; GX_ITM_OFF and the
 * texture-coordinate matrices (GX_ITM_S*, GX_ITM_T*) are not loaded here. */
void GXSetIndTexMtx(GXIndTexMtxID mtx_id, f32 offset[2][3], s8 scale_exp)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto id = static_cast<std::size_t>(mtx_id);
    const auto first = static_cast<std::size_t>(GX_ITM_0);
    if (id < first || id >= first + kIndirectMatrices) {
        return;
    }
    MeleeHostGxIndirectMatrix& matrix = indirect_state.matrices[id - first];
    for (std::size_t row = 0; row < 2; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            matrix.offset[row][column] = offset[row][column];
        }
    }
    matrix.scale_exp = scale_exp;
}

void GXSetTevIndirect(GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                      GXIndTexFormat format, GXIndTexBiasSel bias_sel,
                      GXIndTexMtxID matrix_sel, GXIndTexWrap wrap_s,
                      GXIndTexWrap wrap_t, GXBool add_prev, GXBool utc_lod,
                      GXIndTexAlphaSel alpha_sel)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(tev_stage);
    if (stage >= kTevStages) {
        return;
    }
    MeleeHostGxIndirectTevStage& slot = indirect_state.tev_stages[stage];
    slot.indirect = true;
    slot.ind_stage = static_cast<mh_u32>(ind_stage);
    slot.format = static_cast<mh_u32>(format);
    slot.bias = static_cast<mh_u32>(bias_sel);
    slot.matrix = static_cast<mh_u32>(matrix_sel);
    slot.wrap_s = static_cast<mh_u32>(wrap_s);
    slot.wrap_t = static_cast<mh_u32>(wrap_t);
    slot.add_previous = add_prev != GX_FALSE;
    slot.unmodified_lod = utc_lod != GX_FALSE;
    slot.alpha_select = static_cast<mh_u32>(alpha_sel);
}

/* The SDK writes the all-zero indirect command, which is what a direct stage
 * is. */
void GXSetTevDirect(GXTevStageID tev_stage)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(tev_stage);
    if (stage >= kTevStages) {
        return;
    }
    indirect_state.tev_stages[stage] = MeleeHostGxIndirectTevStage{};
}

void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func,
                       GXTexGenSrc src_param, u32 mtx, GXBool normalize,
                       u32 pt_texmtx)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto coord = static_cast<std::size_t>(dst_coord);
    if (coord >= kTexCoords) {
        return;
    }
    tev_state.texcoord_gens[coord].function = static_cast<mh_u32>(func);
    tev_state.texcoord_gens[coord].source = static_cast<mh_u32>(src_param);
    tev_state.texcoord_gens[coord].matrix = mtx;
    tev_state.texcoord_gens[coord].normalize = normalize != GX_FALSE;
    tev_state.texcoord_gens[coord].post_matrix = pt_texmtx;
}

void GXEnableTexOffsets(GXTexCoordID coord, u8 line_enable, u8 point_enable)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto index = static_cast<std::size_t>(coord);
    if (index >= kTexCoords) {
        return;
    }
    texture_offsets[index].lines = line_enable != 0;
    texture_offsets[index].points = point_enable != 0;
}

/* The hardware has four lighting channels: COLOR0, COLOR1, ALPHA0 and ALPHA1.
 * GX_COLOR0A0 and GX_COLOR1A1 are not channels of their own; each configures
 * both halves of a colour/alpha pair in one call. */
void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src,
                   GXColorSrc mat_src, u32 light_mask, GXDiffuseFn diff_fn,
                   GXAttnFn attn_fn)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();

    for (const std::size_t channel : channels_of(chan)) {
        if (channel >= kChannels) {
            continue;
        }
        channel_controls[channel].lighting_enabled = enable != GX_FALSE;
        channel_controls[channel].ambient_source =
            static_cast<mh_u32>(amb_src);
        channel_controls[channel].material_source =
            static_cast<mh_u32>(mat_src);
        channel_controls[channel].light_mask = light_mask;
        channel_controls[channel].diffuse_function =
            static_cast<mh_u32>(diff_fn);
        channel_controls[channel].attenuation_function =
            static_cast<mh_u32>(attn_fn);
    }
}

void GXInitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height,
                  GXTexFmt format, GXTexWrapMode wrap_s,
                  GXTexWrapMode wrap_t, u8 mipmap)
{
    PackedTexObj packed{};
    split_pointer(image_ptr, packed.image_low, packed.image_high);
    packed.width = width;
    packed.height = height;
    packed.format = static_cast<std::uint8_t>(format);
    packed.wrap_s = static_cast<std::uint8_t>(wrap_s);
    packed.wrap_t = static_cast<std::uint8_t>(wrap_t);
    packed.mipmap = mipmap;
    packed.tlut_name = static_cast<std::uint32_t>(GX_TLUT0);
    packed.min_filter = static_cast<std::uint8_t>(
        mipmap != 0 ? GX_LIN_MIP_LIN : GX_LINEAR);
    packed.mag_filter = static_cast<std::uint8_t>(GX_LINEAR);
    packed.max_anisotropy = static_cast<std::uint8_t>(GX_ANISO_1);
    packed.min_lod_q4 = 0;
    packed.max_lod_q4 = to_lod_q4(mipmap != 0 ? 10.0F : 0.0F);
    packed.lod_bias = 0.0F;
    packed.color_indexed = 0;
    store_pack(obj, packed);
}

void GXInitTexObjCI(GXTexObj* obj, void* image_ptr, u16 width, u16 height,
                    GXTexFmt format, GXTexWrapMode wrap_s,
                    GXTexWrapMode wrap_t, u8 mipmap, u32 tlut_name)
{
    GXInitTexObj(obj, image_ptr, width, height, format, wrap_s, wrap_t,
                 mipmap);
    PackedTexObj packed = load_pack<PackedTexObj>(obj);
    packed.tlut_name = tlut_name;
    packed.color_indexed = 1;
    store_pack(obj, packed);
}

void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter min_filt, GXTexFilter mag_filt,
                     f32 min_lod, f32 max_lod, f32 lod_bias,
                     GXBool bias_clamp, GXBool do_edge_lod,
                     GXAnisotropy max_aniso)
{
    PackedTexObj packed = load_pack<PackedTexObj>(obj);
    packed.min_filter = static_cast<std::uint8_t>(min_filt);
    packed.mag_filter = static_cast<std::uint8_t>(mag_filt);
    packed.min_lod_q4 = to_lod_q4(min_lod);
    packed.max_lod_q4 = to_lod_q4(max_lod);
    packed.lod_bias = lod_bias;
    packed.bias_clamp = static_cast<std::uint8_t>(bias_clamp);
    packed.edge_lod = static_cast<std::uint8_t>(do_edge_lod);
    packed.max_anisotropy = static_cast<std::uint8_t>(max_aniso);
    store_pack(obj, packed);
}

GXTexFmt GXGetTexObjFmt(const GXTexObj* obj)
{
    return static_cast<GXTexFmt>(load_pack<PackedTexObj>(obj).format);
}

u16 GXGetTexObjWidth(const GXTexObj* obj)
{
    return load_pack<PackedTexObj>(obj).width;
}

u16 GXGetTexObjHeight(const GXTexObj* obj)
{
    return load_pack<PackedTexObj>(obj).height;
}

void GXLoadTexObj(GXTexObj* obj, GXTexMapID id)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto texmap = static_cast<std::size_t>(id);
    if (texmap >= kTexMaps) {
        return;
    }
    const PackedTexObj packed = load_pack<PackedTexObj>(obj);
    MeleeHostGxTextureDesc& desc = bound_textures[texmap];
    desc.bound = true;
    desc.color_indexed = packed.color_indexed != 0;
    desc.width = packed.width;
    desc.height = packed.height;
    desc.format = packed.format;
    desc.wrap_s = packed.wrap_s;
    desc.wrap_t = packed.wrap_t;
    desc.mipmap = packed.mipmap != 0;
    desc.tlut_name = packed.tlut_name;
    desc.min_filter = packed.min_filter;
    desc.mag_filter = packed.mag_filter;
    desc.min_lod = from_lod_q4(packed.min_lod_q4);
    desc.max_lod = from_lod_q4(packed.max_lod_q4);
    desc.lod_bias = packed.lod_bias;
    desc.max_anisotropy = packed.max_anisotropy;
    desc.bias_clamp = packed.bias_clamp != 0;
    desc.edge_lod = packed.edge_lod != 0;
    desc.image = combine_pointer(packed.image_low, packed.image_high);
}

void GXInitTlutObj(GXTlutObj* tlut_obj, void* lut, GXTlutFmt fmt,
                   u16 n_entries)
{
    PackedTlutObj packed{};
    split_pointer(lut, packed.entries_low, packed.entries_high);
    packed.format = static_cast<std::uint8_t>(fmt);
    packed.entry_count = n_entries;
    store_pack(tlut_obj, packed);
}

void GXLoadTlut(GXTlutObj* tlut_obj, u32 tlut_name)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    if (tlut_name >= kTluts) {
        return;
    }
    const PackedTlutObj packed = load_pack<PackedTlutObj>(tlut_obj);
    MeleeHostGxTlutDesc& desc = loaded_tluts[tlut_name];
    desc.loaded = true;
    desc.format = packed.format;
    desc.entry_count = packed.entry_count;
    desc.entries = combine_pointer(packed.entries_low, packed.entries_high);
}

void GXInvalidateTexAll(void)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    copy_state.texture_invalidate_count += 1;
}

void GXInitLightColor(GXLightObj* lt_obj, GXColor color)
{
    PackedLightObj packed = load_pack<PackedLightObj>(lt_obj);
    packed.color[0] = color.r;
    packed.color[1] = color.g;
    packed.color[2] = color.b;
    packed.color[3] = color.a;
    store_pack(lt_obj, packed);
}

void GXInitLightPos(GXLightObj* lt_obj, f32 x, f32 y, f32 z)
{
    PackedLightObj packed = load_pack<PackedLightObj>(lt_obj);
    packed.position[0] = x;
    packed.position[1] = y;
    packed.position[2] = z;
    store_pack(lt_obj, packed);
}

void GXInitLightDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz)
{
    PackedLightObj packed = load_pack<PackedLightObj>(lt_obj);
    packed.direction[0] = -nx;
    packed.direction[1] = -ny;
    packed.direction[2] = -nz;
    store_pack(lt_obj, packed);
}

void GXInitLightAttn(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2, f32 k0,
                     f32 k1, f32 k2)
{
    PackedLightObj packed = load_pack<PackedLightObj>(lt_obj);
    packed.angle_attenuation[0] = a0;
    packed.angle_attenuation[1] = a1;
    packed.angle_attenuation[2] = a2;
    packed.distance_attenuation[0] = k0;
    packed.distance_attenuation[1] = k1;
    packed.distance_attenuation[2] = k2;
    store_pack(lt_obj, packed);
}

/* Spot and distance attenuation derive the same coefficient triples the
 * explicit GXInitLightAttn call writes, using the SDK's documented curves. */
void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, GXSpotFn spot_func)
{
    PackedLightObj packed = load_pack<PackedLightObj>(lt_obj);
    float a0 = 1.0F;
    float a1 = 0.0F;
    float a2 = 0.0F;

    if (cutoff <= 0.0F || cutoff > 90.0F) {
        spot_func = GX_SP_OFF;
    }

    const float radians = cutoff * 3.1415927F / 180.0F;
    const float cosine = std::cos(radians);
    const float denominator = cosine - 1.0F;

    switch (spot_func) {
    case GX_SP_FLAT:
        a0 = -1000.0F * cosine;
        a1 = 1000.0F;
        a2 = 0.0F;
        break;
    case GX_SP_COS:
        a0 = -cosine / denominator;
        a1 = 1.0F / denominator;
        a2 = 0.0F;
        break;
    case GX_SP_COS2:
        a0 = 0.0F;
        a1 = -cosine / (denominator * denominator);
        a2 = 1.0F / (denominator * denominator);
        break;
    case GX_SP_SHARP: {
        const float d = denominator * denominator;
        a0 = (cosine * (cosine - 2.0F)) / d;
        a1 = 2.0F / d;
        a2 = -1.0F / d;
        break;
    }
    case GX_SP_RING1: {
        const float d = denominator * denominator;
        a0 = (-4.0F * cosine) / d;
        a1 = (4.0F * (1.0F + cosine)) / d;
        a2 = -4.0F / d;
        break;
    }
    case GX_SP_RING2: {
        const float d = denominator * denominator;
        a0 = 1.0F - ((2.0F * cosine * cosine) / d);
        a1 = (4.0F * cosine * (1.0F + cosine)) / d;
        a2 = -2.0F * (1.0F + (2.0F * cosine) + (cosine * cosine)) / d;
        break;
    }
    case GX_SP_OFF:
    default:
        a0 = 1.0F;
        a1 = 0.0F;
        a2 = 0.0F;
        break;
    }

    packed.angle_attenuation[0] = a0;
    packed.angle_attenuation[1] = a1;
    packed.angle_attenuation[2] = a2;
    store_pack(lt_obj, packed);
}

void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_dist, f32 ref_br,
                         GXDistAttnFn dist_func)
{
    PackedLightObj packed = load_pack<PackedLightObj>(lt_obj);
    float k0 = 1.0F;
    float k1 = 0.0F;
    float k2 = 0.0F;

    if (ref_dist < 0.0F || ref_br <= 0.0F || ref_br >= 1.0F) {
        dist_func = GX_DA_OFF;
    }

    switch (dist_func) {
    case GX_DA_GENTLE:
        k1 = (1.0F - ref_br) / (ref_br * ref_dist);
        break;
    case GX_DA_MEDIUM:
        k1 = 0.5F * (1.0F - ref_br) / (ref_br * ref_dist);
        k2 = 0.5F * (1.0F - ref_br) / (ref_br * ref_dist * ref_dist);
        break;
    case GX_DA_STEEP:
        k2 = (1.0F - ref_br) / (ref_br * ref_dist * ref_dist);
        break;
    case GX_DA_OFF:
    default:
        break;
    }

    packed.distance_attenuation[0] = k0;
    packed.distance_attenuation[1] = k1;
    packed.distance_attenuation[2] = k2;
    store_pack(lt_obj, packed);
}

void GXLoadLightObjImm(GXLightObj* lt_obj, GXLightID light)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    /* GXLightID is a one-hot mask: GX_LIGHT0 is bit 0. */
    std::size_t index = 0;
    auto mask = static_cast<std::uint32_t>(light);
    while (mask > 1U && index < kLights) {
        mask >>= 1U;
        ++index;
    }
    if (index >= kLights || mask != 1U) {
        return;
    }

    const PackedLightObj packed = load_pack<PackedLightObj>(lt_obj);
    MeleeHostGxLightDesc& desc = lights[index];
    desc.loaded = true;
    for (std::size_t i = 0; i < 4; ++i) {
        desc.color[i] = packed.color[i];
    }
    for (std::size_t i = 0; i < 3; ++i) {
        desc.position[i] = packed.position[i];
        desc.direction[i] = packed.direction[i];
        desc.angle_attenuation[i] = packed.angle_attenuation[i];
        desc.distance_attenuation[i] = packed.distance_attenuation[i];
    }
}

void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz,
              GXColor color)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    fog_state.type = static_cast<mh_u32>(type);
    fog_state.start_z = startz;
    fog_state.end_z = endz;
    fog_state.near_z = nearz;
    fog_state.far_z = farz;
    fog_state.color[0] = color.r;
    fog_state.color[1] = color.g;
    fog_state.color[2] = color.b;
    fog_state.color[3] = color.a;
}

/* The SDK derives ten range-adjustment factors from the projection.  That
 * derivation is not modelled, so the host writes the neutral table (1.0 in
 * 8.8 fixed point) and reports it as unmodelled through the fog state. */
void GXInitFogAdjTable(GXFogAdjTable* table, u16 width, f32 projmtx[4][4])
{
    (void) width;
    (void) projmtx;
    for (u16& entry : table->r) {
        entry = 256;
    }
}

void GXSetFogRangeAdj(GXBool enable, u16 center, GXFogAdjTable* table)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    fog_state.range_adjust_enabled = enable != GX_FALSE;
    fog_state.range_adjust_center = center;
    fog_state.range_adjust_modelled = false;
    if (table != nullptr) {
        for (std::size_t i = 0; i < MELEE_HOST_GX_FOG_ADJ_ENTRIES; ++i) {
            fog_state.range_adjust_table[i] = table->r[i];
        }
    }
}

void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    copy_state.source_left = left;
    copy_state.source_top = top;
    copy_state.source_width = wd;
    copy_state.source_height = ht;
}

void GXSetTexCopyDst(u16 wd, u16 ht, GXTexFmt fmt, GXBool mipmap)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    copy_state.destination_width = wd;
    copy_state.destination_height = ht;
    copy_state.destination_format = static_cast<mh_u32>(fmt);
    copy_state.destination_mipmap = mipmap != GX_FALSE;
}

void GXCopyTex(void* dest, GXBool clear)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    copy_state.copy_count += 1;
    copy_state.last_destination = dest;
    copy_state.last_clear = clear != GX_FALSE;
    melee_host_gx_note_texture_copy(dest);
    /* HSD shadows are I4 EFB copies.  Resolve that compact, untextured pass
     * now so the texture sampled by the later stage draw is defined. */
    if (copy_state.destination_format == GX_CTF_R4) {
        static_cast<void>(melee_host_gx_copy_efb_to_i4(
            dest, copy_state.source_left, copy_state.source_top,
            copy_state.source_width, copy_state.source_height,
            copy_state.destination_width, copy_state.destination_height));
    } else {
        /* The results' portraits and the magnifier's close-up are colour
         * copies of a fighter drawn over an erased rectangle. */
        static_cast<void>(melee_host_gx_copy_efb_to_texture(
            dest, copy_state.destination_format, copy_state.source_left,
            copy_state.source_top, copy_state.source_width,
            copy_state.source_height, copy_state.destination_width,
            copy_state.destination_height, display_copy_state.clear_color,
            display_copy_state.clear_depth));
    }
    if (clear != GX_FALSE) {
        melee_host_gx_note_efb_clear(
            copy_state.source_left, copy_state.source_top,
            copy_state.source_width, copy_state.source_height,
            display_copy_state.clear_color, display_copy_state.clear_depth);
    }
}

/* GXSetTevOp is a shorthand the SDK expands into the four calls below.  A
 * stage configured through those calls no longer has a single mode, so the
 * recorded mode becomes the custom sentinel and readers must look at the
 * individual inputs and operations. */
void GXSetTevColorIn(GXTevStageID stage_id, GXTevColorArg a, GXTevColorArg b,
                     GXTevColorArg c, GXTevColorArg d)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(stage_id);
    if (stage >= kTevStages) {
        return;
    }
    MeleeHostGxTevStage& slot = tev_state.stages[stage];
    slot.color_input[0] = static_cast<mh_u32>(a);
    slot.color_input[1] = static_cast<mh_u32>(b);
    slot.color_input[2] = static_cast<mh_u32>(c);
    slot.color_input[3] = static_cast<mh_u32>(d);
    slot.mode = MELEE_HOST_GX_TEV_MODE_CUSTOM;
}

void GXSetTevAlphaIn(GXTevStageID stage_id, GXTevAlphaArg a, GXTevAlphaArg b,
                     GXTevAlphaArg c, GXTevAlphaArg d)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(stage_id);
    if (stage >= kTevStages) {
        return;
    }
    MeleeHostGxTevStage& slot = tev_state.stages[stage];
    slot.alpha_input[0] = static_cast<mh_u32>(a);
    slot.alpha_input[1] = static_cast<mh_u32>(b);
    slot.alpha_input[2] = static_cast<mh_u32>(c);
    slot.alpha_input[3] = static_cast<mh_u32>(d);
    slot.mode = MELEE_HOST_GX_TEV_MODE_CUSTOM;
}

void GXSetTevColorOp(GXTevStageID stage_id, GXTevOp op, GXTevBias bias,
                     GXTevScale scale, GXBool clamp, GXTevRegID out_reg)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(stage_id);
    if (stage >= kTevStages) {
        return;
    }
    MeleeHostGxTevStage& slot = tev_state.stages[stage];
    slot.color_op = static_cast<mh_u32>(op);
    slot.color_bias = static_cast<mh_u32>(bias);
    slot.color_scale = static_cast<mh_u32>(scale);
    slot.color_clamp = clamp != GX_FALSE;
    slot.color_out_reg = static_cast<mh_u32>(out_reg);
    slot.mode = MELEE_HOST_GX_TEV_MODE_CUSTOM;
}

void GXSetTevAlphaOp(GXTevStageID stage_id, GXTevOp op, GXTevBias bias,
                     GXTevScale scale, GXBool clamp, GXTevRegID out_reg)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(stage_id);
    if (stage >= kTevStages) {
        return;
    }
    MeleeHostGxTevStage& slot = tev_state.stages[stage];
    slot.alpha_op = static_cast<mh_u32>(op);
    slot.alpha_bias = static_cast<mh_u32>(bias);
    slot.alpha_scale = static_cast<mh_u32>(scale);
    slot.alpha_clamp = clamp != GX_FALSE;
    slot.alpha_out_reg = static_cast<mh_u32>(out_reg);
    slot.mode = MELEE_HOST_GX_TEV_MODE_CUSTOM;
}

void GXSetTevKColorSel(GXTevStageID stage_id, GXTevKColorSel sel)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(stage_id);
    if (stage >= kTevStages) {
        return;
    }
    tev_state.stages[stage].konst_color_select = static_cast<mh_u32>(sel);
}

void GXSetTevKAlphaSel(GXTevStageID stage_id, GXTevKAlphaSel sel)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(stage_id);
    if (stage >= kTevStages) {
        return;
    }
    tev_state.stages[stage].konst_alpha_select = static_cast<mh_u32>(sel);
}

void GXSetTevSwapMode(GXTevStageID stage_id, GXTevSwapSel ras_sel,
                      GXTevSwapSel tex_sel)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto stage = static_cast<std::size_t>(stage_id);
    if (stage >= kTevStages) {
        return;
    }
    tev_state.stages[stage].raster_swap = static_cast<mh_u32>(ras_sel);
    tev_state.stages[stage].texture_swap = static_cast<mh_u32>(tex_sel);
}

/* The TEV evaluator and the shaders it generates use GXInit's tables, so a
 * different table would draw wrong without a sign.  The only call in the code
 * the host builds, the sprite library's, installs GXInit's own SWAP0; anything
 * else stops here by name. */
void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red,
                           GXTevColorChan green, GXTevColorChan blue,
                           GXTevColorChan alpha)
{
    const auto index = static_cast<std::size_t>(table);
    const std::array<mh_u32, 4> requested{
        static_cast<mh_u32>(red), static_cast<mh_u32>(green),
        static_cast<mh_u32>(blue), static_cast<mh_u32>(alpha)
    };
    if (index < kInitSwapTables.size() && requested == kInitSwapTables[index]) {
        return;
    }
    std::fprintf(stderr,
                 "GXSetTevSwapModeTable: table %zu as %u %u %u %u is not "
                 "modelled by the host's TEV\n",
                 index, requested[0], requested[1], requested[2],
                 requested[3]);
    std::abort();
}

/* The eight-bit form of a TEV register write widens into the same signed
 * ten-bit storage the S10 form uses. */
void GXSetTevColor(GXTevRegID id, GXColor color)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto reg = static_cast<std::size_t>(id);
    if (reg >= MELEE_HOST_GX_MAX_TEVREG) {
        return;
    }
    tev_state.registers[reg][0] = color.r;
    tev_state.registers[reg][1] = color.g;
    tev_state.registers[reg][2] = color.b;
    tev_state.registers[reg][3] = color.a;
}

void GXSetTevColorS10(GXTevRegID id, GXColorS10 color)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto reg = static_cast<std::size_t>(id);
    if (reg >= MELEE_HOST_GX_MAX_TEVREG) {
        return;
    }
    tev_state.registers[reg][0] = color.r;
    tev_state.registers[reg][1] = color.g;
    tev_state.registers[reg][2] = color.b;
    tev_state.registers[reg][3] = color.a;
}

void GXSetTevKColor(GXTevKColorID id, GXColor color)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    const auto konst = static_cast<std::size_t>(id);
    if (konst >= MELEE_HOST_GX_MAX_KCOLOR) {
        return;
    }
    tev_state.konst_colors[konst][0] = color.r;
    tev_state.konst_colors[konst][1] = color.g;
    tev_state.konst_colors[konst][2] = color.b;
    tev_state.konst_colors[konst][3] = color.a;
}

void GXSetChanAmbColor(GXChannelID chan, GXColor amb_color)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    for (const std::size_t channel : channels_of(chan)) {
        if (channel >= kChannels) {
            continue;
        }
        channel_controls[channel].ambient_color[0] = amb_color.r;
        channel_controls[channel].ambient_color[1] = amb_color.g;
        channel_controls[channel].ambient_color[2] = amb_color.b;
        channel_controls[channel].ambient_color[3] = amb_color.a;
    }
}

void GXSetChanMatColor(GXChannelID chan, GXColor mat_color)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    for (const std::size_t channel : channels_of(chan)) {
        if (channel >= kChannels) {
            continue;
        }
        channel_controls[channel].material_color[0] = mat_color.r;
        channel_controls[channel].material_color[1] = mat_color.g;
        channel_controls[channel].material_color[2] = mat_color.b;
        channel_controls[channel].material_color[3] = mat_color.a;
    }
}

void GXSetDither(GXBool dither)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.dither_enabled = dither != GX_FALSE;
}

void GXSetDstAlpha(GXBool enable, u8 alpha)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.destination_alpha_enabled = enable != GX_FALSE;
    pixel_state.destination_alpha = alpha;
}

void GXSetPixelFmt(GXPixelFmt pix_fmt, GXZFmt16 z_fmt)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.pixel_format = static_cast<mh_u32>(pix_fmt);
    pixel_state.depth_format = static_cast<mh_u32>(z_fmt);
}

void GXSetLineWidth(u8 width, GXTexOffset texOffsets)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.line_width = width;
    pixel_state.line_texture_offsets = static_cast<mh_u32>(texOffsets);
}

void GXSetPointSize(u8 pointSize, GXTexOffset texOffsets)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.point_size = pointSize;
    pixel_state.point_texture_offsets = static_cast<mh_u32>(texOffsets);
}

void GXSetFieldMode(GXBool field_mode, GXBool half_aspect_ratio)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    pixel_state.field_mode = field_mode != GX_FALSE;
    pixel_state.half_aspect_ratio = half_aspect_ratio != GX_FALSE;
}

/* NTSC 480i with deflicker, the render mode gmMain installs.  The values are
 * the SDK's: a 640x480 external framebuffer centred in the 720-pixel NTSC
 * line, double-field mode, and the standard deflicker vertical filter. */
GXRenderModeObj GXNtsc480IntDf = {
    VI_TVMODE_NTSC_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 },
      { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 } },
    { 8, 8, 10, 12, 10, 8, 8 },
};

/* The same mode without deflicker, and the progressive-scan one.  The game
 * picks between the three by its deflicker and progressive settings
 * (gmMainLib_8015F500).  Values are the SDK's. */
GXRenderModeObj GXNtsc480Int = {
    VI_TVMODE_NTSC_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 },
      { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 } },
    { 0, 0, 21, 22, 21, 0, 0 },
};

GXRenderModeObj GXNtsc480Prog = {
    VI_TVMODE_NTSC_PROG,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 },
      { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 } },
    { 0, 0, 21, 22, 21, 0, 0 },
};

void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    display_copy_state.source_left = left;
    display_copy_state.source_top = top;
    display_copy_state.source_width = wd;
    display_copy_state.source_height = ht;
}

void GXSetDispCopyDst(u16 wd, u16 ht)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    display_copy_state.destination_width = wd;
    display_copy_state.destination_height = ht;
}

/* Returns the number of external-framebuffer lines the copy will produce,
 * which is what the caller uses to size the XFB. */
u32 GXSetDispCopyYScale(f32 vscale)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    display_copy_state.vertical_scale = vscale;
    const float lines =
        static_cast<float>(display_copy_state.source_height) * vscale;
    return static_cast<u32>(lines + 0.5F);
}

void GXSetDispCopyGamma(GXGamma gamma)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    display_copy_state.gamma = static_cast<mh_u32>(gamma);
}

void GXSetCopyClamp(GXFBClamp clamp)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    display_copy_state.clamp = static_cast<mh_u32>(clamp);
}

void GXSetCopyClear(GXColor clear_clr, u32 clear_z)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    display_copy_state.clear_color[0] = clear_clr.r;
    display_copy_state.clear_color[1] = clear_clr.g;
    display_copy_state.clear_color[2] = clear_clr.b;
    display_copy_state.clear_color[3] = clear_clr.a;
    display_copy_state.clear_depth = clear_z;
}

/* A null sample pattern or filter means "keep the hardware default", which is
 * how the SDK documents the call, so the host leaves its copy untouched. */
void GXSetCopyFilter(GXBool aa, const u8 sample_pattern[12][2], GXBool vf,
                     const u8 vfilter[7])
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    display_copy_state.antialiasing = aa != GX_FALSE;
    display_copy_state.vertical_filter = vf != GX_FALSE;
    if (aa != GX_FALSE && sample_pattern != nullptr) {
        for (std::size_t point = 0; point < 12; ++point) {
            display_copy_state.sample_pattern[point][0] =
                sample_pattern[point][0];
            display_copy_state.sample_pattern[point][1] =
                sample_pattern[point][1];
        }
    }
    if (vf != GX_FALSE && vfilter != nullptr) {
        for (std::size_t tap = 0; tap < 7; ++tap) {
            display_copy_state.filter_weights[tap] = vfilter[tap];
        }
    }
}

/* Like GXCopyTex, this records the request: there is no host framebuffer to
 * resolve the EFB into yet.  The copy ends a frame, so a frame sink receives
 * the capture here. */
void GXCopyDisp(void* dest, GXBool clear)
{
    MeleeHostGxFrameSink sink = nullptr;
    void* sink_user_data = nullptr;
    {
        const std::lock_guard<std::mutex> guard(state_mutex);
        ensure_initialized_locked();
        display_copy_state.copy_count += 1;
        display_copy_state.last_destination = dest;
        display_copy_state.last_clear = clear != GX_FALSE;
        sink = frame_sink;
        sink_user_data = frame_sink_user_data;
    }
    if (sink != nullptr) {
        sink(sink_user_data);
        melee_host_gx_reset_command_log();
    }
}

void melee_host_gx_set_frame_sink(MeleeHostGxFrameSink sink, void* user_data)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    frame_sink = sink;
    frame_sink_user_data = user_data;
}

GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    GXDrawDoneCallback previous = draw_done_callback;
    draw_done_callback = cb;
    return previous;
}

void GXSetDrawDone(void)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    draw_sync_state.pending = true;
    draw_sync_state.fence_count += 1;
}

/* Drains one pending fence and runs its callback outside the lock, because a
 * callback is game code that may call back into GX. */
static bool drain_draw_done_locked()
{
    if (!draw_sync_state.pending) {
        return false;
    }
    draw_sync_state.pending = false;
    if (draw_done_callback != nullptr) {
        draw_sync_state.callback_count += 1;
        return true;
    }
    return false;
}

void GXWaitDrawDone(void)
{
    GXDrawDoneCallback callback = nullptr;
    {
        const std::lock_guard<std::mutex> guard(state_mutex);
        ensure_initialized_locked();
        draw_sync_state.wait_count += 1;
        if (drain_draw_done_locked()) {
            callback = draw_done_callback;
        }
    }
    if (callback != nullptr) {
        callback();
    }
}

void GXDrawDone(void)
{
    GXSetDrawDone();
    GXWaitDrawDone();
}

bool melee_host_gx_drain_draw_done(void)
{
    GXDrawDoneCallback callback = nullptr;
    bool drained = false;
    {
        const std::lock_guard<std::mutex> guard(state_mutex);
        ensure_initialized_locked();
        drained = draw_sync_state.pending;
        if (drain_draw_done_locked()) {
            callback = draw_done_callback;
        }
    }
    if (callback != nullptr) {
        callback();
    }
    return drained;
}

void melee_host_gx_display_copy_state(MeleeHostGxDisplayCopyState* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    *output = display_copy_state;
}

void melee_host_gx_draw_sync_state(MeleeHostGxDrawSyncState* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    *output = draw_sync_state;
}

/* Byte size of a texture in GX memory, including every mip level when the
 * texture is mipmapped.  Each format has a fixed block footprint, and a level
 * always occupies whole blocks, so a 1x1 level still costs a full block. */
u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, u8 mipmap,
                       u8 max_lod)
{
    unsigned block_width = 8;
    unsigned block_height = 4;
    unsigned block_bytes = 32;

    switch (format) {
    case GX_TF_I4:
    case GX_TF_C4:
    case GX_TF_CMPR:
        block_width = 8;
        block_height = 8;
        block_bytes = 32;
        break;
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_C8:
        block_width = 8;
        block_height = 4;
        block_bytes = 32;
        break;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_C14X2:
        block_width = 4;
        block_height = 4;
        block_bytes = 32;
        break;
    case GX_TF_RGBA8:
        block_width = 4;
        block_height = 4;
        block_bytes = 64;
        break;
    default:
        return 0;
    }
    /* CMPR packs four 4x4 DXT1 blocks into one 32-byte GX block, which the
     * 8x8 footprint above already accounts for. */

    unsigned levels = 1;
    if (mipmap != GX_FALSE) {
        levels = static_cast<unsigned>(max_lod) + 1U;
    }

    u32 total = 0;
    unsigned level_width = width;
    unsigned level_height = height;
    for (unsigned level = 0; level < levels; ++level) {
        const unsigned blocks_across =
            (level_width + block_width - 1) / block_width;
        const unsigned blocks_down =
            (level_height + block_height - 1) / block_height;
        total += static_cast<u32>(blocks_across * blocks_down * block_bytes);
        if (level_width == 1 && level_height == 1) {
            break;
        }
        level_width = level_width > 1 ? level_width / 2 : 1;
        level_height = level_height > 1 ? level_height / 2 : 1;
    }
    return total;
}

void melee_host_gx_pixel_state(MeleeHostGxPixelState* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    *output = pixel_state;
}

void melee_host_gx_indirect_state(MeleeHostGxIndirectState* output)
{
    if (output == nullptr) {
        return;
    }
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    *output = indirect_state;
}

void melee_host_gx_transform_state(MeleeHostGxTransformState* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    *output = transform_state;
}

void melee_host_gx_tev_state(MeleeHostGxTevState* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    *output = tev_state;
}

bool melee_host_gx_channel_control(mh_u32 channel,
                                   MeleeHostGxChannelControl* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    if (output == nullptr || channel >= kChannels) {
        return false;
    }
    *output = channel_controls[channel];
    return true;
}

bool melee_host_gx_bound_texture(mh_u32 texmap, MeleeHostGxTextureDesc* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    if (output == nullptr || texmap >= kTexMaps) {
        return false;
    }
    *output = bound_textures[texmap];
    return bound_textures[texmap].bound;
}

bool melee_host_gx_loaded_tlut(mh_u32 tlut_name, MeleeHostGxTlutDesc* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    if (output == nullptr || tlut_name >= kTluts) {
        return false;
    }
    *output = loaded_tluts[tlut_name];
    return loaded_tluts[tlut_name].loaded;
}

bool melee_host_gx_light(mh_u32 light_index, MeleeHostGxLightDesc* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    if (output == nullptr || light_index >= kLights) {
        return false;
    }
    *output = lights[light_index];
    return lights[light_index].loaded;
}

void melee_host_gx_evaluate_lighting(const MeleeHostGxCapturedVertex* vertex,
                                     mh_u8 color0a0[4], mh_u8 color1a1[4])
{
    if (vertex == nullptr || color0a0 == nullptr || color1a1 == nullptr) {
        return;
    }
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    evaluate_channel_locked(*vertex, 0, 2, color0a0);
    evaluate_channel_locked(*vertex, 1, 3, color1a1);
}

void melee_host_gx_fog_state(MeleeHostGxFogState* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    *output = fog_state;
}

/* MELEE_HOST_FOG=0 sets this, so a route can draw the same frames without
 * the fog the game asked for. */
static std::atomic<bool> fog_enabled{ true };

void melee_host_gx_set_fog_enabled(bool enabled)
{
    fog_enabled.store(enabled, std::memory_order_relaxed);
}

bool melee_host_gx_fog_enabled(void)
{
    return fog_enabled.load(std::memory_order_relaxed);
}

void melee_host_gx_copy_state(MeleeHostGxCopyState* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    *output = copy_state;
}

bool melee_host_gx_matrix_loaded(mh_u32 row_id)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    return row_id + 3 <= kMatrixRows && matrix_loaded[row_id] &&
           matrix_loaded[row_id + 1] && matrix_loaded[row_id + 2];
}

bool melee_host_gx_matrix(mh_u32 row_id, MeleeHostGxAffineTransform* output)
{
    const std::lock_guard<std::mutex> guard(state_mutex);
    ensure_initialized_locked();
    if (output == nullptr || row_id + 3 > kMatrixRows) {
        return false;
    }
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            output->values[row][column] =
                matrix_memory[row_id + row][column];
        }
    }
    return true;
}

} // extern "C"
