#ifndef MELEE_HOST_GX_TEV_HPP
#define MELEE_HOST_GX_TEV_HPP

#include <melee_host/gx.h>

#include <array>
#include <cstdint>
#include <string>

namespace melee::gx {

/* What reaches the TEV for one fragment: the two rasterized colours and the
 * texel each texture map yields there, as 8-bit components.  Sampling is not
 * part of this model; the caller has already filtered the textures. */
struct TevFragmentInputs {
    std::array<std::array<int, 4>, 2> raster{};
    std::array<std::array<int, 4>, MELEE_HOST_GX_MAX_TEXMAP> texmap{};
};

/* Runs a captured TEV program on one fragment the way the hardware combines
 * it: integer arithmetic on 8-bit inputs and 11-bit signed registers, the
 * lerp that stretches c from 255 to 256, the per-operation rounding, and the
 * comparison modes.  Returns the RGBA the fragment leaves the last stage
 * with, before the alpha test.
 *
 * This is the reference the shader generator below is held to.  The formulas
 * follow the software rasterizer in Dolphin, which was derived from hardware
 * tests; nothing here is taken from its code. */
std::array<int, 4> evaluate_tev(const MeleeHostGxTevState& tev,
                                const TevFragmentInputs& inputs);

/* The normalized s/t displacement an indirect TEV stage applies after its
 * indirect texture was sampled as 8-bit RGBA. `direct_coord` supplies the
 * unmodified normalized coordinate needed by GX_ITM_Sn/GX_ITM_Tn. This is the
 * numeric reference for the matching GLSL emission; a stage that is direct,
 * disabled or names an invalid input returns zero. */
std::array<float, 2> indirect_texture_offset(
    const MeleeHostGxIndirectState& indirect, std::size_t tev_stage,
    const std::array<int, 4>& texel,
    const std::array<float, 2>& direct_coord = {});

/* Both alpha comparisons and the logic that combines them, against the alpha
 * the TEV produced.  No reduction: every GXAlphaOp is evaluated as written. */
bool alpha_test_passes(const MeleeHostGxDrawState& state, int alpha);

/* How much of the fog colour the hardware mixes into a fragment at that
 * eye-space depth, which under a perspective projection is the clip w the
 * divide already carries.  GXSetFog's start and end normalise the depth, and
 * the type picks the curve; GX_FOG_NONE gives zero.  The shader below applies
 * the same formula. */
float fog_blend(const MeleeHostGxDrawState& state, float eye_z);

/* That fraction as the 0..256 weight both the rasterizer and the shader mix
 * with: `(colour * (256 - w) + fog * w + 128) >> 8`.  Keeping the mix in
 * integers is what makes the two paths round the same way. */
int fog_weight(const MeleeHostGxDrawState& state, float eye_z);
/* One colour component through that mix. */
int fog_mix(int component, int fog_component, int weight);

/* Parts of a program the per-fragment path does not reproduce.  A program
 * with none of these is evaluated exactly, up to texture filtering. */
enum TevUnmodelled : std::uint32_t {
    kTevUnmodelledNone = 0,
    /* An input selector the hardware does not define. */
    kTevUnmodelledArgument = 1U << 0,
};

std::uint32_t tev_unmodelled_features(const MeleeHostGxTevState& tev);
std::string describe_tev_unmodelled(std::uint32_t features);

/* GLSL implementing the same program.  The structure of a program becomes
 * code and its constants become uniforms, so two draws that differ only in
 * register or konst values share one shader, and the source text itself is a
 * sufficient cache key.
 *
 * Contract with the consumer:
 *   attributes  0 a_position vec3, 1 a_color0 vec4, 2 a_color1 vec4,
 *               3..10 a_texgen vec3[8] (s, t, q after texgen)
 *   uniforms    u_mvp mat4; u_texmap sampler2D[8];
 *               u_register ivec4[4] (index 1..3 are C0..C2);
 *               u_konst ivec4[4];
 *               u_alpha_test ivec4 (comp0, ref0, op, comp1);
 *               u_alpha_ref1 int;
 *               u_fog_type int, u_fog_range vec2 (start, end) and
 *               u_fog_color vec4, which the fragment's own depth reads;
 *               u_texmap_format int[8], used to reconstruct Z8, Z16 and
 *               Z24X8 depth textures; u_indirect_matrix vec3[6] and
 *               u_indirect_matrix_scale int[3] for GX indirect matrices
 *   output      frag_color, the final register over 255; fragments failing
 *               the alpha test are discarded. */
std::string tev_vertex_shader_source();
std::string tev_fragment_shader_source(
    const MeleeHostGxTevState& tev,
    const MeleeHostGxIndirectState& indirect = {});

} // namespace melee::gx

#endif
