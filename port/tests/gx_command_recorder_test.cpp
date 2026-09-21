#include "test.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <vector>

extern "C" {
#include <dolphin/gx/GXCommandList.h>
#include <dolphin/gx/GXVert.h>
#include <dolphin/gx/GXGeometry.h>
#include <dolphin/gx/GXLighting.h>
#include <dolphin/gx/GXDispList.h>
#include <dolphin/gx/GXPixel.h>
#include <dolphin/gx/GXTev.h>
#include <dolphin/gx/GXBump.h>
#include <dolphin/gx/GXCull.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTransform.h>
#include <melee_host/gx.h>
}

TEST_CASE("host GX assembles a position-only GX_TRIANGLES primitive")
{
    melee_host_gx_reset_command_log();
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(-1.0F, -1.0F, 0.0F);
    GXPosition3f32(1.0F, -1.0F, 0.0F);
    GXPosition3f32(0.0F, 1.0F, 0.0F);
    GXEnd();

    REQUIRE(melee_host_gx_command_count() == 11);
    REQUIRE(melee_host_gx_triangle_count() == 1);
    MeleeHostGxTriangle triangle{};
    REQUIRE(melee_host_gx_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].x == -1.0F);
    REQUIRE(triangle.vertices[1].x == 1.0F);
    REQUIRE(triangle.vertices[2].x == 0.0F);
    REQUIRE(triangle.vertices[2].y == 1.0F);
}

TEST_CASE("host GX vertex writes never access the GameCube FIFO address")
{
    melee_host_gx_reset_command_log();
    GXCmd1u8(0x98U);
    GXPosition3f32(1.0F, -2.5F, 3.25F);
    GXColor4u8(1U, 2U, 3U, 4U);

    REQUIRE(melee_host_gx_command_count() == 8);

    MeleeHostGxCommand command{};
    REQUIRE(melee_host_gx_command_at(0, &command));
    REQUIRE(command.type == MELEE_HOST_GX_U8);
    REQUIRE(command.bits == 0x98U);

    REQUIRE(melee_host_gx_command_at(2, &command));
    REQUIRE(command.type == MELEE_HOST_GX_F32);
    REQUIRE(command.bits == std::bit_cast<mh_u32>(-2.5F));

    REQUIRE(melee_host_gx_command_at(7, &command));
    REQUIRE(command.type == MELEE_HOST_GX_U8);
    REQUIRE(command.bits == 4U);
}

TEST_CASE("host GX converts strips fans and quads to triangle lists")
{
    MeleeHostGxTriangle triangle{};

    melee_host_gx_reset_command_log();
    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);
    for (int x = 0; x < 4; ++x) {
        GXPosition3f32(static_cast<float>(x), 0.0F, 0.0F);
    }
    REQUIRE(melee_host_gx_triangle_count() == 2);
    REQUIRE(melee_host_gx_triangle_at(1, &triangle));
    REQUIRE(triangle.vertices[0].x == 2.0F);
    REQUIRE(triangle.vertices[1].x == 1.0F);
    REQUIRE(triangle.vertices[2].x == 3.0F);

    melee_host_gx_reset_command_log();
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, 4);
    for (int x = 0; x < 4; ++x) {
        GXPosition3f32(static_cast<float>(x), 0.0F, 0.0F);
    }
    REQUIRE(melee_host_gx_triangle_count() == 2);
    REQUIRE(melee_host_gx_triangle_at(1, &triangle));
    REQUIRE(triangle.vertices[0].x == 0.0F);
    REQUIRE(triangle.vertices[1].x == 2.0F);
    REQUIRE(triangle.vertices[2].x == 3.0F);

    melee_host_gx_reset_command_log();
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    for (int x = 0; x < 4; ++x) {
        GXPosition3f32(static_cast<float>(x), 0.0F, 0.0F);
    }
    REQUIRE(melee_host_gx_triangle_count() == 2);
    REQUIRE(melee_host_gx_triangle_at(1, &triangle));
    REQUIRE(triangle.vertices[0].x == 0.0F);
    REQUIRE(triangle.vertices[1].x == 2.0F);
    REQUIRE(triangle.vertices[2].x == 3.0F);
}

TEST_CASE("host GX captures direct position normal color and UV attributes")
{
    melee_host_gx_reset_command_log();
    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition3f32(1.0F, 2.0F, 3.0F);
    GXNormal3f32(0.0F, 0.0F, 1.0F);
    GXColor4u8(10, 20, 30, 40);
    GXTexCoord2f32(0.25F, 0.75F);
    GXEnd();

    REQUIRE(melee_host_gx_captured_vertex_count() == 1);
    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.attributes ==
            (MELEE_HOST_GX_VERTEX_POSITION | MELEE_HOST_GX_VERTEX_NORMAL |
             MELEE_HOST_GX_VERTEX_COLOR | MELEE_HOST_GX_VERTEX_TEXCOORD));
    REQUIRE(vertex.position.y == 2.0F);
    REQUIRE(vertex.normal.z == 1.0F);
    REQUIRE(vertex.color[2] == 30);
    REQUIRE(vertex.texcoord[0] == 0.25F);
    REQUIRE(vertex.texcoord[1] == 0.75F);
}

TEST_CASE("host GX keeps complete attributes when assembling a triangle")
{
    melee_host_gx_reset_command_log();
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    for (int index = 0; index < 3; ++index) {
        GXPosition3f32(static_cast<float>(index), 0.0F, 0.0F);
        GXNormal3f32(0.0F, 0.0F, 1.0F);
        GXColor4u8(static_cast<u8>(10 + index), 20, 30, 40);
        GXTexCoord2f32(static_cast<float>(index) * 0.5F, 0.25F);
    }
    GXEnd();

    MeleeHostGxCapturedTriangle triangle{};
    REQUIRE(melee_host_gx_captured_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].normal.z == 1.0F);
    REQUIRE(triangle.vertices[1].color[0] == 11);
    REQUIRE(triangle.vertices[2].texcoord[0] == 1.0F);
    REQUIRE(triangle.vertices[2].texcoord[1] == 0.25F);
}

TEST_CASE("host GX applies an affine transform to completed geometry")
{
    melee_host_gx_reset_command_log();
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXNormal3f32(1.0F, 1.0F, 0.0F);
    GXPosition3f32(1.0F, 0.0F, 0.0F);
    GXNormal3f32(1.0F, 1.0F, 0.0F);
    GXPosition3f32(0.0F, 1.0F, 0.0F);
    GXNormal3f32(1.0F, 1.0F, 0.0F);
    GXEnd();

    MeleeHostGxAffineTransform transform{ {
        { 2.0F, 0.0F, 0.0F, 10.0F },
        { 0.0F, 3.0F, 0.0F, 20.0F },
        { 0.0F, 0.0F, 4.0F, 30.0F },
    } };
    melee_host_gx_transform_vertices(0, 3, &transform);

    MeleeHostGxCapturedTriangle triangle{};
    REQUIRE(melee_host_gx_captured_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].position.x == 10.0F);
    REQUIRE(triangle.vertices[1].position.x == 12.0F);
    REQUIRE(triangle.vertices[2].position.y == 23.0F);
    MeleeHostGxTriangle positions{};
    REQUIRE(melee_host_gx_triangle_at(0, &positions));
    REQUIRE(positions.vertices[2].y == 23.0F);
    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(std::fabs(vertex.normal.x - 3.0F / std::sqrt(13.0F)) < 0.0001F);
    REQUIRE(std::fabs(vertex.normal.y - 2.0F / std::sqrt(13.0F)) < 0.0001F);
}

