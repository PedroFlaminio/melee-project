#include "hsd_include.hpp"
#include "test.hpp"

#include <melee_host/gx.h>

MELEE_HOST_TEST_HSD_BEGIN
#include <dolphin/gx/GXCull.h>
#include <dolphin/gx/GXCommandList.h>
#include <dolphin/gx/GXVert.h>
#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXGeometry.h>
#include <dolphin/gx/GXGet.h>
#include <dolphin/gx/GXManage.h>
#include <dolphin/gx/GXFrameBuffer.h>
#include <dolphin/gx/GXLighting.h>
#include <dolphin/gx/GXPixel.h>
#include <dolphin/gx/GXStruct.h>
#include <dolphin/gx/GXTev.h>
#include <dolphin/gx/GXBump.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTransform.h>
#include <dolphin/vi/vitypes.h>
#include <dolphin/mtx.h>
MELEE_HOST_TEST_HSD_END

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

bool near(f32 value, f32 expected, f32 tolerance = 1.0e-5F)
{
    return std::fabs(value - expected) <= tolerance;
}

} // namespace

TEST_CASE("GX state resets to the pipeline defaults the SDK installs")
{
    melee_host_gx_state_reset();

    MeleeHostGxPixelState pixel{};
    melee_host_gx_pixel_state(&pixel);
    REQUIRE(pixel.z_compare_enable);
    REQUIRE(pixel.z_update_enable);
    REQUIRE(pixel.z_func == GX_LEQUAL);
    REQUIRE(pixel.color_update_enable);
    REQUIRE(pixel.cull_mode == GX_CULL_BACK);
    REQUIRE(pixel.pixel_sync_count == 0);

    MeleeHostGxTevState tev{};
    melee_host_gx_tev_state(&tev);
    REQUIRE(tev.stage_count == 1);
    // GXInit orders the first eight stages onto their own map and coordinate,
    // generates one coordinate and leaves the rest unordered.
    REQUIRE(tev.stages[0].texmap == GX_TEXMAP0);
    REQUIRE(tev.stages[7].texcoord == GX_TEXCOORD7);
    REQUIRE(tev.stages[8].texmap == GX_TEXMAP_NULL);
    REQUIRE(tev.texcoord_gen_count == 1);
    REQUIRE(tev.channel_count == 0);
}

TEST_CASE("pixel engine calls land in the host pipeline state")
{
    melee_host_gx_state_reset();

    GXSetZMode(GX_FALSE, GX_GREATER, GX_TRUE);
    GXSetZCompLoc(GX_FALSE);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
                   GX_LO_SET);
    GXSetColorUpdate(GX_FALSE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetAlphaCompare(GX_GEQUAL, 128, GX_AOP_OR, GX_LESS, 64);
    GXSetCullMode(GX_CULL_FRONT);
    GXSetScissor(4, 8, 320, 240);
    GXPixModeSync();
    GXPixModeSync();

    MeleeHostGxPixelState pixel{};
    melee_host_gx_pixel_state(&pixel);
    REQUIRE(!pixel.z_compare_enable);
    REQUIRE(pixel.z_update_enable);
    REQUIRE(pixel.z_func == GX_GREATER);
    REQUIRE(!pixel.z_before_texture);
    REQUIRE(pixel.blend_mode == GX_BM_BLEND);
    REQUIRE(pixel.blend_src_factor == GX_BL_SRCALPHA);
    REQUIRE(pixel.blend_dst_factor == GX_BL_INVSRCALPHA);
    REQUIRE(pixel.blend_logic_op == GX_LO_SET);
    REQUIRE(!pixel.color_update_enable);
    REQUIRE(!pixel.alpha_update_enable);
    REQUIRE(pixel.alpha_compare_0 == GX_GEQUAL);
    REQUIRE(pixel.alpha_ref_0 == 128);
    REQUIRE(pixel.alpha_op == GX_AOP_OR);
    REQUIRE(pixel.alpha_compare_1 == GX_LESS);
    REQUIRE(pixel.alpha_ref_1 == 64);
    REQUIRE(pixel.cull_mode == GX_CULL_FRONT);
    REQUIRE(pixel.scissor_left == 4);
    REQUIRE(pixel.scissor_top == 8);
    REQUIRE(pixel.scissor_width == 320);
    REQUIRE(pixel.scissor_height == 240);
    REQUIRE(pixel.pixel_sync_count == 2);
}

TEST_CASE("viewport and projection round-trip through the GX getters")
{
    melee_host_gx_state_reset();

    GXSetViewport(1.0F, 2.0F, 640.0F, 480.0F, 0.0F, 1.0F);
    std::array<f32, 6> viewport{};
    GXGetViewportv(viewport.data());
    REQUIRE(near(viewport[0], 1.0F));
    REQUIRE(near(viewport[2], 640.0F));
    REQUIRE(near(viewport[5], 1.0F));

    MeleeHostGxTransformState transform{};
    melee_host_gx_transform_state(&transform);
    // GXSetViewport is the jitter form with both fields enabled.
    REQUIRE(transform.viewport_field == 1);
    GXSetViewportJitter(0.0F, 0.0F, 640.0F, 480.0F, 0.0F, 1.0F, 0);
    melee_host_gx_transform_state(&transform);
    REQUIRE(transform.viewport_field == 0);

    Mtx44 projection;
    MTXPerspective(projection, 60.0F, 4.0F / 3.0F, 1.0F, 100.0F);
    GXSetProjection(projection, GX_PERSPECTIVE);

    std::array<f32, 7> readback{};
    GXGetProjectionv(readback.data());
    REQUIRE(readback[0] == static_cast<f32>(GX_PERSPECTIVE));
    REQUIRE(near(readback[1], projection[0][0]));
    REQUIRE(near(readback[2], projection[0][2]));
    REQUIRE(near(readback[3], projection[1][1]));
    REQUIRE(near(readback[4], projection[1][2]));
    REQUIRE(near(readback[5], projection[2][2]));
    REQUIRE(near(readback[6], projection[2][3]));

    // The orthographic form reports the translation column instead.
    Mtx44 ortho;
    MTXOrtho(ortho, 0.0F, -480.0F, 0.0F, 640.0F, 0.0F, 2.0F);
    GXSetProjection(ortho, GX_ORTHOGRAPHIC);
    GXGetProjectionv(readback.data());
    REQUIRE(readback[0] == static_cast<f32>(GX_ORTHOGRAPHIC));
    REQUIRE(near(readback[2], ortho[0][3]));
    REQUIRE(near(readback[4], ortho[1][3]));
}

