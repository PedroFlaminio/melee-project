#include "gx/tev.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace melee::gx {
namespace {

constexpr mh_u32 kColorArgZero = 15;
constexpr mh_u32 kAlphaArgZero = 7;
constexpr mh_u32 kOpSub = 1;
constexpr mh_u32 kScaleDivide2 = 3;
constexpr mh_u32 kBiasAddHalf = 1;
constexpr mh_u32 kBiasSubHalf = 2;

/* The swap tables GXInit installs.  The only GXSetTevSwapModeTable call in the
 * code the host builds installs GXInit's own SWAP0, and the recorder stops by
 * name on any other table, so these are the tables every draw uses. */
constexpr std::array<std::array<std::size_t, 4>, 4> kSwapTables{ {
    { 0, 1, 2, 3 },
    { 0, 0, 0, 3 },
    { 1, 1, 1, 3 },
    { 2, 2, 2, 3 },
} };
constexpr std::array<const char*, 4> kSwapSwizzles{ "rgba", "rrra", "ggga",
                                                     "bbba" };

/* KCSEL/KASEL 0..7 name fixed fractions rather than a konst register. */
constexpr std::array<int, 8> kKonstFractions{ 255, 223, 191, 159,
                                               128, 96,  64,  32 };

std::size_t stage_count(const MeleeHostGxTevState& tev)
{
    return std::clamp<std::size_t>(tev.stage_count, 1,
                                   MELEE_HOST_GX_MAX_TEVSTAGE);
}

std::size_t swap_table(mh_u32 selection)
{
    return selection < kSwapTables.size() ? selection : 0;
}

/* GXSetTevOrder resolves a channel id to one of the two rasterized colours or
 * to zero, the same way the SDK packs it into the order register. */
int raster_channel(mh_u32 channel)
{
    switch (channel) {
    case 0: // GX_COLOR0
    case 2: // GX_ALPHA0
    case 4: // GX_COLOR0A0
        return 0;
    case 1:
    case 3:
    case 5:
        return 1;
    default:
        return -1;
    }
}

enum class TexelSource { Sampled, White, Zero };

/* A stage without a texture map reads white, unless no texture coordinate is
 * generated at all, in which case sampling yields zero. */
TexelSource texel_source(const MeleeHostGxTevState& tev,
                         const MeleeHostGxTevStage& stage)
{
    if (tev.texcoord_gen_count == 0) {
        return TexelSource::Zero;
    }
    return stage.texmap < MELEE_HOST_GX_MAX_TEXMAP ? TexelSource::Sampled
                                                   : TexelSource::White;
}

bool is_compare(mh_u32 op)
{
    return op > kOpSub;
}

/* Registers are 11 bits wide and signed; d reads all of them. */
int sign_extend_11(int value)
{
    return ((value & 0x7FF) ^ 0x400) - 0x400;
}

int left_shift_of(mh_u32 scale)
{
    return scale == 1 ? 2 : scale == 2 ? 4 : 1;
}

/* GX_ITF_* names a retained bit width, not an enum shift amount. */
int indirect_texel_shift(mh_u32 format)
{
    switch (format) {
    case 1: return 3; /* GX_ITF_5 */
    case 2: return 4; /* GX_ITF_4 */
    case 3: return 5; /* GX_ITF_3 */
    default: return 0; /* GX_ITF_8 and an invalid value */
    }
}

int combine_regular(int a, int b, int c, int d, mh_u32 op, mh_u32 bias,
                    mh_u32 scale)
{
    const int stretched = c + (c >> 7);
    const int multiplier = left_shift_of(scale);
    int lerp = (a * (256 - stretched) + b * stretched) * multiplier;
    lerp += scale == kScaleDivide2 ? 0 : (op == kOpSub ? 127 : 128);
    lerp >>= 8;
    if (op == kOpSub) {
        lerp = -lerp;
    }
    const int bias_value =
        bias == kBiasAddHalf ? 128 : bias == kBiasSubHalf ? -128 : 0;
    const int result = (d + bias_value) * multiplier + lerp;
    return scale == kScaleDivide2 ? result >> 1 : result;
}

int clamp_output(int value, bool clamp)
{
    return clamp ? std::clamp(value, 0, 255) : std::clamp(value, -1024, 1023);
}

/* The packed forms R8, GR16 and BGR24 read the colour inputs, even for the
 * alpha combiner. */
int packed(const std::array<int, 3>& value, mh_u32 mode)
{
    switch (mode) {
    case 0:
        return value[0];
    case 1:
        return (value[1] << 8) | value[0];
    default:
        return (value[2] << 16) | (value[1] << 8) | value[0];
    }
}

bool compare_holds(int a, int b, mh_u32 op)
{
    return (op & 1U) != 0 ? a == b : a > b;
}

} // namespace

