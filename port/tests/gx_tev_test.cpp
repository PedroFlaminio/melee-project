#include "test.hpp"

#include "gx/tev.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include <dolphin/gx/GXStruct.h>
#include <dolphin/gx/GXTev.h>

namespace {

using melee::gx::TevFragmentInputs;

bool close(float value, float expected)
{
    return std::fabs(value - expected) < 1.0e-6F;
}

/* Every stage starts inert: all inputs zero, neutral arithmetic, writing the
 * final register, sampling map 0 through coordinate 0 and colour channel 0. */
MeleeHostGxTevState program(std::size_t stages)
{
    MeleeHostGxTevState tev{};
    tev.stage_count = static_cast<mh_u8>(stages);
    tev.texcoord_gen_count = 1;
    tev.channel_count = 1;
    for (auto& stage : tev.stages) {
        stage.mode = MELEE_HOST_GX_TEV_MODE_CUSTOM;
        stage.texcoord = GX_TEXCOORD0;
        stage.texmap = GX_TEXMAP0;
        stage.color_channel = GX_COLOR0A0;
        for (auto& input : stage.color_input) {
            input = GX_CC_ZERO;
        }
        for (auto& input : stage.alpha_input) {
            input = GX_CA_ZERO;
        }
        stage.color_op = GX_TEV_ADD;
        stage.alpha_op = GX_TEV_ADD;
        stage.color_bias = GX_TB_ZERO;
        stage.alpha_bias = GX_TB_ZERO;
        stage.color_scale = GX_CS_SCALE_1;
        stage.alpha_scale = GX_CS_SCALE_1;
        stage.color_clamp = true;
        stage.alpha_clamp = true;
        stage.color_out_reg = GX_TEVPREV;
        stage.alpha_out_reg = GX_TEVPREV;
        stage.konst_color_select = GX_TEV_KCSEL_1;
        stage.konst_alpha_select = GX_TEV_KASEL_1;
        stage.raster_swap = GX_TEV_SWAP0;
        stage.texture_swap = GX_TEV_SWAP0;
    }
    return tev;
}

void set_color_in(MeleeHostGxTevStage& stage, mh_u32 a, mh_u32 b, mh_u32 c,
                  mh_u32 d)
{
    stage.color_input[0] = a;
    stage.color_input[1] = b;
    stage.color_input[2] = c;
    stage.color_input[3] = d;
}

void set_alpha_in(MeleeHostGxTevStage& stage, mh_u32 a, mh_u32 b, mh_u32 c,
                  mh_u32 d)
{
    stage.alpha_input[0] = a;
    stage.alpha_input[1] = b;
    stage.alpha_input[2] = c;
    stage.alpha_input[3] = d;
}

void set_register(MeleeHostGxTevState& tev, std::size_t reg, int r, int g,
                  int b, int a)
{
    tev.registers[reg][0] = static_cast<mh_s16>(r);
    tev.registers[reg][1] = static_cast<mh_s16>(g);
    tev.registers[reg][2] = static_cast<mh_s16>(b);
    tev.registers[reg][3] = static_cast<mh_s16>(a);
}

bool rgba(const std::array<int, 4>& value, int r, int g, int b, int a)
{
    return value[0] == r && value[1] == g && value[2] == b && value[3] == a;
}

} // namespace

TEST_CASE("a modulate stage multiplies with the hardware's lerp and rounding")
{
    // c stretches from 255 to 256 before the lerp, and an addition rounds by
    // adding 128 before the shift.  So 255 x 128 lands on 128, not 128.5
    // rounded either way, and 128 x 255 also lands on 128.
    MeleeHostGxTevState tev = program(1);
    set_color_in(tev.stages[0], GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC,
                 GX_CC_ZERO);
    set_alpha_in(tev.stages[0], GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA,
                 GX_CA_ZERO);
    TevFragmentInputs inputs{};
    inputs.texmap[0] = { 255, 128, 0, 255 };
    inputs.raster[0] = { 128, 255, 255, 64 };
    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 128, 128, 0, 64));
}