TEST_CASE("matrix memory keeps each loaded matrix in its own rows")
{
    melee_host_gx_state_reset();

    Mtx first;
    Mtx second;
    PSMTXTrans(first, 1.0F, 2.0F, 3.0F);
    PSMTXTrans(second, -4.0F, -5.0F, -6.0F);
    GXLoadPosMtxImm(first, GX_PNMTX0);
    GXLoadPosMtxImm(second, GX_PNMTX1);
    GXSetCurrentMtx(GX_PNMTX1);

    MeleeHostGxAffineTransform loaded{};
    REQUIRE(melee_host_gx_matrix(GX_PNMTX0, &loaded));
    REQUIRE(near(loaded.values[0][3], 1.0F));
    REQUIRE(near(loaded.values[2][3], 3.0F));
    REQUIRE(melee_host_gx_matrix(GX_PNMTX1, &loaded));
    REQUIRE(near(loaded.values[0][3], -4.0F));
    REQUIRE(near(loaded.values[2][3], -6.0F));

    MeleeHostGxTransformState transform{};
    melee_host_gx_transform_state(&transform);
    REQUIRE(transform.current_matrix == GX_PNMTX1);

    // A row id that would spill past matrix memory is refused, not clamped.
    REQUIRE(!melee_host_gx_matrix(MELEE_HOST_GX_MATRIX_ROWS - 1, &loaded));
    GXLoadPosMtxImm(first, MELEE_HOST_GX_MATRIX_ROWS - 1);
    REQUIRE(melee_host_gx_matrix(GX_PNMTX0, &loaded));
    REQUIRE(near(loaded.values[0][3], 1.0F));
}

TEST_CASE("TEV stage and texgen configuration is recorded per slot")
{
    melee_host_gx_state_reset();

    GXSetNumTevStages(2);
    GXSetNumTexGens(1);
    GXSetNumChans(1);
    GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE1, GX_PASSCLR);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
                  GX_COLOR_NULL);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0,
                      GX_TRUE, GX_PTIDENTITY);

    MeleeHostGxTevState tev{};
    melee_host_gx_tev_state(&tev);
    REQUIRE(tev.stage_count == 2);
    REQUIRE(tev.texcoord_gen_count == 1);
    REQUIRE(tev.channel_count == 1);
    REQUIRE(tev.stages[0].mode == GX_MODULATE);
    REQUIRE(tev.stages[0].texcoord == GX_TEXCOORD0);
    REQUIRE(tev.stages[0].texmap == GX_TEXMAP0);
    REQUIRE(tev.stages[0].color_channel == GX_COLOR0A0);
    REQUIRE(tev.stages[1].mode == GX_PASSCLR);
    REQUIRE(tev.stages[1].texmap == GX_TEXMAP_NULL);
    REQUIRE(tev.texcoord_gens[0].function == GX_TG_MTX2x4);
    REQUIRE(tev.texcoord_gens[0].source == GX_TG_TEX0);
    REQUIRE(tev.texcoord_gens[0].matrix == GX_TEXMTX0);
    REQUIRE(tev.texcoord_gens[0].normalize);

    // GXInit's swap tables are what the TEV evaluator models, so installing
    // them again is accepted; the sprite library does it for SWAP0.
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE,
                          GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED,
                          GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN,
                          GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE,
                          GX_CH_ALPHA);
}

TEST_CASE("indirect texture configuration round-trips through host GX state")
{
    melee_host_gx_state_reset();

    f32 matrix[2][3] = {
        { 0.25F, -0.5F, 0.75F },
        { 1.0F, 0.125F, -0.25F },
    };
    GXSetNumIndStages(1);
    GXSetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD3, GX_TEXMAP2);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_4, GX_ITS_8);
    GXSetIndTexMtx(GX_ITM_1, matrix, -2);
    GXSetTevIndirect(GX_TEVSTAGE2, GX_INDTEXSTAGE0, GX_ITF_5, GX_ITB_ST,
                     GX_ITM_1, GX_ITW_64, GX_ITW_16, GX_TRUE, GX_FALSE,
                     GX_ITBA_U);

    MeleeHostGxIndirectState indirect{};
    melee_host_gx_indirect_state(&indirect);
    REQUIRE(indirect.stage_count == 1);
    REQUIRE(indirect.stages[0].texcoord == GX_TEXCOORD3);
    REQUIRE(indirect.stages[0].texmap == GX_TEXMAP2);
    REQUIRE(indirect.stages[0].scale_s == GX_ITS_4);
    REQUIRE(indirect.stages[0].scale_t == GX_ITS_8);
    REQUIRE(near(indirect.matrices[1].offset[0][0], 0.25F));
    REQUIRE(near(indirect.matrices[1].offset[0][1], -0.5F));
    REQUIRE(near(indirect.matrices[1].offset[1][2], -0.25F));
    REQUIRE(indirect.matrices[1].scale_exp == -2);
    REQUIRE(indirect.tev_stages[2].indirect);
    REQUIRE(indirect.tev_stages[2].ind_stage == GX_INDTEXSTAGE0);
    REQUIRE(indirect.tev_stages[2].format == GX_ITF_5);
    REQUIRE(indirect.tev_stages[2].bias == GX_ITB_ST);
    REQUIRE(indirect.tev_stages[2].matrix == GX_ITM_1);
    REQUIRE(indirect.tev_stages[2].wrap_s == GX_ITW_64);
    REQUIRE(indirect.tev_stages[2].wrap_t == GX_ITW_16);
    REQUIRE(indirect.tev_stages[2].add_previous);
    REQUIRE(!indirect.tev_stages[2].unmodified_lod);
    REQUIRE(indirect.tev_stages[2].alpha_select == GX_ITBA_U);

    GXSetTevDirect(GX_TEVSTAGE2);
    melee_host_gx_indirect_state(&indirect);
    REQUIRE(!indirect.tev_stages[2].indirect);
}