TEST_CASE("host GX modulates captured vertex colors with a material")
{
    melee_host_gx_reset_command_log();
    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXColor4u8(128, 255, 64, 255);
    GXEnd();
    const mh_u8 diffuse[4]{ 128, 64, 255, 128 };
    melee_host_gx_apply_material(0, 1, diffuse, MELEE_HOST_GX_NO_TEXTURE,
                                 1U << 30U);

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.color[0] == 64);
    REQUIRE(vertex.color[1] == 64);
    REQUIRE(vertex.color[2] == 64);
    REQUIRE(vertex.color[3] == 128);
    REQUIRE(vertex.render_mode == (1U << 30U));
}

TEST_CASE("host GX evaluates two independent raster lighting channels")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();
    GXSetNumChans(2);

    GXLightObj light{};
    GXInitLightColor(&light, GXColor{ 100, 200, 50, 255 });
    GXInitLightPos(&light, 0.0F, 0.0F, 10.0F);
    GXInitLightDir(&light, 0.0F, 0.0F, -1.0F);
    GXInitLightAttn(&light, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F);
    GXLoadLightObjImm(&light, GX_LIGHT0);
    GXSetChanAmbColor(GX_COLOR0, GXColor{ 10, 10, 10, 255 });
    GXSetChanMatColor(GX_COLOR0, GXColor{ 128, 128, 128, 255 });
    GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
                  GX_DF_CLAMP, GX_AF_SPOT);

    GXSetChanAmbColor(GX_COLOR1, GXColor{ 7, 8, 9, 255 });
    GXSetChanMatColor(GX_COLOR1, GXColor{ 255, 255, 255, 255 });
    GXSetChanCtrl(GX_COLOR1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0,
                  GX_DF_NONE, GX_AF_NONE);

    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXNormal3f32(0.0F, 0.0F, 1.0F);
    GXEnd();

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.raster_color[0][0] == 55);
    REQUIRE(vertex.raster_color[0][1] == 105);
    REQUIRE(vertex.raster_color[0][2] == 30);
    /* The second channel has lighting off, so it passes its material colour
     * through and its ambient register (7, 8, 9) takes no part. */
    REQUIRE(vertex.raster_color[1][0] == 255);
    REQUIRE(vertex.raster_color[1][1] == 255);
    REQUIRE(vertex.raster_color[1][2] == 255);
}

TEST_CASE("a lit channel that evaluates to NaN stores zero")
{
    // Converting NaN to an integer is undefined, and the results screen
    // reaches it with a degenerate light; UBSan reports it, the value is 0.
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();
    GXSetNumChans(1);

    const float zero = 0.0F;
    const float not_a_number = zero / zero;
    GXLightObj light{};
    GXInitLightColor(&light, GXColor{ 100, 200, 50, 255 });
    GXInitLightPos(&light, 0.0F, 0.0F, 10.0F);
    GXInitLightDir(&light, 0.0F, 0.0F, -1.0F);
    GXInitLightAttn(&light, not_a_number, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F);
    GXLoadLightObjImm(&light, GX_LIGHT0);
    GXSetChanAmbColor(GX_COLOR0, GXColor{ 10, 10, 10, 255 });
    GXSetChanMatColor(GX_COLOR0, GXColor{ 128, 128, 128, 255 });
    GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
                  GX_DF_CLAMP, GX_AF_SPOT);

    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXNormal3f32(0.0F, 0.0F, 1.0F);
    GXEnd();

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.raster_color[0][0] == 0);
    REQUIRE(vertex.raster_color[0][1] == 0);
    REQUIRE(vertex.raster_color[0][2] == 0);
}

TEST_CASE("host GX resolves indexed big-endian VCD and VAT attributes")
{
    const std::array<u8, 18> positions{
        0xFF, 0xFC, 0xFF, 0xF8, 0x00, 0x00,
        0x00, 0x04, 0xFF, 0xF8, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
    };
    const std::array<u8, 9> normals{
        0x00, 0x00, 0x7F,
        0x00, 0x00, 0x7F,
        0x00, 0x00, 0x7F,
    };
    const std::array<u8, 12> colors{
        10, 20, 30, 40,
        50, 60, 70, 80,
        90, 100, 110, 120,
    };
    const std::array<u8, 12> texcoords{
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x80, 0x01, 0x00,
    };

    const GXVtxDescList descriptors[]{
        { GX_VA_POS, GX_INDEX16 },
        { GX_VA_NRM, GX_INDEX8 },
        { GX_VA_CLR0, GX_INDEX8 },
        { GX_VA_TEX0, GX_INDEX16 },
        { GX_VA_NULL, GX_NONE },
    };
    const GXVtxAttrFmtList formats[]{
        { GX_VA_POS, GX_POS_XYZ, GX_S16, 2 },
        { GX_VA_NRM, GX_NRM_XYZ, GX_S8, 7 },
        { GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0 },
        { GX_VA_TEX0, GX_TEX_ST, GX_U16, 8 },
        { GX_VA_NULL, GX_POS_XY, GX_U8, 0 },
    };

    melee_host_gx_reset_command_log();
    GXClearVtxDesc();
    GXSetVtxDescv(descriptors);
    GXSetVtxAttrFmtv(GX_VTXFMT3, formats);
    GXSetArray(GX_VA_POS, positions.data(), 6);
    GXSetArray(GX_VA_NRM, normals.data(), 3);
    GXSetArray(GX_VA_CLR0, colors.data(), 4);
    GXSetArray(GX_VA_TEX0, texcoords.data(), 4);

    GXBegin(GX_TRIANGLES, GX_VTXFMT3, 3);
    for (u16 index = 0; index < 3; ++index) {
        GXPosition1x16(index);
        GXNormal1x8(static_cast<u8>(index));
        GXColor1x8(static_cast<u8>(index));
        GXTexCoord1x16(index);
    }
    GXEnd();

    REQUIRE(melee_host_gx_command_count() == 14);
    REQUIRE(melee_host_gx_captured_vertex_count() == 3);
    REQUIRE(melee_host_gx_triangle_count() == 1);

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.attributes ==
            (MELEE_HOST_GX_VERTEX_POSITION | MELEE_HOST_GX_VERTEX_NORMAL |
             MELEE_HOST_GX_VERTEX_COLOR | MELEE_HOST_GX_VERTEX_TEXCOORD));
    REQUIRE(vertex.position.x == -1.0F);
    REQUIRE(vertex.position.y == -2.0F);
    REQUIRE(std::fabs(vertex.normal.z - 127.0F / 128.0F) < 0.0001F);
    REQUIRE(vertex.color[0] == 10);
    REQUIRE(vertex.color[3] == 40);

    REQUIRE(melee_host_gx_captured_vertex_at(2, &vertex));
    REQUIRE(vertex.position.y == 2.0F);
    REQUIRE(vertex.color[2] == 110);
    REQUIRE(vertex.texcoord[0] == 0.5F);
    REQUIRE(vertex.texcoord[1] == 1.0F);

    MeleeHostGxCommand command{};
    REQUIRE(melee_host_gx_command_at(2, &command));
    REQUIRE(command.type == MELEE_HOST_GX_U16);
    REQUIRE(command.bits == 0);
    REQUIRE(melee_host_gx_command_at(3, &command));
    REQUIRE(command.type == MELEE_HOST_GX_U8);
}