TEST_CASE("bias, scale, subtraction and clamping follow the hardware formula")
{
    MeleeHostGxTevState tev = program(1);
    set_color_in(tev.stages[0], GX_CC_ZERO, GX_CC_ONE, GX_CC_ONE, GX_CC_C0);
    set_alpha_in(tev.stages[0], GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    set_register(tev, GX_TEVREG0, 200, 100, 20, 77);
    const TevFragmentInputs inputs{};

    // d - lerp, with a subtraction rounding by 127: 200 - 255 clamps to zero.
    tev.stages[0].color_op = GX_TEV_SUB;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 0);
    // Without the clamp the register keeps -55, and the output takes the low
    // eight bits of it.
    tev.stages[0].color_clamp = false;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 201);

    // Halving shifts the whole sum and skips the rounding term; a negative
    // sum shifts arithmetically, so (100 - 255) / 2 is -78, not -77.
    tev.stages[0].color_scale = GX_CS_DIVIDE_2;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[1] == (-78 & 255));
    tev.stages[0].color_op = GX_TEV_ADD;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[1] == 177);

    // Bias is added to d before the scale multiplies it.
    set_color_in(tev.stages[0], GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    tev.stages[0].color_bias = GX_TB_ADDHALF;
    tev.stages[0].color_scale = GX_CS_SCALE_2;
    tev.stages[0].color_clamp = true;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[2] == 255);
    tev.stages[0].color_bias = GX_TB_SUBHALF;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[2] == 0);
    tev.stages[0].color_clamp = false;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[2] ==
            (((20 - 128) * 2) & 255));
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[3] == 77);
}

TEST_CASE("an unclamped register carries eleven signed bits into later stages")
{
    // Stage 0 writes 296 into REG1 without clamping; stage 1 reads it back as
    // d and only then clamps.  An 8-bit register would have wrapped to 40.
    MeleeHostGxTevState tev = program(2);
    set_color_in(tev.stages[0], GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    tev.stages[0].color_bias = GX_TB_ADDHALF;
    tev.stages[0].color_scale = GX_CS_SCALE_2;
    tev.stages[0].color_clamp = false;
    tev.stages[0].color_out_reg = GX_TEVREG1;
    set_color_in(tev.stages[1], GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C1);
    set_register(tev, GX_TEVREG0, 20, 20, 20, 0);
    const TevFragmentInputs inputs{};
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 255);

    // As a or b the same register is read through its low eight bits.
    set_color_in(tev.stages[1], GX_CC_ZERO, GX_CC_C1, GX_CC_ONE, GX_CC_ZERO);
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 40);
}

TEST_CASE("comparison stages add c only where the packed comparison holds")
{
    MeleeHostGxTevState tev = program(1);
    set_color_in(tev.stages[0], GX_CC_C0, GX_CC_C1, GX_CC_C2, GX_CC_ZERO);
    set_alpha_in(tev.stages[0], GX_CA_A0, GX_CA_A1, GX_CA_A2, GX_CA_ZERO);
    set_register(tev, GX_TEVREG0, 10, 2, 7, 77);
    set_register(tev, GX_TEVREG1, 5, 2, 9, 77);
    set_register(tev, GX_TEVREG2, 30, 40, 50, 33);
    const TevFragmentInputs inputs{};

    // R8 compares red alone and applies the result to every component.  The
    // alpha side is still an ordinary lerp of 77 toward 77.
    tev.stages[0].color_op = GX_TEV_COMP_R8_GT;
    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 30, 40, 50, 77));
    // GR16 packs green over red: 2:10 against 2:5.
    tev.stages[0].color_op = GX_TEV_COMP_GR16_EQ;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 0);
    tev.stages[0].color_op = GX_TEV_COMP_GR16_GT;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 30);
    // BGR24 puts blue on top, where 7 < 9 decides.
    tev.stages[0].color_op = GX_TEV_COMP_BGR24_GT;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[1] == 0);
    // RGB8 compares per component.
    tev.stages[0].color_op = GX_TEV_COMP_RGB8_GT;
    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 30, 0, 0, 77));
    tev.stages[0].color_op = GX_TEV_COMP_RGB8_EQ;
    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 0, 40, 0, 77));

    // A8 compares the alpha inputs; the packed forms on the alpha side read
    // the colour inputs instead.
    tev.stages[0].alpha_op = GX_TEV_COMP_A8_EQ;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[3] == 33);
    tev.stages[0].alpha_op = GX_TEV_COMP_BGR24_GT;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[3] == 0);
    tev.stages[0].alpha_op = GX_TEV_COMP_R8_GT;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[3] == 33);
}