TEST_CASE("texture objects survive as 64-bit pointers inside the SDK blob")
{
    melee_host_gx_state_reset();

    std::array<std::uint8_t, 64> image{};
    GXTexObj texture;
    GXInitTexObj(&texture, image.data(), 32, 16, GX_TF_RGB5A3, GX_CLAMP,
                 GX_REPEAT, GX_FALSE);
    GXInitTexObjLOD(&texture, GX_LINEAR, GX_NEAR, 0.5F, 3.25F, -1.5F,
                    GX_TRUE, GX_FALSE, GX_ANISO_4);
    GXLoadTexObj(&texture, GX_TEXMAP2);

    MeleeHostGxTextureDesc desc{};
    REQUIRE(melee_host_gx_bound_texture(GX_TEXMAP2, &desc));
    REQUIRE(desc.width == 32);
    REQUIRE(desc.height == 16);
    REQUIRE(desc.format == GX_TF_RGB5A3);
    REQUIRE(desc.wrap_s == GX_CLAMP);
    REQUIRE(desc.wrap_t == GX_REPEAT);
    REQUIRE(!desc.mipmap);
    REQUIRE(!desc.color_indexed);
    REQUIRE(desc.min_filter == GX_LINEAR);
    REQUIRE(desc.mag_filter == GX_NEAR);
    REQUIRE(near(desc.min_lod, 0.5F, 1.0e-3F));
    REQUIRE(near(desc.max_lod, 3.25F, 1.0e-3F));
    REQUIRE(near(desc.lod_bias, -1.5F));
    REQUIRE(desc.max_anisotropy == GX_ANISO_4);
    REQUIRE(desc.bias_clamp);
    REQUIRE(!desc.edge_lod);
    // The SDK's getters read the same blob back.
    REQUIRE(GXGetTexObjWidth(&texture) == 32);
    REQUIRE(GXGetTexObjHeight(&texture) == 16);
    REQUIRE(GXGetTexObjFmt(&texture) == GX_TF_RGB5A3);
    // The heap pointer must come back whole, not truncated to 32 bits.
    REQUIRE(desc.image == image.data());

    MeleeHostGxTextureDesc unbound{};
    REQUIRE(!melee_host_gx_bound_texture(GX_TEXMAP5, &unbound));
    REQUIRE(!melee_host_gx_bound_texture(MELEE_HOST_GX_MAX_TEXMAP, &unbound));
}

TEST_CASE("palettized textures carry their TLUT to the bound texture map")
{
    melee_host_gx_state_reset();

    std::array<std::uint8_t, 32> image{};
    std::array<std::uint16_t, 16> palette{};
    GXTlutObj tlut;
    GXInitTlutObj(&tlut, palette.data(), GX_TL_RGB5A3, 16);
    GXLoadTlut(&tlut, GX_TLUT1);

    GXTexObj texture;
    GXInitTexObjCI(&texture, image.data(), 8, 8, GX_TF_C4, GX_REPEAT,
                   GX_REPEAT, GX_FALSE, GX_TLUT1);
    GXLoadTexObj(&texture, GX_TEXMAP0);

    MeleeHostGxTextureDesc desc{};
    REQUIRE(melee_host_gx_bound_texture(GX_TEXMAP0, &desc));
    REQUIRE(desc.color_indexed);
    REQUIRE(desc.tlut_name == GX_TLUT1);

    MeleeHostGxTlutDesc tlut_desc{};
    REQUIRE(melee_host_gx_loaded_tlut(GX_TLUT1, &tlut_desc));
    REQUIRE(tlut_desc.format == GX_TL_RGB5A3);
    REQUIRE(tlut_desc.entry_count == 16);
    REQUIRE(tlut_desc.entries == palette.data());
    REQUIRE(!melee_host_gx_loaded_tlut(GX_TLUT0, &tlut_desc));
}