TEST_CASE("host GX decodes packed indexed color formats")
{
    const std::array<u8, 2> rgba4{ 0xF0, 0x08 };
    const std::array<u8, 6> position{ 0, 0, 0, 0, 0, 0 };

    melee_host_gx_reset_command_log();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA4, 0);
    GXSetArray(GX_VA_POS, position.data(), 6);
    GXSetArray(GX_VA_CLR0, rgba4.data(), 2);

    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition1x8(0);
    GXColor1x8(0);
    GXEnd();

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.color[0] == 255);
    REQUIRE(vertex.color[1] == 0);
    REQUIRE(vertex.color[2] == 0);
    REQUIRE(vertex.color[3] == 136);
}

TEST_CASE("host GX executes an indexed PObj-style display list")
{
    const std::array<u8, 18> positions{
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
        0x00, 0x01, 0xFF, 0xFF, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    };
    alignas(32) std::array<u8, 32> display_list{};
    display_list[0] = static_cast<u8>(static_cast<u8>(GX_TRIANGLES) |
                                      static_cast<u8>(GX_VTXFMT2));
    display_list[1] = 0;
    display_list[2] = 3;
    display_list[3] = 0;
    display_list[4] = 1;
    display_list[5] = 2;

    melee_host_gx_reset_command_log();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GXSetArray(GX_VA_POS, positions.data(), 6);
    GXCallDisplayList(display_list.data(), display_list.size());

    REQUIRE(melee_host_gx_display_list_error_count() == 0);
    REQUIRE(melee_host_gx_captured_vertex_count() == 3);
    REQUIRE(melee_host_gx_triangle_count() == 1);
    MeleeHostGxTriangle triangle{};
    REQUIRE(melee_host_gx_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].x == -1.0F);
    REQUIRE(triangle.vertices[1].x == 1.0F);
    REQUIRE(triangle.vertices[2].y == 1.0F);
}

TEST_CASE("host GX rejects a truncated display list without over-reading")
{
    const std::array<u8, 6> positions{ 0, 0, 0, 0, 0, 0 };
    std::array<u8, 4> display_list{
        static_cast<u8>(static_cast<u8>(GX_TRIANGLES) |
                        static_cast<u8>(GX_VTXFMT0)),
        0, 3, 0
    };

    melee_host_gx_reset_command_log();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX16);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GXSetArray(GX_VA_POS, positions.data(), 6);
    GXCallDisplayList(display_list.data(), display_list.size());

    REQUIRE(melee_host_gx_display_list_error_count() == 1);
    REQUIRE(melee_host_gx_triangle_count() == 0);
}

TEST_CASE("host GX rejects an indexed vertex outside a bounded array")
{
    const std::array<u8, 6> positions{ 0, 1, 0, 2, 0, 3 };

    melee_host_gx_reset_command_log();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    melee_host_gx_set_array_bounded(GX_VA_POS, positions.data(),
                                    positions.size(), 6);

    GXBegin(GX_POINTS, GX_VTXFMT0, 1);
    GXPosition1x8(1);
    GXEnd();

    REQUIRE(melee_host_gx_captured_vertex_count() == 0);
}

TEST_CASE("host GX preserves the primary normal from a direct NBT stream")
{
    alignas(32) std::array<u8, 32> display_list{};
    display_list[0] = static_cast<u8>(static_cast<u8>(GX_POINTS) |
                                      static_cast<u8>(GX_VTXFMT0));
    display_list[1] = 0;
    display_list[2] = 1;
    display_list[3] = 0x3F; // position x = 1.0F, big-endian
    display_list[4] = 0x80;
    display_list[5] = 0;
    display_list[6] = 0;
    display_list[7] = 0;
    display_list[8] = 0;
    display_list[9] = 0;
    display_list[10] = 0;
    display_list[11] = 0;
    display_list[12] = 0;
    display_list[13] = 0;
    display_list[15] = 1;  // normal
    display_list[16] = 2;
    display_list[17] = 3;
    display_list[18] = 4;  // tangent
    display_list[19] = 5;
    display_list[20] = 6;
    display_list[21] = 7;  // binormal
    display_list[22] = 8;
    display_list[23] = 9;

    melee_host_gx_reset_command_log();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_NBT, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NBT, GX_NRM_NBT, GX_S8, 0);
    GXCallDisplayList(display_list.data(), display_list.size());

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_display_list_error_count() == 0);
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE((vertex.attributes & MELEE_HOST_GX_VERTEX_NORMAL) != 0);
    REQUIRE(vertex.normal.x == 1.0F);
    REQUIRE(vertex.normal.y == 2.0F);
    REQUIRE(vertex.normal.z == 3.0F);
    REQUIRE((vertex.attributes & MELEE_HOST_GX_VERTEX_TANGENT) != 0);
    REQUIRE((vertex.attributes & MELEE_HOST_GX_VERTEX_BINORMAL) != 0);
    REQUIRE(vertex.tangent.x == 4.0F);
    REQUIRE(vertex.tangent.y == 5.0F);
    REQUIRE(vertex.tangent.z == 6.0F);
    REQUIRE(vertex.binormal.x == 7.0F);
    REQUIRE(vertex.binormal.y == 8.0F);
    REQUIRE(vertex.binormal.z == 9.0F);
}