TEST_CASE("konst selectors name fractions, whole colours or one component")
{
    MeleeHostGxTevState tev = program(1);
    set_color_in(tev.stages[0], GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
                 GX_CC_KONST);
    set_alpha_in(tev.stages[0], GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
                 GX_CA_KONST);
    tev.konst_colors[1][0] = 11;
    tev.konst_colors[1][1] = 22;
    tev.konst_colors[1][2] = 33;
    tev.konst_colors[2][3] = 99;
    const TevFragmentInputs inputs{};

    tev.stages[0].konst_color_select = GX_TEV_KCSEL_K1;
    tev.stages[0].konst_alpha_select = GX_TEV_KASEL_K2_A;
    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 11, 22, 33, 99));
    tev.stages[0].konst_color_select = GX_TEV_KCSEL_K1_G;
    tev.stages[0].konst_alpha_select = GX_TEV_KASEL_3_4;
    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 22, 22, 22, 191));
    tev.stages[0].konst_color_select = GX_TEV_KCSEL_1_8;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 32);
}

TEST_CASE("a stage writing a colour register feeds the stages after it")
{
    MeleeHostGxTevState tev = program(2);
    set_color_in(tev.stages[0], GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
                 GX_CC_TEXC);
    tev.stages[0].color_out_reg = GX_TEVREG0;
    set_alpha_in(tev.stages[0], GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
                 GX_CA_TEXA);
    set_color_in(tev.stages[1], GX_CC_ZERO, GX_CC_C0, GX_CC_RASC, GX_CC_ZERO);
    set_alpha_in(tev.stages[1], GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
                 GX_CA_APREV);
    // The value GXSetTevColor left in REG0 is overwritten for this fragment.
    set_register(tev, GX_TEVREG0, 1, 1, 1, 1);
    TevFragmentInputs inputs{};
    inputs.texmap[0] = { 200, 100, 50, 180 };
    inputs.raster[0] = { 255, 255, 255, 255 };
    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 200, 100, 50, 180));
}

TEST_CASE("missing textures, channels and swaps read what the hardware reads")
{
    MeleeHostGxTevState tev = program(1);
    set_color_in(tev.stages[0], GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
                 GX_CC_TEXC);
    set_alpha_in(tev.stages[0], GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
                 GX_CA_RASA);
    TevFragmentInputs inputs{};
    inputs.texmap[0] = { 10, 20, 30, 40 };
    inputs.raster[0] = { 1, 2, 3, 4 };
    inputs.raster[1] = { 5, 6, 7, 8 };

    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 10, 20, 30, 4));
    tev.stages[0].texture_swap = GX_TEV_SWAP1;
    REQUIRE(rgba(melee::gx::evaluate_tev(tev, inputs), 10, 10, 10, 4));
    tev.stages[0].texture_swap = GX_TEV_SWAP0;

    // A stage without a texture map reads white...
    tev.stages[0].texmap = GX_TEXMAP_NULL;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 255);
    // ...unless no coordinate is generated at all.
    tev.texcoord_gen_count = 0;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[0] == 0);

    tev.stages[0].color_channel = GX_COLOR1A1;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[3] == 8);
    tev.stages[0].color_channel = GX_COLOR_NULL;
    REQUIRE(melee::gx::evaluate_tev(tev, inputs)[3] == 0);
}

TEST_CASE("the alpha test evaluates both comparisons and their logic")
{
    MeleeHostGxDrawState state{};
    state.alpha_compare_0 = GX_GREATER;
    state.alpha_ref_0 = 0;
    state.alpha_op = GX_AOP_AND;
    state.alpha_compare_1 = GX_GREATER;
    state.alpha_ref_1 = 0;
    REQUIRE(!melee::gx::alpha_test_passes(state, 0));
    REQUIRE(melee::gx::alpha_test_passes(state, 1));

    state.alpha_compare_0 = GX_GEQUAL;
    state.alpha_ref_0 = 102;
    state.alpha_compare_1 = GX_LEQUAL;
    state.alpha_ref_1 = 255;
    REQUIRE(!melee::gx::alpha_test_passes(state, 101));
    REQUIRE(melee::gx::alpha_test_passes(state, 102));

    state.alpha_compare_0 = GX_LESS;
    state.alpha_ref_0 = 10;
    state.alpha_op = GX_AOP_OR;
    state.alpha_compare_1 = GX_GREATER;
    state.alpha_ref_1 = 200;
    REQUIRE(melee::gx::alpha_test_passes(state, 5));
    REQUIRE(!melee::gx::alpha_test_passes(state, 100));
    REQUIRE(melee::gx::alpha_test_passes(state, 250));

    state.alpha_compare_0 = GX_EQUAL;
    state.alpha_compare_1 = GX_ALWAYS;
    state.alpha_op = GX_AOP_XOR;
    REQUIRE(!melee::gx::alpha_test_passes(state, 10));
    REQUIRE(melee::gx::alpha_test_passes(state, 11));
    state.alpha_op = GX_AOP_XNOR;
    REQUIRE(melee::gx::alpha_test_passes(state, 10));
}