TEST_CASE("light objects load into the slot their one-hot id selects")
{
    melee_host_gx_state_reset();

    GXLightObj light;
    const GXColor color{ 200, 150, 100, 255 };
    GXInitLightColor(&light, color);
    GXInitLightPos(&light, 10.0F, 20.0F, 30.0F);
    GXInitLightDir(&light, 0.0F, -1.0F, 0.0F);
    GXInitLightAttn(&light, 1.0F, 0.0F, 0.0F, 1.0F, 0.5F, 0.25F);
    GXLoadLightObjImm(&light, GX_LIGHT3);

    MeleeHostGxLightDesc desc{};
    REQUIRE(melee_host_gx_light(3, &desc));
    REQUIRE(desc.color[0] == 200);
    REQUIRE(desc.color[3] == 255);
    REQUIRE(near(desc.position[1], 20.0F));
    // GX stores the direction negated, the way the hardware consumes it.
    REQUIRE(near(desc.direction[1], 1.0F));
    REQUIRE(near(desc.distance_attenuation[1], 0.5F));
    REQUIRE(!melee_host_gx_light(0, &desc));

    // Distance attenuation derives its coefficients from the SDK curve.
    GXInitLightDistAttn(&light, 4.0F, 0.5F, GX_DA_GENTLE);
    GXLoadLightObjImm(&light, GX_LIGHT0);
    REQUIRE(melee_host_gx_light(0, &desc));
    REQUIRE(near(desc.distance_attenuation[0], 1.0F));
    REQUIRE(near(desc.distance_attenuation[1], 0.25F));
    REQUIRE(near(desc.distance_attenuation[2], 0.0F));
}

TEST_CASE("channel control records the lighting configuration per channel")
{
    melee_host_gx_state_reset();

    GXSetChanCtrl(GX_COLOR0A0, GX_TRUE, GX_SRC_REG, GX_SRC_VTX,
                  GX_LIGHT0 | GX_LIGHT2, GX_DF_CLAMP, GX_AF_SPOT);

    // GX_COLOR0A0 configures the colour and alpha halves of the same pair.
    MeleeHostGxChannelControl control{};
    REQUIRE(melee_host_gx_channel_control(GX_COLOR0, &control));
    REQUIRE(control.lighting_enabled);
    REQUIRE(control.ambient_source == GX_SRC_REG);
    REQUIRE(control.material_source == GX_SRC_VTX);
    REQUIRE(control.light_mask == static_cast<mh_u32>(GX_LIGHT0 | GX_LIGHT2));
    REQUIRE(control.diffuse_function == GX_DF_CLAMP);
    REQUIRE(control.attenuation_function == GX_AF_SPOT);

    MeleeHostGxChannelControl alpha{};
    REQUIRE(melee_host_gx_channel_control(GX_ALPHA0, &alpha));
    REQUIRE(alpha.lighting_enabled);
    REQUIRE(alpha.attenuation_function == GX_AF_SPOT);

    // The other pair must be untouched by that call.
    MeleeHostGxChannelControl other{};
    REQUIRE(melee_host_gx_channel_control(GX_COLOR1, &other));
    REQUIRE(!other.lighting_enabled);

    // A single-channel call must not spill into its pair partner.
    GXSetChanCtrl(GX_COLOR1, GX_TRUE, GX_SRC_VTX, GX_SRC_REG, GX_LIGHT1,
                  GX_DF_NONE, GX_AF_NONE);
    REQUIRE(melee_host_gx_channel_control(GX_COLOR1, &other));
    REQUIRE(other.lighting_enabled);
    MeleeHostGxChannelControl partner{};
    REQUIRE(melee_host_gx_channel_control(GX_ALPHA1, &partner));
    REQUIRE(!partner.lighting_enabled);

    REQUIRE(!melee_host_gx_channel_control(MELEE_HOST_GX_MAX_CHANNEL,
                                           &control));
}

TEST_CASE("fog records its parameters and flags range adjust as unmodelled")
{
    melee_host_gx_state_reset();

    const GXColor color{ 10, 20, 30, 40 };
    GXSetFog(GX_FOG_LIN, 1.0F, 50.0F, 0.5F, 100.0F, color);

    MeleeHostGxFogState fog{};
    melee_host_gx_fog_state(&fog);
    REQUIRE(fog.type == GX_FOG_LIN);
    REQUIRE(near(fog.start_z, 1.0F));
    REQUIRE(near(fog.far_z, 100.0F));
    REQUIRE(fog.color[2] == 30);
    REQUIRE(!fog.range_adjust_enabled);

    Mtx44 projection;
    MTXPerspective(projection, 60.0F, 1.0F, 1.0F, 100.0F);
    GXFogAdjTable table{};
    GXInitFogAdjTable(&table, 640, projection);
    GXSetFogRangeAdj(GX_TRUE, 320, &table);

    melee_host_gx_fog_state(&fog);
    REQUIRE(fog.range_adjust_enabled);
    REQUIRE(fog.range_adjust_center == 320);
    // A neutral table means no adjustment, and the flag says so out loud.
    REQUIRE(fog.range_adjust_table[0] == 256);
    REQUIRE(fog.range_adjust_table[9] == 256);
    REQUIRE(!fog.range_adjust_modelled);
}

TEST_CASE("EFB copies are recorded and written to their destination")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    /* The whole texture: 320x240 RGBA8 is 4 bytes a texel.  With nothing
     * drawn the copy is the clear colour, opaque. */
    std::vector<std::uint8_t> destination(320U * 240U * 4U, 0x5A);
    GXSetTexCopySrc(0, 0, 640, 480);
    GXSetTexCopyDst(320, 240, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(destination.data(), GX_TRUE);
    GXInvalidateTexAll();

    MeleeHostGxCopyState copy{};
    melee_host_gx_copy_state(&copy);
    REQUIRE(copy.source_width == 640);
    REQUIRE(copy.destination_width == 320);
    REQUIRE(copy.destination_height == 240);
    REQUIRE(copy.destination_format == GX_TF_RGBA8);
    REQUIRE(copy.copy_count == 1);
    REQUIRE(copy.last_destination == destination.data());
    REQUIRE(copy.last_clear);
    REQUIRE(copy.texture_invalidate_count == 1);
    REQUIRE(destination[3] == 0xFF);
    REQUIRE(destination[destination.size() - 1] == 0xFF);
}