TEST_CASE("host GX resolves the three indexed NBT3 vectors")
{
    const std::array<u8, 12> positions{
        0x3F, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    const std::array<u8, 9> nbt{
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
    };
    alignas(32) std::array<u8, 32> display_list{};
    display_list[0] = static_cast<u8>(static_cast<u8>(GX_POINTS) |
                                      static_cast<u8>(GX_VTXFMT0));
    display_list[1] = 0;
    display_list[2] = 1;
    display_list[3] = 0;
    display_list[4] = 0;
    display_list[5] = 1;
    display_list[6] = 2;

    melee_host_gx_reset_command_log();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxDesc(GX_VA_NBT, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NBT, GX_NRM_NBT3, GX_S8, 0);
    melee_host_gx_set_array_bounded(GX_VA_POS, positions.data(),
                                    positions.size(), 12);
    melee_host_gx_set_array_bounded(GX_VA_NBT, nbt.data(), nbt.size(), 3);
    GXCallDisplayList(display_list.data(), display_list.size());

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_display_list_error_count() == 0);
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.normal.z == 3.0F);
    REQUIRE(vertex.tangent.y == 5.0F);
    REQUIRE(vertex.binormal.x == 7.0F);
}

TEST_CASE("a captured vertex is transformed by the position matrix in effect")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    // A translation loaded where GX_PNMTX0 lives, which is where the HSD
    // display path puts a rigid model's matrix.
    f32 translate[3][4] = {
        { 1.0F, 0.0F, 0.0F, 10.0F },
        { 0.0F, 1.0F, 0.0F, 20.0F },
        { 0.0F, 0.0F, 1.0F, 30.0F },
    };
    GXLoadPosMtxImm(translate, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);

    static const u8 positions[12] = { 0x3F, 0x80, 0, 0, 0x40, 0,
                                      0,    0,    0x40, 0x40, 0, 0 };
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetArray(GX_VA_POS, positions, 12);

    std::array<u8, 32> list{};
    list[0] = 0x90;
    list[2] = 0x03;
    GXCallDisplayList(list.data(), static_cast<u32>(list.size()));

    REQUIRE(melee_host_gx_triangle_count() == 1);
    MeleeHostGxTriangle triangle{};
    REQUIRE(melee_host_gx_triangle_at(0, &triangle));
    // (1, 2, 3) moved by the loaded matrix.
    REQUIRE(std::fabs(triangle.vertices[0].x - 11.0F) < 1.0e-5F);
    REQUIRE(std::fabs(triangle.vertices[0].y - 22.0F) < 1.0e-5F);
    REQUIRE(std::fabs(triangle.vertices[0].z - 33.0F) < 1.0e-5F);
}

TEST_CASE("a matrix index moves one vertex by its own joint matrix")
{
    // This is the envelope-skinning case: the PObj loads a matrix per joint and
    // names one per vertex, so a single transform applied to the whole draw
    // could not reproduce it.
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    f32 first[3][4] = {
        { 1.0F, 0.0F, 0.0F, 100.0F },
        { 0.0F, 1.0F, 0.0F, 0.0F },
        { 0.0F, 0.0F, 1.0F, 0.0F },
    };
    f32 second[3][4] = {
        { 1.0F, 0.0F, 0.0F, 0.0F },
        { 0.0F, 1.0F, 0.0F, 200.0F },
        { 0.0F, 0.0F, 1.0F, 0.0F },
    };
    GXLoadPosMtxImm(first, GX_PNMTX0);
    GXLoadPosMtxImm(second, GX_PNMTX1);
    GXSetCurrentMtx(GX_PNMTX0);

    static const u8 positions[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_PNMTXIDX, GX_DIRECT);
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetArray(GX_VA_POS, positions, 12);

    // Three vertices at the origin: the first two name GX_PNMTX0 and
    // GX_PNMTX1, the third repeats the second.
    std::array<u8, 32> list{};
    list[0] = 0x90;
    list[2] = 0x03;
    list[3] = GX_PNMTX0;
    list[4] = 0;
    list[5] = GX_PNMTX1;
    list[6] = 0;
    list[7] = GX_PNMTX1;
    list[8] = 0;
    GXCallDisplayList(list.data(), static_cast<u32>(list.size()));

    REQUIRE(melee_host_gx_display_list_error_count() == 0);
    REQUIRE(melee_host_gx_triangle_count() == 1);
    MeleeHostGxTriangle triangle{};
    REQUIRE(melee_host_gx_triangle_at(0, &triangle));
    REQUIRE(std::fabs(triangle.vertices[0].x - 100.0F) < 1.0e-5F);
    REQUIRE(std::fabs(triangle.vertices[0].y) < 1.0e-5F);
    REQUIRE(std::fabs(triangle.vertices[1].x) < 1.0e-5F);
    REQUIRE(std::fabs(triangle.vertices[1].y - 200.0F) < 1.0e-5F);
    REQUIRE(std::fabs(triangle.vertices[2].y - 200.0F) < 1.0e-5F);
}

TEST_CASE("geometry drawn with no matrix loaded is left where it was")
{
    // Matrix memory resets to zeros rather than identity, so transforming by
    // an untouched row would collapse every vertex onto the origin.
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(-1.0F, -1.0F, 0.0F);
    GXPosition3f32(1.0F, -1.0F, 0.0F);
    GXPosition3f32(0.0F, 1.0F, 0.0F);
    GXEnd();

    MeleeHostGxTriangle triangle{};
    REQUIRE(melee_host_gx_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].x == -1.0F);
    REQUIRE(triangle.vertices[2].y == 1.0F);
}

TEST_CASE("a captured vertex names the texture bound when its draw began")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    static u8 image[32] ATTRIBUTE_ALIGN(32) = { 0 };
    GXTexObj texture;
    GXInitTexObj(&texture, image, 8, 8, GX_TF_I8, GX_REPEAT, GX_CLAMP,
                 GX_FALSE);
    GXLoadTexObj(&texture, GX_TEXMAP0);

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXPosition3f32(1.0F, 0.0F, 0.0F);
    GXPosition3f32(0.0F, 1.0F, 0.0F);
    GXEnd();

    REQUIRE(melee_host_gx_captured_texture_count() == 1);
    MeleeHostGxTextureDesc desc{};
    REQUIRE(melee_host_gx_captured_texture_at(0, &desc));
    REQUIRE(desc.image == image);
    REQUIRE(desc.width == 8);
    REQUIRE(desc.wrap_s == GX_REPEAT);
    REQUIRE(desc.wrap_t == GX_CLAMP);

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE((vertex.attributes & MELEE_HOST_GX_VERTEX_TEXTURE_IMAGE) != 0);
    REQUIRE(vertex.texture_image == 0);

    // An untextured draw must not inherit the binding by leaving the id at a
    // value that is also a valid index.
    melee_host_gx_reset_command_log();
    melee_host_gx_state_reset();
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXPosition3f32(1.0F, 0.0F, 0.0F);
    GXPosition3f32(0.0F, 1.0F, 0.0F);
    GXEnd();
    REQUIRE(melee_host_gx_captured_texture_count() == 0);
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE((vertex.attributes & MELEE_HOST_GX_VERTEX_TEXTURE_IMAGE) == 0);
}