TEST_CASE("a bump coordinate is evaluated before the per-fragment model")
{
    MeleeHostGxTevState tev = program(2);
    REQUIRE(melee::gx::tev_unmodelled_features(tev) ==
            melee::gx::kTevUnmodelledNone);
    REQUIRE(melee::gx::describe_tev_unmodelled(0) == "none");

    tev.texcoord_gen_count = 2;
    tev.texcoord_gens[1].function = GX_TG_BUMP0;
    tev.stages[1].texcoord = GX_TEXCOORD1;
    REQUIRE(melee::gx::tev_unmodelled_features(tev) ==
            melee::gx::kTevUnmodelledNone);

    // A stage that does not sample does not care how the coordinate is made.
    tev.stages[1].texmap = GX_TEXMAP_NULL;
    REQUIRE(melee::gx::tev_unmodelled_features(tev) ==
            melee::gx::kTevUnmodelledNone);
}

TEST_CASE("a generated shader depends on the program, not on its constants")
{
    MeleeHostGxTevState first = program(2);
    set_color_in(first.stages[0], GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC,
                 GX_CC_ZERO);
    MeleeHostGxTevState second = first;
    set_register(second, GX_TEVREG0, 1, 2, 3, 4);
    second.konst_colors[3][2] = 200;
    const std::string source = melee::gx::tev_fragment_shader_source(first);
    REQUIRE(source == melee::gx::tev_fragment_shader_source(second));
    REQUIRE(source.find("#version 330 core") == 0);
    REQUIRE(source.find("// stage 1") != std::string::npos);
    REQUIRE(source.find("// stage 2") == std::string::npos);
    REQUIRE(source.find("discard") != std::string::npos);
    REQUIRE(source.find("u_texmap_format") != std::string::npos);
    REQUIRE(source.find("last_texel_raw") != std::string::npos);

    second.stages[1].color_op = GX_TEV_COMP_RGB8_GT;
    REQUIRE(source != melee::gx::tev_fragment_shader_source(second));
}

TEST_CASE("a generated shader applies the captured indirect texture setup")
{
    MeleeHostGxTevState tev = program(1);
    tev.texcoord_gen_count = 2;
    tev.stages[0].texcoord = GX_TEXCOORD1;
    tev.stages[0].texmap = GX_TEXMAP1;

    MeleeHostGxIndirectState indirect{};
    indirect.stage_count = 1;
    indirect.stages[0].texcoord = GX_TEXCOORD0;
    indirect.stages[0].texmap = GX_TEXMAP0;
    indirect.stages[0].scale_s = GX_ITS_1;
    indirect.stages[0].scale_t = GX_ITS_1;
    indirect.tev_stages[0].indirect = true;
    indirect.tev_stages[0].ind_stage = GX_INDTEXSTAGE0;
    indirect.tev_stages[0].format = GX_ITF_8;
    indirect.tev_stages[0].bias = GX_ITB_ST;
    indirect.tev_stages[0].matrix = GX_ITM_0;
    indirect.tev_stages[0].wrap_s = GX_ITW_OFF;
    indirect.tev_stages[0].wrap_t = GX_ITW_OFF;

    const std::string source =
        melee::gx::tev_fragment_shader_source(tev, indirect);
    REQUIRE(source.find("u_indirect_matrix[0]") != std::string::npos);
    REQUIRE(source.find("u_indirect_matrix_scale[0]") !=
            std::string::npos);
    REQUIRE(source.find("indirect_texel.r -= 128") != std::string::npos);
    REQUIRE(source.find("indirect_texel.g -= 128") != std::string::npos);
    REQUIRE(source.find("vec3 tev_coord") != std::string::npos);

    // Matrix entries are uniforms, so animated refraction offsets reuse the
    // linked program.  The texture-stage structure remains the cache key.
    MeleeHostGxIndirectState changed_matrix = indirect;
    changed_matrix.matrices[0].offset[0][0] = -0.5F;
    changed_matrix.matrices[0].scale_exp = 1;
    REQUIRE(source == melee::gx::tev_fragment_shader_source(tev,
                                                              changed_matrix));
    changed_matrix.tev_stages[0].wrap_s = GX_ITW_64;
    REQUIRE(source != melee::gx::tev_fragment_shader_source(tev,
                                                              changed_matrix));
}