TEST_CASE("I4 EFB copies rasterize the recorded shadow mask")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    Mtx44 projection;
    MTXOrtho(projection, 1.0F, -1.0F, -1.0F, 1.0F, 0.0F, 1.0F);
    GXSetProjection(projection, GX_ORTHOGRAPHIC);
    Mtx identity;
    PSMTXIdentity(identity);
    GXLoadPosMtxImm(identity, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXSetViewport(0.0F, 0.0F, 8.0F, 8.0F, 0.0F, 1.0F);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(-1.0F, 1.0F, 0.0F);
    GXColor4u8(0, 0, 0, 255);
    GXPosition3f32(1.0F, 1.0F, 0.0F);
    GXColor4u8(0, 0, 0, 255);
    GXPosition3f32(-1.0F, -1.0F, 0.0F);
    GXEnd();

    MeleeHostGxCapturedTriangle captured{};
    REQUIRE(melee_host_gx_captured_triangle_at(0, &captured));
    REQUIRE(captured.vertices[0].raster_color[0][0] == 0);

    std::array<std::uint8_t, 32> destination{};
    GXSetTexCopySrc(0, 0, 8, 8);
    GXSetTexCopyDst(8, 8, GX_CTF_R4, GX_FALSE);
    GXCopyTex(destination.data(), GX_TRUE);
    /* I4 is 8x8 tiled, two texels per byte.  The triangle changed at least
     * one nibble from the white EFB clear, but it does not cover the tile. */
    REQUIRE(std::any_of(destination.begin(), destination.end(),
                        [](std::uint8_t byte) { return byte != 0xFFU; }));
    REQUIRE(std::any_of(destination.begin(), destination.end(),
                        [](std::uint8_t byte) { return byte != 0; }));
}

TEST_CASE("colour EFB copies rasterize the frame so far and keep its clears")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    Mtx44 projection;
    MTXOrtho(projection, 1.0F, -1.0F, -1.0F, 1.0F, 0.0F, 1.0F);
    GXSetProjection(projection, GX_ORTHOGRAPHIC);
    Mtx identity;
    PSMTXIdentity(identity);
    GXLoadPosMtxImm(identity, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXSetViewport(0.0F, 0.0F, 8.0F, 8.0F, 0.0F, 1.0F);
    GXSetCopyClear(GXColor{ 0, 0, 255, 255 }, 0x00FFFFFF);
    /* With no colour channel the raster colour is the vertex colour. */
    GXSetNumChans(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
                  GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetCullMode(GX_CULL_NONE);
    GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_ENABLE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetColorUpdate(GX_TRUE);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    /* The upper-left half of the 8x8 viewport, in red. */
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(-1.0F, 1.0F, 0.0F);
    GXColor4u8(255, 0, 0, 255);
    GXPosition3f32(1.0F, 1.0F, 0.0F);
    GXColor4u8(255, 0, 0, 255);
    GXPosition3f32(-1.0F, -1.0F, 0.0F);
    GXColor4u8(255, 0, 0, 255);
    GXEnd();

    /* RGB5A3 in 4x4 blocks, big-endian, opaque texels. */
    const auto texel = [](const std::uint8_t* image, int width, int x, int y) {
        const int blocks_wide = (width + 3) / 4;
        const int block = (y / 4) * blocks_wide + x / 4;
        const auto at = static_cast<std::size_t>((block * 16 + (y % 4) * 4 + x % 4) * 2);
        return static_cast<std::uint16_t>((image[at] << 8) | image[at + 1]);
    };
    constexpr std::uint16_t kRed = 0x8000 | (31 << 10);
    constexpr std::uint16_t kBlue = 0x8000 | 31;

    std::array<std::uint8_t, 128> frame{};
    GXSetTexCopySrc(0, 0, 8, 8);
    GXSetTexCopyDst(8, 8, GX_TF_RGB5A3, GX_FALSE);
    GXCopyTex(frame.data(), GX_FALSE);
    REQUIRE(texel(frame.data(), 8, 1, 1) == kRed);
    REQUIRE(texel(frame.data(), 8, 5, 1) == kRed);
    REQUIRE(texel(frame.data(), 8, 6, 6) == kBlue);

    /* A copy that clears the upper-left 4x4: what is drawn later in the frame
     * sees blue and the far plane there. */
    std::array<std::uint8_t, 32> corner{};
    GXSetTexCopySrc(0, 0, 4, 4);
    GXSetTexCopyDst(4, 4, GX_TF_RGB5A3, GX_FALSE);
    GXCopyTex(corner.data(), GX_TRUE);
    REQUIRE(texel(corner.data(), 4, 1, 1) == kRed);

    GXSetTexCopySrc(0, 0, 8, 8);
    GXSetTexCopyDst(8, 8, GX_TF_RGB5A3, GX_FALSE);
    GXCopyTex(frame.data(), GX_FALSE);
    REQUIRE(texel(frame.data(), 8, 1, 1) == kBlue);
    REQUIRE(texel(frame.data(), 8, 5, 1) == kRed);
    REQUIRE(melee_host_gx_texture_copy_generation(frame.data()) == 2);
    REQUIRE(melee_host_gx_texture_copy_generation(corner.data()) == 1);
}

TEST_CASE("projection helpers build the GX frustum and orthographic forms")
{
    Mtx44 frustum;
    MTXFrustum(frustum, 1.0F, -1.0F, -2.0F, 2.0F, 1.0F, 101.0F);
    REQUIRE(near(frustum[0][0], 0.5F));
    REQUIRE(near(frustum[1][1], 1.0F));
    REQUIRE(near(frustum[2][2], -0.01F));
    REQUIRE(near(frustum[2][3], -1.01F));
    REQUIRE(near(frustum[3][2], -1.0F));

    Mtx44 perspective;
    MTXPerspective(perspective, 90.0F, 2.0F, 1.0F, 101.0F);
    REQUIRE(near(perspective[0][0], 0.5F, 1.0e-4F));
    REQUIRE(near(perspective[1][1], 1.0F, 1.0e-4F));
    REQUIRE(near(perspective[3][2], -1.0F));

    Mtx44 ortho;
    MTXOrtho(ortho, 1.0F, -1.0F, -2.0F, 2.0F, 0.0F, 2.0F);
    REQUIRE(near(ortho[0][0], 0.5F));
    REQUIRE(near(ortho[1][1], 1.0F));
    REQUIRE(near(ortho[2][2], -0.5F));
    REQUIRE(near(ortho[3][3], 1.0F));
}

TEST_CASE("look-at builds a view matrix that places the camera at the origin")
{
    Vec eye{ 0.0F, 0.0F, 10.0F };
    Vec up{ 0.0F, 1.0F, 0.0F };
    Vec target{ 0.0F, 0.0F, 0.0F };
    Mtx view;
    C_MTXLookAt(view, &eye, &up, &target);

    Vec eye_in_view;
    PSMTXMultVec(view, &eye, &eye_in_view);
    REQUIRE(near(eye_in_view.x, 0.0F));
    REQUIRE(near(eye_in_view.y, 0.0F));
    REQUIRE(near(eye_in_view.z, 0.0F));

    // The target sits down the negative z axis of view space.
    Vec target_in_view;
    PSMTXMultVec(view, &target, &target_in_view);
    REQUIRE(near(target_in_view.z, -10.0F));
}

TEST_CASE("axis rotation by name matches the paired-single rotation")
{
    Mtx by_name;
    MTXRotRad(by_name, 'z', 1.5707963267948966F);

    Vec axis{ 0.0F, 0.0F, 1.0F };
    Mtx by_axis;
    PSMTXRotAxisRad(by_axis, &axis, 1.5707963267948966F);

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
            REQUIRE(near(by_name[row][column], by_axis[row][column], 1.0e-4F));
        }
    }

    Mtx upper;
    MTXRotRad(upper, 'X', 0.75F);
    Mtx lower;
    MTXRotRad(lower, 'x', 0.75F);
    REQUIRE(near(upper[1][1], lower[1][1]));
    REQUIRE(near(upper[2][1], lower[2][1]));
}