TEST_CASE("a captured indexed texture keeps the palette of its own draw")
{
    // The match draws many colour-indexed textures through GX_TLUT0, and the
    // capture is read after the frame's last draw, when the name holds the
    // last palette loaded under it.
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    static u8 image[32] ATTRIBUTE_ALIGN(32) = { 0 };
    static u8 first_palette[4] = { 0x80, 0x00, 0xFF, 0xFF };
    static u8 second_palette[8] = { 0 };
    GXTlutObj tlut;
    GXTexObj texture;
    GXInitTexObjCI(&texture, image, 8, 4, GX_TF_C8, GX_CLAMP, GX_CLAMP,
                   GX_FALSE, GX_TLUT0);
    GXLoadTexObj(&texture, GX_TEXMAP0);

    const auto draw = []() {
        GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
        GXPosition3f32(0.0F, 0.0F, 0.0F);
        GXPosition3f32(1.0F, 0.0F, 0.0F);
        GXPosition3f32(0.0F, 1.0F, 0.0F);
        GXEnd();
    };
    GXInitTlutObj(&tlut, first_palette, GX_TL_RGB5A3, 2);
    GXLoadTlut(&tlut, GX_TLUT0);
    draw();
    GXInitTlutObj(&tlut, second_palette, GX_TL_IA8, 4);
    GXLoadTlut(&tlut, GX_TLUT0);
    draw();
    // Drawn again with the second palette, the same texture is not added.
    draw();

    REQUIRE(melee_host_gx_captured_texture_count() == 2);
    MeleeHostGxTlutDesc first{};
    REQUIRE(melee_host_gx_captured_texture_tlut(0, &first));
    REQUIRE(first.entries == first_palette);
    REQUIRE(first.entry_count == 2);
    REQUIRE(first.format == GX_TL_RGB5A3);
    MeleeHostGxTlutDesc second{};
    REQUIRE(melee_host_gx_captured_texture_tlut(1, &second));
    REQUIRE(second.entries == second_palette);
    REQUIRE(second.entry_count == 4);
    REQUIRE(second.format == GX_TL_IA8);

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.texture_image == 0);
    REQUIRE(melee_host_gx_captured_vertex_at(3, &vertex));
    REQUIRE(vertex.texture_image == 1);
    REQUIRE(melee_host_gx_captured_vertex_at(6, &vertex));
    REQUIRE(vertex.texture_image == 1);
}

TEST_CASE("draws are grouped by the pixel state they ran under")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    const auto draw = []() {
        GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
        GXPosition3f32(0.0F, 0.0F, 0.0F);
        GXPosition3f32(1.0F, 0.0F, 0.0F);
        GXPosition3f32(0.0F, 1.0F, 0.0F);
        GXEnd();
    };

    // An opaque draw, then a translucent one, which is the pair HSD's render
    // modes produce.
    GXSetCullMode(GX_CULL_BACK);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    draw();

    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
                   GX_LO_CLEAR);
    draw();

    // A third draw repeating the first state must reuse its entry rather than
    // add another.
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    draw();

    REQUIRE(melee_host_gx_captured_draw_state_count() == 2);
    REQUIRE(melee_host_gx_triangle_count() == 3);

    MeleeHostGxDrawState opaque{};
    MeleeHostGxDrawState blended{};
    REQUIRE(melee_host_gx_captured_draw_state_at(0, &opaque));
    REQUIRE(melee_host_gx_captured_draw_state_at(1, &blended));
    REQUIRE(opaque.cull_mode == GX_CULL_BACK);
    REQUIRE(opaque.blend_mode == GX_BM_NONE);
    REQUIRE(opaque.z_update_enable);
    REQUIRE(opaque.z_func == GX_LEQUAL);
    REQUIRE(blended.blend_mode == GX_BM_BLEND);
    REQUIRE(blended.blend_src_factor == GX_BL_SRCALPHA);
    REQUIRE(blended.blend_dst_factor == GX_BL_INVSRCALPHA);
    REQUIRE(!blended.z_update_enable);

    MeleeHostGxCapturedTriangle triangle{};
    REQUIRE(melee_host_gx_captured_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].draw_state == 0);
    REQUIRE(melee_host_gx_captured_triangle_at(1, &triangle));
    REQUIRE(triangle.vertices[0].draw_state == 1);
    REQUIRE(melee_host_gx_captured_triangle_at(2, &triangle));
    REQUIRE(triangle.vertices[0].draw_state == 0);
}

TEST_CASE("draws retain and distinguish their indirect texture state")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    const auto draw = []() {
        GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
        GXPosition3f32(0.0F, 0.0F, 0.0F);
        GXPosition3f32(1.0F, 0.0F, 0.0F);
        GXPosition3f32(0.0F, 1.0F, 0.0F);
        GXEnd();
    };

    static u8 normal_image[64] ATTRIBUTE_ALIGN(32) = { 0 };
    static u8 copied_efb[64] ATTRIBUTE_ALIGN(32) = { 0 };
    GXTexObj normal;
    GXTexObj copied;
    GXInitTexObj(&normal, normal_image, 8, 8, GX_TF_I8, GX_REPEAT,
                 GX_REPEAT, GX_FALSE);
    GXInitTexObj(&copied, copied_efb, 8, 8, GX_TF_I8, GX_CLAMP, GX_CLAMP,
                 GX_FALSE);
    GXLoadTexObj(&normal, GX_TEXMAP0);
    GXLoadTexObj(&copied, GX_TEXMAP1);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);

    f32 matrix[2][3] = {
        { 0.125F, 0.0F, 0.0F },
        { 0.0F, 0.125F, 0.0F },
    };
    GXSetNumIndStages(1);
    GXSetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP0);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
    GXSetIndTexMtx(GX_ITM_0, matrix, 1);
    GXSetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST,
                     GX_ITM_0, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE,
                     GX_ITBA_OFF);
    draw();

    // The same state is interned, while a direct stage is a distinct draw
    // state even though all pixel-state fields are unchanged.
    draw();
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumIndStages(0);
    draw();

    REQUIRE(melee_host_gx_captured_draw_state_count() == 2);
    MeleeHostGxDrawState refracted{};
    MeleeHostGxDrawState direct{};
    REQUIRE(melee_host_gx_captured_draw_state_at(0, &refracted));
    REQUIRE(melee_host_gx_captured_draw_state_at(1, &direct));
    REQUIRE(refracted.indirect.stage_count == 1);
    REQUIRE(refracted.indirect.stages[0].texcoord == GX_TEXCOORD0);
    REQUIRE(refracted.indirect.stages[0].texmap == GX_TEXMAP0);
    REQUIRE(refracted.indirect.tev_stages[0].indirect);
    REQUIRE(refracted.indirect.tev_stages[0].bias == GX_ITB_ST);
    REQUIRE(std::fabs(refracted.indirect.matrices[0].offset[0][0] - 0.125F) <
            1.0e-6F);
    REQUIRE(refracted.indirect.matrices[0].scale_exp == 1);
    REQUIRE(direct.indirect.stage_count == 0);
    REQUIRE(!direct.indirect.tev_stages[0].indirect);

    std::array<mh_u32, MELEE_HOST_GX_MAX_TEXMAP> textures{};
    REQUIRE(melee_host_gx_captured_texture_set_at(0, textures.data()));
    MeleeHostGxTextureDesc texture{};
    REQUIRE(melee_host_gx_captured_texture_at(textures[GX_TEXMAP0],
                                               &texture));
    REQUIRE(texture.image == normal_image);
    REQUIRE(melee_host_gx_captured_texture_at(textures[GX_TEXMAP1],
                                               &texture));
    REQUIRE(texture.image == copied_efb);

    MeleeHostGxCapturedTriangle triangle{};
    REQUIRE(melee_host_gx_captured_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].draw_state == 0);
    REQUIRE(melee_host_gx_captured_triangle_at(1, &triangle));
    REQUIRE(triangle.vertices[0].draw_state == 0);
    REQUIRE(melee_host_gx_captured_triangle_at(2, &triangle));
    REQUIRE(triangle.vertices[0].draw_state == 1);
}