std::array<int, 4> evaluate_tev(const MeleeHostGxTevState& tev,
                                const TevFragmentInputs& inputs)
{
    std::array<std::array<int, 4>, MELEE_HOST_GX_MAX_TEVREG> reg{};
    for (std::size_t index = 1; index < reg.size(); ++index) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            reg[index][channel] = tev.registers[index][channel];
        }
    }

    const std::size_t stages = stage_count(tev);
    for (std::size_t index = 0; index < stages; ++index) {
        const MeleeHostGxTevStage& stage = tev.stages[index];

        std::array<int, 4> texel{};
        switch (texel_source(tev, stage)) {
        case TexelSource::Sampled:
            texel = inputs.texmap[stage.texmap];
            break;
        case TexelSource::White:
            texel = { 255, 255, 255, 255 };
            break;
        case TexelSource::Zero:
            break;
        }
        std::array<int, 4> raster{};
        const int channel = raster_channel(stage.color_channel);
        if (channel >= 0) {
            raster = inputs.raster[static_cast<std::size_t>(channel)];
        }
        const auto swap = [](const std::array<int, 4>& value,
                             mh_u32 selection) {
            const auto& table = kSwapTables[swap_table(selection)];
            return std::array<int, 4>{ value[table[0]], value[table[1]],
                                       value[table[2]], value[table[3]] };
        };
        texel = swap(texel, stage.texture_swap);
        raster = swap(raster, stage.raster_swap);

        const auto konst_component = [&tev](mh_u32 selection) {
            const std::size_t konst = (selection - 16U) % 4U;
            const std::size_t component = (selection - 16U) / 4U;
            return static_cast<int>(tev.konst_colors[konst][component]);
        };
        std::array<int, 3> konst_color{};
        const mh_u32 kcsel = stage.konst_color_select;
        if (kcsel < kKonstFractions.size()) {
            konst_color.fill(kKonstFractions[kcsel]);
        } else if (kcsel >= 12 && kcsel <= 15) {
            for (std::size_t component = 0; component < 3; ++component) {
                konst_color[component] =
                    tev.konst_colors[kcsel - 12U][component];
            }
        } else if (kcsel >= 16 && kcsel <= 31) {
            konst_color.fill(konst_component(kcsel));
        }
        const mh_u32 kasel = stage.konst_alpha_select;
        int konst_alpha = 0;
        if (kasel < kKonstFractions.size()) {
            konst_alpha = kKonstFractions[kasel];
        } else if (kasel >= 16 && kasel <= 31) {
            konst_alpha = konst_component(kasel);
        }

        const auto color_arg = [&](mh_u32 arg, std::size_t component) {
            switch (arg) {
            case 0: case 2: case 4: case 6:
                return reg[arg / 2][component];
            case 1: case 3: case 5: case 7:
                return reg[arg / 2][3];
            case 8:
                return texel[component];
            case 9:
                return texel[3];
            case 10:
                return raster[component];
            case 11:
                return raster[3];
            case 12:
                return 255;
            case 13:
                return 128;
            case 14:
                return konst_color[component];
            default:
                return 0;
            }
        };
        const auto alpha_arg = [&](mh_u32 arg) {
            switch (arg) {
            case 0: case 1: case 2: case 3:
                return reg[arg][3];
            case 4:
                return texel[3];
            case 5:
                return raster[3];
            case 6:
                return konst_alpha;
            default:
                return 0;
            }
        };

        std::array<int, 3> ca{};
        std::array<int, 3> cb{};
        std::array<int, 3> cc{};
        std::array<int, 3> cd{};
        for (std::size_t component = 0; component < 3; ++component) {
            ca[component] = color_arg(stage.color_input[0], component) & 255;
            cb[component] = color_arg(stage.color_input[1], component) & 255;
            cc[component] = color_arg(stage.color_input[2], component) & 255;
            cd[component] =
                sign_extend_11(color_arg(stage.color_input[3], component));
        }
        const int aa = alpha_arg(stage.alpha_input[0]) & 255;
        const int ab = alpha_arg(stage.alpha_input[1]) & 255;
        const int ac = alpha_arg(stage.alpha_input[2]) & 255;
        const int ad = sign_extend_11(alpha_arg(stage.alpha_input[3]));

        std::array<int, 3> color{};
        if (!is_compare(stage.color_op)) {
            for (std::size_t component = 0; component < 3; ++component) {
                color[component] = combine_regular(
                    ca[component], cb[component], cc[component], cd[component],
                    stage.color_op, stage.color_bias, stage.color_scale);
            }
        } else {
            const mh_u32 mode = (stage.color_op >> 1U) & 3U;
            for (std::size_t component = 0; component < 3; ++component) {
                const bool holds =
                    mode == 3 ? compare_holds(ca[component], cb[component],
                                              stage.color_op)
                              : compare_holds(packed(ca, mode),
                                              packed(cb, mode),
                                              stage.color_op);
                color[component] = cd[component] + (holds ? cc[component] : 0);
            }
        }

        int alpha = 0;
        if (!is_compare(stage.alpha_op)) {
            alpha = combine_regular(aa, ab, ac, ad, stage.alpha_op,
                                    stage.alpha_bias, stage.alpha_scale);
        } else {
            const mh_u32 mode = (stage.alpha_op >> 1U) & 3U;
            const bool holds =
                mode == 3 ? compare_holds(aa, ab, stage.alpha_op)
                          : compare_holds(packed(ca, mode), packed(cb, mode),
                                          stage.alpha_op);
            alpha = ad + (holds ? ac : 0);
        }

        /* Both combiners read the registers as they were before this stage,
         * so the writes happen only after both results exist. */
        if (stage.color_out_reg < reg.size()) {
            for (std::size_t component = 0; component < 3; ++component) {
                reg[stage.color_out_reg][component] =
                    clamp_output(color[component], stage.color_clamp);
            }
        }
        if (stage.alpha_out_reg < reg.size()) {
            reg[stage.alpha_out_reg][3] =
                clamp_output(alpha, stage.alpha_clamp);
        }
    }
    return { reg[0][0] & 255, reg[0][1] & 255, reg[0][2] & 255,
             reg[0][3] & 255 };
}