TEST_CASE("explicit TEV configuration replaces the shorthand mode")
{
    melee_host_gx_state_reset();

    // GXSetTevOp records a mode; the individual calls it expands into cannot,
    // so the stage reports the custom sentinel instead of a stale mode.
    GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    MeleeHostGxTevState tev{};
    melee_host_gx_tev_state(&tev);
    REQUIRE(tev.stages[0].mode == GX_MODULATE);

    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC,
                    GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA,
                    GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_SUBHALF, GX_CS_SCALE_2,
                    GX_FALSE, GX_TEVREG1);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_4,
                    GX_TRUE, GX_TEVREG2);
    GXSetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K1);
    GXSetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K2_A);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP1, GX_TEV_SWAP2);

    melee_host_gx_tev_state(&tev);
    const MeleeHostGxTevStage& stage = tev.stages[0];
    REQUIRE(stage.mode == MELEE_HOST_GX_TEV_MODE_CUSTOM);
    REQUIRE(stage.color_input[1] == GX_CC_TEXC);
    REQUIRE(stage.color_input[2] == GX_CC_RASC);
    REQUIRE(stage.alpha_input[1] == GX_CA_TEXA);
    REQUIRE(stage.color_op == GX_TEV_SUB);
    REQUIRE(stage.color_bias == GX_TB_SUBHALF);
    REQUIRE(stage.color_scale == GX_CS_SCALE_2);
    REQUIRE(!stage.color_clamp);
    REQUIRE(stage.color_out_reg == GX_TEVREG1);
    REQUIRE(stage.alpha_scale == GX_CS_SCALE_4);
    REQUIRE(stage.alpha_clamp);
    REQUIRE(stage.alpha_out_reg == GX_TEVREG2);
    REQUIRE(stage.konst_color_select == GX_TEV_KCSEL_K1);
    REQUIRE(stage.konst_alpha_select == GX_TEV_KASEL_K2_A);
    REQUIRE(stage.raster_swap == GX_TEV_SWAP1);
    REQUIRE(stage.texture_swap == GX_TEV_SWAP2);

    // A stage past the hardware limit is dropped, not wrapped onto stage 0.
    GXSetTevColorOp(static_cast<GXTevStageID>(MELEE_HOST_GX_MAX_TEVSTAGE),
                    GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE,
                    GX_TEVPREV);
    melee_host_gx_tev_state(&tev);
    REQUIRE(tev.stages[0].color_op == GX_TEV_SUB);
}

TEST_CASE("TEV registers keep signed ten-bit values without clamping")
{
    melee_host_gx_state_reset();

    const GXColor eight_bit{ 255, 128, 0, 64 };
    GXSetTevColor(GX_TEVREG0, eight_bit);
    const GXColorS10 wide{ 511, -256, 1023, -512 };
    GXSetTevColorS10(GX_TEVREG1, wide);
    const GXColor konst{ 1, 2, 3, 4 };
    GXSetTevKColor(GX_KCOLOR2, konst);

    MeleeHostGxTevState tev{};
    melee_host_gx_tev_state(&tev);
    REQUIRE(tev.registers[GX_TEVREG0][0] == 255);
    REQUIRE(tev.registers[GX_TEVREG0][3] == 64);
    REQUIRE(tev.registers[GX_TEVREG1][1] == -256);
    REQUIRE(tev.registers[GX_TEVREG1][2] == 1023);
    REQUIRE(tev.registers[GX_TEVREG1][3] == -512);
    REQUIRE(tev.konst_colors[GX_KCOLOR2][2] == 3);
}