TEST_CASE("indirect texture offsets match the refraction matrix arithmetic")
{
    MeleeHostGxIndirectState indirect{};
    indirect.stage_count = 1;
    indirect.tev_stages[0].indirect = true;
    indirect.tev_stages[0].ind_stage = GX_INDTEXSTAGE0;
    indirect.tev_stages[0].format = GX_ITF_8;
    indirect.tev_stages[0].bias = GX_ITB_ST;
    indirect.tev_stages[0].matrix = GX_ITM_0;
    indirect.matrices[0].offset[0][0] = -0.5F;
    indirect.matrices[0].offset[1][1] = -0.5F;
    indirect.matrices[0].scale_exp = 1;

    // This is lbrefract.c's texture_offset matrix.  A full positive S sample
    // shifts the copied EFB almost half a normalized texture left; a zero T
    // sample shifts it half a texture down after the ST bias.
    const auto offset = melee::gx::indirect_texture_offset(
        indirect, 0, { 255, 0, 42, 255 });
    REQUIRE(close(offset[0], -127.0F / 256.0F));
    REQUIRE(close(offset[1], 0.5F));

    indirect.tev_stages[0].format = GX_ITF_5;
    indirect.tev_stages[0].bias = GX_ITB_NONE;
    indirect.tev_stages[0].matrix = GX_ITM_OFF;
    const auto disabled = melee::gx::indirect_texture_offset(
        indirect, 0, { 248, 120, 0, 0 });
    const std::array<float, 2> no_offset{};
    REQUIRE(disabled == no_offset);

    indirect.tev_stages[0].matrix = GX_ITM_S0;
    indirect.matrices[0].scale_exp = 1;
    const auto s_matrix = melee::gx::indirect_texture_offset(
        indirect, 0, { 248, 120, 0, 0 }, { 0.5F, 0.25F });
    REQUIRE(close(s_matrix[0], 31.0F / 256.0F));
    REQUIRE(close(s_matrix[1], 31.0F / 512.0F));
    indirect.tev_stages[0].matrix = GX_ITM_T0;
    const auto t_matrix = melee::gx::indirect_texture_offset(
        indirect, 0, { 248, 120, 0, 0 }, { 0.5F, 0.25F });
    REQUIRE(close(t_matrix[0], 15.0F / 256.0F));
    REQUIRE(close(t_matrix[1], 15.0F / 512.0F));
    REQUIRE(melee::gx::indirect_texture_offset(indirect, 1, { 1, 2, 3, 4 })
            == no_offset);
}

TEST_CASE("indirect shaders honor alpha bump, S/T matrices and original LOD")
{
    MeleeHostGxTevState tev = program(1);
    tev.texcoord_gen_count = 2;
    tev.stages[0].texcoord = GX_TEXCOORD1;
    tev.stages[0].texmap = GX_TEXMAP1;
    tev.stages[0].color_channel = GX_ALPHA_BUMPN;

    MeleeHostGxIndirectState indirect{};
    indirect.stage_count = 1;
    indirect.stages[0].texcoord = GX_TEXCOORD0;
    indirect.stages[0].texmap = GX_TEXMAP0;
    indirect.tev_stages[0].indirect = true;
    indirect.tev_stages[0].ind_stage = GX_INDTEXSTAGE0;
    indirect.tev_stages[0].format = GX_ITF_5;
    indirect.tev_stages[0].matrix = GX_ITM_S0;
    indirect.tev_stages[0].unmodified_lod = true;
    indirect.tev_stages[0].alpha_select = GX_ITBA_T;

    const std::string source =
        melee::gx::tev_fragment_shader_source(tev, indirect);
    REQUIRE(source.find("float(indirect_texel.r)") != std::string::npos);
    REQUIRE(source.find("alpha_bump = (indirect_texel.g & 31) << 3") !=
            std::string::npos);
    REQUIRE(source.find("alpha_bump | (alpha_bump >> 5)") !=
            std::string::npos);
    REQUIRE(source.find("sample_texmap_lod(1, tev_coord, direct_coord)") !=
            std::string::npos);
    REQUIRE(source.find("textureGrad") != std::string::npos);
}