TEST_CASE("draws carry the projection, viewport and scissor they ran under")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    const auto draw = []() {
        GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
        GXPosition3f32(0.0F, 0.0F, -1.0F);
        GXPosition3f32(1.0F, 0.0F, -1.0F);
        GXPosition3f32(0.0F, 1.0F, -1.0F);
        GXEnd();
    };

    f32 perspective[4][4] = {
        { 1.5F, 0.0F, 0.25F, 0.0F },
        { 0.0F, 2.0F, 0.5F, 0.0F },
        { 0.0F, 0.0F, -0.01F, -1.01F },
        { 0.0F, 0.0F, -1.0F, 0.0F },
    };
    GXSetProjection(perspective, GX_PERSPECTIVE);
    GXSetViewport(0.0F, 0.0F, 640.0F, 480.0F, 0.0F, 1.0F);
    GXSetScissor(0, 0, 640, 480);
    draw();

    // A second camera in the same frame: an overlay in its own box.
    GXSetViewport(20.0F, 40.0F, 320.0F, 240.0F, 0.0F, 1.0F);
    GXSetScissor(20, 40, 320, 240);
    draw();

    // Back to the first view, which must reuse its entry.
    GXSetViewport(0.0F, 0.0F, 640.0F, 480.0F, 0.0F, 1.0F);
    GXSetScissor(0, 0, 640, 480);
    draw();

    REQUIRE(melee_host_gx_captured_view_state_count() == 2);
    MeleeHostGxViewState full{};
    MeleeHostGxViewState overlay{};
    REQUIRE(melee_host_gx_captured_view_state_at(0, &full));
    REQUIRE(melee_host_gx_captured_view_state_at(1, &overlay));
    REQUIRE(!melee_host_gx_captured_view_state_at(2, &overlay));
    REQUIRE(full.projection_type == GX_PERSPECTIVE);
    REQUIRE(full.projection[0] == 1.5F);
    REQUIRE(full.projection[1] == 0.25F);
    REQUIRE(full.projection[3] == 0.5F);
    REQUIRE(full.projection[5] == -1.01F);
    REQUIRE(full.viewport_width == 640.0F);
    REQUIRE(full.viewport_far == 1.0F);
    REQUIRE(full.scissor_height == 480);
    REQUIRE(overlay.viewport_left == 20.0F);
    REQUIRE(overlay.viewport_top == 40.0F);
    REQUIRE(overlay.scissor_width == 320);
    REQUIRE(overlay.scissor_height == 240);

    MeleeHostGxCapturedTriangle triangle{};
    REQUIRE(melee_host_gx_captured_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].view_state == 0);
    REQUIRE(melee_host_gx_captured_triangle_at(1, &triangle));
    REQUIRE(triangle.vertices[0].view_state == 1);
    REQUIRE(melee_host_gx_captured_triangle_at(2, &triangle));
    REQUIRE(triangle.vertices[0].view_state == 0);
}

TEST_CASE("a captured state carries the alpha compare the draw ran under")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    // The shape a cutout material uses: keep what passes one threshold, with
    // the second comparison left open.
    GXSetAlphaCompare(GX_GREATER, 128, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXPosition3f32(1.0F, 0.0F, 0.0F);
    GXPosition3f32(0.0F, 1.0F, 0.0F);
    GXEnd();

    MeleeHostGxDrawState state{};
    REQUIRE(melee_host_gx_captured_draw_state_count() == 1);
    REQUIRE(melee_host_gx_captured_draw_state_at(0, &state));
    REQUIRE(state.alpha_compare_0 == GX_GREATER);
    REQUIRE(state.alpha_ref_0 == 128);
    REQUIRE(state.alpha_op == GX_AOP_AND);
    REQUIRE(state.alpha_compare_1 == GX_ALWAYS);
    REQUIRE(state.color_update_enable);
    REQUIRE(!state.alpha_update_enable);
}

TEST_CASE("the alpha test reduces to the comparison that carries information")
{
    MeleeHostGxDrawState state{};
    mh_u32 compare = 0;
    mh_u8 reference = 0;

    // Both sides open: no test at all.
    state.alpha_compare_0 = GX_ALWAYS;
    state.alpha_compare_1 = GX_ALWAYS;
    state.alpha_op = GX_AOP_AND;
    REQUIRE(!melee_host_gx_resolve_alpha_test(&state, &compare, &reference));

    // The pair the game writes for a cutout: the same comparison twice.
    state.alpha_compare_0 = GX_GREATER;
    state.alpha_ref_0 = 0;
    state.alpha_compare_1 = GX_GREATER;
    state.alpha_ref_1 = 0;
    REQUIRE(melee_host_gx_resolve_alpha_test(&state, &compare, &reference));
    REQUIRE(compare == GX_GREATER);
    REQUIRE(reference == 0);

    // A band whose upper half is open, because alpha never exceeds 255.
    state.alpha_compare_0 = GX_GEQUAL;
    state.alpha_ref_0 = 102;
    state.alpha_compare_1 = GX_LEQUAL;
    state.alpha_ref_1 = 255;
    REQUIRE(melee_host_gx_resolve_alpha_test(&state, &compare, &reference));
    REQUIRE(compare == GX_GEQUAL);
    REQUIRE(reference == 102);

    // The same band written the other way round.
    state.alpha_compare_0 = GX_LEQUAL;
    state.alpha_ref_0 = 255;
    state.alpha_compare_1 = GX_GEQUAL;
    state.alpha_ref_1 = 102;
    REQUIRE(melee_host_gx_resolve_alpha_test(&state, &compare, &reference));
    REQUIRE(compare == GX_GEQUAL);
    REQUIRE(reference == 102);

    // A lower bound of zero is open too.
    state.alpha_compare_0 = GX_GEQUAL;
    state.alpha_ref_0 = 0;
    state.alpha_compare_1 = GX_LESS;
    state.alpha_ref_1 = 64;
    REQUIRE(melee_host_gx_resolve_alpha_test(&state, &compare, &reference));
    REQUIRE(compare == GX_LESS);
    REQUIRE(reference == 64);
}