TEST_CASE("channel colors follow the same pairing rule as channel control")
{
    melee_host_gx_state_reset();

    const GXColor ambient{ 10, 20, 30, 40 };
    const GXColor material{ 200, 210, 220, 230 };
    GXSetChanAmbColor(GX_COLOR0A0, ambient);
    GXSetChanMatColor(GX_COLOR0, material);

    MeleeHostGxChannelControl color{};
    MeleeHostGxChannelControl alpha{};
    REQUIRE(melee_host_gx_channel_control(GX_COLOR0, &color));
    REQUIRE(melee_host_gx_channel_control(GX_ALPHA0, &alpha));
    REQUIRE(color.ambient_color[1] == 20);
    REQUIRE(alpha.ambient_color[1] == 20);
    REQUIRE(color.material_color[0] == 200);
    // GX_COLOR0 alone must not write the alpha half of the pair, which keeps
    // the white GXInit left there.
    REQUIRE(alpha.material_color[0] == 255);
}

TEST_CASE("a channel with lighting off passes its material colour through")
{
    melee_host_gx_state_reset();

    /* Hyrule Temple draws its stage models with lighting off, so only the
     * material colour reaches TEV.  Multiplying by the ambient register
     * instead tinted the whole stage with whatever colour the last material
     * had left there: dark blue normally, dark red while Link's boomerang
     * was in the air. */
    const GXColor ambient{ 81, 20, 25, 40 };
    const GXColor material{ 200, 210, 220, 230 };
    GXSetChanAmbColor(GX_COLOR0A0, ambient);
    GXSetChanMatColor(GX_COLOR0A0, material);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
                  GX_DF_CLAMP, GX_AF_NONE);

    MeleeHostGxCapturedVertex vertex{};
    vertex.normal.z = 1.0F;
    mh_u8 color0a0[4]{};
    mh_u8 color1a1[4]{};
    melee_host_gx_evaluate_lighting(&vertex, color0a0, color1a1);
    REQUIRE(color0a0[0] == 200);
    REQUIRE(color0a0[1] == 210);
    REQUIRE(color0a0[2] == 220);
    REQUIRE(color0a0[3] == 230);

    /* With lighting on and no light loaded, the ambient register is all the
     * channel has, and it scales the material colour. */
    GXSetChanCtrl(GX_COLOR0A0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
                  GX_DF_CLAMP, GX_AF_NONE);
    melee_host_gx_evaluate_lighting(&vertex, color0a0, color1a1);
    REQUIRE(color0a0[0] == static_cast<mh_u8>((200 * 81 + 127) / 255));
    REQUIRE(color0a0[1] == static_cast<mh_u8>((210 * 20 + 127) / 255));
    REQUIRE(color0a0[3] == static_cast<mh_u8>((230 * 40 + 127) / 255));
}

TEST_CASE("raster and framebuffer format state is recorded")
{
    melee_host_gx_state_reset();

    GXSetDither(GX_TRUE);
    GXSetDstAlpha(GX_TRUE, 96);
    GXSetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_NEAR);
    GXSetLineWidth(12, GX_TO_ONE);
    GXSetPointSize(24, GX_TO_ZERO);
    GXSetFieldMode(GX_TRUE, GX_FALSE);

    MeleeHostGxPixelState pixel{};
    melee_host_gx_pixel_state(&pixel);
    REQUIRE(pixel.dither_enabled);
    REQUIRE(pixel.destination_alpha_enabled);
    REQUIRE(pixel.destination_alpha == 96);
    REQUIRE(pixel.pixel_format == GX_PF_RGBA6_Z24);
    REQUIRE(pixel.depth_format == GX_ZC_NEAR);
    REQUIRE(pixel.line_width == 12);
    REQUIRE(pixel.line_texture_offsets == GX_TO_ONE);
    REQUIRE(pixel.point_size == 24);
    REQUIRE(pixel.point_texture_offsets == GX_TO_ZERO);
    REQUIRE(pixel.field_mode);
    REQUIRE(!pixel.half_aspect_ratio);
}

namespace {

int draw_done_calls = 0;

void record_draw_done()
{
    draw_done_calls += 1;
}

} // namespace

TEST_CASE("NTSC 480i deflicker describes a centred 640x480 framebuffer")
{
    REQUIRE(GXNtsc480IntDf.viTVmode == VI_TVMODE_NTSC_INT);
    REQUIRE(GXNtsc480IntDf.fbWidth == 640);
    REQUIRE(GXNtsc480IntDf.efbHeight == 480);
    REQUIRE(GXNtsc480IntDf.xfbHeight == 480);
    // A 640-pixel image inside the 720-pixel NTSC line leaves 40 each side.
    REQUIRE(GXNtsc480IntDf.viXOrigin == 40);
    REQUIRE(GXNtsc480IntDf.viYOrigin == 0);
    REQUIRE(GXNtsc480IntDf.xFBmode == VI_XFBMODE_DF);
    REQUIRE(GXNtsc480IntDf.field_rendering == GX_FALSE);
    REQUIRE(GXNtsc480IntDf.aa == GX_FALSE);
    // The deflicker filter is symmetric and sums to 64.
    int weight_total = 0;
    for (const u8 tap : GXNtsc480IntDf.vfilter) {
        weight_total += tap;
    }
    REQUIRE(weight_total == 64);
    REQUIRE(GXNtsc480IntDf.vfilter[0] == GXNtsc480IntDf.vfilter[6]);
    REQUIRE(GXNtsc480IntDf.vfilter[3] == 12);
}