/* The GL conformance run only compiles the programs a capture happened to
 * draw, and it needs a context, so it is a diagnostic rather than a test.
 * Nothing else here reads the emitted GLSL as a language: a truncated
 * expression still contains every substring the cases above look for, and it
 * would only fail when a refraction draw reached a real driver.  Balanced
 * delimiters are the cheap property that catches an unfinished emission. */
TEST_CASE("every generated shader emits balanced GLSL delimiters")
{
    const auto balanced = [](const std::string& source) {
        int parentheses = 0;
        int braces = 0;
        int brackets = 0;
        for (const char character : source) {
            parentheses += character == '(';
            parentheses -= character == ')';
            braces += character == '{';
            braces -= character == '}';
            brackets += character == '[';
            brackets -= character == ']';
            if (parentheses < 0 || braces < 0 || brackets < 0) {
                return false;
            }
        }
        return parentheses == 0 && braces == 0 && brackets == 0;
    };

    REQUIRE(balanced(melee::gx::tev_vertex_shader_source()));

    MeleeHostGxTevState tev = program(2);
    tev.texcoord_gen_count = 2;
    tev.stages[0].texcoord = GX_TEXCOORD1;
    tev.stages[0].texmap = GX_TEXMAP1;
    REQUIRE(balanced(melee::gx::tev_fragment_shader_source(tev)));

    /* Every indirect knob that steers a distinct emission branch: the matrix
     * id picks regular, S/T or disabled arithmetic, the format sets the texel
     * shift, the bias picks which channels are centred, and the wrap ids
     * select pass-through, a period or a forced zero. */
    for (const mh_u32 matrix :
         { (mh_u32) GX_ITM_OFF, (mh_u32) GX_ITM_0, (mh_u32) GX_ITM_1,
           (mh_u32) GX_ITM_2, (mh_u32) GX_ITM_S0,
           (mh_u32) GX_ITM_T0 }) {
        for (const mh_u32 format :
             { (mh_u32) GX_ITF_8, (mh_u32) GX_ITF_5, (mh_u32) GX_ITF_4,
               (mh_u32) GX_ITF_3 }) {
            for (mh_u32 bias = GX_ITB_NONE; bias <= GX_ITB_STU; ++bias) {
                for (mh_u32 wrap = GX_ITW_OFF; wrap <= GX_ITW_0; ++wrap) {
                    MeleeHostGxIndirectState indirect{};
                    indirect.stage_count = 1;
                    indirect.stages[0].texcoord = GX_TEXCOORD0;
                    indirect.stages[0].texmap = GX_TEXMAP0;
                    indirect.stages[0].scale_s = GX_ITS_2;
                    indirect.stages[0].scale_t = GX_ITS_4;
                    indirect.tev_stages[0].indirect = true;
                    indirect.tev_stages[0].ind_stage = GX_INDTEXSTAGE0;
                    indirect.tev_stages[0].format = format;
                    indirect.tev_stages[0].bias = bias;
                    indirect.tev_stages[0].matrix = matrix;
                    indirect.tev_stages[0].wrap_s = wrap;
                    indirect.tev_stages[0].wrap_t = wrap;
                    indirect.tev_stages[0].add_previous = wrap % 2 == 0;
                    REQUIRE(balanced(melee::gx::tev_fragment_shader_source(
                        tev, indirect)));
                }
            }
        }
    }
}