TEST_CASE("draws under different material programs are captured separately")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    const auto draw = []() {
        GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
        GXPosition3f32(0.0F, 0.0F, 0.0F);
        GXPosition3f32(1.0F, 0.0F, 0.0F);
        GXPosition3f32(0.0F, 1.0F, 0.0F);
        GXEnd();
    };

    GXSetNumTevStages(1);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC,
                    GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                    GX_TRUE, GX_TEVPREV);
    draw();

    // The same stage with a different konst colour is a different program.
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_KONST, GX_CC_RASC,
                    GX_CC_ZERO);
    draw();
    // And repeating the first one must reuse its entry.
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC,
                    GX_CC_ZERO);
    draw();

    REQUIRE(melee_host_gx_captured_tev_state_count() == 2);
    MeleeHostGxCapturedTriangle triangle{};
    REQUIRE(melee_host_gx_captured_triangle_at(0, &triangle));
    REQUIRE(triangle.vertices[0].tev_state == 0);
    REQUIRE(melee_host_gx_captured_triangle_at(1, &triangle));
    REQUIRE(triangle.vertices[0].tev_state == 1);
    REQUIRE(melee_host_gx_captured_triangle_at(2, &triangle));
    REQUIRE(triangle.vertices[0].tev_state == 0);
}

namespace {

void push_be_u32(std::vector<u8>* out, u32 value)
{
    out->push_back(static_cast<u8>(value >> 24U));
    out->push_back(static_cast<u8>(value >> 16U));
    out->push_back(static_cast<u8>(value >> 8U));
    out->push_back(static_cast<u8>(value));
}

void push_be_f32(std::vector<u8>* out, f32 value)
{
    push_be_u32(out, std::bit_cast<u32>(value));
}

} // namespace

TEST_CASE("bump texgen projects its selected light onto the tangent basis")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    GXSetNumTexGens(2);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetTexCoordGen(GX_TEXCOORD1, GX_TG_BUMP0, GX_TG_TEXCOORD0,
                     GX_IDENTITY);
    GXLightObj light{};
    GXInitLightPos(&light, 1.0F, 1.0F, 0.0F);
    GXLoadLightObjImm(&light, GX_LIGHT0);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_NBT, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NBT, GX_NRM_NBT, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    std::vector<u8> list;
    list.push_back(static_cast<u8>(static_cast<u8>(GX_POINTS) |
                                   static_cast<u8>(GX_VTXFMT0)));
    list.push_back(0);
    list.push_back(1);
    // Position, normal, tangent, binormal and the base texture coordinate.
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 1.0F);
    push_be_f32(&list, 1.0F);
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 1.0F);
    push_be_f32(&list, 0.0F);
    push_be_f32(&list, 0.25F);
    push_be_f32(&list, 0.5F);
    GXCallDisplayList(list.data(), static_cast<u32>(list.size()));
    GXClearVtxDesc();

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_display_list_error_count() == 0);
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    const f32 projected_light = 1.0F / std::sqrt(2.0F);
    REQUIRE(std::fabs(vertex.texgen[0][0] - 0.25F) < 1.0e-5F);
    REQUIRE(std::fabs(vertex.texgen[0][1] - 0.5F) < 1.0e-5F);
    REQUIRE(std::fabs(vertex.texgen[1][0] - (0.25F + projected_light)) <
            1.0e-5F);
    REQUIRE(std::fabs(vertex.texgen[1][1] - (0.5F + projected_light)) <
            1.0e-5F);
    REQUIRE(vertex.texgen[1][2] == 1.0F);
}

TEST_CASE("texgen carries a coordinate through the post-transform matrix")
{
    // HSD keeps every texture transform in a GX_PTTEXMTXn matrix and asks for
    // GX_IDENTITY first, so this is the path every textured material takes.
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY,
                      GX_FALSE, GX_PTTEXMTX0);
    f32 post[3][4] = {
        { 2.0F, 0.0F, 0.0F, 0.5F },
        { 0.0F, 3.0F, 0.0F, 0.0F },
        { 0.0F, 0.0F, 1.0F, 0.0F },
    };
    GXLoadTexMtxImm(post, GX_PTTEXMTX0, GX_MTX3x4);

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    for (int corner = 0; corner < 3; ++corner) {
        GXPosition3f32(0.0F, 0.0F, 0.0F);
        GXTexCoord2f32(0.25F, 0.5F);
    }
    GXEnd();

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.texgen[0][0] == 1.0F);
    REQUIRE(vertex.texgen[0][1] == 1.5F);
    REQUIRE(vertex.texgen[0][2] == 1.0F);
    // The raw coordinate is still what the stream carried.
    REQUIRE(vertex.texcoord[0] == 0.25F);
}

TEST_CASE("a normal texgen transforms the raw normal and normalizes it")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX3x4, GX_TG_NRM, GX_TEXMTX0,
                      GX_TRUE, GX_PTIDENTITY);
    f32 stretch[3][4] = {
        { 4.0F, 0.0F, 0.0F, 0.0F },
        { 0.0F, 1.0F, 0.0F, 0.0F },
        { 0.0F, 0.0F, 1.0F, 0.0F },
    };
    GXLoadTexMtxImm(stretch, GX_TEXMTX0, GX_MTX3x4);
    // The position matrix moves the vertex but texgen reads the raw normal.
    f32 position[3][4] = {
        { 0.0F, 0.0F, 1.0F, 5.0F },
        { 0.0F, 1.0F, 0.0F, 0.0F },
        { -1.0F, 0.0F, 0.0F, 0.0F },
    };
    GXLoadPosMtxImm(position, GX_PNMTX0);
    GXLoadNrmMtxImm(position, GX_PNMTX0);

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    for (int corner = 0; corner < 3; ++corner) {
        GXPosition3f32(0.0F, 0.0F, 0.0F);
        GXNormal3f32(1.0F, 1.0F, 0.0F);
    }
    GXEnd();

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    const f32 length = std::sqrt(17.0F);
    REQUIRE(std::fabs(vertex.texgen[0][0] - 4.0F / length) < 1.0e-5F);
    REQUIRE(std::fabs(vertex.texgen[0][1] - 1.0F / length) < 1.0e-5F);
    REQUIRE(std::fabs(vertex.texgen[0][2]) < 1.0e-5F);
    REQUIRE(vertex.position.x == 5.0F);
}