TEST_CASE("display copy configuration is recorded and reports its line count")
{
    melee_host_gx_state_reset();

    GXSetDispCopySrc(0, 0, 640, 480);
    GXSetDispCopyDst(640, 480);
    REQUIRE(GXSetDispCopyYScale(1.0F) == 480);
    // Half-scaling a 480-line source yields a 240-line framebuffer.
    REQUIRE(GXSetDispCopyYScale(0.5F) == 240);
    GXSetDispCopyGamma(GX_GM_1_7);
    GXSetCopyClamp(GX_CLAMP_TOP);
    const GXColor clear{ 1, 2, 3, 4 };
    GXSetCopyClear(clear, 0x00FFFFFF);

    const u8 pattern[12][2] = { { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 },
                                { 5, 5 }, { 6, 6 }, { 7, 7 }, { 8, 8 },
                                { 9, 9 }, { 10, 10 }, { 11, 11 },
                                { 12, 12 } };
    const u8 filter[7] = { 1, 2, 3, 4, 3, 2, 1 };
    GXSetCopyFilter(GX_TRUE, pattern, GX_TRUE, filter);

    std::array<std::uint8_t, 64> framebuffer{};
    GXCopyDisp(framebuffer.data(), GX_TRUE);

    MeleeHostGxDisplayCopyState copy{};
    melee_host_gx_display_copy_state(&copy);
    REQUIRE(copy.source_width == 640);
    REQUIRE(copy.destination_height == 480);
    REQUIRE(near(copy.vertical_scale, 0.5F));
    REQUIRE(copy.gamma == GX_GM_1_7);
    REQUIRE(copy.clamp == GX_CLAMP_TOP);
    REQUIRE(copy.clear_color[1] == 2);
    REQUIRE(copy.clear_depth == 0x00FFFFFF);
    REQUIRE(copy.antialiasing);
    REQUIRE(copy.vertical_filter);
    REQUIRE(copy.sample_pattern[11][0] == 12);
    REQUIRE(copy.filter_weights[3] == 4);
    REQUIRE(copy.copy_count == 1);
    REQUIRE(copy.last_destination == framebuffer.data());
    REQUIRE(copy.last_clear);

    // Disabling a filter must not overwrite the pattern the hardware keeps.
    GXSetCopyFilter(GX_FALSE, nullptr, GX_FALSE, nullptr);
    melee_host_gx_display_copy_state(&copy);
    REQUIRE(!copy.antialiasing);
    REQUIRE(!copy.vertical_filter);
    REQUIRE(copy.sample_pattern[11][0] == 12);
    REQUIRE(copy.filter_weights[3] == 4);
}

TEST_CASE("a draw-done fence stays pending until something drains it")
{
    melee_host_gx_state_reset();
    draw_done_calls = 0;
    REQUIRE(GXSetDrawDoneCallback(record_draw_done) == nullptr);

    GXSetDrawDone();
    MeleeHostGxDrawSyncState sync{};
    melee_host_gx_draw_sync_state(&sync);
    REQUIRE(sync.pending);
    REQUIRE(sync.fence_count == 1);
    // The host has no graphics processor interrupt, so nothing has run yet.
    REQUIRE(draw_done_calls == 0);

    GXWaitDrawDone();
    melee_host_gx_draw_sync_state(&sync);
    REQUIRE(!sync.pending);
    REQUIRE(sync.wait_count == 1);
    REQUIRE(sync.callback_count == 1);
    REQUIRE(draw_done_calls == 1);

    // Waiting again with no fence outstanding must not re-run the callback.
    GXWaitDrawDone();
    melee_host_gx_draw_sync_state(&sync);
    REQUIRE(sync.wait_count == 2);
    REQUIRE(sync.callback_count == 1);
    REQUIRE(draw_done_calls == 1);

    // The frame loop can drain the fence instead of blocking on it.
    GXSetDrawDone();
    REQUIRE(melee_host_gx_drain_draw_done());
    REQUIRE(draw_done_calls == 2);
    REQUIRE(!melee_host_gx_drain_draw_done());
    REQUIRE(draw_done_calls == 2);

    // GXDrawDone is the fence and the wait in one call.
    GXDrawDone();
    REQUIRE(draw_done_calls == 3);

    REQUIRE(GXSetDrawDoneCallback(nullptr) == record_draw_done);
    GXDrawDone();
    REQUIRE(draw_done_calls == 3);
}

namespace {

int frames_received = 0;
size_t commands_in_frame = 0;

void record_frame(void* user_data)
{
    frames_received += 1;
    commands_in_frame = melee_host_gx_command_count();
    *static_cast<int*>(user_data) += 1;
}

} // namespace

TEST_CASE("a frame sink receives each display copy's capture before it clears")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();
    frames_received = 0;
    commands_in_frame = 0;
    int user_calls = 0;

    melee_host_gx_submit_u32(1);
    melee_host_gx_set_frame_sink(record_frame, &user_calls);
    GXCopyDisp(nullptr, GX_TRUE);
    REQUIRE(frames_received == 1);
    REQUIRE(user_calls == 1);
    REQUIRE(commands_in_frame == 1);
    REQUIRE(melee_host_gx_command_count() == 0);

    // Without a sink the copy is only counted and the capture accumulates.
    melee_host_gx_set_frame_sink(nullptr, nullptr);
    melee_host_gx_submit_u32(2);
    GXCopyDisp(nullptr, GX_TRUE);
    REQUIRE(frames_received == 1);
    REQUIRE(melee_host_gx_command_count() == 1);
    MeleeHostGxDisplayCopyState copy{};
    melee_host_gx_display_copy_state(&copy);
    REQUIRE(copy.copy_count == 2);
    melee_host_gx_reset_command_log();
}