std::array<float, 2> indirect_texture_offset(
    const MeleeHostGxIndirectState& indirect, std::size_t tev_stage,
    const std::array<int, 4>& texel)
{
    if (tev_stage >= MELEE_HOST_GX_MAX_TEVSTAGE ||
        !indirect.tev_stages[tev_stage].indirect ||
        indirect.tev_stages[tev_stage].ind_stage >= indirect.stage_count ||
        indirect.tev_stages[tev_stage].ind_stage >=
            MELEE_HOST_GX_MAX_INDIRECT_STAGE) {
        return {};
    }
    const MeleeHostGxIndirectTevStage& config =
        indirect.tev_stages[tev_stage];
    const int shift = indirect_texel_shift(config.format);
    const int center = 128 >> shift;
    std::array<int, 3> value{ texel[0] >> shift, texel[1] >> shift,
                              texel[2] >> shift };
    if (config.bias == 1 || config.bias == 3 || config.bias == 5 ||
        config.bias == 7) {
        value[0] -= center;
    }
    if (config.bias == 2 || config.bias == 3 || config.bias == 6 ||
        config.bias == 7) {
        value[1] -= center;
    }
    if (config.bias == 4 || config.bias == 5 || config.bias == 6 ||
        config.bias == 7) {
        value[2] -= center;
    }
    if (config.matrix < 1 || config.matrix > 3) {
        return { static_cast<float>(value[0]) / 256.0F,
                 static_cast<float>(value[1]) / 256.0F };
    }
    const MeleeHostGxIndirectMatrix& matrix =
        indirect.matrices[config.matrix - 1U];
    const float scale = std::exp2(static_cast<float>(matrix.scale_exp)) /
                        256.0F;
    std::array<float, 2> result{};
    for (std::size_t row = 0; row < result.size(); ++row) {
        for (std::size_t column = 0; column < value.size(); ++column) {
            result[row] += matrix.offset[row][column] *
                           static_cast<float>(value[column]);
        }
        result[row] *= scale;
    }
    return result;
}

bool alpha_test_passes(const MeleeHostGxDrawState& state, int alpha)
{
    const auto compare = [alpha](mh_u32 function, int reference) {
        switch (function) {
        case 0:
            return false;
        case 1:
            return alpha < reference;
        case 2:
            return alpha == reference;
        case 3:
            return alpha <= reference;
        case 4:
            return alpha > reference;
        case 5:
            return alpha != reference;
        case 6:
            return alpha >= reference;
        default:
            return true;
        }
    };
    const bool first = compare(state.alpha_compare_0, state.alpha_ref_0);
    const bool second = compare(state.alpha_compare_1, state.alpha_ref_1);
    switch (state.alpha_op) {
    case 0:
        return first && second;
    case 1:
        return first || second;
    case 2:
        return first != second;
    default:
        return first == second;
    }
}

