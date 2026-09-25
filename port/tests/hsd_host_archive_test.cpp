#include "test.hpp"

#include "assets/hsd_materialize.hpp"

#include <melee_host/baselib.h>
#include <melee_host/boot.h>
#include <melee_host/hsd_archive.h>
#include "hsd_include.hpp"
MELEE_HOST_TEST_HSD_BEGIN
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <sysdolphin/baselib/particle.h>
#include <sysdolphin/baselib/psstructs.h>
MELEE_HOST_TEST_HSD_END

MELEE_HOST_HSD_BEGIN
#include <sysdolphin/baselib/archive.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/fog.h>
#include <sysdolphin/baselib/jobj.h>
#include <sysdolphin/baselib/lobj.h>
#include <sysdolphin/baselib/object.h>
/* lobj.h declares the class as hsdLobj, but lobj.c defines hsdLObj; nothing
 * in the game refers to it by the header's spelling. */
extern HSD_LObjInfo hsdLObj;
MELEE_HOST_HSD_END

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

/* Lays out an archive the way the tools that built the game's files did:
 * header, data section, relocation table, public and extern tables, then the
 * names they point at. */
class ArchiveBuilder final {
public:
    explicit ArchiveBuilder(std::uint32_t data_size) : data_(data_size) {}

    void u8(std::uint32_t at, std::uint8_t value)
    {
        data_.at(at) = static_cast<std::byte>(value);
    }

    void u16(std::uint32_t at, std::uint16_t value)
    {
        u8(at, static_cast<std::uint8_t>(value >> 8U));
        u8(at + 1, static_cast<std::uint8_t>(value));
    }

    void u32(std::uint32_t at, std::uint32_t value)
    {
        u8(at, static_cast<std::uint8_t>(value >> 24U));
        u8(at + 1, static_cast<std::uint8_t>(value >> 16U));
        u8(at + 2, static_cast<std::uint8_t>(value >> 8U));
        u8(at + 3, static_cast<std::uint8_t>(value));
    }

    void f32(std::uint32_t at, float value)
    {
        u32(at, std::bit_cast<std::uint32_t>(value));
    }

    /* A pointer field is an offset plus the relocation entry that makes it
     * one; a field without the entry must read as NULL. */
    void pointer(std::uint32_t at, std::uint32_t target)
    {
        u32(at, target);
        relocations_.push_back(at);
    }

    void public_symbol(std::uint32_t at, std::string name)
    {
        publics_.emplace_back(at, std::move(name));
    }

    void extern_symbol(std::uint32_t chain_head, std::string name)
    {
        externs_.emplace_back(chain_head, std::move(name));
    }

    [[nodiscard]] std::vector<std::byte> build() const
    {
        std::string names;
        std::vector<std::uint32_t> public_names;
        std::vector<std::uint32_t> extern_names;
        for (const auto& [offset, name] : publics_) {
            static_cast<void>(offset);
            public_names.push_back(static_cast<std::uint32_t>(names.size()));
            names += name;
            names.push_back('\0');
        }
        for (const auto& [offset, name] : externs_) {
            static_cast<void>(offset);
            extern_names.push_back(static_cast<std::uint32_t>(names.size()));
            names += name;
            names.push_back('\0');
        }

        const std::size_t file_size =
            0x20 + data_.size() + relocations_.size() * 4 +
            (publics_.size() + externs_.size()) * 8 + names.size();
        std::vector<std::byte> out;
        out.reserve(file_size);
        const auto put32 = [&out](std::size_t value) {
            const auto word = static_cast<std::uint32_t>(value);
            out.push_back(static_cast<std::byte>(word >> 24U));
            out.push_back(static_cast<std::byte>(word >> 16U));
            out.push_back(static_cast<std::byte>(word >> 8U));
            out.push_back(static_cast<std::byte>(word));
        };
        put32(file_size);
        put32(data_.size());
        put32(relocations_.size());
        put32(publics_.size());
        put32(externs_.size());
        put32(0x001B0000U);
        put32(0);
        put32(0);
        out.insert(out.end(), data_.begin(), data_.end());
        for (const std::uint32_t relocation : relocations_) {
            put32(relocation);
        }
        for (std::size_t index = 0; index < publics_.size(); ++index) {
            put32(publics_[index].first);
            put32(public_names[index]);
        }
        for (std::size_t index = 0; index < externs_.size(); ++index) {
            put32(externs_[index].first);
            put32(extern_names[index]);
        }
        for (const char letter : names) {
            out.push_back(static_cast<std::byte>(letter));
        }
        return out;
    }

private:
    std::vector<std::byte> data_;
    std::vector<std::uint32_t> relocations_;
    std::vector<std::pair<std::uint32_t, std::string>> publics_;
    std::vector<std::pair<std::uint32_t, std::string>> externs_;
};

/* Data offsets of the records a title-like archive holds, each on its own
 * boundary so a field written by mistake cannot land in another record. */
enum : std::uint32_t {
    kCamera = 0x000,
    kEye = 0x040,
    kInterest = 0x060,
    kLightTable = 0x080,
    kAmbientList = 0x090,
    kPointList = 0x0A0,
    kAmbientDesc = 0x0B0,
    kPointDesc = 0x0D0,
    kPointParameters = 0x0F0,
    kLightPosition = 0x100,
    kLightAnimTable = 0x120,
    kLightAnim = 0x130,
    kLightAObj = 0x140,
    kLightPositionAnim = 0x150,
    kFog = 0x160,
    kFogAdj = 0x180,
    kSprite = 0x1D0,
    kSpriteImage = 0x1E0,
    kSpriteTlut = 0x200,
    kImageBytes = 0x220,
    kPaletteBytes = 0x230,
    kJoint = 0x240,
    kDataSize = 0x280,
};

std::vector<std::byte> make_title_like_archive()
{
    ArchiveBuilder archive(kDataSize);

    // A perspective camera looking at the origin from +z.
    archive.u16(kCamera + 0x06, PROJ_PERSPECTIVE);
    archive.u16(kCamera + 0x0A, 640); // viewport xmax
    archive.u16(kCamera + 0x0E, 480); // viewport ymax
    archive.u16(kCamera + 0x12, 640); // scissor right
    archive.u16(kCamera + 0x16, 480); // scissor bottom
    archive.pointer(kCamera + 0x18, kEye);
    archive.pointer(kCamera + 0x1C, kInterest);
    archive.f32(kCamera + 0x28, 1.0F);    // near
    archive.f32(kCamera + 0x2C, 1000.0F); // far
    archive.f32(kCamera + 0x30, 30.0F);   // fov
    archive.f32(kCamera + 0x34, 1.18F);   // aspect
    archive.f32(kEye + 0x0C, 100.0F);     // eye z

    // Two light lists: an ambient light, and a point light carrying an
    // animation that also moves its position.
    archive.pointer(kLightTable + 0x00, kAmbientList);
    archive.pointer(kLightTable + 0x04, kPointList);
    archive.pointer(kAmbientList + 0x00, kAmbientDesc);
    archive.pointer(kPointList + 0x00, kPointDesc);
    archive.pointer(kPointList + 0x04, kLightAnimTable);

    archive.u16(kAmbientDesc + 0x08, 0x0004); // ambient, diffuse
    archive.u8(kAmbientDesc + 0x0C, 40);
    archive.u8(kAmbientDesc + 0x0D, 40);
    archive.u8(kAmbientDesc + 0x0E, 40);
    archive.u8(kAmbientDesc + 0x0F, 255);

    archive.u16(kPointDesc + 0x08, 0x0006); // point, diffuse
    archive.u8(kPointDesc + 0x0C, 200);
    archive.u8(kPointDesc + 0x0D, 180);
    archive.u8(kPointDesc + 0x0E, 160);
    archive.u8(kPointDesc + 0x0F, 255);
    archive.pointer(kPointDesc + 0x10, kLightPosition);
    archive.pointer(kPointDesc + 0x18, kPointParameters);
    archive.f32(kPointParameters + 0x00, 0.5F);   // ref_br
    archive.f32(kPointParameters + 0x04, 400.0F); // ref_dist
    archive.u32(kPointParameters + 0x08, 2);      // dist_func
    archive.f32(kLightPosition + 0x04, 10.0F);
    archive.f32(kLightPosition + 0x08, 20.0F);
    archive.f32(kLightPosition + 0x0C, 30.0F);

    archive.pointer(kLightAnimTable + 0x00, kLightAnim);
    archive.pointer(kLightAnim + 0x04, kLightAObj);
    archive.pointer(kLightAnim + 0x08, kLightPositionAnim);
    archive.f32(kLightAObj + 0x04, 30.0F); // end frame
    archive.pointer(kLightPositionAnim + 0x00, kLightAObj);

    // Linear fog with a range adjustment table.
    archive.u32(kFog + 0x00, 2);
    archive.pointer(kFog + 0x04, kFogAdj);
    archive.f32(kFog + 0x08, 80.0F);
    archive.f32(kFog + 0x0C, 300.0F);
    archive.u8(kFog + 0x10, 0x26);
    archive.u8(kFog + 0x11, 0x26);
    archive.u8(kFog + 0x12, 0x26);
    archive.u8(kFog + 0x13, 0xFF);
    archive.u16(kFogAdj + 0x00, 320);
    archive.u16(kFogAdj + 0x02, 640);
    for (std::uint32_t index = 0; index < 16; ++index) {
        archive.f32(kFogAdj + 0x04 + index * 4,
                    static_cast<float>(index) + 1.0F);
    }

    // A screen sprite: a 4x4 C4 image and its two-entry palette.
    archive.pointer(kSprite + 0x00, kSpriteImage);
    archive.pointer(kSprite + 0x04, kSpriteTlut);
    archive.pointer(kSpriteImage + 0x00, kImageBytes);
    archive.u16(kSpriteImage + 0x04, 4);
    archive.u16(kSpriteImage + 0x06, 4);
    archive.u32(kSpriteImage + 0x08, GX_TF_C4);
    archive.pointer(kSpriteTlut + 0x00, kPaletteBytes);
    archive.u32(kSpriteTlut + 0x04, GX_TL_RGB565);
    archive.u16(kSpriteTlut + 0x0C, 2);
    for (std::uint32_t index = 0; index < 8; ++index) {
        archive.u8(kImageBytes + index,
                   static_cast<std::uint8_t>(0x01 + index * 0x22));
    }
    archive.u16(kPaletteBytes + 0x00, 0xF800);
    archive.u16(kPaletteBytes + 0x02, 0x07E0);

    // A joint whose child and matrix are both references to a joint another
    // archive provides.  Neither is relocated: each holds the next link of
    // the extern chain, and the last holds -1.
    archive.u32(kJoint + 0x08, kJoint + 0x38);
    archive.f32(kJoint + 0x20, 1.0F);
    archive.f32(kJoint + 0x24, 1.0F);
    archive.f32(kJoint + 0x28, 1.0F);
    archive.u32(kJoint + 0x38, 0xFFFFFFFFU);

    archive.public_symbol(kCamera, "test_cam_int1_camera");
    archive.public_symbol(kLightTable, "test_scene_lights");
    archive.public_symbol(kFog, "test_fog");
    archive.public_symbol(kSprite, "test_sobjdesc");
    archive.public_symbol(kJoint, "test_joint");
    archive.public_symbol(kCamera, "test_scene_widget");
    archive.extern_symbol(kJoint + 0x08, "shared_joint");
    return archive.build();
}