TEST_CASE("a normal matrix does not replace the position matrix of its id")
{
    // HSD loads a lit PObj's position matrix and then, under the same id, its
    // inverse transpose, which has no translation.  GX keeps normal matrices
    // in their own memory; sharing the rows put every lit vertex at the
    // camera.
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();
    f32 position[3][4] = {
        { 1.0F, 0.0F, 0.0F, 10.0F },
        { 0.0F, 1.0F, 0.0F, 20.0F },
        { 0.0F, 0.0F, 1.0F, -300.0F },
    };
    f32 normal[3][4] = {
        { 1.0F, 0.0F, 0.0F, 0.0F },
        { 0.0F, 1.0F, 0.0F, 0.0F },
        { 0.0F, 0.0F, 1.0F, 0.0F },
    };
    GXSetCurrentMtx(GX_PNMTX0);
    GXLoadPosMtxImm(position, GX_PNMTX0);
    GXLoadNrmMtxImm(normal, GX_PNMTX0);

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(1.0F, 2.0F, 3.0F);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXPosition3f32(0.0F, 1.0F, 0.0F);
    GXEnd();

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.position.x == 11.0F);
    REQUIRE(vertex.position.y == 22.0F);
    REQUIRE(vertex.position.z == -297.0F);
    MeleeHostGxAffineTransform loaded{};
    REQUIRE(melee_host_gx_matrix(GX_PNMTX0, &loaded));
    REQUIRE(loaded.values[2][3] == -300.0F);
}

TEST_CASE("a texture matrix index in the stream applies to one vertex")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0,
                      GX_FALSE, GX_PTIDENTITY);
    f32 shift[3][4] = {
        { 1.0F, 0.0F, 0.0F, 1.0F },
        { 0.0F, 1.0F, 0.0F, 0.0F },
        { 0.0F, 0.0F, 1.0F, 0.0F },
    };
    GXLoadTexMtxImm(shift, GX_TEXMTX1, GX_MTX3x4);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_TEX0MTXIDX, GX_DIRECT);
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    std::vector<u8> list{ static_cast<u8>(GX_DRAW_TRIANGLES), 0x00, 0x03 };
    const std::array<u8, 3> matrices{ GX_TEXMTX1, GX_TEXMTX0, GX_IDENTITY };
    for (u8 matrix : matrices) {
        list.push_back(matrix);
        push_be_f32(&list, 0.0F);
        push_be_f32(&list, 0.0F);
        push_be_f32(&list, 0.0F);
        push_be_f32(&list, 0.25F);
        push_be_f32(&list, 0.75F);
    }
    GXCallDisplayList(list.data(), static_cast<u32>(list.size()));
    GXClearVtxDesc();

    REQUIRE(melee_host_gx_display_list_error_count() == 0);
    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.texgen[0][0] == 1.25F);
    REQUIRE(vertex.texgen[0][1] == 0.75F);
    // GX_TEXMTX0 was never loaded, which reads as identity like GX_IDENTITY.
    REQUIRE(melee_host_gx_captured_vertex_at(1, &vertex));
    REQUIRE(vertex.texgen[0][0] == 0.25F);
    REQUIRE(melee_host_gx_captured_vertex_at(2, &vertex));
    REQUIRE(vertex.texgen[0][0] == 0.25F);
}

TEST_CASE("a draw's texture set names the texture of each map it samples")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    static u8 first_image[32] ATTRIBUTE_ALIGN(32) = { 0 };
    static u8 second_image[32] ATTRIBUTE_ALIGN(32) = { 0 };
    static u8 unused_image[32] ATTRIBUTE_ALIGN(32) = { 0 };
    GXTexObj first;
    GXTexObj second;
    GXTexObj unused;
    GXInitTexObj(&first, first_image, 8, 8, GX_TF_I8, GX_REPEAT, GX_REPEAT,
                 GX_FALSE);
    GXInitTexObj(&second, second_image, 8, 8, GX_TF_I8, GX_REPEAT, GX_REPEAT,
                 GX_FALSE);
    GXInitTexObj(&unused, unused_image, 8, 8, GX_TF_I8, GX_REPEAT, GX_REPEAT,
                 GX_FALSE);
    GXLoadTexObj(&first, GX_TEXMAP0);
    GXLoadTexObj(&second, GX_TEXMAP2);
    GXLoadTexObj(&unused, GX_TEXMAP5);
    GXSetNumTevStages(2);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP2, GX_COLOR0A0);

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(0.0F, 0.0F, 0.0F);
    GXPosition3f32(1.0F, 0.0F, 0.0F);
    GXPosition3f32(0.0F, 1.0F, 0.0F);
    GXEnd();

    REQUIRE(melee_host_gx_captured_texture_set_count() == 1);
    std::array<mh_u32, MELEE_HOST_GX_MAX_TEXMAP> set{};
    REQUIRE(melee_host_gx_captured_texture_set_at(0, set.data()));
    MeleeHostGxTextureDesc desc{};
    REQUIRE(melee_host_gx_captured_texture_at(set[0], &desc));
    REQUIRE(desc.image == first_image);
    REQUIRE(melee_host_gx_captured_texture_at(set[2], &desc));
    REQUIRE(desc.image == second_image);
    // A map no stage samples stays out of the set, bound or not.
    REQUIRE(set[1] == MELEE_HOST_GX_NO_TEXTURE);
    REQUIRE(set[5] == MELEE_HOST_GX_NO_TEXTURE);

    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.texture_set == 0);
}

TEST_CASE("without lighting channels the rasterized colour is the vertex's")
{
    melee_host_gx_state_reset();
    melee_host_gx_reset_command_log();

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    for (int corner = 0; corner < 3; ++corner) {
        GXPosition3f32(0.0F, 0.0F, 0.0F);
        GXColor4u8(10, 20, 30, 40);
    }
    GXEnd();
    MeleeHostGxCapturedVertex vertex{};
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.raster_color[0][0] == 10);
    REQUIRE(vertex.raster_color[0][3] == 40);
    // With fewer than two channels the second repeats the first.
    REQUIRE(vertex.raster_color[1][2] == 30);

    // One unlit channel whose material is a register: the first colour is
    // that register, and the second still repeats it.
    melee_host_gx_reset_command_log();
    GXSetNumChans(1);
    GXSetChanMatColor(GX_COLOR0A0, GXColor{ 90, 80, 70, 60 });
    GXSetChanAmbColor(GX_COLOR0A0, GXColor{ 0, 0, 0, 0 });
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0,
                  GX_DF_NONE, GX_AF_NONE);
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    for (int corner = 0; corner < 3; ++corner) {
        GXPosition3f32(0.0F, 0.0F, 0.0F);
        GXColor4u8(10, 20, 30, 40);
    }
    GXEnd();
    REQUIRE(melee_host_gx_captured_vertex_at(0, &vertex));
    REQUIRE(vertex.raster_color[1][0] == vertex.raster_color[0][0]);
}