std::uint32_t tev_unmodelled_features(const MeleeHostGxTevState& tev)
{
    std::uint32_t features = kTevUnmodelledNone;
    const std::size_t stages = stage_count(tev);
    for (std::size_t index = 0; index < stages; ++index) {
        const MeleeHostGxTevStage& stage = tev.stages[index];
        for (std::size_t input = 0; input < 4; ++input) {
            if (stage.color_input[input] > kColorArgZero ||
                stage.alpha_input[input] > kAlphaArgZero)
            {
                features |= kTevUnmodelledArgument;
            }
        }
    }
    return features;
}

std::string describe_tev_unmodelled(std::uint32_t features)
{
    std::string text;
    const auto append = [&text](const char* name) {
        if (!text.empty()) {
            text += ", ";
        }
        text += name;
    };
    if ((features & kTevUnmodelledArgument) != 0) {
        append("undefined input");
    }
    return text.empty() ? "none" : text;
}

float fog_blend(const MeleeHostGxDrawState& state, float eye_z)
{
    /* GX_FOG_NONE, and the types the SDK does not define, leave the colour
     * alone.  A range of zero would divide by zero; the hardware's own
     * parameters are undefined there, so the host reads it as no fog. */
    if (state.fog_type == 0 || state.fog_end_z == state.fog_start_z) {
        return 0.0F;
    }
    const float span = state.fog_end_z - state.fog_start_z;
    const float t =
        std::clamp((eye_z - state.fog_start_z) / span, 0.0F, 1.0F);
    switch (state.fog_type) {
    case 2: /* GX_FOG_LIN */
        return t;
    case 4: /* GX_FOG_EXP */
        return 1.0F - std::exp2(-8.0F * t);
    case 5: /* GX_FOG_EXP2 */
        return 1.0F - std::exp2(-8.0F * t * t);
    case 6: /* GX_FOG_REVEXP */
        return std::exp2(-8.0F * (1.0F - t));
    case 7: /* GX_FOG_REVEXP2 */
        return std::exp2(-8.0F * (1.0F - t) * (1.0F - t));
    default:
        return 0.0F;
    }
}

int fog_weight(const MeleeHostGxDrawState& state, float eye_z)
{
    const float blend = fog_blend(state, eye_z);
    return std::clamp(static_cast<int>(std::lround(blend * 256.0F)), 0, 256);
}

int fog_mix(int component, int fog_component, int weight)
{
    return (component * (256 - weight) + fog_component * weight + 128) >> 8;
}

std::string tev_vertex_shader_source()
{
    return R"(#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color0;
layout(location = 2) in vec4 a_color1;
layout(location = 3) in vec3 a_texgen[8];
uniform mat4 u_mvp;
out vec4 v_color0;
out vec4 v_color1;
out vec3 v_texgen[8];
void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color0 = a_color0;
    v_color1 = a_color1;
    for (int i = 0; i < 8; ++i) {
        v_texgen[i] = a_texgen[i];
    }
}
)";
}