u8* bytes_of(std::vector<std::byte>& bytes)
{
    return reinterpret_cast<u8*>(bytes.data());
}

/* The loop lbArchive_InitializeDAT runs after every parse. */
void locate_externs_as_null(HSD_Archive* archive)
{
    for (int index = 0;; ++index) {
        const char* const symbol = HSD_ArchiveGetExtern(archive, index);
        if (symbol == nullptr) {
            return;
        }
        HSD_ArchiveLocateExtern(archive, symbol, nullptr);
    }
}

MeleeHostHsdArchiveStats archive_stats()
{
    MeleeHostHsdArchiveStats stats{};
    REQUIRE(melee_host_hsd_archive_stats(&stats) == MELEE_HOST_OK);
    return stats;
}

} // namespace

TEST_CASE("the host archive exposes the header archive.c parses")
{
    std::vector<std::byte> bytes = make_title_like_archive();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    REQUIRE(archive.header.file_size == bytes.size());
    REQUIRE(archive.header.data_size == kDataSize);
    REQUIRE(archive.header.nb_public == 6);
    REQUIRE(archive.header.nb_extern == 1);
    REQUIRE(archive.data == bytes_of(bytes) + 0x20);
    REQUIRE(archive.top_ptr == bytes_of(bytes));
    // The tables stay big-endian, so nothing is handed a pointer to them.
    REQUIRE(archive.public_info == nullptr);
    REQUIRE(archive.reloc_info == nullptr);

    HSD_Archive truncated{};
    REQUIRE(HSD_ArchiveParse(&truncated, bytes_of(bytes), bytes.size() - 1) ==
            -1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

TEST_CASE("the host archive rebuilds a title screen's camera, lights, fog "
          "and sprite")
{
    REQUIRE(melee_host_baselib_bootstrap() == MELEE_HOST_OK);
    std::vector<std::byte> bytes = make_title_like_archive();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    locate_externs_as_null(&archive);

    auto* const camera = static_cast<HSD_CObjDesc*>(
        HSD_ArchiveGetPublicAddress(&archive, "test_cam_int1_camera"));
    REQUIRE(camera != nullptr);
    REQUIRE(camera->common.projection_type == PROJ_PERSPECTIVE);
    REQUIRE(camera->common.eyepos != nullptr);
    REQUIRE(camera->common.eyepos->pos.z == 100.0F);
    REQUIRE(camera->common.interest != nullptr);
    REQUIRE(camera->perspective.fov == 30.0F);
    HSD_CObj* const cobj = HSD_CObjLoadDesc(camera);
    REQUIRE(cobj != nullptr);
    hsdDelete(cobj);

    auto** const lights = static_cast<melee::assets::MaterializedLightList**>(
        HSD_ArchiveGetPublicAddress(&archive, "test_scene_lights"));
    REQUIRE(lights != nullptr);
    REQUIRE(lights[0] != nullptr);
    REQUIRE(lights[1] != nullptr);
    REQUIRE(lights[2] == nullptr);
    const HSD_LightDesc* const ambient = lights[0]->desc;
    REQUIRE(ambient != nullptr);
    REQUIRE((ambient->flags & LOBJ_TYPE_MASK) == LOBJ_AMBIENT);
    REQUIRE(ambient->color.r == 40);
    REQUIRE(ambient->position == nullptr);
    REQUIRE(lights[0]->anims == nullptr);
    const HSD_LightDesc* const point = lights[1]->desc;
    REQUIRE(point != nullptr);
    REQUIRE((point->flags & LOBJ_TYPE_MASK) == LOBJ_POINT);
    REQUIRE(point->position != nullptr);
    REQUIRE(point->position->pos.y == 20.0F);
    REQUIRE(point->u.point != nullptr);
    REQUIRE(point->u.point->ref_dist == 400.0F);
    REQUIRE(point->u.point->dist_func == 2);
    REQUIRE(lights[1]->anims != nullptr);
    REQUIRE(lights[1]->anims[0] != nullptr);
    REQUIRE(lights[1]->anims[1] == nullptr);
    const HSD_LightAnim* const light_anim = lights[1]->anims[0];
    REQUIRE(light_anim->aobjdesc != nullptr);
    REQUIRE(light_anim->aobjdesc->end_frame == 30.0F);
    REQUIRE(light_anim->position_anim != nullptr);
    // Both fields name the same record, so they share one descriptor.
    REQUIRE(light_anim->position_anim->aobjdesc == light_anim->aobjdesc);

    const u32 lobjs_before = hsdLObj.parent.parent.head.nb_exist;
    HSD_LObj* const ambient_lobj = HSD_LObjLoadDesc(lights[0]->desc);
    HSD_LObj* const point_lobj = HSD_LObjLoadDesc(lights[1]->desc);
    REQUIRE(ambient_lobj != nullptr);
    REQUIRE(point_lobj != nullptr);
    HSD_LObjAddAnimAll(point_lobj, lights[1]->anims[0]);
    REQUIRE(point_lobj->aobj != nullptr);
    REQUIRE(point_lobj->position != nullptr);
    REQUIRE(point_lobj->position->aobj != nullptr);
    HSD_LObjRemoveAll(point_lobj);
    HSD_LObjRemoveAll(ambient_lobj);
    REQUIRE(hsdLObj.parent.parent.head.nb_exist == lobjs_before);

    auto* const fog = static_cast<HSD_FogDesc*>(
        HSD_ArchiveGetPublicAddress(&archive, "test_fog"));
    REQUIRE(fog != nullptr);
    REQUIRE(fog->type == 2);
    REQUIRE(fog->start == 80.0F);
    REQUIRE(fog->end == 300.0F);
    REQUIRE(fog->color.r == 0x26);
    REQUIRE(fog->color.a == 0xFF);
    REQUIRE(fog->fogadjdesc != nullptr);
    REQUIRE(fog->fogadjdesc->center == 320);
    REQUIRE(fog->fogadjdesc->width == 640);
    REQUIRE(fog->fogadjdesc->mtx[2][3] == 12.0F);
    HSD_Fog* const fog_object = HSD_FogLoadDesc(fog);
    REQUIRE(fog_object != nullptr);
    REQUIRE(fog_object->end == 300.0F);
    REQUIRE(fog_object->fog_adj != nullptr);
    hsdDelete(fog_object);

    auto* const sprite = static_cast<HSD_SObjDesc*>(
        HSD_ArchiveGetPublicAddress(&archive, "test_sobjdesc"));
    REQUIRE(sprite != nullptr);
    REQUIRE(sprite->image != nullptr);
    REQUIRE(sprite->image->width == 4);
    REQUIRE(sprite->image->format == GX_TF_C4);
    REQUIRE(static_cast<const u8*>(sprite->image->image_ptr)[0] == 0x01);
    REQUIRE(sprite->tlut != nullptr);
    REQUIRE(sprite->tlut->n_entries == 2);
    REQUIRE(sprite->tlut->fmt == GX_TL_RGB565);
    // Palettes keep the file's byte order, which is what GX reads.
    REQUIRE(static_cast<const u8*>(sprite->tlut->lut)[0] == 0xF8);

    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

TEST_CASE("a public symbol comes back as the same descriptor every time")
{
    std::vector<std::byte> bytes = make_title_like_archive();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    locate_externs_as_null(&archive);

    const MeleeHostHsdArchiveStats before = archive_stats();
    void* const first =
        HSD_ArchiveGetPublicAddress(&archive, "test_cam_int1_camera");
    void* const second =
        HSD_ArchiveGetPublicAddress(&archive, "test_cam_int1_camera");
    REQUIRE(first != nullptr);
    REQUIRE(first == second);
    REQUIRE(archive_stats().symbols_translated ==
            before.symbols_translated + 1);

    // A second HSD_Archive over the same buffer is the same archive, which is
    // how ftdata.c's stack archives keep working after their frame is gone.
    HSD_Archive alias{};
    alias.top_ptr = bytes_of(bytes);
    REQUIRE(HSD_ArchiveGetPublicAddress(&alias, "test_cam_int1_camera") ==
            first);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

TEST_CASE("an extern resolved to NULL reads as a null pointer field")
{
    REQUIRE(melee_host_baselib_bootstrap() == MELEE_HOST_OK);
    std::vector<std::byte> bytes = make_title_like_archive();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);

    // Until the extern is located, the joint's child holds a chain link, a
    // non-zero value with no relocation, and the lookup refuses it.
    const MeleeHostHsdArchiveStats before = archive_stats();
    REQUIRE(HSD_ArchiveGetPublicAddress(&archive, "test_joint") == nullptr);
    const MeleeHostHsdArchiveStats refused = archive_stats();
    REQUIRE(refused.symbols_refused == before.symbols_refused + 1);

    // Parsing the buffer again starts over, as the console would.
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    REQUIRE(std::string_view(HSD_ArchiveGetExtern(&archive, 0)) ==
            "shared_joint");
    REQUIRE(HSD_ArchiveGetExtern(&archive, 1) == nullptr);
    REQUIRE(HSD_ArchiveGetExtern(&archive, -1) == nullptr);
    locate_externs_as_null(&archive);
    REQUIRE(archive_stats().extern_fields_nulled ==
            refused.extern_fields_nulled + 2);

    auto* const joint = static_cast<HSD_Joint*>(
        HSD_ArchiveGetPublicAddress(&archive, "test_joint"));
    REQUIRE(joint != nullptr);
    REQUIRE(joint->child == nullptr);
    REQUIRE(joint->mtx == nullptr);
    REQUIRE(joint->scale.x == 1.0F);
    HSD_JObj* const jobj = HSD_JObjLoadJoint(joint);
    REQUIRE(jobj != nullptr);
    HSD_JObjRemoveAll(jobj);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

TEST_CASE("a symbol the host has no translation for is refused, not guessed")
{
    std::vector<std::byte> bytes = make_title_like_archive();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    locate_externs_as_null(&archive);

    REQUIRE(HSD_ArchiveGetPublicAddress(&archive, "test_scene_widget") ==
            nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("test_scene_widget") != std::string_view::npos);
    REQUIRE(HSD_ArchiveGetPublicAddress(&archive, "missing_joint") == nullptr);

    HSD_Archive stranger{};
    REQUIRE(HSD_ArchiveGetPublicAddress(&stranger, "test_joint") == nullptr);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

TEST_CASE("an SIS text table points each entry at its verbatim string")
{
    // Two strings, and a first entry pointing at the end of the data, which
    // four of the game's text archives have.
    constexpr std::uint32_t kDataSize = 0x20;
    ArchiveBuilder builder(kDataSize);
    builder.pointer(0x00, kDataSize);
    builder.pointer(0x04, 0x0C);
    builder.pointer(0x08, 0x14);
    builder.u16(0x0C, 0x2041);
    builder.u16(0x14, 0x0C00);
    builder.public_symbol(0x00, "SIS_TestData");
    std::vector<std::byte> bytes = builder.build();

    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto** const table = static_cast<std::uint8_t**>(
        HSD_ArchiveGetPublicAddress(&archive, "SIS_TestData"));
    REQUIRE(table != nullptr);
    // The strings keep the file's byte order and their distance apart.
    REQUIRE(table[1][0] == 0x20);
    REQUIRE(table[1][1] == 0x41);
    REQUIRE(table[1][2] == 0x00);
    REQUIRE(table[2][0] == 0x0C);
    REQUIRE(table[2] - table[1] == 8);
    REQUIRE(table[0][0] == 0x00);
    REQUIRE(table[0][3] == 0x00);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

namespace {

struct TestGameData {
    std::uint16_t word;
    float number;
    std::uint8_t* bytes;
    TestGameData* next;
};

void* translate_test_game_data(MeleeHostHsdReader* reader, mh_u32 root)
{
    auto* const data = static_cast<TestGameData*>(melee_host_hsd_reader_allocate(
        reader, sizeof(TestGameData), alignof(TestGameData)));
    if (data == nullptr) {
        return nullptr;
    }
    data->word = melee_host_hsd_reader_u16(reader, root);
    data->number = melee_host_hsd_reader_f32(reader, root + 4);
    mh_u32 target = 0;
    if (melee_host_hsd_reader_pointer(reader, root + 8, &target)) {
        data->bytes = static_cast<std::uint8_t*>(
            melee_host_hsd_reader_payload(reader, target, 2));
    }
    if (melee_host_hsd_reader_pointer(reader, root + 12, &target)) {
        data->next = data;
    }
    return melee_host_hsd_reader_failed(reader) ? nullptr : data;
}

} // namespace

TEST_CASE("a translator registered by name reads the archive through the C "
          "reader")
{
    // Two records: the second holds a pointer-sized value no relocation
    // vouches for, which the reader must refuse rather than follow.
    ArchiveBuilder builder(0x30);
    builder.u16(0x00, 0x1234);
    builder.f32(0x04, 2.5F);
    builder.pointer(0x08, 0x10);
    builder.u16(0x10, 0xABCD);
    builder.u16(0x18, 0x0001);
    builder.pointer(0x20, 0x10);
    builder.u32(0x24, 0x11111111U);
    builder.public_symbol(0x00, "test_game_data");
    builder.public_symbol(0x18, "test_bad_game_data");
    std::vector<std::byte> bytes = builder.build();

    REQUIRE(melee_host_hsd_register_translator(
                "test_game_data", translate_test_game_data) == MELEE_HOST_OK);
    REQUIRE(melee_host_hsd_register_translator(
                "test_bad_game_data", translate_test_game_data) ==
            MELEE_HOST_OK);
    REQUIRE(melee_host_hsd_symbol_kind("test_game_data") ==
            MELEE_HOST_HSD_SYMBOL_GAME_DATA);

    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto* const data = static_cast<TestGameData*>(
        HSD_ArchiveGetPublicAddress(&archive, "test_game_data"));
    REQUIRE(data != nullptr);
    REQUIRE(data->word == 0x1234);
    REQUIRE(data->number == 2.5F);
    REQUIRE(data->bytes != nullptr);
    REQUIRE(data->bytes[0] == 0xAB);
    REQUIRE(data->bytes[1] == 0xCD);
    // A NULL pointer field reads as absent.
    REQUIRE(data->next == nullptr);

    REQUIRE(HSD_ArchiveGetPublicAddress(&archive, "test_bad_game_data") ==
            nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("no relocation") != std::string_view::npos);

    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
    REQUIRE(melee_host_hsd_register_translator("test_game_data", nullptr) ==
            MELEE_HOST_OK);
    REQUIRE(melee_host_hsd_register_translator("test_bad_game_data",
                                               nullptr) == MELEE_HOST_OK);
    REQUIRE(melee_host_hsd_symbol_kind("test_game_data") ==
            MELEE_HOST_HSD_SYMBOL_UNSUPPORTED);
}

TEST_CASE("the refraction table translates to a count and host floats")
{
    // LbRf.dat's layout: six floats, then the record with the count and a
    // pointer back to them.  The host record is lbrefract.c's private one.
    struct HostRefractData {
        std::uint8_t count;
        float* params;
    };
    const float floats[6] = { 0.0F, 0.0F, 0.0F, 0.1F, 0.2F, 5.0F };
    ArchiveBuilder builder(0x20);
    for (std::uint32_t i = 0; i < 6; ++i) {
        builder.f32(i * 4, floats[i]);
    }
    builder.u8(0x18, 3);
    builder.pointer(0x1C, 0x00);
    builder.public_symbol(0x18, "lbRefData");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    REQUIRE(melee_host_hsd_symbol_kind("lbRefData") ==
            MELEE_HOST_HSD_SYMBOL_GAME_DATA);

    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto* const data = static_cast<HostRefractData*>(
        HSD_ArchiveGetPublicAddress(&archive, "lbRefData"));
    REQUIRE(data != nullptr);
    REQUIRE(data->count == 3);
    REQUIRE(data->params != nullptr);
    for (std::size_t i = 0; i < 6; ++i) {
        REQUIRE(data->params[i] == floats[i]);
    }
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);

    // Without the pointer there is nothing to count; the table is refused.
    ArchiveBuilder empty(0x20);
    empty.u8(0x18, 3);
    empty.public_symbol(0x18, "lbRefData");
    std::vector<std::byte> empty_bytes = empty.build();
    HSD_Archive empty_archive{};
    REQUIRE(HSD_ArchiveParse(&empty_archive, bytes_of(empty_bytes),
                             empty_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&empty_archive, "lbRefData") ==
            nullptr);
    REQUIRE(melee_host_hsd_archive_release(empty_bytes.data()) ==
            MELEE_HOST_OK);

    REQUIRE(melee_host_hsd_register_translator("lbRefData", nullptr) ==
            MELEE_HOST_OK);
}

TEST_CASE("the player common data translates to a pointer and host words")
{
    // PdPm.dat's layout: the 0x184-byte table, then the pointer to it, which
    // is the public symbol.  Floats and integers convert in place; the four
    // bytes at +0xC0 keep their order.
    ArchiveBuilder builder(0x188);
    builder.f32(0x000, 2.5F);
    builder.u32(0x004, 6);
    builder.u32(0x0BC, 0x12345678U);
    builder.u8(0x0C0, 1);
    builder.u8(0x0C1, 2);
    builder.u8(0x0C2, 3);
    builder.u8(0x0C3, 4);
    builder.f32(0x180, 90.0F);
    builder.pointer(0x184, 0x000);
    builder.public_symbol(0x184, "plLoadCommonData");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    REQUIRE(melee_host_hsd_symbol_kind("plLoadCommonData") ==
            MELEE_HOST_HSD_SYMBOL_GAME_DATA);

    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto* const record = static_cast<unsigned char**>(
        HSD_ArchiveGetPublicAddress(&archive, "plLoadCommonData"));
    REQUIRE(record != nullptr);
    const unsigned char* const table = *record;
    REQUIRE(table != nullptr);
    float first = 0.0F;
    std::uint32_t count = 0;
    std::uint32_t before_bytes = 0;
    float last = 0.0F;
    std::memcpy(&first, table + 0x000, sizeof(first));
    std::memcpy(&count, table + 0x004, sizeof(count));
    std::memcpy(&before_bytes, table + 0x0BC, sizeof(before_bytes));
    std::memcpy(&last, table + 0x180, sizeof(last));
    REQUIRE(first == 2.5F);
    REQUIRE(count == 6);
    REQUIRE(before_bytes == 0x12345678U);
    REQUIRE(table[0x0C0] == 1);
    REQUIRE(table[0x0C1] == 2);
    REQUIRE(table[0x0C2] == 3);
    REQUIRE(table[0x0C3] == 4);
    REQUIRE(last == 90.0F);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);

    // A NULL pointer leaves no table to read; the symbol is refused.
    ArchiveBuilder empty(0x188);
    empty.public_symbol(0x184, "plLoadCommonData");
    std::vector<std::byte> empty_bytes = empty.build();
    HSD_Archive empty_archive{};
    REQUIRE(HSD_ArchiveParse(&empty_archive, bytes_of(empty_bytes),
                             empty_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&empty_archive, "plLoadCommonData") ==
            nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("pointer is NULL") != std::string_view::npos);
    REQUIRE(melee_host_hsd_archive_release(empty_bytes.data()) ==
            MELEE_HOST_OK);
}

TEST_CASE("the ground parameters translate with their stage rows")
{
    // GrSh.dat's layout: the StageParam rows (0x64 bytes each), then the
    // parameters, whose pointer to the rows sits at +0xB0 with the count
    // after it and nine colors from +0xB8.  On the host the pointer is eight
    // bytes wide, so the count and the colors move by four.
    struct HostGroundParam {
        unsigned char prefix[0xB0];
        unsigned char* stage_params;
        std::int32_t stage_param_count;
        std::uint8_t colors[9 * 4];
    };
    REQUIRE(offsetof(HostGroundParam, stage_param_count) == 0xB8);
    REQUIRE(offsetof(HostGroundParam, colors) == 0xBC);

    ArchiveBuilder builder(0x200);
    // Two rows at data+0x00.
    builder.u32(0x00, 14);
    builder.u32(0x04, 0xFFFFFFFFU);
    builder.u16(0x14, 0xFFF6);
    builder.u16(0x1A, 7);
    builder.u16(0x62, 9);
    builder.u32(0x64, 15);
    builder.u16(0xC6, 11);
    // The parameters at data+0x100.
    builder.f32(0x100, 0.9F);
    builder.u16(0x104, 195);
    builder.u32(0x10C, 83);
    builder.u32(0x114, 0xFFFFFFF6U);
    builder.f32(0x118, 0.15F);
    builder.u16(0x12E, 60);
    builder.u8(0x14C, 1);
    builder.f32(0x160, -1.0F);
    builder.u16(0x168, 76);
    builder.u16(0x16A, 160);
    builder.u16(0x1AE, 40);
    builder.pointer(0x1B0, 0x00);
    builder.u32(0x1B4, 2);
    builder.u32(0x1B8, 0x649BFAFFU);
    builder.u32(0x1D8, 0x5A78D2FFU);
    builder.public_symbol(0x100, "grGroundParam");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    REQUIRE(melee_host_hsd_symbol_kind("grGroundParam") ==
            MELEE_HOST_HSD_SYMBOL_GAME_DATA);

    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto* const param = static_cast<HostGroundParam*>(
        HSD_ArchiveGetPublicAddress(&archive, "grGroundParam"));
    REQUIRE(param != nullptr);
    const auto f32_at = [](const unsigned char* base, std::size_t at) {
        float value = 0.0F;
        std::memcpy(&value, base + at, sizeof(value));
        return value;
    };
    const auto s32_at = [](const unsigned char* base, std::size_t at) {
        std::int32_t value = 0;
        std::memcpy(&value, base + at, sizeof(value));
        return value;
    };
    const auto s16_at = [](const unsigned char* base, std::size_t at) {
        std::int16_t value = 0;
        std::memcpy(&value, base + at, sizeof(value));
        return value;
    };
    REQUIRE(f32_at(param->prefix, 0x00) == 0.9F);
    REQUIRE(s16_at(param->prefix, 0x04) == 195);
    REQUIRE(s32_at(param->prefix, 0x0C) == 83);
    REQUIRE(s32_at(param->prefix, 0x14) == -10);
    REQUIRE(f32_at(param->prefix, 0x18) == 0.15F);
    REQUIRE(s16_at(param->prefix, 0x2E) == 60);
    REQUIRE(param->prefix[0x4C] == 1);
    REQUIRE(f32_at(param->prefix, 0x60) == -1.0F);
    REQUIRE(s16_at(param->prefix, 0x68) == 76);
    REQUIRE(s16_at(param->prefix, 0x6A) == 160);
    REQUIRE(s16_at(param->prefix, 0xAE) == 40);
    REQUIRE(param->stage_param_count == 2);
    REQUIRE(param->colors[0] == 0x64);
    REQUIRE(param->colors[3] == 0xFF);
    REQUIRE(param->colors[32] == 0x5A);
    REQUIRE(param->colors[34] == 0xD2);

    const unsigned char* const rows = param->stage_params;
    REQUIRE(rows != nullptr);
    REQUIRE(s32_at(rows, 0x00) == 14);
    REQUIRE(s32_at(rows, 0x04) == -1);
    REQUIRE(s16_at(rows, 0x14) == -10);
    REQUIRE(s16_at(rows, 0x1A) == 7);
    REQUIRE(s16_at(rows, 0x62) == 9);
    REQUIRE(s32_at(rows, 0x64) == 15);
    REQUIRE(s16_at(rows, 0xC6) == 11);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);

    // Rows counted with no pointer to them are refused.
    ArchiveBuilder lost(0x200);
    lost.u32(0x1B4, 2);
    lost.public_symbol(0x100, "grGroundParam");
    std::vector<std::byte> lost_bytes = lost.build();
    HSD_Archive lost_archive{};
    REQUIRE(HSD_ArchiveParse(&lost_archive, bytes_of(lost_bytes),
                             lost_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&lost_archive, "grGroundParam") ==
            nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("stage rows") != std::string_view::npos);
    REQUIRE(melee_host_hsd_archive_release(lost_bytes.data()) ==
            MELEE_HOST_OK);
}

TEST_CASE("the trophy tables translate up to and including their end rows")
{
    // TyDatai.usd's tables hold no pointer and end with a row whose first
    // field is -1.  The host rows are the game's own layouts.
    struct HostTrophyData {
        std::int32_t id;
        std::int32_t x04;
        float x08, x0C, x10, x14, x18, x1C;
        std::int8_t x20, x21, x22, x23;
    };
    struct HostDisplayEntry {
        std::int32_t x00;
        std::uint8_t x04, x05, pad[2];
        float x08, x0C;
    };
    constexpr std::uint32_t kTrophies = 293;
    constexpr std::uint32_t kSort = 0x100;

    ArchiveBuilder builder(0x1000);
    // Two model rows and the end row, which keeps its own floats.
    builder.u32(0x00, 29);
    builder.u32(0x04, 2);
    builder.f32(0x08, -0.82F);
    builder.u8(0x20, 0x63);
    builder.u8(0x21, 0xFF);
    builder.u32(0x24, 30);
    builder.u32(0x48, 0xFFFFFFFFU);
    builder.f32(0x50, 1.0F);
    // Trophy numbers.
    builder.u16(0x80, 0x1D);
    builder.u16(0x82, 0x67);
    builder.u16(0x84, 0xFFFF);
    // Display rows.
    builder.u32(0xA0, 0x1D);
    builder.u8(0xA4, 0x0F);
    builder.u8(0xA5, 0x05);
    builder.f32(0xA8, 1.0F);
    builder.f32(0xAC, 1.7F);
    builder.u32(0xB0, 0xFFFFFFFFU);
    // One sort row per trophy, then the end row.
    for (std::uint32_t row = 0; row < kTrophies; ++row) {
        builder.u16(kSort + row * 12, static_cast<std::uint16_t>(row));
        builder.u16(kSort + row * 12 + 10, static_cast<std::uint16_t>(row + 5));
    }
    builder.u16(kSort + kTrophies * 12, 0xFFFF);
    builder.u16(kSort + kTrophies * 12 + 2, 0xFFFF);
    builder.public_symbol(0x00, "tyInitModelDTbl");
    builder.public_symbol(0x80, "tyExpDifferentTbl");
    builder.public_symbol(0xA0, "tyDisplayModelUsTbl");
    builder.public_symbol(kSort, "tyModelSortTbl");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);

    auto* const models = static_cast<HostTrophyData*>(
        HSD_ArchiveGetPublicAddress(&archive, "tyInitModelDTbl"));
    REQUIRE(models != nullptr);
    REQUIRE(models[0].id == 29);
    REQUIRE(models[0].x04 == 2);
    REQUIRE(models[0].x08 == -0.82F);
    REQUIRE(models[0].x20 == 0x63);
    REQUIRE(models[0].x21 == -1);
    REQUIRE(models[1].id == 30);
    REQUIRE(models[2].id == -1);
    REQUIRE(models[2].x08 == 1.0F);

    auto* const numbers = static_cast<std::int16_t*>(
        HSD_ArchiveGetPublicAddress(&archive, "tyExpDifferentTbl"));
    REQUIRE(numbers != nullptr);
    REQUIRE(numbers[0] == 0x1D);
    REQUIRE(numbers[1] == 0x67);
    REQUIRE(numbers[2] == -1);

    auto* const display = static_cast<HostDisplayEntry*>(
        HSD_ArchiveGetPublicAddress(&archive, "tyDisplayModelUsTbl"));
    REQUIRE(display != nullptr);
    REQUIRE(display[0].x00 == 0x1D);
    REQUIRE(display[0].x04 == 0x0F);
    REQUIRE(display[0].x05 == 0x05);
    REQUIRE(display[0].x0C == 1.7F);
    REQUIRE(display[1].x00 == -1);

    auto* const sort = static_cast<std::int16_t*>(
        HSD_ArchiveGetPublicAddress(&archive, "tyModelSortTbl"));
    REQUIRE(sort != nullptr);
    REQUIRE(sort[0] == 0);
    REQUIRE(sort[5] == 5);
    REQUIRE(sort[(kTrophies - 1) * 6] == static_cast<std::int16_t>(kTrophies - 1));
    REQUIRE(sort[(kTrophies - 1) * 6 + 5] ==
            static_cast<std::int16_t>(kTrophies + 4));
    REQUIRE(sort[kTrophies * 6] == -1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);

    // A sort table that ends before every trophy has a row is refused.
    ArchiveBuilder short_sort(0x40);
    short_sort.u16(0x0C, 0xFFFF);
    short_sort.public_symbol(0x00, "tyModelSortTbl");
    std::vector<std::byte> short_bytes = short_sort.build();
    HSD_Archive short_archive{};
    REQUIRE(HSD_ArchiveParse(&short_archive, bytes_of(short_bytes),
                             short_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&short_archive, "tyModelSortTbl") ==
            nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("fewer rows than trophies") != std::string_view::npos);
    REQUIRE(melee_host_hsd_archive_release(short_bytes.data()) ==
            MELEE_HOST_OK);
}

extern "C" {
struct MeleeHostCommandRun {
    int steps;
    float timer;
    unsigned loop_count;
    int finished;
};
void melee_host_test_run_generic_commands(void* stream, int max_steps,
                                          MeleeHostCommandRun* out);
}

TEST_CASE("a command stream converts in place and runs the generic commands")
{
    // As the console encodes them: the opcode in the top six bits of each
    // big-endian word, and the subroutine and goto targets relocated in the
    // word after.  A loop runs a timer twice, then a subroutine adds another
    // and a goto reaches the reset.
    const auto command = [](std::uint32_t opcode, std::uint32_t value) {
        return (opcode << 26U) | value;
    };
    const auto translate = +[](MeleeHostHsdReader* reader,
                               mh_u32 root) -> void* {
        mh_u32 target = 0;
        if (!melee_host_hsd_reader_pointer(reader, root, &target)) {
            return nullptr;
        }
        return melee_host_hsd_reader_command_stream(reader, target);
    };
    REQUIRE(melee_host_hsd_register_translator("testCommandStream",
                                               translate) == MELEE_HOST_OK);

    ArchiveBuilder builder(0x40);
    builder.u32(0x00, command(3, 2));
    builder.u32(0x04, command(1, 5));
    builder.u32(0x08, command(4, 0));
    builder.u32(0x0C, command(5, 0));
    builder.pointer(0x10, 0x20);
    builder.u32(0x14, command(7, 0));
    builder.pointer(0x18, 0x28);
    builder.u32(0x20, command(1, 3));
    builder.u32(0x24, command(6, 0));
    builder.u32(0x28, command(0, 0));
    builder.pointer(0x30, 0x00);
    builder.public_symbol(0x30, "testCommandStream");
    std::vector<std::byte> bytes = builder.build();

    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto* const words = static_cast<std::uint32_t*>(
        HSD_ArchiveGetPublicAddress(&archive, "testCommandStream"));
    REQUIRE(words != nullptr);
    // Native words, and each target as its distance from the operand.
    REQUIRE(words[0] == command(3, 2));
    REQUIRE(words[1] == command(1, 5));
    REQUIRE(static_cast<std::int32_t>(words[4]) == 0x10);
    REQUIRE(static_cast<std::int32_t>(words[6]) == 0x10);
    REQUIRE(words[8] == command(1, 3));
    REQUIRE(words[9] == command(6, 0));

    MeleeHostCommandRun run{};
    melee_host_test_run_generic_commands(words, 32, &run);
    // SetLoop, timer, loop, timer, loop, subroutine, timer, return, goto and
    // reset.
    REQUIRE(run.finished == 1);
    REQUIRE(run.steps == 10);
    REQUIRE(run.timer == 13.0F);
    REQUIRE(run.loop_count == 0);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);

    // A pointer that no subroutine or goto owns is refused.
    ArchiveBuilder stray(0x40);
    stray.u32(0x00, command(1, 5));
    stray.pointer(0x04, 0x10);
    stray.u32(0x10, command(0, 0));
    stray.pointer(0x20, 0x00);
    stray.public_symbol(0x20, "testCommandStream");
    std::vector<std::byte> stray_bytes = stray.build();
    HSD_Archive stray_archive{};
    REQUIRE(HSD_ArchiveParse(&stray_archive, bytes_of(stray_bytes),
                             stray_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&stray_archive, "testCommandStream") ==
            nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("does not follow a subroutine or a goto") !=
            std::string_view::npos);
    REQUIRE(melee_host_hsd_archive_release(stray_bytes.data()) ==
            MELEE_HOST_OK);
    REQUIRE(melee_host_hsd_register_translator("testCommandStream", nullptr) ==
            MELEE_HOST_OK);
}

extern "C" int melee_host_test_check_item_public_data(void* translated,
                                                     char* message,
                                                     std::size_t size);

TEST_CASE("the common item data translates, leaving out per-kind layouts")
{
    // A small ItCo.usd: the record, ItemCommonData, the article tables of the
    // common items (43), the character items (118) and the Pokemon (47),
    // it_804D6D40_t and the color animations.  Two common entries share an
    // article, and a character article shares its attributes and states.
    // item_data_check.c reads the result through the game's types.
    const auto command = [](std::uint32_t opcode, std::uint32_t value) {
        return (opcode << 26U) | value;
    };
    ArchiveBuilder builder(0x800);
    builder.pointer(0x000, 0x020);
    builder.pointer(0x004, 0x180);
    builder.pointer(0x008, 0x22C);
    builder.pointer(0x00C, 0x404);
    builder.pointer(0x010, 0x4C0);
    builder.pointer(0x014, 0x4E0);
    // ItemCommonData.
    builder.u32(0x020, 7);
    builder.u8(0x068, 0x5A);
    builder.u8(0x104, 1);
    builder.u8(0x105, 2);
    builder.u8(0x106, 3);
    builder.u8(0x107, 4);
    builder.f32(0x17C, 2.5F);
    // Article tables.
    builder.pointer(0x180, 0x500);
    builder.pointer(0x184, 0x500);
    // The Bob-omb, kind 6, whose attributes are all floats.
    builder.pointer(0x198, 0x700);
    builder.pointer(0x704, 0x740);
    builder.f32(0x740, 3.0F);
    builder.f32(0x760, 0.8F);
    builder.f32(0x764, 0.7F);
    builder.pointer(0x22C + 117 * 4, 0x540);
    // it_804D6D40_t.
    builder.u32(0x4C0, 3);
    builder.f32(0x4C4, 1.5F);
    // Color animations: none, then the state's script at priority 30.
    builder.pointer(0x4E8, 0x6C0);
    builder.u8(0x4EC, 30);
    builder.u8(0x4ED, 2);
    // A common article.
    builder.pointer(0x500, 0x580);
    builder.pointer(0x504, 0x608);
    builder.pointer(0x508, 0x610);
    builder.pointer(0x50C, 0x640);
    builder.pointer(0x510, 0x660);
    // A character article with dynamics.
    builder.pointer(0x540, 0x580);
    builder.pointer(0x54C, 0x640);
    builder.pointer(0x554, 0x670);
    // ItemAttr: flag bytes 0x8B and 0x6D, then the scale.
    builder.u8(0x580, 0x8B);
    builder.u8(0x581, 0x6D);
    builder.f32(0x5E0, 1.25F);
    builder.u32(0x608, 0xDEADBEEFU);
    // Hurtboxes.
    builder.u32(0x610, 1);
    builder.pointer(0x614, 0x620);
    builder.u32(0x620, 4);
    builder.f32(0x624, 1.0F);
    builder.f32(0x63C, 2.0F);
    // Two states up to the model: a script, then nothing.
    builder.pointer(0x64C, 0x6C0);
    // Model.
    builder.u32(0x664, 5);
    builder.u32(0x668, 0xFFFFFFFFU);
    builder.u8(0x66C, 0x80);
    builder.u32(0x670, 1);
    // The script.
    builder.u32(0x6C0, command(1, 5));
    builder.public_symbol(0x000, "itPublicData");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const data = HSD_ArchiveGetPublicAddress(&archive, "itPublicData");
    REQUIRE(data != nullptr);
    char message[256] = {};
    REQUIRE(melee_host_test_check_item_public_data(data, message,
                                                   sizeof(message)) == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

extern "C" int melee_host_test_check_stage_coll_data(void* translated,
                                                    char* message,
                                                    std::size_t size);

TEST_CASE("a stage's coll_data translates its vertices, lines and joints")
{
    // Three vertices, two floor lines between them and one joint spanning
    // them, then the record, whose trailing word is the next data on the disc.
    // stage_data_check.c reads it back through the game's types.
    ArchiveBuilder builder(0x200);
    builder.f32(0x000, 1.5F);
    builder.f32(0x004, -2.0F);
    builder.f32(0x008, 3.0F);
    builder.f32(0x00C, 4.25F);
    builder.f32(0x010, -8.0F);
    builder.f32(0x014, 0.5F);
    builder.u16(0x020, 0);
    builder.u16(0x022, 1);
    builder.u16(0x024, 0xFFFF);
    builder.u16(0x026, 1);
    builder.u16(0x028, 0xFFFF);
    builder.u16(0x02A, 0xFFFF);
    builder.u16(0x02C, 0x0100);
    builder.u16(0x02E, 0x0001);
    builder.u16(0x030, 1);
    builder.u16(0x032, 2);
    builder.u16(0x034, 0);
    builder.u16(0x036, 0xFFFF);
    builder.u16(0x038, 0xFFFF);
    builder.u16(0x03A, 0xFFFF);
    builder.u16(0x03E, 0x0004);
    builder.u16(0x042, 2);
    builder.u16(0x044, 0xFFFF);
    builder.f32(0x054, -8.0F);
    builder.f32(0x058, -2.0F);
    builder.f32(0x05C, 3.0F);
    builder.f32(0x060, 4.25F);
    builder.u16(0x066, 3);
    builder.pointer(0x100, 0x000);
    builder.u32(0x104, 3);
    builder.pointer(0x108, 0x020);
    builder.u32(0x10C, 2);
    builder.u16(0x112, 2);
    builder.u16(0x114, 0xFFFF);
    builder.pointer(0x124, 0x040);
    builder.u32(0x128, 1);
    builder.u32(0x12C, 0x42001EU);
    builder.public_symbol(0x100, "coll_data");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    REQUIRE(melee_host_hsd_symbol_kind("coll_data") ==
            MELEE_HOST_HSD_SYMBOL_GAME_DATA);
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const coll = HSD_ArchiveGetPublicAddress(&archive, "coll_data");
    REQUIRE(coll != nullptr);
    char message[256] = {};
    const int checked =
        melee_host_test_check_stage_coll_data(coll, message, sizeof(message));
    if (checked != 1) {
        std::fprintf(stderr, "coll_data check failed: %s\n", message);
    }
    REQUIRE(checked == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);

    // A line naming a vertex past the map's vertices is refused.
    ArchiveBuilder past(0x200);
    past.u16(0x020, 0);
    past.u16(0x022, 3);
    past.pointer(0x100, 0x000);
    past.u32(0x104, 3);
    past.pointer(0x108, 0x020);
    past.u32(0x10C, 1);
    past.public_symbol(0x100, "coll_data");
    std::vector<std::byte> past_bytes = past.build();
    HSD_Archive past_archive{};
    REQUIRE(HSD_ArchiveParse(&past_archive, bytes_of(past_bytes),
                             past_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&past_archive, "coll_data") ==
            nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("past the map's vertices") != std::string_view::npos);
    REQUIRE(melee_host_hsd_archive_release(past_bytes.data()) ==
            MELEE_HOST_OK);

    // Lines counted with no pointer to them are refused.
    ArchiveBuilder lost(0x200);
    lost.u32(0x10C, 2);
    lost.public_symbol(0x100, "coll_data");
    std::vector<std::byte> lost_bytes = lost.build();
    HSD_Archive lost_archive{};
    REQUIRE(HSD_ArchiveParse(&lost_archive, bytes_of(lost_bytes),
                             lost_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&lost_archive, "coll_data") ==
            nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("counts an array") != std::string_view::npos);
    REQUIRE(melee_host_hsd_archive_release(lost_bytes.data()) ==
            MELEE_HOST_OK);
}

extern "C" int melee_host_test_check_stage_map_head(void* translated,
                                                   char* message,
                                                   std::size_t size);

TEST_CASE("a stage's map_head translates with its lights shared by address")
{
    // One model with its joint, an animation table that is only its
    // terminator, a light list naming an ambient light, GrJoints, flag bytes
    // and s16 values; a table of s16 pairs naming the model's joint; light
    // overrides counted twice, as on the disc, whose first entry names the
    // model's light; a table nothing reads; and a material.
    // stage_data_check.c reads it back through the game's types.
    ArchiveBuilder builder(0x200);
    // The model, whose joint is a bare descriptor.
    builder.pointer(0x000, 0x180);
    builder.pointer(0x004, 0x040);
    builder.pointer(0x014, 0x050);
    builder.pointer(0x018, 0x060);
    builder.pointer(0x020, 0x070);
    builder.u32(0x024, 2);
    builder.pointer(0x028, 0x07C);
    builder.pointer(0x02C, 0x080);
    builder.u32(0x030, 3);
    builder.u32(0x050, 0xABCDU);
    builder.pointer(0x060, 0x090);
    builder.u16(0x070, 1);
    builder.u16(0x072, 2);
    builder.u16(0x074, 3);
    builder.u16(0x076, 4);
    builder.u16(0x078, 0xFFFD);
    builder.u16(0x07A, 6);
    builder.u8(0x07C, 1);
    builder.u16(0x080, 10);
    builder.u16(0x082, 11);
    builder.u16(0x084, 0xFFF9);
    // The light list entry and an ambient light.
    builder.pointer(0x090, 0x0A0);
    builder.u8(0x0AC, 0x40);
    builder.u8(0x0AD, 0x80);
    builder.u8(0x0AE, 0xC0);
    builder.u8(0x0AF, 0xFF);
    // s16 pairs for the model's joint.
    builder.pointer(0x0C0, 0x180);
    builder.pointer(0x0C4, 0x0D0);
    builder.u32(0x0C8, 2);
    builder.u16(0x0D0, 1);
    builder.u16(0x0D2, 0x94);
    builder.u16(0x0D4, 2);
    builder.u16(0x0D6, 0x95);
    // Two real overrides: the light, then the material.
    builder.pointer(0x0E0, 0x0A0);
    builder.u8(0x0E4, 0xC0);
    builder.pointer(0x0E8, 0x150);
    builder.u8(0x0EC, 0x20);
    // What the doubled count reads next: a plain word, then the material
    // table's pointer.
    builder.u32(0x0F0, 0x12345678U);
    builder.pointer(0x0F8, 0x150);
    // map_head.
    builder.pointer(0x100, 0x0C0);
    builder.u32(0x104, 1);
    builder.pointer(0x108, 0x000);
    builder.u32(0x10C, 1);
    builder.pointer(0x118, 0x0E0);
    builder.u32(0x11C, 4);
    builder.pointer(0x120, 0x0F0);
    builder.u32(0x124, 1);
    builder.pointer(0x128, 0x0F8);
    builder.u32(0x12C, 1);
    // The material, render mode 0x12.
    builder.u32(0x154, 0x12);
    builder.public_symbol(0x100, "map_head");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const head = HSD_ArchiveGetPublicAddress(&archive, "map_head");
    REQUIRE(head != nullptr);
    char message[256] = {};
    REQUIRE(melee_host_test_check_stage_map_head(head, message,
                                                 sizeof(message)) == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

extern "C" int melee_host_test_check_stage_extras(void* plit, void* quake,
                                                  void* items, char* message,
                                                  std::size_t size);
extern "C" void melee_host_test_set_stage_grkind(int grkind);
extern "C" int melee_host_test_check_icemt_yakumono(void* translated,
                                                     char* message,
                                                     std::size_t size);

TEST_CASE("a stage's map_plit, quake_model_set and itemdata translate")
{
    // A light table of one list naming an ambient light; a quake model with a
    // bare joint and a table of one animation; and a stage item table that is
    // only its terminator, as Hyrule Temple's.  stage_data_check.c reads them
    // back.
    ArchiveBuilder builder(0x200);
    builder.pointer(0x000, 0x010);
    builder.pointer(0x010, 0x020);
    builder.u8(0x02C, 0x11);
    builder.u8(0x02D, 0x22);
    builder.u8(0x02E, 0x33);
    builder.u8(0x02F, 0xFF);
    builder.pointer(0x080, 0x100);
    builder.pointer(0x084, 0x0A0);
    builder.pointer(0x0A0, 0x160);
    builder.public_symbol(0x000, "map_plit");
    builder.public_symbol(0x080, "quake_model_set");
    builder.public_symbol(0x0C0, "itemdata");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const plit = HSD_ArchiveGetPublicAddress(&archive, "map_plit");
    void* const quake =
        HSD_ArchiveGetPublicAddress(&archive, "quake_model_set");
    void* const items = HSD_ArchiveGetPublicAddress(&archive, "itemdata");
    REQUIRE(plit != nullptr);
    REQUIRE(quake != nullptr);
    REQUIRE(items != nullptr);
    char message[256] = {};
    REQUIRE(melee_host_test_check_stage_extras(plit, quake, items, message,
                                               sizeof(message)) == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

TEST_CASE("a stage's ALDYakuAll and yakumono_param translate")
{
    // GrSh.dat's shape: yakumono_param is a block of zeroes the stage keeps
    // without ever reading it, the one state script the stage gives the
    // random item lives in the words after it, and ALDYakuAll is the table
    // Ground_801C0800 walks from index 1 until a NULL.
    const auto command = [](std::uint32_t opcode, std::uint32_t value) {
        return (opcode << 26U) | value;
    };
    ArchiveBuilder builder(0x80);
    builder.u32(0x010, command(1, 5));
    builder.u32(0x014, command(0, 0));
    builder.pointer(0x024, 0x010);
    builder.public_symbol(0x000, "yakumono_param");
    builder.public_symbol(0x020, "ALDYakuAll");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto* const scripts = static_cast<void**>(
        HSD_ArchiveGetPublicAddress(&archive, "ALDYakuAll"));
    REQUIRE(scripts != nullptr);
    REQUIRE(scripts[0] == nullptr);
    REQUIRE(scripts[1] != nullptr);
    REQUIRE(scripts[2] == nullptr);
    // The script is a converted command stream, in native words.
    REQUIRE(static_cast<std::uint32_t*>(scripts[1])[0] == command(1, 5));
    auto* const params = static_cast<std::uint32_t*>(
        HSD_ArchiveGetPublicAddress(&archive, "yakumono_param"));
    REQUIRE(params != nullptr);
    REQUIRE(params[0] == 0);
    REQUIRE(params[3] == 0);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);

    // A non-zero block cannot be identified from its size alone.  It must not
    // be silently replaced with zeroes or interpreted as another stage.
    ArchiveBuilder unknown(0x40);
    unknown.f32(0x00, 1.5F);
    unknown.public_symbol(0x00, "yakumono_param");
    std::vector<std::byte> unknown_bytes = unknown.build();
    HSD_Archive unknown_archive{};
    REQUIRE(HSD_ArchiveParse(&unknown_archive, bytes_of(unknown_bytes),
                             unknown_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&unknown_archive, "yakumono_param") ==
            nullptr);
    REQUIRE(melee_host_hsd_archive_release(unknown_bytes.data()) ==
            MELEE_HOST_OK);
}

TEST_CASE("Icicle's yakumono s16 tables translate as values, not commands")
{
    // The three pointed-to lists precede the 0x13C-byte parameter record so
    // their archive extents give exact element counts.  In particular, the
    // first sequence must remain 0,1,2,3,4 on a little-endian host.
    constexpr std::uint32_t root = 0x50;
    ArchiveBuilder builder(root + 0x13C);
    for (std::uint32_t i = 0; i < 16; ++i) {
        builder.u16(i * 2, static_cast<std::uint16_t>(i));
    }
    builder.u16(0x20, static_cast<std::uint16_t>(-3));
    builder.u16(0x22, 0x1234);
    builder.u16(0x38, 200);
    builder.u16(0x3A, static_cast<std::uint16_t>(-200));
    builder.pointer(root + 0xAC, 0x00);
    builder.pointer(root + 0xB0, 0x20);
    builder.pointer(root + 0xB4, 0x38);
    builder.public_symbol(root, "yakumono_param");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    melee_host_test_set_stage_grkind(22); // GrKind_Icemt
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const params =
        HSD_ArchiveGetPublicAddress(&archive, "yakumono_param");
    REQUIRE(params != nullptr);
    char message[256] = {};
    REQUIRE(melee_host_test_check_icemt_yakumono(params, message,
                                                  sizeof(message)) == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
    melee_host_test_set_stage_grkind(-1);
}

extern "C" int melee_host_test_check_fighter_common_data(void* translated,
                                                        char* message,
                                                        std::size_t size);

TEST_CASE("the fighter common data translates its 23 tables")
{
    // A small PlCo.dat: the record of 23 pointers and one small instance of
    // each table.  fighter_data_check.c reads the result through the game's
    // types.
    const auto command = [](std::uint32_t opcode, std::uint32_t value) {
        return (opcode << 26U) | value;
    };
    const std::uint32_t tables[23] = {
        0x100, 0x920, 0x938, 0x94C, 0x954, 0x974, 0x984, 0x994,
        0x99C, 0x9A4, 0x9BC, 0x9CC, 0x9D4, 0xA70, 0xAAC, 0xAD0,
        0xE00, 0xAD8, 0xAEC, 0xAF0, 0xE00, 0xAF4, 0xB38,
    };
    ArchiveBuilder builder(0x1000);
    for (std::uint32_t index = 0; index < 23; ++index) {
        builder.pointer(index * 4, tables[index]);
    }
    // [0] ftCommonData.
    builder.f32(0x100, 0.25F);
    builder.u8(0x100 + 0x6DC, 1);
    builder.u8(0x100 + 0x6DF, 4);
    builder.u8(0x100 + 0x6EC, 9);
    builder.u8(0x100 + 0x7D8, 0x10);
    builder.u8(0x100 + 0x7DB, 0x40);
    builder.u32(0x100 + 0x814, 7);
    // [1] two item throw rows, [2] a swing row, [3] two staling floats.
    builder.f32(0x920, 1.5F);
    builder.f32(0x934, 2.5F);
    builder.f32(0x93C, 3.0F);
    builder.f32(0x950, 0.5F);
    // [4] two parts slots, the first with its byte tables.
    builder.pointer(0x954, 0x960);
    builder.pointer(0x960, 0x96C);
    builder.pointer(0x964, 0x970);
    builder.u32(0x968, 3);
    builder.u8(0x96D, 6);
    builder.u8(0x972, 10);
    // [5] one slot of part records.
    builder.pointer(0x974, 0x978);
    builder.pointer(0x978, 0x980);
    builder.u32(0x97C, 1);
    builder.u8(0x980, 4);
    // [6] color animations: none, then the script at 30; [7] the script at 50.
    builder.pointer(0x98C, 0xF00);
    builder.u8(0x990, 30);
    builder.pointer(0x994, 0xF00);
    builder.u8(0x998, 50);
    // [8] the respawn platform's joint and animation.
    builder.pointer(0x99C, 0xE00);
    builder.pointer(0x9A0, 0xE80);
    // [9] a Vec2 list of two and its length.
    builder.pointer(0x9A4, 0x9AC);
    builder.u32(0x9A8, 2);
    builder.f32(0x9AC, 1.0F);
    builder.f32(0x9B0, 2.0F);
    builder.f32(0x9B4, 3.0F);
    builder.f32(0x9B8, 4.0F);
    // [10] and [11] shake tables sharing a Vec2 list of one.
    builder.pointer(0x9BC, 0x9C4);
    builder.u32(0x9C0, 1);
    builder.f32(0x9C4, 5.0F);
    builder.f32(0x9C8, 6.0F);
    builder.pointer(0x9CC, 0x9C4);
    builder.u32(0x9D0, 1);
    // [12] to [15] modifiers.
    builder.f32(0x9D4, 1.25F);
    builder.f32(0xA70, 0.75F);
    builder.f32(0xAAC + 0xC, -2.0F);
    builder.f32(0xAD0 + 0x4, 4.0F);
    // [17] what nothing reads, [18] and [19] bytes, [21] the crowd.
    builder.u32(0xAD8, 0xDEADBEEFU);
    builder.u8(0xAED, 0x22);
    builder.u8(0xAF0, 0x33);
    builder.f32(0xAF4, 30.0F);
    // [22] CPU tables: scripts, one attack list, distances and reaches.
    builder.pointer(0xB38, 0xB60);
    builder.pointer(0xB3C, 0xB6C);
    builder.pointer(0xB58, 0xBB8);
    builder.pointer(0xB5C, 0xBC0);
    builder.pointer(0xB60, 0xB68);
    builder.u8(0xB68, 0x92);
    builder.pointer(0xB6C, 0xB70);
    builder.u32(0xB70, 2);
    builder.f32(0xB70 + 0x18, 0.5F);
    builder.f32(0xBBC, 13.0F);
    builder.f32(0xBC0, 7.0F);
    // The script both color animation tables name.
    builder.u32(0xF00, command(1, 5));
    builder.public_symbol(0x000, "ftLoadCommonData");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const data =
        HSD_ArchiveGetPublicAddress(&archive, "ftLoadCommonData");
    REQUIRE(data != nullptr);
    char message[256] = {};
    REQUIRE(melee_host_test_check_fighter_common_data(data, message,
                                                      sizeof(message)) == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

extern "C" int melee_host_test_check_scene_data(void* translated,
                                               char* message,
                                               std::size_t size);

TEST_CASE("a scene_data symbol translates its models, cameras, lights and fogs")
{
    // As in GmPause.dat and IfAll.dat: a model table, a camera array with an
    // animation table and no terminator, light lists with animations, and a
    // fog array that the SceneDesc itself follows.  scene_data_check.c reads
    // it back through the game's SceneDesc.
    ArchiveBuilder builder(0x200);
    // Models: one, with a joint and a material animation table.
    builder.pointer(0x000, 0x010);
    builder.pointer(0x010, 0x100);
    builder.pointer(0x018, 0x020);
    builder.pointer(0x020, 0x140);
    // Cameras: one entry, bounded by its animation table.
    builder.pointer(0x030, 0x150);
    builder.pointer(0x034, 0x038);
    builder.pointer(0x038, 0x040);
    builder.u16(0x150 + 0x06, PROJ_PERSPECTIVE);
    builder.f32(0x150 + 0x28, 1.5F);
    // Lights: one list with an animation table.
    builder.pointer(0x050, 0x058);
    builder.pointer(0x058, 0x190);
    builder.pointer(0x05C, 0x060);
    builder.pointer(0x060, 0x068);
    // Fogs: one entry, and then the SceneDesc.
    builder.pointer(0x080, 0x1C0);
    builder.f32(0x1C0 + 0x08, 10.0F);
    builder.pointer(0x088, 0x000);
    builder.pointer(0x08C, 0x030);
    builder.pointer(0x090, 0x050);
    builder.pointer(0x094, 0x080);
    builder.public_symbol(0x088, "ScTest_scene_data");
    std::vector<std::byte> bytes = builder.build();

    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const scene =
        HSD_ArchiveGetPublicAddress(&archive, "ScTest_scene_data");
    REQUIRE(scene != nullptr);
    char message[256] = {};
    REQUIRE(melee_host_test_check_scene_data(scene, message,
                                             sizeof(message)) == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

extern "C" int melee_host_test_check_scene_models(void* table, char* message,
                                                 std::size_t size);
extern "C" int melee_host_test_check_single_model_table(void* table,
                                                       char* message,
                                                       std::size_t size);

TEST_CASE("model tables translate as DynamicModelDesc, suffixed or named")
{
    // A table of two models, one with an animation table, named both with the
    // _scene_models suffix and as IfAll.dat's tdsce, and, as the magnifier's
    // lupe, a table of one that the model record comes right before.
    // scene_data_check.c reads them back.
    ArchiveBuilder builder(0x140);
    builder.pointer(0x000, 0x010);
    builder.pointer(0x004, 0x020);
    builder.pointer(0x010, 0x080);
    builder.pointer(0x014, 0x030);
    builder.pointer(0x020, 0x0C0);
    builder.pointer(0x030, 0x040);
    builder.pointer(0x100, 0x020);
    builder.public_symbol(0x000, "ScTest_scene_models");
    builder.public_symbol(0x000, "tdsce");
    builder.public_symbol(0x100, "lupe");
    std::vector<std::byte> bytes = builder.build();

    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const table =
        HSD_ArchiveGetPublicAddress(&archive, "ScTest_scene_models");
    void* const named = HSD_ArchiveGetPublicAddress(&archive, "tdsce");
    void* const lupe = HSD_ArchiveGetPublicAddress(&archive, "lupe");
    REQUIRE(table != nullptr);
    REQUIRE(named != nullptr);
    REQUIRE(lupe != nullptr);
    char message[256] = {};
    REQUIRE(melee_host_test_check_scene_models(table, message,
                                               sizeof(message)) == 1);
    REQUIRE(melee_host_test_check_scene_models(named, message,
                                               sizeof(message)) == 1);
    REQUIRE(melee_host_test_check_single_model_table(lupe, message,
                                                     sizeof(message)) == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

extern "C" int melee_host_test_check_fighter_fox_data(void* translated,
                                                     char* message,
                                                     std::size_t size);

TEST_CASE("Fox's fighter data translates its records and tables")
{
    // A small PlFx.dat: the ftData record and one small instance of what each
    // translated field names.  fighter_fox_data_check.c reads the result
    // through the game's types.
    const auto command = [](std::uint32_t opcode, std::uint32_t value) {
        return (opcode << 26U) | value;
    };
    ArchiveBuilder builder(0x700);
    // The record: attributes, Fox's attributes, parts, two action tables,
    // guard joints, wait pairs, dynamics, hurtboxes, ledge, items, sounds, a
    // word list and the IK record.
    builder.pointer(0x00, 0x100);
    builder.pointer(0x04, 0x290);
    builder.pointer(0x08, 0x510);
    builder.pointer(0x0C, 0x370);
    builder.pointer(0x14, 0x3B0);
    builder.pointer(0x20, 0x558);
    builder.pointer(0x24, 0x3C8);
    builder.pointer(0x2C, 0x3D8);
    builder.pointer(0x30, 0x448);
    builder.pointer(0x44, 0x478);
    builder.pointer(0x48, 0x494);
    builder.pointer(0x4C, 0x4A4);
    builder.pointer(0x54, 0x4EC);
    builder.pointer(0x58, 0x4F4);
    // ftCo_DatAttrs and ftFox_DatAttrs: words, then a byte at the end.
    builder.f32(0x100, 1.5F);
    builder.u8(0x100 + 0x180, 0x81);
    builder.f32(0x290, 2.5F);
    builder.u8(0x290 + 0xD0, 1);
    // Two actions, a named one with a script and an empty one; one more in
    // the second table.
    builder.pointer(0x370, 0x3A0);
    builder.u32(0x374, 0x100);
    builder.u32(0x378, 0x40);
    builder.pointer(0x37C, 0x3A8);
    builder.u32(0x380, 7);
    builder.u8(0x3A0, 'W');
    builder.u8(0x3A1, 'a');
    builder.u8(0x3A2, 'i');
    builder.u8(0x3A3, 't');
    builder.u32(0x3A8, command(1, 5));
    builder.u32(0x3B4, 0x20);
    builder.u32(0x3B8, 0x10);
    // Wait pairs: one, then the -1 entry.
    builder.u32(0x3C8, 1);
    builder.u32(0x3CC, 2);
    builder.u32(0x3D0, 0xFFFFFFFFU);
    // Dynamics: one bone with one record, then a scalar list.
    builder.u32(0x3D8, 1);
    builder.pointer(0x3DC, 0x3EC);
    builder.u32(0x3E0, 3);
    builder.pointer(0x3E4, 0x440);
    builder.u32(0x3EC, 7);
    builder.pointer(0x3F0, 0x404);
    builder.u32(0x3F4, 1);
    builder.f32(0x3F8, 1.0F);
    builder.f32(0x3FC, 2.0F);
    builder.f32(0x400, 3.0F);
    builder.f32(0x404, 0.5F);
    builder.f32(0x440, 9.0F);
    builder.f32(0x444, 10.0F);
    // One hurtbox.
    builder.u32(0x448, 1);
    builder.pointer(0x44C, 0x450);
    builder.u32(0x450, 5);
    builder.f32(0x450 + 0x24, 2.0F);
    // Ledge: s16 values, then words.
    builder.u16(0x478, 0xFFFD);
    builder.f32(0x478 + 0xC, 4.5F);
    // Items: a slot naming int pairs, which hold no pointer, and in the
    // blaster's slot an article with zeroed common attributes and ten floats
    // of special attributes, past the guard joint's record.
    builder.pointer(0x494, 0x49C);
    builder.u32(0x49C, 3);
    builder.u32(0x4A0, 0xFFFFFFFFU);
    builder.pointer(0x498, 0x600);
    builder.pointer(0x600, 0x620);
    builder.pointer(0x604, 0x6B0);
    builder.f32(0x6B0, 4.5F);
    builder.f32(0x6B0 + 0x18, 1.0F);
    builder.f32(0x6B0 + 0x24, 7.25F);
    // Sounds: the smash list, a word and the x20 list; no x1C.
    builder.pointer(0x4A4, 0x4DC);
    builder.u32(0x4A8, 11);
    builder.pointer(0x4A4 + 0x20, 0x4DC);
    builder.u32(0x4DC, 2);
    builder.pointer(0x4E0, 0x4E4);
    builder.u32(0x4E4, 100);
    builder.u32(0x4E8, 101);
    // x54 words and the IK record.
    builder.u32(0x4EC, 9);
    builder.u32(0x4F0, 8);
    builder.u8(0x4F4, 1);
    builder.f32(0x4F8, 2.5F);
    builder.f32(0x4F4 + 0x18, 6.0F);
    // Parts: one model, one visibility row naming a lookup of one TempS, and
    // one row of two TObj indices.
    builder.u32(0x510, 1);
    builder.pointer(0x514, 0x528);
    builder.u32(0x518, 2);
    builder.pointer(0x51C, 0x550);
    builder.u8(0x520, 3);
    builder.u8(0x524, 9);
    builder.pointer(0x528, 0x538);
    builder.u32(0x538, 1);
    builder.pointer(0x53C, 0x540);
    builder.u32(0x540, 2);
    builder.pointer(0x544, 0x548);
    builder.u8(0x548, 4);
    builder.u8(0x549, 5);
    builder.pointer(0x550, 0x554);
    builder.u16(0x554, 7);
    builder.u16(0x556, 8);
    // Guard joints: a small integer, nothing, a bare joint and nothing.
    builder.pointer(0x558, 0x560);
    builder.f32(0x55C, 1.25F);
    builder.u32(0x560, 5);
    builder.pointer(0x568, 0x570);
    builder.public_symbol(0x000, "ftDataFox");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    void* const data = HSD_ArchiveGetPublicAddress(&archive, "ftDataFox");
    REQUIRE(data != nullptr);
    char message[256] = {};
    REQUIRE(melee_host_test_check_fighter_fox_data(data, message,
                                                   sizeof(message)) == 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

TEST_CASE("a character's demo motion file hands over its bytes as they are")
{
    // ftData_80085B98 adds each demo action's offset to this block's address
    // and parses the nested archive there, so the bytes must stay big-endian.
    ArchiveBuilder builder(0x40);
    builder.u32(0x00, 0x00001234U);
    builder.u32(0x04, 0xDEADBEEFU);
    builder.u32(0x3C, 0x01020304U);
    builder.public_symbol(0x00, "ftDemoResultMotionFileFox");
    std::vector<std::byte> bytes = builder.build();

    melee_host_game_register_data_translators();
    REQUIRE(melee_host_hsd_symbol_kind("ftDemoResultMotionFileFox") ==
            MELEE_HOST_HSD_SYMBOL_GAME_DATA);
    REQUIRE(melee_host_hsd_symbol_kind("ftDemoViWaitMotionFileMario") ==
            MELEE_HOST_HSD_SYMBOL_GAME_DATA);
    REQUIRE(melee_host_hsd_symbol_kind("ftDemoResultMotionFileNobody") ==
            MELEE_HOST_HSD_SYMBOL_UNSUPPORTED);
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto* const block = static_cast<const std::uint8_t*>(
        HSD_ArchiveGetPublicAddress(&archive, "ftDemoResultMotionFileFox"));
    REQUIRE(block != nullptr);
    const std::uint8_t expected_head[] = { 0x00, 0x00, 0x12, 0x34,
                                           0xDE, 0xAD, 0xBE, 0xEF };
    const std::uint8_t expected_tail[] = { 0x01, 0x02, 0x03, 0x04 };
    REQUIRE(std::memcmp(block, expected_head, sizeof(expected_head)) == 0);
    REQUIRE(std::memcmp(block + 0x3C, expected_tail, sizeof(expected_tail)) ==
            0);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
}

TEST_CASE("an effect table's particle banks load through the particle system")
{
    // The table points at a command bank and a texture bank and is followed
    // by two effect records before the first bank.  Inside the banks every
    // offset is bank-relative, not an archive relocation.
    ArchiveBuilder builder(0x100);
    builder.pointer(0x00, 0x40);
    builder.pointer(0x04, 0xC0);
    builder.f32(0x08, 7.5F);
    // Command bank: version 0x42, list IDs from 100, one list at +0x10.
    builder.u32(0x40, 0x00420000U);
    builder.u32(0x44, 100);
    builder.u32(0x48, 1);
    builder.u32(0x4C, 0x10);
    builder.u16(0x50, 1);
    builder.u16(0x52, 0);
    builder.u16(0x54, 2);
    builder.u16(0x56, 3);
    builder.u32(0x58, 0x06000001U);
    builder.f32(0x5C, 0.5F);
    builder.f32(0x88, 9.0F);
    builder.u8(0x8C, 0xAB);
    // Texture bank: one I8 group at +0x08 with its image at +0x30.
    builder.u32(0xC0, 1);
    builder.u32(0xC4, 0x08);
    builder.u32(0xC8, 1);
    builder.u32(0xCC, 1);
    builder.u32(0xD4, 4);
    builder.u32(0xD8, 4);
    builder.u32(0xE0, 0x30);
    builder.u8(0xF0, 0x5A);
    builder.public_symbol(0x00, "effTestDataTable");
    std::vector<std::byte> bytes = builder.build();

    struct HostEffectDesc {
        float lifetime;
        void* model[4];
    };
    struct HostEffectTable {
        MeleeHostParticleCmdBank* commands;
        MeleeHostParticleTexBank* textures;
        HostEffectDesc effects[2];
    };

    REQUIRE(melee_host_hsd_symbol_kind("effTestDataTable") ==
            MELEE_HOST_HSD_SYMBOL_GAME_DATA);
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    auto* const table = static_cast<HostEffectTable*>(
        HSD_ArchiveGetPublicAddress(&archive, "effTestDataTable"));
    REQUIRE(table != nullptr);

    MeleeHostParticleCmdBank* const commands = table->commands;
    REQUIRE(commands != nullptr);
    REQUIRE(commands->magic == MELEE_HOST_PARTICLE_CMD_BANK_MAGIC);
    REQUIRE(commands->list_end == 101);
    REQUIRE(commands->lists[99] == nullptr);
    HSD_PSCmdList* const list = commands->lists[100];
    REQUIRE(list != nullptr);
    REQUIRE(list->type == 1);
    REQUIRE(list->genLife == 2);
    REQUIRE(list->life == 3);
    // psInitDataBankLocate clears bits 0x0E000000 and sets 0x08000000.
    REQUIRE(list->kind == 0x08000001U);
    REQUIRE(list->grav == 0.5F);
    REQUIRE(list->param3 == 9.0F);
    REQUIRE(list->cmdList[0] == 0xAB);

    MeleeHostParticleTexBank* const textures = table->textures;
    REQUIRE(textures != nullptr);
    REQUIRE(textures->magic == MELEE_HOST_PARTICLE_TEX_BANK_MAGIC);
    REQUIRE(textures->group_count == 1);
    HSD_PSTexGroup* const group = textures->groups[0];
    REQUIRE(group != nullptr);
    REQUIRE(group->num == 1);
    REQUIRE(group->fmt == 1);
    REQUIRE(group->width == 4);
    REQUIRE(group->texTable[0] != nullptr);
    REQUIRE(group->texTable[0][0] == 0x5A);

    REQUIRE(table->effects[0].lifetime == 7.5F);
    REQUIRE(table->effects[1].lifetime == 0.0F);
    REQUIRE(table->effects[0].model[0] == nullptr);

    // The particle system takes the tables from the host banks.
    psInitDataBank(7, static_cast<int*>(static_cast<void*>(commands)),
                   static_cast<int*>(static_cast<void*>(textures)), nullptr,
                   nullptr);
    REQUIRE(psCmdListArray[7] == 101);
    REQUIRE(ptclref_804D0E5C[7][100] == list);
    REQUIRE(psTexGroupArray[7][0] == group);
    psCmdListArray[7] = 0;
    ptclref_804D0E5C[7] = nullptr;
    psTexGroupArray[7] = nullptr;
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);

    // A table with one bank and not the other is refused.
    ArchiveBuilder lopsided(0x100);
    lopsided.pointer(0x00, 0x40);
    lopsided.u32(0x40, 0x00420000U);
    lopsided.public_symbol(0x00, "effTestDataTable");
    std::vector<std::byte> lopsided_bytes = lopsided.build();
    HSD_Archive lopsided_archive{};
    REQUIRE(HSD_ArchiveParse(&lopsided_archive, bytes_of(lopsided_bytes),
                             lopsided_bytes.size()) == 0);
    REQUIRE(HSD_ArchiveGetPublicAddress(&lopsided_archive,
                                        "effTestDataTable") == nullptr);
    REQUIRE(std::string_view(melee_host_hsd_archive_last_error())
                .find("one particle bank") != std::string_view::npos);
    REQUIRE(melee_host_hsd_archive_release(lopsided_bytes.data()) ==
            MELEE_HOST_OK);
}

TEST_CASE("parsing a buffer again replaces what was built from it")
{
    std::vector<std::byte> bytes = make_title_like_archive();
    const MeleeHostHsdArchiveStats before = archive_stats();
    HSD_Archive archive{};
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    REQUIRE(HSD_ArchiveParse(&archive, bytes_of(bytes), bytes.size()) == 0);
    REQUIRE(archive_stats().archives_live == before.archives_live + 1);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) == MELEE_HOST_OK);
    REQUIRE(archive_stats().archives_live == before.archives_live);
    REQUIRE(melee_host_hsd_archive_release(bytes.data()) ==
            MELEE_HOST_INVALID_ARGUMENT);
}

TEST_CASE("symbol kinds follow the name's suffix, longest first")
{
    REQUIRE(melee_host_hsd_symbol_kind("TtlMoji_Top_joint") ==
            MELEE_HOST_HSD_SYMBOL_JOINT);
    REQUIRE(melee_host_hsd_symbol_kind("TtlMoji_Top_matanim_joint") ==
            MELEE_HOST_HSD_SYMBOL_MAT_ANIM_JOINT);
    REQUIRE(melee_host_hsd_symbol_kind("TtlMoji_Top_shapeanim_joint") ==
            MELEE_HOST_HSD_SYMBOL_SHAPE_ANIM_JOINT);
    REQUIRE(melee_host_hsd_symbol_kind("TtlMoji_Top_animjoint") ==
            MELEE_HOST_HSD_SYMBOL_ANIM_JOINT);
    REQUIRE(melee_host_hsd_symbol_kind("ScTitle_cam_int1_camera") ==
            MELEE_HOST_HSD_SYMBOL_CAMERA);
    REQUIRE(melee_host_hsd_symbol_kind("ScTitle_scene_lights") ==
            MELEE_HOST_HSD_SYMBOL_SCENE_LIGHTS);
    REQUIRE(melee_host_hsd_symbol_kind("ScTitle_fog") ==
            MELEE_HOST_HSD_SYMBOL_FOG);
    REQUIRE(melee_host_hsd_symbol_kind("TitleMark_sobjdesc") ==
            MELEE_HOST_HSD_SYMBOL_SOBJ_DESC);
    REQUIRE(melee_host_hsd_symbol_kind(
                "PlyMario5K_Share_ACTION_Wait1_figatree") ==
            MELEE_HOST_HSD_SYMBOL_FIGATREE);
    REQUIRE(melee_host_hsd_symbol_kind("ScTitle_scene_data") ==
            MELEE_HOST_HSD_SYMBOL_SCENE_DATA);
    REQUIRE(melee_host_hsd_symbol_kind("DmgNum_scene_models") ==
            MELEE_HOST_HSD_SYMBOL_SCENE_MODELS);
    // The HUD's models, which IfAll.dat names without a suffix.
    REQUIRE(melee_host_hsd_symbol_kind("Stc_scemdls") ==
            MELEE_HOST_HSD_SYMBOL_SCENE_MODELS);
    REQUIRE(melee_host_hsd_symbol_kind("lupe") ==
            MELEE_HOST_HSD_SYMBOL_SCENE_MODELS);
    // The results screen's scenes, which GmRst names without a suffix.
    REQUIRE(melee_host_hsd_symbol_kind("pnlsce") ==
            MELEE_HOST_HSD_SYMBOL_SCENE_DATA);
    REQUIRE(melee_host_hsd_symbol_kind("flmsce") ==
            MELEE_HOST_HSD_SYMBOL_SCENE_DATA);
    // Text tables are named by a prefix instead.
    REQUIRE(melee_host_hsd_symbol_kind("SIS_MenuData") ==
            MELEE_HOST_HSD_SYMBOL_SIS_TABLE);
    REQUIRE(melee_host_hsd_symbol_kind("SIS_") ==
            MELEE_HOST_HSD_SYMBOL_UNSUPPORTED);
    // A suffix on its own names nothing.
    REQUIRE(melee_host_hsd_symbol_kind("_joint") ==
            MELEE_HOST_HSD_SYMBOL_UNSUPPORTED);
    REQUIRE(melee_host_hsd_symbol_kind(nullptr) ==
            MELEE_HOST_HSD_SYMBOL_UNSUPPORTED);
}