TEST_CASE("fog mixes by the curve its type names over the normalised depth")
{
    MeleeHostGxDrawState state{};
    state.fog_start_z = 100.0F;
    state.fog_end_z = 300.0F;

    // No fog, and a range of zero, leave the colour alone.
    state.fog_type = GX_FOG_NONE;
    REQUIRE(melee::gx::fog_blend(state, 200.0F) == 0.0F);
    state.fog_type = GX_FOG_LIN;
    state.fog_end_z = 100.0F;
    REQUIRE(melee::gx::fog_blend(state, 200.0F) == 0.0F);
    state.fog_end_z = 300.0F;

    // Linear fog is the depth's place between start and end, clamped at both.
    REQUIRE(melee::gx::fog_blend(state, 50.0F) == 0.0F);
    REQUIRE(melee::gx::fog_blend(state, 100.0F) == 0.0F);
    REQUIRE(close(melee::gx::fog_blend(state, 150.0F), 0.25F));
    REQUIRE(close(melee::gx::fog_blend(state, 200.0F), 0.5F));
    REQUIRE(melee::gx::fog_blend(state, 300.0F) == 1.0F);
    REQUIRE(melee::gx::fog_blend(state, 1000.0F) == 1.0F);

    // The exponential curves are 2^-8t and its square, and the reverse ones
    // the same curves read from the far end.
    state.fog_type = GX_FOG_EXP;
    REQUIRE(close(melee::gx::fog_blend(state, 200.0F), 1.0F - 0.0625F));
    state.fog_type = GX_FOG_EXP2;
    REQUIRE(close(melee::gx::fog_blend(state, 200.0F), 1.0F - 0.25F));
    state.fog_type = GX_FOG_REVEXP;
    REQUIRE(close(melee::gx::fog_blend(state, 200.0F), 0.0625F));
    REQUIRE(close(melee::gx::fog_blend(state, 300.0F), 1.0F));
    state.fog_type = GX_FOG_REVEXP2;
    REQUIRE(close(melee::gx::fog_blend(state, 200.0F), 0.25F));

    // The shader carries the same curve and reads the fragment's own depth.
    const std::string source =
        melee::gx::tev_fragment_shader_source(program(1));
    REQUIRE(source.find("u_fog_type") != std::string::npos);
    REQUIRE(source.find("gl_FragCoord.w") != std::string::npos);
    REQUIRE(source.find("u_fog_color.rgb * fog") != std::string::npos);

    // The weight and the mix are integers, so the shader and the rasterizer
    // round alike: half of 200 and 100 is 150, and a weight of zero leaves
    // the component alone.
    state.fog_type = GX_FOG_LIN;
    REQUIRE(melee::gx::fog_weight(state, 200.0F) == 128);
    REQUIRE(melee::gx::fog_weight(state, 50.0F) == 0);
    REQUIRE(melee::gx::fog_weight(state, 400.0F) == 256);
    REQUIRE(melee::gx::fog_mix(200, 100, 128) == 150);
    REQUIRE(melee::gx::fog_mix(200, 100, 0) == 200);
    REQUIRE(melee::gx::fog_mix(200, 100, 256) == 100);
}

TEST_CASE("GXSetTevOp records the inputs its SDK expansion writes")
{
    melee_host_gx_state_reset();
    MeleeHostGxTevState tev{};
    melee_host_gx_tev_state(&tev);
    // GXInit leaves stage 0 replacing with texture map 0.
    REQUIRE(tev.stages[0].color_input[3] == GX_CC_TEXC);
    REQUIRE(tev.stages[0].alpha_input[3] == GX_CA_TEXA);
    REQUIRE(tev.texcoord_gen_count == 1);
    REQUIRE(tev.texcoord_gens[2].source == GX_TG_TEX2);
    REQUIRE(tev.texcoord_gens[2].post_matrix == GX_PTIDENTITY);
    REQUIRE(tev.stages[3].texmap == GX_TEXMAP3);
    REQUIRE(tev.stages[9].texmap == GX_TEXMAP_NULL);

    GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GXSetTevOp(GX_TEVSTAGE1, GX_MODULATE);
    melee_host_gx_tev_state(&tev);
    REQUIRE(tev.stages[0].mode == GX_MODULATE);
    // The first stage modulates by the rasterized colour, later ones by the
    // previous stage's result.
    REQUIRE(tev.stages[0].color_input[2] == GX_CC_RASC);
    REQUIRE(tev.stages[1].color_input[1] == GX_CC_TEXC);
    REQUIRE(tev.stages[1].color_input[2] == GX_CC_CPREV);
    REQUIRE(tev.stages[1].alpha_input[2] == GX_CA_APREV);
    REQUIRE(tev.stages[1].color_out_reg == GX_TEVPREV);
}