namespace {

std::string color_arg_glsl(mh_u32 arg)
{
    switch (arg) {
    case 0: case 2: case 4: case 6:
        return "reg[" + std::to_string(arg / 2) + "].rgb";
    case 1: case 3: case 5: case 7:
        return "ivec3(reg[" + std::to_string(arg / 2) + "].a)";
    case 8:
        return "tex.rgb";
    case 9:
        return "ivec3(tex.a)";
    case 10:
        return "ras.rgb";
    case 11:
        return "ivec3(ras.a)";
    case 12:
        return "ivec3(255)";
    case 13:
        return "ivec3(128)";
    case 14:
        return "konst.rgb";
    default:
        return "ivec3(0)";
    }
}

std::string alpha_arg_glsl(mh_u32 arg)
{
    switch (arg) {
    case 0: case 1: case 2: case 3:
        return "reg[" + std::to_string(arg) + "].a";
    case 4:
        return "tex.a";
    case 5:
        return "ras.a";
    case 6:
        return "konst.a";
    default:
        return "0";
    }
}

std::string konst_component_glsl(mh_u32 selection)
{
    constexpr std::array<char, 4> kComponents{ 'r', 'g', 'b', 'a' };
    return "u_konst[" + std::to_string((selection - 16U) % 4U) + "]." +
           kComponents[(selection - 16U) / 4U];
}

std::string konst_color_glsl(mh_u32 selection)
{
    if (selection < kKonstFractions.size()) {
        return "ivec3(" + std::to_string(kKonstFractions[selection]) + ")";
    }
    if (selection >= 12 && selection <= 15) {
        return "u_konst[" + std::to_string(selection - 12U) + "].rgb";
    }
    if (selection >= 16 && selection <= 31) {
        return "ivec3(" + konst_component_glsl(selection) + ")";
    }
    return "ivec3(0)";
}

std::string konst_alpha_glsl(mh_u32 selection)
{
    if (selection < kKonstFractions.size()) {
        return std::to_string(kKonstFractions[selection]);
    }
    if (selection >= 16 && selection <= 31) {
        return konst_component_glsl(selection);
    }
    return "0";
}

std::string packed_glsl(const char* value, mh_u32 mode)
{
    const std::string v = value;
    switch (mode) {
    case 0:
        return v + ".r";
    case 1:
        return "((" + v + ".g << 8) | " + v + ".r)";
    default:
        return "((" + v + ".b << 16) | (" + v + ".g << 8) | " + v + ".r)";
    }
}

std::string regular_glsl(const char* type, const char* a, const char* b,
                         const char* c, const char* d, mh_u32 op, mh_u32 bias,
                         mh_u32 scale)
{
    const std::string t = type;
    const int multiplier = left_shift_of(scale);
    const int rounding =
        scale == kScaleDivide2 ? 0 : (op == kOpSub ? 127 : 128);
    const int bias_value =
        bias == kBiasAddHalf ? 128 : bias == kBiasSubHalf ? -128 : 0;
    std::string code;
    code += "        " + t + " stretched = " + c + " + (" + c + " >> 7);\n";
    code += "        " + t + " lerp = ((" + a + " * (256 - stretched) + " + b +
            " * stretched) * " + std::to_string(multiplier) + " + " +
            std::to_string(rounding) + ") >> 8;\n";
    std::string result = "((" + std::string(d) + " + (" +
                         std::to_string(bias_value) + ")) * " +
                         std::to_string(multiplier) +
                         (op == kOpSub ? " - " : " + ") + "lerp)";
    if (scale == kScaleDivide2) {
        result = "(" + result + " >> 1)";
    }
    code += "        result = " + result + ";\n";
    return code;
}

bool indirect_stage_active(const MeleeHostGxIndirectState& indirect,
                           std::size_t tev_stage)
{
    return tev_stage < MELEE_HOST_GX_MAX_TEVSTAGE &&
           indirect.tev_stages[tev_stage].indirect &&
           indirect.tev_stages[tev_stage].ind_stage < indirect.stage_count &&
           indirect.tev_stages[tev_stage].ind_stage <
               MELEE_HOST_GX_MAX_INDIRECT_STAGE;
}

std::string texgen_glsl(mh_u32 texcoord, const MeleeHostGxTevState& tev)
{
    return texcoord < tev.texcoord_gen_count &&
                   texcoord < MELEE_HOST_GX_MAX_TEXCOORD
               ? "v_texgen[" + std::to_string(texcoord) + "]"
               : std::string("vec3(0.0, 0.0, 1.0)");
}

std::string indirect_wrap_glsl(mh_u32 wrap, const char* coordinate)
{
    // GX_ITW_OFF preserves the coordinate; GX_ITW_0 forces it to zero.  The
    // remaining ids are 256, 128, 64, 32 and 16 texel periods in a normalized
    // 256-wide texture coordinate.
    if (wrap == 0) {
        return coordinate;
    }
    if (wrap == 6) {
        return "0.0";
    }
    const int multiplier = 1 << static_cast<int>(wrap - 1U);
    return "(fract(" + std::string(coordinate) + " * " +
           std::to_string(multiplier) + ".0) / " +
           std::to_string(multiplier) + ".0)";
}

} // namespace

std::string tev_fragment_shader_source(const MeleeHostGxTevState& tev,
                                       const MeleeHostGxIndirectState& indirect)
{
    std::string code = R"(#version 330 core
in vec4 v_color0;
in vec4 v_color1;
in vec3 v_texgen[8];
uniform sampler2D u_texmap[8];
uniform ivec4 u_register[4];
uniform ivec4 u_konst[4];
uniform ivec4 u_alpha_test;
uniform int u_alpha_ref1;
uniform int u_fog_type;
uniform vec2 u_fog_range;
uniform ivec4 u_fog_color;
uniform int u_z_texture_op;
uniform int u_z_texture_bias;
uniform int u_texmap_format[8];
uniform vec3 u_indirect_matrix[6];
uniform int u_indirect_matrix_scale[3];
out vec4 frag_color;

/* The same curve as melee::gx::fog_blend, over the fragment's eye-space
 * depth, which under a perspective projection is the clip w that
 * gl_FragCoord.w carries the reciprocal of. */
float gx_fog_blend(float eye_z)
{
    if (u_fog_type == 0 || u_fog_range.y == u_fog_range.x) {
        return 0.0;
    }
    float t = clamp((eye_z - u_fog_range.x) / (u_fog_range.y - u_fog_range.x),
                    0.0, 1.0);
    if (u_fog_type == 2) return t;
    if (u_fog_type == 4) return 1.0 - exp2(-8.0 * t);
    if (u_fog_type == 5) return 1.0 - exp2(-8.0 * t * t);
    if (u_fog_type == 6) return exp2(-8.0 * (1.0 - t));
    if (u_fog_type == 7) return exp2(-8.0 * (1.0 - t) * (1.0 - t));
    return 0.0;
}

/* The same 0..256 weight melee::gx::fog_weight gives, so both paths round
 * the mix the same way. */
int gx_fog_weight(float eye_z)
{
    return clamp(int(round(gx_fog_blend(eye_z) * 256.0)), 0, 256);
}

bool gx_compare(int function, int value, int reference)
{
    if (function == 0) return false;
    if (function == 1) return value < reference;
    if (function == 2) return value == reference;
    if (function == 3) return value <= reference;
    if (function == 4) return value > reference;
    if (function == 5) return value != reference;
    if (function == 6) return value >= reference;
    return true;
}

ivec3 sign_extend_11(ivec3 value) { return ((value & 2047) ^ 1024) - 1024; }
int sign_extend_11(int value) { return ((value & 2047) ^ 1024) - 1024; }

ivec4 sample_texmap_raw(int texmap, vec3 coord)
{
    float q = coord.z == 0.0 ? 1.0 : coord.z;
    return ivec4(round(texture(u_texmap[texmap], coord.xy / q) * 255.0));
}

ivec4 sample_texmap(int texmap, vec3 coord)
{
    ivec4 raw = sample_texmap_raw(texmap, coord);
    /* Z textures provide their most significant depth byte to TEV in every
     * channel.  The raw bytes remain available for GX_ZT_REPLACE below. */
    if (u_texmap_format[texmap] == 17 || u_texmap_format[texmap] == 19 ||
        u_texmap_format[texmap] == 22) {
        return ivec4(raw.r);
    }
    return raw;
}

void main()
{
    ivec4 raster0 = ivec4(round(clamp(v_color0, 0.0, 1.0) * 255.0));
    ivec4 raster1 = ivec4(round(clamp(v_color1, 0.0, 1.0) * 255.0));
    ivec4 reg[4];
    reg[0] = ivec4(0);
    reg[1] = u_register[1];
    reg[2] = u_register[2];
    reg[3] = u_register[3];
)";

    const std::size_t stages = stage_count(tev);
    code += "    ivec4 last_texel = ivec4(0);\n";
    code += "    ivec4 last_texel_raw = ivec4(0);\n";
    code += "    int last_texmap = -1;\n";
    code += "    vec2 indirect_previous = vec2(0.0);\n";
    for (std::size_t index = 0; index < stages; ++index) {
        const MeleeHostGxTevStage& stage = tev.stages[index];
        code += "    // stage " + std::to_string(index) + "\n    {\n";

        std::string texel;
        std::string raw_texel = "ivec4(0)";
        switch (texel_source(tev, stage)) {
        case TexelSource::Sampled: {
            /* A coordinate past the generated ones samples at the origin. */
            std::string coord = texgen_glsl(stage.texcoord, tev);
            if (indirect_stage_active(indirect, index)) {
                const MeleeHostGxIndirectTevStage& config =
                    indirect.tev_stages[index];
                const MeleeHostGxIndirectTexStage& input =
                    indirect.stages[config.ind_stage];
                const int shift = indirect_texel_shift(config.format);
                const int center = 128 >> shift;
                const mh_u32 scale_s = std::min<mh_u32>(input.scale_s, 8U);
                const mh_u32 scale_t = std::min<mh_u32>(input.scale_t, 8U);
                code += "        vec3 direct_coord = " + coord + ";\n";
                code += "        vec3 indirect_coord = " +
                        texgen_glsl(input.texcoord, tev) + ";\n";
                code += "        indirect_coord.xy /= vec2(" +
                        std::to_string(1U << scale_s) + ".0, " +
                        std::to_string(1U << scale_t) + ".0);\n";
                if (input.texmap < MELEE_HOST_GX_MAX_TEXMAP) {
                    code += "        ivec3 indirect_texel = sample_texmap_raw(" +
                            std::to_string(input.texmap) +
                            ", indirect_coord).rgb >> " +
                            std::to_string(shift) + ";\n";
                } else {
                    code += "        ivec3 indirect_texel = ivec3(0);\n";
                }
                if (config.bias == 1 || config.bias == 3 || config.bias == 5 ||
                    config.bias == 7) {
                    code += "        indirect_texel.r -= " +
                            std::to_string(center) + ";\n";
                }
                if (config.bias == 2 || config.bias == 3 || config.bias == 6 ||
                    config.bias == 7) {
                    code += "        indirect_texel.g -= " +
                            std::to_string(center) + ";\n";
                }
                if (config.bias == 4 || config.bias == 5 || config.bias == 6 ||
                    config.bias == 7) {
                    code += "        indirect_texel.b -= " +
                            std::to_string(center) + ";\n";
                }
                if (config.matrix >= 1 && config.matrix <= 3) {
                    const std::size_t matrix = config.matrix - 1U;
                    code += "        vec2 indirect_offset = vec2(dot(u_indirect_matrix[" +
                            std::to_string(matrix * 2U) +
                            "], vec3(indirect_texel)), dot(u_indirect_matrix[" +
                            std::to_string(matrix * 2U + 1U) +
                            "], vec3(indirect_texel))) * exp2(float(u_indirect_matrix_scale[" +
                            std::to_string(matrix) + "])) / 256.0;\n";
                } else {
                    code += "        vec2 indirect_offset = vec2(indirect_texel) / 256.0;\n";
                }
                code += "        vec2 base_coord = direct_coord.xy / "
                        "(direct_coord.z == 0.0 ? 1.0 : direct_coord.z);\n";
                code += "        base_coord = vec2(" +
                        indirect_wrap_glsl(config.wrap_s, "base_coord.x") +
                        ", " + indirect_wrap_glsl(config.wrap_t, "base_coord.y") +
                        ");\n";
                if (config.add_previous) {
                    code += "        indirect_offset += indirect_previous;\n";
                }
                code += "        indirect_previous = indirect_offset;\n";
                code += "        vec3 tev_coord = vec3(base_coord + indirect_offset, 1.0);\n";
                coord = "tev_coord";
            }
            texel = "sample_texmap(" + std::to_string(stage.texmap) + ", " +
                    coord + ")";
            raw_texel = "sample_texmap_raw(" +
                        std::to_string(stage.texmap) + ", " + coord + ")";
            break;
        }
        case TexelSource::White:
            texel = "ivec4(255)";
            raw_texel = texel;
            break;
        case TexelSource::Zero:
            texel = "ivec4(0)";
            break;
        }
        const int channel = raster_channel(stage.color_channel);
        const std::string raster =
            channel < 0 ? "ivec4(0)"
                        : "raster" + std::to_string(channel);
        code += "        ivec4 tex_raw = " + raw_texel + ";\n";
        code += "        ivec4 tex = (" + texel + ")." +
                kSwapSwizzles[swap_table(stage.texture_swap)] + ";\n";
        code += "        last_texel = tex;\n";
        code += "        last_texel_raw = tex_raw;\n";
        code += "        last_texmap = " + std::to_string(stage.texmap) + ";\n";
        code += "        ivec4 ras = (" + raster + ")." +
                kSwapSwizzles[swap_table(stage.raster_swap)] + ";\n";
        code += "        ivec4 konst = ivec4(" +
                konst_color_glsl(stage.konst_color_select) + ", " +
                konst_alpha_glsl(stage.konst_alpha_select) + ");\n";
        code += "        ivec3 ca = " + color_arg_glsl(stage.color_input[0]) +
                " & 255;\n";
        code += "        ivec3 cb = " + color_arg_glsl(stage.color_input[1]) +
                " & 255;\n";
        code += "        ivec3 cc = " + color_arg_glsl(stage.color_input[2]) +
                " & 255;\n";
        code += "        ivec3 cd = sign_extend_11(" +
                color_arg_glsl(stage.color_input[3]) + ");\n";
        code += "        int aa = " + alpha_arg_glsl(stage.alpha_input[0]) +
                " & 255;\n";
        code += "        int ab = " + alpha_arg_glsl(stage.alpha_input[1]) +
                " & 255;\n";
        code += "        int ac = " + alpha_arg_glsl(stage.alpha_input[2]) +
                " & 255;\n";
        code += "        int ad = sign_extend_11(" +
                alpha_arg_glsl(stage.alpha_input[3]) + ");\n";

        code += "        ivec3 color;\n        {\n            ivec3 result;\n";
        if (!is_compare(stage.color_op)) {
            std::string block =
                regular_glsl("ivec3", "ca", "cb", "cc", "cd", stage.color_op,
                             stage.color_bias, stage.color_scale);
            code += block;
        } else {
            const mh_u32 mode = (stage.color_op >> 1U) & 3U;
            const char* relation = (stage.color_op & 1U) != 0 ? "equal"
                                                              : "greaterThan";
            if (mode == 3) {
                code += "            result = cd + cc * ivec3(" +
                        std::string(relation) + "(ca, cb));\n";
            } else {
                const char* scalar =
                    (stage.color_op & 1U) != 0 ? " == " : " > ";
                code += "            result = cd + ((" + packed_glsl("ca", mode) +
                        scalar + packed_glsl("cb", mode) +
                        ") ? cc : ivec3(0));\n";
            }
        }
        code += "            color = result;\n        }\n";

        code += "        int alpha;\n        {\n            int result;\n";
        if (!is_compare(stage.alpha_op)) {
            code += regular_glsl("int", "aa", "ab", "ac", "ad", stage.alpha_op,
                                 stage.alpha_bias, stage.alpha_scale);
        } else {
            const mh_u32 mode = (stage.alpha_op >> 1U) & 3U;
            const char* scalar = (stage.alpha_op & 1U) != 0 ? " == " : " > ";
            const std::string left =
                mode == 3 ? std::string("aa") : packed_glsl("ca", mode);
            const std::string right =
                mode == 3 ? std::string("ab") : packed_glsl("cb", mode);
            code += "            result = ad + ((" + left + scalar + right +
                    ") ? ac : 0);\n";
        }
        code += "            alpha = result;\n        }\n";

        const auto clamp_glsl = [](const char* value, bool clamp) {
            return clamp ? "clamp(" + std::string(value) + ", 0, 255)"
                         : "clamp(" + std::string(value) + ", -1024, 1023)";
        };
        if (stage.color_out_reg < MELEE_HOST_GX_MAX_TEVREG) {
            code += "        reg[" + std::to_string(stage.color_out_reg) +
                    "].rgb = " + clamp_glsl("color", stage.color_clamp) +
                    ";\n";
        }
        if (stage.alpha_out_reg < MELEE_HOST_GX_MAX_TEVREG) {
            code += "        reg[" + std::to_string(stage.alpha_out_reg) +
                    "].a = " + clamp_glsl("alpha", stage.alpha_clamp) + ";\n";
        }
        code += "    }\n";
    }

    code += R"(    ivec4 final_color = reg[0] & 255;
    bool first = gx_compare(u_alpha_test.x, final_color.a, u_alpha_test.y);
    bool second = gx_compare(u_alpha_test.w, final_color.a, u_alpha_ref1);
    bool passes;
    if (u_alpha_test.z == 0) passes = first && second;
    else if (u_alpha_test.z == 1) passes = first || second;
    else if (u_alpha_test.z == 2) passes = first != second;
    else passes = first == second;
    if (!passes) {
        discard;
    }
    int fog = gx_fog_weight(1.0 / gl_FragCoord.w);
    final_color.rgb = (final_color.rgb * (256 - fog) +
                       u_fog_color.rgb * fog + 128) >> 8;
    frag_color = vec4(final_color) / 255.0;
    /* A shader that writes gl_FragDepth must define it on every path. */
    gl_FragDepth = gl_FragCoord.z;
    if (u_z_texture_op == 2 && last_texmap >= 0 &&
        (u_texmap_format[last_texmap] == 17 ||
         u_texmap_format[last_texmap] == 19 ||
         u_texmap_format[last_texmap] == 22)) { /* GX_ZT_REPLACE */
        /* Depth replacement reads the unswizzled depth texel.  TEV swaps
         * affect the colour combiner only, not GX_ZT_REPLACE. */
        int depth = last_texel_raw.r * 65793; /* GX_TF_Z8 */
        if (u_texmap_format[last_texmap] == 19) { /* GX_TF_Z16 */
            depth = (last_texel_raw.r << 16) | (last_texel_raw.g << 8) |
                    last_texel_raw.r;
        } else if (u_texmap_format[last_texmap] == 22) { /* GX_TF_Z24X8 */
            depth = (last_texel_raw.r << 16) | (last_texel_raw.g << 8) |
                    last_texel_raw.b;
        }
        gl_FragDepth = clamp(float(min(depth + u_z_texture_bias, 16777215)) /
                                 16777215.0,
                             0.0, 1.0);
    }
}
)";
    return code;
}

} // namespace melee::gx
