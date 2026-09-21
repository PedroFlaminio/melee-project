#include "assets/hsd_archive.hpp"
#include "gx/tev.hpp"
#include "gx/view.hpp"
#include "assets/gx_texture.hpp"
#include "assets/hsd_runtime_archive.hpp"
#include "assets/schemas/db_common.hpp"
#include "assets/schemas/scene_graphics.hpp"
#include "assets/virtual_disc.hpp"

#if defined(MELEE_HOST_SDL_RENDERER)
#include "render/sdl_gl_renderer.hpp"
#include "custom_textures.hpp"
#endif

#if defined(_WIN32)
#include "windows_asset_setup.hpp"
#endif

#include <dolphin/dvd.h>
#include <dolphin/gx/GXDispList.h>
#include <dolphin/gx/GXStruct.h>
#include <dolphin/gx/GXTev.h>
#include <dolphin/gx/GXGeometry.h>
#include <dolphin/gx/GXManage.h>
#include <dolphin/pad.h>
#include <dolphin/vi.h>
#include <melee_host/archive_probe.h>
#include <melee_host/ax_mixer.h>
#include <melee_host/baselib.h>
#include <melee_host/boot.h>
#include <melee_host/dolphin_time.h>
#include <melee_host/hsd_archive.h>
#include <melee_host/gx.h>
#include <melee_host/host.h>
#include <melee_host/input.h>
#include <melee_host/local_match.h>
#include <melee_host/match_trace.h>
#include <melee_host/match_rules.h>
#include <melee_host/menu_native.h>
#include <melee_host/scene_graphics.h>
#include <melee_host/scene_runtime.h>
#include <melee_host/sound_bank.h>
#include <melee_host/video.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace {

/* The canonical 44-byte header of 16-bit stereo PCM at 32 kHz, over the
 * placeholder a WAV= entry wrote first. */
bool write_wav_header(std::FILE* file, mh_u64 pairs)
{
    const auto put32 = [](unsigned char* out, mh_u32 value) {
        for (int i = 0; i < 4; ++i) {
            out[i] = static_cast<unsigned char>(value >> (8 * i));
        }
    };
    const auto put16 = [](unsigned char* out, mh_u32 value) {
        out[0] = static_cast<unsigned char>(value);
        out[1] = static_cast<unsigned char>(value >> 8);
    };
    const mh_u32 data_bytes = static_cast<mh_u32>(pairs * 4U);
    std::array<unsigned char, 44> header{};
    std::memcpy(header.data(), "RIFF", 4);
    put32(header.data() + 4, 36U + data_bytes);
    std::memcpy(header.data() + 8, "WAVEfmt ", 8);
    put32(header.data() + 16, 16U);
    put16(header.data() + 20, 1U);
    put16(header.data() + 22, 2U);
    put32(header.data() + 24, 32000U);
    put32(header.data() + 28, 32000U * 4U);
    put16(header.data() + 32, 4U);
    put16(header.data() + 34, 16U);
    std::memcpy(header.data() + 36, "data", 4);
    put32(header.data() + 40, data_bytes);
    return std::fseek(file, 0, SEEK_SET) == 0 &&
           std::fwrite(header.data(), 1, header.size(), file) ==
               header.size();
}

} // namespace

namespace {

struct DiagnosticDvdRead {
    bool completed = false;
    s32 result = DVD_RESULT_FATAL_ERROR;
};

void diagnostic_dvd_read_complete(s32 result, DVDFileInfo* file_info)
{
    auto* state = static_cast<DiagnosticDvdRead*>(file_info->cb.userData);
    state->completed = true;
    state->result = result;
}

std::vector<std::byte> read_file(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("unable to open " + path.string());
    }

    const std::vector<char> chars((std::istreambuf_iterator<char>(stream)),
                                  std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes;
    bytes.reserve(chars.size());
    for (const char value : chars) {
        bytes.push_back(static_cast<std::byte>(
            static_cast<unsigned char>(value)));
    }
    return bytes;
}

void print_usage(const char* executable)
{
    std::cerr << "usage:\n"
              << "  " << executable << " --diagnose\n"
              << "  " << executable << " --inspect-match-rules\n"
              << "  " << executable << " --reset-match-rules\n"
              << "  " << executable << " --diagnose-native-menu\n"
              << "  " << executable << " --diagnose-local-match\n"
              << "  " << executable << " --diagnose-scene-runtime [FRAMES]\n"
              << "  " << executable << " --inspect-hsd FILE\n"
              << "  " << executable << " --load-archive FILE [SYMBOL...]\n"
              << "  " << executable << " --sweep-archives DIRECTORY\n"
              << "  " << executable << " --boot-title-archive DIRECTORY\n"
              << "  " << executable << " --inspect-pobj FILE SYMBOL\n"
              << "  " << executable
              << " --load-scene FILE SYMBOL [MODEL_INDEX]\n"
              << "  " << executable << " --load-joint FILE SYMBOL\n"
              << "  " << executable
              << " --render-scene FILE SYMBOL [MODEL_INDEX]\n"
              << "  " << executable << " --render-joint FILE SYMBOL\n"
              << "  " << executable
              << " --animate-joint FILE SYMBOL ANIMFILE ANIMSYM MATSYM"
                 " [FRAMES]\n"
              << "  " << executable << " --list-animations FILE\n"
              << "  " << executable
              << " --animate-named FILE SYMBOL ANIMFILE ANIMSYM [FRAMES]\n"
#if defined(MELEE_HOST_SDL_RENDERER)
              << "  " << executable
              << " --view-animation FILE SYMBOL ANIMFILE ANIMSYM\n"
              << " --view-title-scene DIR [BMP FRAME]\n"
#endif
              << ""
#if defined(MELEE_HOST_SDL_RENDERER)
              << "  " << executable << " --view-pobj FILE SYMBOL\n"
              << "  " << executable
              << " --view-scene FILE SYMBOL [MODEL_INDEX]\n"
              << "  " << executable << " --view-joint FILE SYMBOL\n"
              << "  " << executable
              << " --tev-conformance-scene FILE SYMBOL [MODEL_INDEX]\n"
              << "  " << executable << " --tev-conformance-joint FILE SYMBOL\n"
#endif
              << "  " << executable << " --inspect-resources DIRECTORY\n"
              << "  " << executable << " --read-resource DIRECTORY PATH\n";
}

int diagnose()
{
    MeleeHostContext* context = nullptr;
    const MeleeHostConfig config{ .resource_root = nullptr, .headless = true };
    const MeleeHostStatus create_status = melee_host_create(&config, &context);
    if (create_status != MELEE_HOST_OK) {
        std::cerr << "host create failed: "
                  << melee_host_status_string(create_status) << '\n';
        return 1;
    }

    const mh_u64 first = melee_host_monotonic_nanoseconds();
    const mh_u64 second = melee_host_monotonic_nanoseconds();
    const MeleeHostStatus baselib_status = melee_host_baselib_bootstrap();
    const MeleeHostStatus step_status = melee_host_step(context);
    melee_host_destroy(context);

    std::cout << "melee native host skeleton\n"
              << "  pointer bits: " << sizeof(void*) * 8 << '\n'
              << "  mh_u32 bytes: " << sizeof(mh_u32) << '\n'
              << "  monotonic clock: " << (second >= first ? "ok" : "failed")
              << '\n'
              << "  baselib bootstrap: "
              << melee_host_status_string(baselib_status) << '\n'
              << "  game step: " << melee_host_status_string(step_status)
              << '\n';
    return second >= first && baselib_status == MELEE_HOST_OK ? 0 : 1;
}

int inspect_match_rules()
{
    MeleeHostMatchRules rules{};
    const MeleeHostStatus status = melee_host_match_rules_get(&rules);
    if (status != MELEE_HOST_OK) {
        std::cerr << "match rules unavailable: "
                  << melee_host_status_string(status) << '\n';
        return 1;
    }
    std::cout << "native VS rules store\n"
              << "  mode: " << static_cast<unsigned>(rules.mode) << '\n'
              << "  time limit: "
              << static_cast<unsigned>(rules.time_limit) << '\n'
              << "  stock count: "
              << static_cast<unsigned>(rules.stock_count) << '\n'
              << "  handicap: " << static_cast<unsigned>(rules.handicap)
              << '\n'
              << "  damage ratio: "
              << static_cast<unsigned>(rules.damage_ratio) << '\n'
              << "  friendly fire: " << (rules.friendly_fire ? "on" : "off")
              << '\n'
              << "  pause: " << (rules.pause ? "on" : "off") << '\n';
    return 0;
}

int reset_match_rules()
{
    const MeleeHostStatus status = melee_host_match_rules_reset_defaults();
    if (status != MELEE_HOST_OK) {
        std::cerr << "match rule reset failed: "
                  << melee_host_status_string(status) << '\n';
        return 1;
    }
    return inspect_match_rules();
}

int diagnose_native_menu()
{
    MeleeHostContext* context = nullptr;
    const MeleeHostConfig config{ .resource_root = nullptr, .headless = true };
    if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
        melee_host_activate_pad_backend(context) != MELEE_HOST_OK ||
        melee_host_native_menu_init() != MELEE_HOST_OK)
    {
        std::cerr << "could not initialize native menu input\n";
        melee_host_destroy(context);
        return 1;
    }
    const MeleeHostPadState input{
        .buttons = PAD_BUTTON_A,
        .stick_x = 0,
        .stick_y = 0,
        .c_stick_x = 0,
        .c_stick_y = 0,
        .trigger_left = 0,
        .trigger_right = 0,
        .connected = true,
    };
    mh_u32 events = 0;
    const MeleeHostStatus submit =
        melee_host_submit_pad_state(context, 0, &input);
    const MeleeHostStatus step = melee_host_step(context);
    const MeleeHostStatus update = melee_host_native_menu_update(
        MELEE_HOST_MAX_CONTROLLERS, &events);
    melee_host_destroy(context);
    if (submit != MELEE_HOST_OK || step != MELEE_HOST_NOT_READY ||
        update != MELEE_HOST_OK)
    {
        std::cerr << "native menu input update failed\n";
        return 1;
    }
    std::cout << "native menu input\n"
              << "  A event: "
              << (((events & MELEE_HOST_NATIVE_MENU_A) != 0) ? "ok" : "failed")
              << '\n'
              << "  confirm event: "
              << (((events & MELEE_HOST_NATIVE_MENU_CONFIRM) != 0) ? "ok"
                                                                  : "failed")
              << '\n';
    return (events & (MELEE_HOST_NATIVE_MENU_A |
                      MELEE_HOST_NATIVE_MENU_CONFIRM)) ==
                   (MELEE_HOST_NATIVE_MENU_A | MELEE_HOST_NATIVE_MENU_CONFIRM)
               ? 0
               : 1;
}

int diagnose_local_match()
{
    if (melee_host_match_rules_reset_defaults() != MELEE_HOST_OK) {
        std::cerr << "could not initialize native VS rules\n";
        return 1;
    }
    const MeleeHostStatus prepare =
        melee_host_prepare_local_two_player_match(2, 8, 20);
    MeleeHostPreparedMatch match{};
    const MeleeHostStatus inspect = melee_host_prepared_match_get(&match);
    const MeleeHostStatus initialize =
        melee_host_initialize_prepared_player_state();
    MeleeHostPlayerState first{};
    MeleeHostPlayerState second{};
    const MeleeHostStatus first_status = melee_host_player_state_get(0, &first);
    const MeleeHostStatus second_status =
        melee_host_player_state_get(1, &second);
    if (prepare != MELEE_HOST_OK || inspect != MELEE_HOST_OK ||
        initialize != MELEE_HOST_OK || first_status != MELEE_HOST_OK ||
        second_status != MELEE_HOST_OK)
    {
        std::cerr << "could not prepare native VS start data\n";
        return 1;
    }
    std::cout << "native local VS start data\n"
              << "  players: " << static_cast<unsigned>(match.player_count)
              << "\n  characters: " << static_cast<int>(match.characters[0])
              << ", " << static_cast<int>(match.characters[1])
              << "\n  stage: " << match.stage_kind
              << "\n  match kind: " << static_cast<unsigned>(match.match_kind)
              << "\n  timer seconds: " << match.time_limit_seconds
              << "\n  materialized slots: " << static_cast<int>(first.character)
              << ", " << static_cast<int>(second.character) << '\n';
    return 0;
}

struct SceneRuntimeProbe {
    unsigned frames = 0;
    unsigned p_link = 0;
};

void scene_runtime_probe_tick(void* user_data)
{
    static_cast<SceneRuntimeProbe*>(user_data)->frames += 1;
}

unsigned scene_runtime_draw_done_callbacks = 0;

void scene_runtime_draw_done_callback(void)
{
    scene_runtime_draw_done_callbacks += 1;
}

// Drives the original HSD process scheduler for a fixed number of frames.
// One probe object runs unpaused while a second sits on a paused p_link, so
// the report shows both that processes fire and that the pause mask is
// honoured by the original HSD_GObj_RunProcs.
int diagnose_scene_runtime(unsigned frames)
{
    if (melee_host_scene_runtime_init() != MELEE_HOST_OK) {
        std::cerr << "could not initialize the native HSD object runtime\n";
        return 1;
    }

    /* Exercise the same frame boundary the host game loop will use: a GX
     * fence completes after process work, then VI presents one field. */
    VIInit();
    melee_host_gx_state_reset();
    scene_runtime_draw_done_callbacks = 0;
    GXSetDrawDoneCallback(scene_runtime_draw_done_callback);

    SceneRuntimeProbe running{ .frames = 0, .p_link = 0 };
    SceneRuntimeProbe paused{ .frames = 0, .p_link = 1 };
    MeleeHostSceneObject running_object = 0;
    MeleeHostSceneObject paused_object = 0;
    if (melee_host_scene_runtime_add_object(
            1, static_cast<mh_u8>(running.p_link), 0, 0,
            scene_runtime_probe_tick, &running, &running_object) !=
            MELEE_HOST_OK ||
        melee_host_scene_runtime_add_object(
            2, static_cast<mh_u8>(paused.p_link), 0, 1,
            scene_runtime_probe_tick, &paused, &paused_object) !=
            MELEE_HOST_OK)
    {
        std::cerr << "could not create native scene objects\n";
        return 1;
    }

    if (melee_host_scene_runtime_set_paused_links(
            1ULL << paused.p_link) != MELEE_HOST_OK)
    {
        std::cerr << "could not apply the native pause mask\n";
        return 1;
    }

    MeleeHostSceneRuntimeStats start{};
    if (melee_host_scene_runtime_stats(&start) != MELEE_HOST_OK) {
        std::cerr << "could not read native scene runtime statistics\n";
        return 1;
    }
    for (unsigned frame = 0; frame < frames; ++frame) {
        GXSetDrawDone();
        if (melee_host_scene_runtime_run_frame() != MELEE_HOST_OK) {
            std::cerr << "native scene frame " << frame << " failed\n";
            return 1;
        }
    }

    MeleeHostSceneRuntimeStats end{};
    if (melee_host_scene_runtime_stats(&end) != MELEE_HOST_OK) {
        std::cerr << "could not read native scene runtime statistics\n";
        return 1;
    }
    MeleeHostVideoState video{};
    if (melee_host_video_state(&video) != MELEE_HOST_OK) {
        std::cerr << "could not read native video statistics\n";
        return 1;
    }

    std::cout << "native HSD scene runtime\n"
              << "  frames run: " << (end.frame_count - start.frame_count)
              << "\n  live objects: " << end.objects_live
              << "\n  live processes: " << end.procs_live
              << "\n  pooled objects: " << end.objects_pooled
              << "\n  VI retraces: " << video.retrace_count
              << "\n  draw-done callbacks: " << scene_runtime_draw_done_callbacks
              << "\n  unpaused process calls: " << running.frames
              << "\n  paused process calls: " << paused.frames << '\n';

    const bool ok = running.frames == frames && paused.frames == 0 &&
                    end.frame_count - start.frame_count == frames &&
                    video.retrace_count == frames &&
                    scene_runtime_draw_done_callbacks == frames;
    GXSetDrawDoneCallback(nullptr);
    if (melee_host_scene_runtime_remove_object(running_object) !=
            MELEE_HOST_OK ||
        melee_host_scene_runtime_remove_object(paused_object) !=
            MELEE_HOST_OK)
    {
        std::cerr << "could not release native scene objects\n";
        return 1;
    }
    return ok ? 0 : 1;
}

int inspect_hsd(const std::filesystem::path& path)
{
    const std::vector<std::byte> bytes = read_file(path);
    const melee::assets::HsdRuntimeArchive runtime(bytes);
    const melee::assets::HsdArchiveView& archive = runtime.disk_view();
    const auto& header = archive.header();

    std::cout << "HSD archive: " << path << '\n'
              << "  file size: " << header.file_size << '\n'
              << "  data size: " << header.data_size << '\n'
              << "  relocations: " << header.relocation_count << '\n'
              << "  public symbols: " << header.public_count << '\n'
              << "  external symbols: " << header.extern_count << '\n';
    std::cout << "  runtime internal references: "
              << runtime.internal_references().size() << '\n';
    for (const auto& symbol : archive.public_symbols()) {
        std::cout << "    " << symbol.name << " @ data+0x" << std::hex
                  << symbol.data_offset << std::dec << '\n';
        if (symbol.name == "dbLoadCommonData") {
            const auto schema =
                melee::assets::schemas::decode_db_load_common_data(runtime);
            std::cout << "      bonus names @ data+0x" << std::hex
                      << schema.bonus_names.data_offset
                      << ", motion states @ data+0x"
                      << schema.motion_state_names.data_offset
                      << ", submotions @ data+0x"
                      << schema.submotion_names.data_offset << std::dec << '\n';
            const auto first_bonus =
                runtime.reference_at(schema.bonus_names, 0);
            const auto first_motion =
                runtime.reference_at(schema.motion_state_names, 0);
            const auto first_submotion =
                runtime.reference_at(schema.submotion_names, 0);
            std::cout << "      first entries: "
                      << runtime.read_c_string(first_bonus) << ", "
                      << runtime.read_c_string(first_motion) << ", "
                      << runtime.read_c_string(first_submotion) << '\n';
            const auto tables =
                melee::assets::schemas::decode_db_common_name_tables(runtime);
            std::cout << "      table sizes: " << tables.bonus_names.size()
                      << " bonus, " << tables.motion_state_names.size()
                      << " motion, " << tables.submotion_names.size()
                      << " submotion\n";
        }
    }
    return 0;
}

int inspect_resources(const std::filesystem::path& path)
{
    const melee::assets::VirtualDisc disc(path);
    std::cout << "native DVD resources: " << disc.root() << '\n'
              << "  indexed FST files: " << disc.entry_count() << '\n';
    return 0;
}

int inspect_pobj(const std::filesystem::path& path, std::string_view symbol,
                 bool preview = false)
{
    const std::vector<std::byte> bytes = read_file(path);
    const melee::assets::HsdRuntimeArchive runtime(bytes);
    const auto geometries =
        melee::assets::schemas::find_scene_pobjs(runtime, symbol);
    const std::size_t textured_geometries = static_cast<std::size_t>(
        std::count_if(geometries.begin(), geometries.end(),
                      [](const auto& geometry) {
                          return geometry.material.has_texture;
                      }));
    const std::size_t translucent_geometries = static_cast<std::size_t>(
        std::count_if(geometries.begin(), geometries.end(),
                      [](const auto& geometry) {
                          return (geometry.material.render_mode & (1U << 30U)) != 0;
                      }));
    std::map<std::uint32_t, std::size_t> texture_formats;
    std::map<std::uint64_t, mh_u32> texture_ids;
#if defined(MELEE_HOST_SDL_RENDERER)
    std::vector<melee::render::TextureImage> renderer_textures;
#endif
    std::size_t decoded_textures = 0;
    for (const auto& geometry : geometries) {
        if (geometry.material.image_data.has_value()) {
            ++texture_formats[geometry.material.texture_format];
            const bool indexed = geometry.material.texture_format == 8 ||
                                 geometry.material.texture_format == 9 ||
                                 geometry.material.texture_format == 10;
            if (geometry.material.texture_format == 0 ||
                geometry.material.texture_format == 1 ||
                geometry.material.texture_format == 2 ||
                geometry.material.texture_format == 3 ||
                geometry.material.texture_format == 4 ||
                geometry.material.texture_format == 5 ||
                geometry.material.texture_format == 6 || indexed ||
                geometry.material.texture_format == 14)
            {
                const std::size_t byte_count = melee::assets::gx_texture_data_size(
                    geometry.material.texture_width,
                    geometry.material.texture_height,
                    geometry.material.texture_format);
                const auto image = runtime.bytes_at(*geometry.material.image_data,
                                                    byte_count);
                melee::assets::DecodedTexture decoded{};
                std::uint64_t texture_key = geometry.material.image_data->data_offset;
                if (indexed) {
                    if (!geometry.material.tlut_data.has_value() ||
                        geometry.material.tlut_entries == 0)
                    {
                        continue;
                    }
                    const std::size_t tlut_size =
                        static_cast<std::size_t>(geometry.material.tlut_entries) * 2;
                    const auto tlut = runtime.bytes_at(*geometry.material.tlut_data,
                                                       tlut_size);
                    decoded = melee::assets::decode_gx_texture_with_tlut(
                        image, geometry.material.texture_width,
                        geometry.material.texture_height,
                        geometry.material.texture_format, tlut,
                        geometry.material.tlut_format);
                    texture_key |= static_cast<std::uint64_t>(
                        geometry.material.tlut_data->data_offset) << 32U;
                } else {
                    decoded = melee::assets::decode_gx_texture(
                        image, geometry.material.texture_width,
                        geometry.material.texture_height,
                        geometry.material.texture_format);
                }
#if defined(MELEE_HOST_SDL_RENDERER)
                const auto insertion = texture_ids.emplace(
                    texture_key,
                    static_cast<mh_u32>(renderer_textures.size()));
                if (insertion.second) {
                    renderer_textures.push_back({
                        decoded.width, decoded.height,
                        geometry.material.texture_wrap_s,
                        geometry.material.texture_wrap_t, decoded.rgba, false, false
                    });
                }
#else
                static_cast<void>(decoded);
#endif
                ++decoded_textures;
            }
        }
    }

    /* The schema path binds no GX texture and lights nothing: the material it
     * decoded reaches the vertices through melee_host_gx_apply_material.  A
     * modulate stage is the program that combines the two. */
    melee_host_gx_state_reset();
    GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    melee_host_gx_reset_command_log();
    std::size_t display_bytes = 0;
    for (const auto& geometry : geometries) {
        GXClearVtxDesc();
        for (const auto& descriptor : geometry.vertices) {
            if (descriptor.attribute >= GX_VA_MAX_ATTR ||
                descriptor.attribute_type > GX_INDEX16 ||
                descriptor.component_count > GX_NRM_NBT3 ||
                descriptor.component_type > GX_RGBA8 ||
                descriptor.stride > 0xFF)
            {
                throw melee::assets::HsdArchiveError(
                    "PObj contains an unsupported vertex descriptor");
            }
            const auto attribute = static_cast<GXAttr>(descriptor.attribute);
            const auto attribute_type =
                static_cast<GXAttrType>(descriptor.attribute_type);
            GXSetVtxDesc(attribute, attribute_type);
            GXSetVtxAttrFmt(GX_VTXFMT0, attribute,
                            static_cast<GXCompCnt>(descriptor.component_count),
                            static_cast<GXCompType>(descriptor.component_type),
                            descriptor.fractional_bits);
            if (descriptor.array.has_value()) {
                const std::size_t remaining =
                    runtime.disk_view().header().data_size -
                    descriptor.array->data_offset;
                const auto array =
                    runtime.bytes_at(*descriptor.array, remaining);
                melee_host_gx_set_array_bounded(
                    descriptor.attribute, array.data(), array.size(),
                    static_cast<mh_u8>(descriptor.stride));
            }
        }

        const std::size_t display_size =
            static_cast<std::size_t>(geometry.display_list_blocks) * 32;
        const auto display =
            runtime.bytes_at(geometry.display_list, display_size);
        const std::size_t first_vertex = melee_host_gx_captured_vertex_count();
        GXCallDisplayList(const_cast<std::byte*>(display.data()),
                          static_cast<u32>(display.size()));
        MeleeHostGxAffineTransform transform{};
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 4; ++column) {
                transform.values[row][column] =
                    geometry.transform.values[row][column];
            }
        }
        melee_host_gx_transform_vertices(
            first_vertex, melee_host_gx_captured_vertex_count() - first_vertex,
            &transform);
        melee_host_gx_apply_material(
            first_vertex, melee_host_gx_captured_vertex_count() - first_vertex,
            geometry.material.diffuse.data(),
            geometry.material.image_data.has_value() && texture_ids.contains(
                static_cast<std::uint64_t>(geometry.material.image_data->data_offset) |
                (geometry.material.tlut_data.has_value()
                     ? static_cast<std::uint64_t>(
                           geometry.material.tlut_data->data_offset) << 32U
                     : 0U))
                ? texture_ids.at(static_cast<std::uint64_t>(
                                     geometry.material.image_data->data_offset) |
                                 (geometry.material.tlut_data.has_value()
                                      ? static_cast<std::uint64_t>(
                                            geometry.material.tlut_data->data_offset)
                                            << 32U
                                      : 0U))
                : MELEE_HOST_GX_NO_TEXTURE,
            geometry.material.render_mode);
        display_bytes += display.size();
    }

    std::cout << "HSD scene geometry: " << path << " :: " << symbol << '\n'
              << "  drawable PObjs: " << geometries.size() << '\n'
              << "  PObjs with TObj: " << textured_geometries << '\n'
              << "  translucent PObjs: " << translucent_geometries << '\n'
              << "  display list bytes: " << display_bytes << '\n'
              << "  decoded vertices: "
              << melee_host_gx_captured_vertex_count() << '\n'
              << "  decoded triangles: " << melee_host_gx_triangle_count()
              << '\n'
              << "  display list errors: "
              << melee_host_gx_display_list_error_count() << '\n';
    if (!texture_formats.empty()) {
        std::cout << "  image formats:";
        for (const auto& [format, count] : texture_formats) {
            std::cout << " GX_" << format << '=' << count;
        }
        std::cout << '\n';
    }
    if (decoded_textures != 0) {
        std::cout << "  decoded supported textures: " << decoded_textures << '\n';
    }
    if (melee_host_gx_display_list_error_count() != 0) {
        return 1;
    }
#if defined(MELEE_HOST_SDL_RENDERER)
    if (preview) {
        std::string error;
        melee::render::set_texture_images(std::move(renderer_textures));
        MeleeHostContext* context = nullptr;
        const MeleeHostConfig config{ .resource_root = nullptr, .headless = false };
        if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
            melee_host_activate_pad_backend(context) != MELEE_HOST_OK)
        {
            melee_host_destroy(context);
            std::cerr << "could not initialize SDL input context\n";
            return 1;
        }
        const bool rendered = melee::render::show_captured_geometry(context, &error);
        melee_host_destroy(context);
        if (!rendered) {
            std::cerr << "geometry preview failed: " << error << '\n';
            return 1;
        }
    }
#else
    static_cast<void>(preview);
#endif
    return 0;
}

int read_resource(const std::filesystem::path& root, std::string path)
{
    MeleeHostContext* context = nullptr;
    const std::string root_string = root.string();
    const MeleeHostConfig config{ .resource_root = root_string.c_str(),
                                  .headless = true };
    if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
        melee_host_activate_dvd_backend(context) != MELEE_HOST_OK) {
        std::cerr << "could not initialize the virtual DVD\n";
        melee_host_destroy(context);
        return 1;
    }
    std::vector<char> mutable_path(path.begin(), path.end());
    mutable_path.push_back('\0');
    DVDFileInfo file{};
    if (!DVDOpen(mutable_path.data(), &file)) {
        std::cerr << "DVD resource was not found: " << path << '\n';
        melee_host_destroy(context);
        return 1;
    }

    const std::size_t sample_size = std::min<std::size_t>(file.length, 16);
    std::array<std::byte, 16> sample{};
    DiagnosticDvdRead read{};
    file.cb.userData = &read;
    const BOOL submitted = DVDReadAsyncPrio(
        &file, sample.data(), static_cast<s32>(sample_size), 0,
        diagnostic_dvd_read_complete, 2);
    if (submitted) {
        static_cast<void>(melee_host_step(context));
    }
    DVDClose(&file);
    melee_host_destroy(context);
    if (!submitted || !read.completed ||
        read.result != static_cast<s32>(sample_size)) {
        std::cerr << "DVD resource read failed\n";
        return 1;
    }

    std::cout << "DVD resource: " << path << '\n'
              << "  size: " << file.length << '\n'
              << "  asynchronous tick read: ok\n"
              << "  first " << sample_size << " bytes:";
    for (std::size_t index = 0; index < sample_size; ++index) {
        std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0')
                  << std::to_integer<unsigned int>(sample[index]);
    }
    std::cout << std::dec << '\n';
    return 0;
}

/* Loads a scene from disk through the original object layer, rather than
 * through the read-only schema the geometry preview uses. */
#if defined(MELEE_HOST_SDL_RENDERER)
/* Shows what the original display path drew, rather than the geometry the
 * read-only schema decodes separately.  The capture is asked for in world
 * space so the viewer can move its own camera around the model. */
/* Decodes the textures a capture bound, in the order the vertices index them:
 * an image that cannot be decoded still has to occupy its slot or every id
 * after it would point at the wrong picture. */
melee::render::TextureImage decode_captured_texture(mh_u32 id,
                                                    bool* decoded_out)
{
    MeleeHostGxTextureDesc desc{};
    melee::render::TextureImage image{ 1, 1, 0, 0, { 255, 255, 255, 255 },
                                       false, false };
    *decoded_out = false;
    if (!melee_host_gx_captured_texture_at(id, &desc) ||
        desc.image == nullptr)
    {
        return image;
    }
    image.gx_format = desc.format;
    try {
        MeleeHostGxTlutDesc tlut{};
        if (desc.color_indexed) melee_host_gx_captured_texture_tlut(id, &tlut);
        std::vector<std::uint8_t> custom_rgba;
        std::uint16_t custom_w = 0, custom_h = 0;
        if (melee::render::load_custom_texture(desc, tlut, custom_rgba, custom_w, custom_h)) {
            image = { custom_w, custom_h, desc.wrap_s, desc.wrap_t, std::move(custom_rgba), desc.mag_filter != 0, desc.mipmap != 0, image.generation };
            *decoded_out = true;
            return image;
        }

        /* The size throws for a format the decoder does not know, so it
         * belongs inside the try like the decode itself. */
        const std::size_t byte_count = melee::assets::gx_texture_data_size(
            desc.width, desc.height, desc.format);
        const std::span<const std::byte> data{
            static_cast<const std::byte*>(desc.image), byte_count
        };
        melee::assets::DecodedTexture decoded{};
        if (desc.color_indexed &&
            melee_host_gx_captured_texture_tlut(id, &tlut) && tlut.loaded &&
            tlut.entries != nullptr)
        {
            decoded = melee::assets::decode_gx_texture_with_tlut(
                data, desc.width, desc.height, desc.format,
                { static_cast<const std::byte*>(tlut.entries),
                  static_cast<std::size_t>(tlut.entry_count) * 2 },
                tlut.format);
        } else if (!desc.color_indexed) {
            decoded = melee::assets::decode_gx_texture(
                data, desc.width, desc.height, desc.format);
        }
        if (!decoded.rgba.empty()) {
            image = { decoded.width, decoded.height, desc.wrap_s,
                      desc.wrap_t,   std::move(decoded.rgba),
                      desc.mag_filter != 0, desc.mipmap != 0 };
            *decoded_out = true;
        }
    } catch (const std::exception& error) {
        std::cerr << "texture " << id << " (format 0x" << std::hex
                  << desc.format << std::dec << ", " << desc.width << 'x'
                  << desc.height << ") not decoded: " << error.what() << '\n';
    }
    return image;
}

void decode_captured_textures(mh_u32 count,
                              std::vector<melee::render::TextureImage>* images,
                              std::size_t* decoded_count)
{
    for (mh_u32 id = 0; id < count; ++id) {
        bool decoded = false;
        images->push_back(decode_captured_texture(id, &decoded));
        if (decoded) {
            *decoded_count += 1;
        }
    }
}

/* The images for the textures each title frame captured.  The captured texture
 * table is rebuilt every frame, and its order changes as the scene animates,
 * but it names the same image data, so each texture is decoded once, by its
 * data and format, and keeps the address the presenter keys its GL texture
 * on. */
class TitleTextureCache {
public:
    std::vector<const melee::render::TextureImage*> images_for_frame()
    {
        const auto count =
            static_cast<mh_u32>(melee_host_gx_captured_texture_count());
        std::vector<const melee::render::TextureImage*> images;
        images.reserve(count);
        const std::uint64_t revision = melee::render::custom_texture_revision();
        for (mh_u32 id = 0; id < count; ++id) {
            const TextureKey key = key_of(id);
            const mh_u32 generation = generation_of(id);
            auto found = images_.find(key);
            if (found == images_.end()) {
                bool decoded = false;
                CachedTexture entry;
                entry.image = decode_captured_texture(id, &decoded);
                entry.captured_generation = generation;
                entry.custom_texture_revision = revision;
                entry.image.generation = ++image_generation_;
                found = images_.emplace(key, std::move(entry)).first;
            } else if (found->second.captured_generation != generation ||
                       found->second.custom_texture_revision != revision) {
                /* An EFB copy wrote the image again: decode it in place, so
                 * the presenter keeps the address it knows it by. */
                bool decoded = false;
                found->second.image = decode_captured_texture(id, &decoded);
                found->second.captured_generation = generation;
                found->second.custom_texture_revision = revision;
                found->second.image.generation = ++image_generation_;
            }
            images.push_back(&found->second.image);
        }
        return images;
    }

private:
    /* The image and palette data, then size, formats, wrap and filter. */
    using TextureKey = std::array<mh_u64, 6>;

    static mh_u32 generation_of(mh_u32 id)
    {
        MeleeHostGxTextureDesc desc{};
        return melee_host_gx_captured_texture_at(id, &desc)
                   ? melee_host_gx_texture_copy_generation(desc.image)
                   : 0;
    }

    static TextureKey key_of(mh_u32 id)
    {
        MeleeHostGxTextureDesc desc{};
        MeleeHostGxTlutDesc tlut{};
        if (!melee_host_gx_captured_texture_at(id, &desc)) {
            return {};
        }
        if (desc.color_indexed) {
            static_cast<void>(melee_host_gx_captured_texture_tlut(id, &tlut));
        }
        return {
            static_cast<mh_u64>(reinterpret_cast<uintptr_t>(desc.image)),
            static_cast<mh_u64>(reinterpret_cast<uintptr_t>(tlut.entries)),
            (static_cast<mh_u64>(desc.width) << 16U) | desc.height,
            (static_cast<mh_u64>(desc.format) << 32U) | tlut.format,
            (static_cast<mh_u64>(desc.wrap_s) << 32U) | desc.wrap_t,
            desc.mag_filter,
        };
    }

    struct CachedTexture {
        melee::render::TextureImage image;
        mh_u32 captured_generation = 0;
        std::uint64_t custom_texture_revision = 0;
    };

    std::map<TextureKey, CachedTexture> images_;
    mh_u32 image_generation_ = 0;
};

struct TitleView {
    melee::render::FramePresenter presenter;
    TitleTextureCache textures;
    MeleeHostContext* context = nullptr;
    /* With a screenshot path the presenter is hidden, the loop is not paced,
     * and the scene is asked to leave once that frame is written. */
    const char* screenshot_path = nullptr;
    mh_u32 screenshot_frame = 0;
    mh_u32 frames = 0;
    bool screenshot_written = false;
    bool screenshot_failed = false;
    std::string error;
};

/* What one frame's capture holds: its textures, and each view with the
 * triangles drawn through it. */
void report_title_capture()
{
    const std::size_t triangle_count = melee_host_gx_triangle_count();
    const std::size_t view_count = melee_host_gx_captured_view_state_count();
    std::vector<std::size_t> triangles_per_view(view_count, 0);
    for (std::size_t index = 0; index < triangle_count; ++index) {
        MeleeHostGxCapturedTriangle triangle{};
        if (melee_host_gx_captured_triangle_at(index, &triangle) &&
            triangle.vertices[0].view_state < view_count)
        {
            triangles_per_view[triangle.vertices[0].view_state] += 1;
        }
    }
    std::cout << "  capture: " << triangle_count << " triangles, "
              << view_count << " views, "
              << melee_host_gx_captured_texture_count() << " textures\n";
    for (std::size_t id = 0; id < melee_host_gx_captured_texture_count();
         ++id)
    {
        MeleeHostGxTextureDesc desc{};
        if (!melee_host_gx_captured_texture_at(id, &desc)) {
            continue;
        }
        std::cout << "    texture " << id << ": format 0x" << std::hex
                  << desc.format << std::dec << ", " << desc.width << 'x'
                  << desc.height << (desc.color_indexed ? ", indexed" : "")
                  << '\n';
    }
    for (std::size_t index = 0; index < view_count; ++index) {
        MeleeHostGxViewState state{};
        if (!melee_host_gx_captured_view_state_at(index, &state)) {
            continue;
        }
        std::cout << "    view " << index << ": "
                  << (state.projection_type == 0 ? "perspective"
                                                 : "orthographic")
                  << " [";
        for (std::size_t value = 0; value < 6; ++value) {
            std::cout << (value == 0 ? "" : " ") << state.projection[value];
        }
        std::cout << "], viewport " << state.viewport_left << ','
                  << state.viewport_top << ' ' << state.viewport_width << 'x'
                  << state.viewport_height << " z " << state.viewport_near
                  << ".." << state.viewport_far << ", scissor "
                  << state.scissor_left << ',' << state.scissor_top << ' '
                  << state.scissor_width << 'x' << state.scissor_height
                  << ", " << triangles_per_view[index] << " triangles\n";
    }

    /* Runs as the presenter draws them: consecutive triangles sharing pixel
     * state, TEV program, texture set and view, with the box they cover on
     * screen. */
    struct RunSummary {
        MeleeHostGxCapturedVertex lead{};
        std::size_t triangles = 0;
        float left = 1.0e9F;
        float top = 1.0e9F;
        float right = -1.0e9F;
        float bottom = -1.0e9F;
    };
    std::vector<MeleeHostGxViewState> views(view_count);
    for (std::size_t index = 0; index < view_count; ++index) {
        static_cast<void>(
            melee_host_gx_captured_view_state_at(index, &views[index]));
    }
    std::vector<RunSummary> runs;
    for (std::size_t index = 0; index < triangle_count; ++index) {
        MeleeHostGxCapturedTriangle triangle{};
        if (!melee_host_gx_captured_triangle_at(index, &triangle)) {
            continue;
        }
        const MeleeHostGxCapturedVertex& lead = triangle.vertices[0];
        if (runs.empty() || runs.back().lead.draw_state != lead.draw_state ||
            runs.back().lead.tev_state != lead.tev_state ||
            runs.back().lead.texture_set != lead.texture_set ||
            runs.back().lead.view_state != lead.view_state)
        {
            RunSummary summary;
            summary.lead = lead;
            runs.push_back(summary);
        }
        RunSummary& run = runs.back();
        run.triangles += 1;
        if (lead.view_state >= views.size()) {
            continue;
        }
        const MeleeHostGxViewState& state = views[lead.view_state];
        const melee::gx::ClipMatrix clip = melee::gx::clip_matrix(state);
        for (const MeleeHostGxCapturedVertex& vertex : triangle.vertices) {
            const MeleeHostGxPosition3f32& p = vertex.position;
            const float w =
                clip[3] * p.x + clip[7] * p.y + clip[11] * p.z + clip[15];
            if (w <= 0.0F) {
                continue;
            }
            const float ndc_x =
                (clip[0] * p.x + clip[4] * p.y + clip[8] * p.z + clip[12]) / w;
            const float ndc_y =
                (clip[1] * p.x + clip[5] * p.y + clip[9] * p.z + clip[13]) / w;
            const float screen_x = state.viewport_left +
                                   (ndc_x + 1.0F) * 0.5F * state.viewport_width;
            const float screen_y = state.viewport_top + (1.0F - ndc_y) * 0.5F *
                                                            state.viewport_height;
            run.left = std::min(run.left, screen_x);
            run.right = std::max(run.right, screen_x);
            run.top = std::min(run.top, screen_y);
            run.bottom = std::max(run.bottom, screen_y);
        }
    }
    for (std::size_t index = 0; index < runs.size(); ++index) {
        const RunSummary& run = runs[index];
        MeleeHostGxDrawState draw{};
        MeleeHostGxTevState tev{};
        std::array<mh_u32, MELEE_HOST_GX_MAX_TEXMAP> set{};
        set.fill(MELEE_HOST_GX_NO_TEXTURE);
        static_cast<void>(
            melee_host_gx_captured_draw_state_at(run.lead.draw_state, &draw));
        static_cast<void>(
            melee_host_gx_captured_tev_state_at(run.lead.tev_state, &tev));
        static_cast<void>(melee_host_gx_captured_texture_set_at(
            run.lead.texture_set, set.data()));
        std::cout << "    run " << index << ": " << run.triangles
                  << " triangles, view " << run.lead.view_state << ", blend "
                  << draw.blend_mode << ", z "
                  << (draw.z_compare_enable ? "on" : "off") << ", alpha "
                  << draw.alpha_compare_0 << '/'
                  << static_cast<int>(draw.alpha_ref_0) << ' '
                  << draw.alpha_op << ' ' << draw.alpha_compare_1 << '/'
                  << static_cast<int>(draw.alpha_ref_1) << ", tev "
                  << run.lead.tev_state << " ("
                  << static_cast<int>(tev.stage_count)
                  << " stages)";
        if (draw.fog_type != 0) {
            std::cout << ", fog " << draw.fog_type << ' ' << draw.fog_start_z
                      << ".." << draw.fog_end_z;
        }
        std::cout << ", textures";
        for (const mh_u32 id : set) {
            if (id != MELEE_HOST_GX_NO_TEXTURE) {
                std::cout << ' ' << id;
            }
        }
        std::cout << ", screen " << run.left << ',' << run.top << ".."
                  << run.right << ',' << run.bottom << '\n';
    }
}

/* The GX frame sink: shows the frame the title just drew, hands the input to
 * the host for the next pad sample, and holds the loop to 60 frames a second
 * of wall clock.  The OS clock stays frozen, so the game still advances one
 * alarm per frame however long presenting takes. */
void present_title_frame(void* user_data)
{
    auto* const view = static_cast<TitleView*>(user_data);
    view->frames += 1;
    view->presenter.present(view->textures.images_for_frame());
    if (view->screenshot_path != nullptr) {
        if (view->frames == view->screenshot_frame) {
            report_title_capture();
            view->screenshot_written =
                view->presenter.save_bmp(view->screenshot_path, &view->error);
            view->screenshot_failed = !view->screenshot_written;
            melee_host_title_scene_request_exit();
        }
        return;
    }
    MeleeHostPadState pad{};
    if (!view->presenter.poll(&pad)) {
        melee_host_title_scene_request_exit();
        return;
    }
    /* lb_800195D0 steps the host before the next alarm samples the pad, which
     * latches this state for PADRead.  present() has already held this frame
     * to its 60 Hz simulation deadline, so pacing again here would halve the
     * title's real-time cadence. */
    static_cast<void>(melee_host_submit_pad_state(view->context, 0, &pad));
}

/* The window presenter is optional; scripted/headless commands below are
 * available in every build. */
#endif

/* Scripted runs freeze the OS clock at one instant, 3 December 2001 at
 * midnight, so they repeat whenever they run: the title screen draws one
 * HSD_Rand for each second of the current minute, and that seed picks the
 * title demo and the results screen's victory pose. */
void freeze_scripted_clock()
{
    OSCalendarTime instant{};
    instant.year = 2001;
    instant.mon = 11;
    instant.mday = 3;
    melee_host_os_time_freeze_at(OSCalendarTimeToTicks(&instant));
}

/* The title screen through its own frame loop, presented in a window, or into
 * a BMP file at one frame. */
#if defined(MELEE_HOST_SDL_RENDERER)
int view_title_scene(const std::string& root, const char* screenshot_path,
                     mh_u32 screenshot_frame)
{
    MeleeHostContext* context = nullptr;
    const MeleeHostConfig config{ .resource_root = root.c_str(),
                                  .headless = false };
    melee_host_os_time_freeze();
    if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
        melee_host_activate_dvd_backend(context) != MELEE_HOST_OK ||
        melee_host_activate_pad_backend(context) != MELEE_HOST_OK ||
        melee_host_boot_memory_init(0) != MELEE_HOST_OK ||
        melee_host_boot_load_dol_data((root + "/sys/main.dol").c_str()) !=
            MELEE_HOST_OK)
    {
        std::cerr << "boot failed\n";
        melee_host_destroy(context);
        return 1;
    }
    int result = 0;
    {
        TitleView view;
        view.context = context;
        view.screenshot_path = screenshot_path;
        view.screenshot_frame = screenshot_frame;
        std::string error;
        if (!view.presenter.open(screenshot_path != nullptr, &error)) {
            std::cerr << "could not open the presenter: " << error << '\n';
            result = 1;
        } else {
            melee_host_title_scene_enter();
            MeleeHostTitleRunReport run{};
            if (melee_host_title_scene_run(present_title_frame, &view, &run) !=
                MELEE_HOST_OK)
            {
                std::cerr << "the title scene did not run\n";
                result = 1;
            } else {
                std::cout << "presented " << view.frames
                          << " frames of the title screen\n"
                          << "  scene frames: " << run.scene_frames << '\n'
                          << "  exit buttons: 0x" << std::hex
                          << run.exit_buttons << std::dec << '\n';
                if (view.screenshot_failed) {
                    std::cerr << "screenshot failed: " << view.error << '\n';
                    result = 1;
                } else if (screenshot_path != nullptr &&
                           !view.screenshot_written)
                {
                    std::cerr << "the scene left before frame "
                              << screenshot_frame << '\n';
                    result = 1;
                } else if (screenshot_path != nullptr) {
                    std::cout << "  frame " << screenshot_frame
                              << " written to " << screenshot_path << '\n';
                }
            }
        }
    }
    melee_host_destroy(context);
    return result;
}
#endif

/* Plays every voice of a .ssm sound bank once through the host's AX mixer
 * and prints each one's sample count and an FNV-1a hash of its samples, which
 * tools/ssm_to_wav.py --compare-host checks against its own decoder. */
int decode_sound_bank(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        std::cerr << "cannot read " << path << '\n';
        return 1;
    }
    std::vector<mh_u8> bank(bytes.size());
    std::memcpy(bank.data(), bytes.data(), bytes.size());
    const auto size = static_cast<mh_u32>(bank.size());
    const mh_u32 voices = melee_host_sound_bank_voice_count(bank.data(), size);
    std::vector<mh_s16> samples(1U << 22);
    for (mh_u32 index = 0; index < voices; ++index) {
        MeleeHostSoundBankVoice info{};
        const mh_s32 count = melee_host_sound_bank_decode_voice(
            bank.data(), size, index, &info, samples.data(),
            static_cast<mh_u32>(samples.size()));
        if (count < 0) {
            std::cerr << "voice " << index << " did not decode\n";
            return 1;
        }
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        for (mh_s32 i = 0; i < count; ++i) {
            const auto value =
                static_cast<std::uint16_t>(samples[static_cast<std::size_t>(i)]);
            for (const std::uint8_t byte :
                 { static_cast<std::uint8_t>(value & 0xFFU),
                   static_cast<std::uint8_t>(value >> 8U) })
            {
                hash ^= byte;
                hash *= 0x100000001b3ULL;
            }
        }
        std::cout << "voice " << index << ": id 0x" << std::hex
                  << info.sound_id << std::dec << " rate " << info.sample_rate
                  << " samples " << count << " fnv 0x" << std::hex << hash
                  << std::dec << '\n';
    }
    std::cout << voices << " voices\n";
    return 0;
}

/* The remaining scene viewers use the SDL/OpenGL presenter. */
#if defined(MELEE_HOST_SDL_RENDERER)
int view_scene(const char* path, const char* symbol, bool scene_model,
               mh_u32 model_index)
{
    MeleeHostSceneModel model = 0;
    const MeleeHostStatus status =
        scene_model
            ? melee_host_scene_graphics_load_model(path, symbol, model_index,
                                                   &model)
            : melee_host_scene_graphics_load_joint(path, symbol, &model);
    if (status != MELEE_HOST_OK) {
        std::cerr << "scene load failed: "
                  << melee_host_status_string(status) << ": "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }

    MeleeHostSceneRenderStats drawn{};
    if (melee_host_scene_graphics_render(model, MELEE_HOST_SCENE_VIEW_WORLD,
                                         &drawn) != MELEE_HOST_OK) {
        std::cerr << "scene render failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        melee_host_scene_graphics_release(model);
        return 1;
    }

    std::vector<melee::render::TextureImage> images;
    std::size_t decoded_count = 0;
    decode_captured_textures(drawn.textures, &images, &decoded_count);

    std::cout << "showing " << drawn.triangles << " triangles from "
              << symbol << " through the original display path\n"
              << "  textures decoded: " << decoded_count << " of "
              << drawn.textures << '\n';
    if (drawn.display_list_errors != 0 || drawn.rejected_indices != 0) {
        std::cerr << "capture incomplete: " << drawn.display_list_errors
                  << " display list errors, " << drawn.rejected_indices
                  << " rejected indices\n";
        melee_host_scene_graphics_release(model);
        return 1;
    }

    melee::render::set_texture_images(std::move(images));
    MeleeHostContext* context = nullptr;
    const MeleeHostConfig config{ .resource_root = nullptr,
                                  .headless = false };
    if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
        melee_host_activate_pad_backend(context) != MELEE_HOST_OK)
    {
        melee_host_destroy(context);
        melee_host_scene_graphics_release(model);
        std::cerr << "could not initialize SDL input context\n";
        return 1;
    }
    std::string error;
    const bool shown = melee::render::show_captured_geometry(context, &error);
    melee_host_destroy(context);
    /* The captured textures point into the archive payload the handle owns, so
     * the model outlives the window. */
    melee_host_scene_graphics_release(model);
    if (!shown) {
        std::cerr << "preview failed: " << error << '\n';
        return 1;
    }
    return 0;
}

/* Draws a model through the original display path, then holds the generated
 * TEV shaders to the CPU reference for every program and pixel state the
 * capture used.  A mismatch means the GLSL and melee::gx::evaluate_tev
 * disagree about what the asset's material computes. */
int tev_conformance(const char* path, const char* symbol, bool scene_model,
                    mh_u32 model_index)
{
    MeleeHostSceneModel model = 0;
    const MeleeHostStatus status =
        scene_model
            ? melee_host_scene_graphics_load_model(path, symbol, model_index,
                                                   &model)
            : melee_host_scene_graphics_load_joint(path, symbol, &model);
    if (status != MELEE_HOST_OK) {
        std::cerr << "scene load failed: "
                  << melee_host_status_string(status) << ": "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }
    MeleeHostSceneRenderStats drawn{};
    const bool rendered =
        melee_host_scene_graphics_render(model, MELEE_HOST_SCENE_VIEW_WORLD,
                                         &drawn) == MELEE_HOST_OK;
    melee_host_scene_graphics_release(model);
    if (!rendered) {
        std::cerr << "scene render failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }

    melee::render::TevConformanceReport report;
    std::string error;
    if (!melee::render::run_tev_conformance(64, &report, &error)) {
        std::cerr << "TEV conformance did not run: " << error << '\n';
        return 1;
    }
    std::cout << "TEV conformance for " << symbol << '\n'
              << "  programs: " << report.programs << " (of "
              << drawn.tev_states << " captured)\n"
              << "  cases: " << report.cases << '\n'
              << "  mismatches: " << report.mismatches << '\n';
    if (report.mismatches != 0) {
        std::cout << "  first: " << report.first_mismatch << '\n';
        return 1;
    }
    return 0;
}
#endif


/* Loads a model, attaches an animation to it and advances it, reporting where
 * a joint ends up.  A joint that moved is the proof the keyframe streams were
 * translated and the original interpreter read them. */
int animate_scene(const char* model_path, const char* model_symbol,
                  bool scene_model, const char* anim_path,
                  const char* anim_symbol, const char* mat_anim_symbol,
                  unsigned frames, float rate)
{
    MeleeHostSceneModel model = 0;
    const MeleeHostStatus status =
        scene_model ? melee_host_scene_graphics_load_model(
                          model_path, model_symbol, 0, &model)
                    : melee_host_scene_graphics_load_joint(
                          model_path, model_symbol, &model);
    if (status != MELEE_HOST_OK) {
        std::cerr << "scene load failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }

    MeleeHostSceneModelStats tree{};
    melee_host_scene_graphics_stats(model, &tree);
    std::vector<std::array<float, 3>> before(tree.jobjs);
    for (mh_u32 index = 0; index < tree.jobjs; ++index) {
        melee_host_scene_graphics_joint_world_position(model, index,
                                                       before[index].data());
    }

    if (melee_host_scene_graphics_attach_animation(
            model, anim_path, anim_symbol, mat_anim_symbol, nullptr) !=
        MELEE_HOST_OK)
    {
        std::cerr << "attach failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        melee_host_scene_graphics_release(model);
        return 1;
    }

    MeleeHostSceneAnimationStats anim{};
    melee_host_scene_graphics_animation_stats(model, &anim);
    std::cout << "animation from " << anim_path << '\n'
              << "  AnimJoint: " << anim.anim_joints
              << "  MatAnimJoint: " << anim.mat_anim_joints
              << "  ShapeAnimJoint: " << anim.shape_anim_joints << '\n'
              << "  AObjDesc: " << anim.aobj_descs
              << "  FObjDesc: " << anim.fobj_descs
              << "  keyframe bytes: " << anim.anim_data_bytes << '\n'
              << "  live AObj: " << anim.aobjs_live
              << "  live FObj: " << anim.fobjs_live
              << "  AObj na arvore: " << anim.aobjs_in_tree << '\n';

    if (melee_host_scene_graphics_run_animation(model, 0.0F, rate, frames) !=
        MELEE_HOST_OK) {
        std::cerr << "run failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        melee_host_scene_graphics_release(model);
        return 1;
    }

    std::size_t moved = 0;
    float largest = 0.0F;
    mh_u32 largest_joint = 0;
    std::array<float, 3> sample_before{};
    std::array<float, 3> sample_after{};
    for (mh_u32 index = 0; index < tree.jobjs; ++index) {
        std::array<float, 3> after{};
        if (melee_host_scene_graphics_joint_world_position(
                model, index, after.data()) != MELEE_HOST_OK) {
            continue;
        }
        float distance = 0.0F;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const float delta = after[axis] - before[index][axis];
            distance += delta * delta;
        }
        distance = std::sqrt(distance);
        if (distance > 1.0e-4F) {
            ++moved;
        }
        if (distance > largest) {
            largest = distance;
            largest_joint = index;
            sample_before = before[index];
            sample_after = after;
        }
    }
    melee_host_scene_graphics_animation_stats(model, &anim);
    std::cout << "  ran " << frames << " frames to " << anim.current_frame
              << '\n'
              << "  joints moved: " << moved << " of " << tree.jobjs << '\n'
              << "  AObj frame: " << anim.aobj_frame << " of "
              << anim.aobj_end_frame << '\n';
    if (moved != 0) {
        std::cout << "  largest move, joint " << largest_joint << ": "
                  << sample_before[0] << ' ' << sample_before[1] << ' '
                  << sample_before[2] << "  ->  " << sample_after[0] << ' '
                  << sample_after[1] << ' ' << sample_after[2] << "  ("
                  << largest << ")\n";
    }

    melee_host_scene_graphics_release(model);
    return 0;
}

#if defined(MELEE_HOST_SDL_RENDERER)
namespace {
struct AnimationPlayback {
    MeleeHostSceneModel model;
    float end_frame;
    mh_u32 frame;
};

/* Runs once per displayed frame: advance the animation, then draw the tree
 * again so the window reads this frame's geometry.  The animation is requested
 * again when it runs out, which is what makes the action loop. */
void advance_animation_frame(void* user_data)
{
    auto* const playback = static_cast<AnimationPlayback*>(user_data);
    if (playback->end_frame > 0.0F &&
        static_cast<float>(playback->frame) >= playback->end_frame)
    {
        melee_host_scene_graphics_run_animation(playback->model, 0.0F, 1.0F,
                                                0);
        playback->frame = 0;
    }
    melee_host_scene_graphics_step_animation(playback->model, 1);
    playback->frame += 1;
    MeleeHostSceneRenderStats drawn{};
    melee_host_scene_graphics_render(playback->model,
                                     MELEE_HOST_SCENE_VIEW_WORLD, &drawn);
}
} // namespace

/* Shows a character model playing one of its actions. */
int view_animation(const char* model_path, const char* model_symbol,
                   const char* anim_path, const char* anim_symbol)
{
    MeleeHostSceneModel model = 0;
    if (melee_host_scene_graphics_load_joint(model_path, model_symbol,
                                             &model) != MELEE_HOST_OK) {
        std::cerr << "scene load failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }
    if (melee_host_scene_graphics_attach_named_animation(
            model, anim_path, anim_symbol) != MELEE_HOST_OK) {
        std::cerr << "attach failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        melee_host_scene_graphics_release(model);
        return 1;
    }
    if (melee_host_scene_graphics_run_animation(model, 0.0F, 1.0F, 0) !=
        MELEE_HOST_OK) {
        std::cerr << "run failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        melee_host_scene_graphics_release(model);
        return 1;
    }

    /* One capture up front, so the window has geometry and textures before the
     * first callback runs. */
    MeleeHostSceneRenderStats drawn{};
    if (melee_host_scene_graphics_render(model, MELEE_HOST_SCENE_VIEW_WORLD,
                                         &drawn) != MELEE_HOST_OK) {
        std::cerr << "render failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        melee_host_scene_graphics_release(model);
        return 1;
    }

    std::vector<melee::render::TextureImage> images;
    std::size_t decoded_count = 0;
    decode_captured_textures(drawn.textures, &images, &decoded_count);

    MeleeHostSceneAnimationStats anim{};
    melee_host_scene_graphics_animation_stats(model, &anim);
    std::cout << anim_symbol << " on " << model_symbol << '\n'
              << "  " << drawn.triangles << " triangles, "
              << decoded_count << " of " << drawn.textures
              << " textures decoded\n"
              << "  " << anim.aobjs_in_tree << " animated joints over "
              << anim.aobj_end_frame << " frames\n";

    melee::render::set_texture_images(std::move(images));
    MeleeHostContext* context = nullptr;
    const MeleeHostConfig config{ .resource_root = nullptr,
                                  .headless = false };
    if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
        melee_host_activate_pad_backend(context) != MELEE_HOST_OK)
    {
        melee_host_destroy(context);
        melee_host_scene_graphics_release(model);
        std::cerr << "could not initialize SDL input context\n";
        return 1;
    }
    AnimationPlayback playback{ model, anim.aobj_end_frame, 0 };
    std::string error;
    const bool shown = melee::render::show_captured_geometry(
        context, &error, advance_animation_frame, &playback);
    melee_host_destroy(context);
    melee_host_scene_graphics_release(model);
    if (!shown) {
        std::cerr << "preview failed: " << error << '\n';
        return 1;
    }
    return 0;
}
#endif

int list_animations(const char* path)
{
    mh_u32 count = 0;
    if (melee_host_scene_graphics_list_animations(path, 0, nullptr, 0,
                                                  &count) != MELEE_HOST_OK) {
        std::cerr << "list failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }
    std::cout << path << ": " << count << " animations\n";
    for (mh_u32 index = 0; index < count; ++index) {
        std::array<char, 128> symbol{};
        if (melee_host_scene_graphics_list_animations(
                path, index, symbol.data(), symbol.size(), nullptr) ==
            MELEE_HOST_OK)
        {
            std::cout << "  " << index << ": " << symbol.data() << '\n';
        }
    }
    return 0;
}

/* Loads a model and drives it with one named animation out of a file that
 * holds several, which is how a character keeps one archive per action. */
int animate_named(const char* model_path, const char* model_symbol,
                  const char* anim_path, const char* anim_symbol,
                  unsigned frames)
{
    MeleeHostSceneModel model = 0;
    if (melee_host_scene_graphics_load_joint(model_path, model_symbol,
                                             &model) != MELEE_HOST_OK) {
        std::cerr << "scene load failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }

    MeleeHostSceneModelStats tree{};
    melee_host_scene_graphics_stats(model, &tree);
    std::vector<std::array<float, 3>> before(tree.jobjs);
    for (mh_u32 index = 0; index < tree.jobjs; ++index) {
        melee_host_scene_graphics_joint_world_position(model, index,
                                                       before[index].data());
    }

    if (melee_host_scene_graphics_attach_named_animation(
            model, anim_path, anim_symbol) != MELEE_HOST_OK) {
        std::cerr << "attach failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        melee_host_scene_graphics_release(model);
        return 1;
    }

    MeleeHostSceneAnimationStats anim{};
    melee_host_scene_graphics_animation_stats(model, &anim);
    std::cout << anim_symbol << " on " << model_symbol << '\n'
              << "  AnimJoint: " << anim.anim_joints
              << "  AObjDesc: " << anim.aobj_descs
              << "  FObjDesc: " << anim.fobj_descs
              << "  keyframe bytes: " << anim.anim_data_bytes << '\n'
              << "  AObj attached to the tree: " << anim.aobjs_in_tree
              << " of " << tree.jobjs << " joints\n";

    if (melee_host_scene_graphics_run_animation(model, 0.0F, 1.0F, frames) !=
        MELEE_HOST_OK) {
        std::cerr << "run failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        melee_host_scene_graphics_release(model);
        return 1;
    }

    std::size_t moved = 0;
    float largest = 0.0F;
    mh_u32 largest_joint = 0;
    for (mh_u32 index = 0; index < tree.jobjs; ++index) {
        std::array<float, 3> after{};
        if (melee_host_scene_graphics_joint_world_position(
                model, index, after.data()) != MELEE_HOST_OK) {
            continue;
        }
        float distance = 0.0F;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const float delta = after[axis] - before[index][axis];
            distance += delta * delta;
        }
        distance = std::sqrt(distance);
        if (distance > 1.0e-4F) {
            ++moved;
        }
        if (distance > largest) {
            largest = distance;
            largest_joint = index;
        }
    }
    melee_host_scene_graphics_animation_stats(model, &anim);
    std::cout << "  ran " << frames << " frames, AObj frame "
              << anim.aobj_frame << " of " << anim.aobj_end_frame << '\n'
              << "  joints moved: " << moved << " of " << tree.jobjs
              << ", largest " << largest << " at joint " << largest_joint
              << '\n';

    /* Moving joints are not the same claim as moving geometry: the vertices
     * only follow if the draw runs again and picks up the new matrices.  This
     * renders two consecutive frames and compares what the recorder captured,
     * which is exactly what a window would be showing. */
    MeleeHostSceneRenderStats drawn{};
    std::vector<MeleeHostGxPosition3f32> first;
    if (melee_host_scene_graphics_render(model, MELEE_HOST_SCENE_VIEW_WORLD,
                                         &drawn) == MELEE_HOST_OK) {
        first.reserve(drawn.triangles);
        for (mh_u32 index = 0; index < drawn.triangles; ++index) {
            MeleeHostGxCapturedTriangle triangle{};
            if (melee_host_gx_captured_triangle_at(index, &triangle)) {
                first.push_back(triangle.vertices[0].position);
            }
        }
        melee_host_scene_graphics_step_animation(model, 1);
        if (melee_host_scene_graphics_render(
                model, MELEE_HOST_SCENE_VIEW_WORLD, &drawn) == MELEE_HOST_OK)
        {
            std::size_t changed = 0;
            float biggest = 0.0F;
            for (mh_u32 index = 0;
                 index < drawn.triangles && index < first.size(); ++index) {
                MeleeHostGxCapturedTriangle triangle{};
                if (!melee_host_gx_captured_triangle_at(index, &triangle)) {
                    continue;
                }
                const auto& before_vertex = first[index];
                const auto& after_vertex = triangle.vertices[0].position;
                const float dx = after_vertex.x - before_vertex.x;
                const float dy = after_vertex.y - before_vertex.y;
                const float dz = after_vertex.z - before_vertex.z;
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (distance > 1.0e-4F) {
                    ++changed;
                }
                biggest = std::max(biggest, distance);
            }
            std::cout << "  geometry between two frames: " << changed
                      << " of " << first.size()
                      << " triangles moved, largest " << biggest << '\n';
        }
    }

    melee_host_scene_graphics_release(model);
    return moved == 0 ? 1 : 0;
}

struct ArchiveKindTally {
    mh_u32 symbols = 0;
    mh_u32 translated = 0;
    mh_u32 load_attempted = 0;
    mh_u32 loaded = 0;
    mh_u64 objects = 0;
};

struct ArchiveProbeReport {
    std::array<ArchiveKindTally, MELEE_HOST_HSD_SYMBOL_KIND_COUNT> kinds{};
    std::vector<std::string> failures;
    mh_u32 failed = 0;
    std::string file;
    bool verbose = false;
};

void record_archive_symbol(const MeleeHostArchiveProbeSymbol* result,
                           void* user_data)
{
    auto& report = *static_cast<ArchiveProbeReport*>(user_data);
    ArchiveKindTally& tally = report.kinds[result->kind];
    tally.symbols += 1;
    tally.translated += static_cast<mh_u32>(result->translated);
    tally.load_attempted += static_cast<mh_u32>(result->load_attempted);
    tally.loaded += static_cast<mh_u32>(result->loaded);
    tally.objects += result->objects;

    const bool supported = result->kind != MELEE_HOST_HSD_SYMBOL_UNSUPPORTED;
    const bool failed =
        supported && (result->translated == 0 ||
                      (result->load_attempted != 0 && result->loaded == 0));
    std::string error = result->error != nullptr ? result->error : "";
    /* A refusal from the host archive already leads with the symbol. */
    const std::string prefix = std::string(result->symbol) + ": ";
    if (error.starts_with(prefix)) {
        error.erase(0, prefix.size());
    }
    if (report.verbose) {
        std::cout << "  " << result->symbol << " ["
                  << melee_host_hsd_symbol_kind_name(result->kind) << "] ";
        if (!supported) {
            std::cout << "no host translation for this kind";
        } else if (result->translated == 0) {
            std::cout << "refused: " << error;
        } else if (result->load_attempted == 0) {
            std::cout << "translated";
        } else if (result->loaded != 0) {
            std::cout << "loaded, " << result->objects << " objects";
        } else {
            std::cout << "loader failed: " << error;
        }
        std::cout << '\n';
    }
    if (failed) {
        report.failed += 1;
        if (report.failures.size() < 20) {
            report.failures.push_back(report.file + ": " + result->symbol +
                                      ": " + error);
        }
    }
}

void print_archive_report(const ArchiveProbeReport& report)
{
    std::cout << "  kind              symbols translated  loaded objects\n";
    for (std::size_t kind = 0; kind < report.kinds.size(); ++kind) {
        const ArchiveKindTally& tally = report.kinds[kind];
        if (tally.symbols == 0) {
            continue;
        }
        std::cout << "  " << std::left << std::setw(17)
                  << melee_host_hsd_symbol_kind_name(
                         static_cast<MeleeHostHsdSymbolKind>(kind))
                  << std::right << std::setw(8) << tally.symbols
                  << std::setw(11) << tally.translated;
        if (tally.load_attempted != 0) {
            std::cout << std::setw(8) << tally.loaded << std::setw(8)
                      << tally.objects;
        }
        std::cout << '\n';
    }
    std::cout << "  failed: " << report.failed << '\n';
    for (const std::string& failure : report.failures) {
        std::cout << "    " << failure << '\n';
    }
}

/* One file, the symbols a lbArchive_LoadSymbols call would name, or all of
 * them. */
int load_archive(const char* path, const std::vector<const char*>& symbols)
{
    /* The game's own data translators, which the boot registers, so that a
     * symbol like lbRefData reads as the route reads it. */
    melee_host_game_register_data_translators();
    ArchiveProbeReport report;
    report.file = std::filesystem::path(path).filename().string();
    report.verbose = true;
    std::cout << "parsed " << path << " through HSD_ArchiveParse\n";
    const MeleeHostStatus status = melee_host_archive_probe_file(
        path, symbols.data(), static_cast<mh_u32>(symbols.size()),
        record_archive_symbol, &report);
    if (status != MELEE_HOST_OK) {
        std::cerr << "archive load failed: "
                  << melee_host_archive_probe_last_error() << '\n';
        return 1;
    }
    print_archive_report(report);
    return report.failed == 0 ? 0 : 1;
}

/* Every file on the disc that is one archive, every public symbol in it. */
int sweep_archives(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root)) {
        const std::string extension = entry.path().extension().string();
        if (entry.is_regular_file() &&
            (extension == ".dat" || extension == ".usd")) {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    melee_host_game_register_data_translators();
    ArchiveProbeReport report;
    mh_u32 archives = 0;
    mh_u32 not_single = 0;
    mh_u32 unreadable = 0;
    for (const auto& path : paths) {
        /* A file of several archives laid end to end is not loaded whole by
         * lbArchive; its members reach the parse through other paths. */
        std::ifstream stream(path, std::ios::binary);
        std::array<unsigned char, 4> head{};
        stream.read(reinterpret_cast<char*>(head.data()),
                    static_cast<std::streamsize>(head.size()));
        const std::uintmax_t declared =
            (static_cast<std::uintmax_t>(head[0]) << 24U) |
            (static_cast<std::uintmax_t>(head[1]) << 16U) |
            (static_cast<std::uintmax_t>(head[2]) << 8U) | head[3];
        if (!stream || declared != std::filesystem::file_size(path)) {
            not_single += 1;
            continue;
        }
        report.file = path.filename().string();
        if (melee_host_archive_probe_file(path.string().c_str(), nullptr, 0,
                                          record_archive_symbol, &report) !=
            MELEE_HOST_OK)
        {
            unreadable += 1;
            if (report.failures.size() < 20) {
                report.failures.push_back(
                    report.file + ": " + melee_host_archive_probe_last_error());
            }
            continue;
        }
        archives += 1;
    }

    std::cout << "swept " << root.string() << ": " << archives
              << " archives, " << not_single
              << " files that are not a single archive, " << unreadable
              << " unreadable\n";
    print_archive_report(report);
    return report.failed == 0 && unreadable == 0 ? 0 : 1;
}

/* The title screen's archive through the game's own loader, after the memory
 * sequence gmMain runs. */
int boot_title_archive(const std::filesystem::path& root)
{
    MeleeHostContext* context = nullptr;
    const std::string root_string = root.string();
    const MeleeHostConfig config{ .resource_root = root_string.c_str(),
                                  .headless = true };
    if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
        melee_host_activate_dvd_backend(context) != MELEE_HOST_OK) {
        std::cerr << "could not initialize the virtual DVD\n";
        melee_host_destroy(context);
        return 1;
    }
    if (melee_host_boot_memory_init(0) != MELEE_HOST_OK) {
        std::cerr << "the boot memory sequence failed\n";
        melee_host_destroy(context);
        return 1;
    }
    MeleeHostBootMemoryStats memory{};
    static_cast<void>(melee_host_boot_memory_stats(&memory));
    std::cout << "booted memory through HSD_InitComponent, lbMemory and lbHeap\n"
              << "  arena bytes: " << memory.arena_bytes << '\n'
              << "  lb heaps created: " << memory.lb_heaps_created
              << " of 6\n";

    MeleeHostTitleArchiveReport report{};
    const MeleeHostStatus status = melee_host_boot_load_title_archive(&report);
    melee_host_destroy(context);
    if (status != MELEE_HOST_OK) {
        std::cerr << "title archive load failed: "
                  << melee_host_status_string(status) << '\n';
        return 1;
    }
    std::cout << "loaded GmTtAll.usd through lbArchive_LoadSymbols\n"
              << "  file bytes: " << report.file_bytes << '\n'
              << "  symbols resolved: " << report.symbols_resolved
              << " of 12\n"
              << "  title JObjs: " << report.title_jobjs << " (animated "
              << report.title_animated_jobjs << ")\n"
              << "  background JObjs: " << report.background_jobjs
              << " (animated " << report.background_animated_jobjs << ")\n"
              << "  lights: " << report.lights << '\n'
              << "  camera loaded: " << static_cast<int>(report.camera_loaded)
              << '\n'
              << "  fog loaded: " << static_cast<int>(report.fog_loaded)
              << '\n'
              << "  title mark image: "
              << static_cast<int>(report.mark_has_image) << '\n';
    const bool complete = report.symbols_resolved == 12 &&
                          report.title_jobjs != 0 &&
                          report.background_jobjs != 0 &&
                          report.lights != 0 && report.camera_loaded != 0 &&
                          report.fog_loaded != 0 && report.mark_has_image != 0;
    return complete ? 0 : 1;
}

int load_scene(const char* path, const char* symbol, bool scene_model,
               mh_u32 model_index, bool render = false,
               MeleeHostSceneView view = MELEE_HOST_SCENE_VIEW_SCENE_CAMERA)
{
    MeleeHostSceneModel model = 0;
    const MeleeHostStatus status =
        scene_model
            ? melee_host_scene_graphics_load_model(path, symbol, model_index,
                                                   &model)
            : melee_host_scene_graphics_load_joint(path, symbol, &model);
    if (status != MELEE_HOST_OK) {
        std::cerr << "scene load failed: "
                  << melee_host_status_string(status) << ": "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }

    MeleeHostSceneModelStats stats{};
    if (melee_host_scene_graphics_stats(model, &stats) != MELEE_HOST_OK) {
        std::cerr << "scene stats failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }

    std::cout << "loaded " << symbol << " from " << path;
    if (scene_model) {
        std::cout << " model " << model_index;
    }
    std::cout << " through the original HSD object layer\n"
              << "  JObj: " << stats.jobjs << " (depth " << stats.tree_depth
              << ")\n"
              << "  DObj: " << stats.dobjs << '\n'
              << "  MObj: " << stats.mobjs << " (TObj " << stats.tobjs
              << ")\n"
              << "  PObj: " << stats.pobjs << " (textured "
              << stats.textured_pobjs << ")\n"
              << "  display list blocks: " << stats.display_blocks << '\n'
              << "  not drawn: " << stats.undrawn_pobjs << " PObj (hidden "
              << stats.hidden_jobjs << " JObj / " << stats.hidden_dobjs
              << " DObj, both-face cull " << stats.culled_pobjs << ")\n"
              << "  host descriptor bytes: " << stats.descriptor_bytes << '\n'
              << "  archive payload bytes: " << stats.payload_bytes << '\n';

    float position[3] = { 0.0F, 0.0F, 0.0F };
    if (melee_host_scene_graphics_joint_world_position(model, 0, position) ==
        MELEE_HOST_OK)
    {
        std::cout << "  root world position: " << position[0] << ' '
                  << position[1] << ' ' << position[2] << '\n';
    }

    if (render) {
        MeleeHostSceneRenderStats drawn{};
        if (melee_host_scene_graphics_render(model, view, &drawn) !=
            MELEE_HOST_OK) {
            std::cerr << "scene render failed: "
                      << melee_host_scene_graphics_last_error() << '\n';
            return 1;
        }
        std::cout << "rendered through HSD_JObjDispAll"
                  << (drawn.used_scene_camera ? " with the scene camera"
                                              : " with the host camera")
                  << (view == MELEE_HOST_SCENE_VIEW_WORLD
                          ? ", world space"
                          : ", view space")
                  << '\n'
                  << "  triangles: " << drawn.triangles << " (textured "
                  << drawn.textured_triangles << ")\n"
                  << "  textures bound: " << drawn.textures << '\n'
                  << "  distinct draw states: " << drawn.draw_states << "\n"
                  << "  vertices: " << drawn.vertices << '\n'
                  << "  per pass opa/texedge/xlu: " << drawn.pass_triangles[0]
                  << '/' << drawn.pass_triangles[1] << '/'
                  << drawn.pass_triangles[2] << '\n'
                  << "  display list errors: " << drawn.display_list_errors
                  << '\n'
                  << "  rejected vertex indices: " << drawn.rejected_indices
                  << '\n';
        std::cout << "  distinct TEV states: " << drawn.tev_states << '\n'
                  << "  texture sets: " << drawn.texture_sets << '\n'
                  << "  TEV evaluated per fragment: "
                  << drawn.tev_evaluated_triangles << " triangles, "
                  << drawn.tev_unmodelled_triangles
                  << " with unmodelled features\n";
        for (std::size_t id = 0;
             id < melee_host_gx_captured_tev_state_count() ; ++id) {
            MeleeHostGxTevState tev{};
            if (!melee_host_gx_captured_tev_state_at(id, &tev)) {
                continue;
            }
            std::cout << "  tev " << id << ": stages="
                      << static_cast<unsigned>(tev.stage_count)
                      << " texgens="
                      << static_cast<unsigned>(tev.texcoord_gen_count)
                      << " channels="
                      << static_cast<unsigned>(tev.channel_count);
            for (std::size_t stage = 0; stage < tev.stage_count && stage < 4;
                 ++stage) {
                std::cout << " [" << stage << " mode="
                          << (tev.stages[stage].mode ==
                                      MELEE_HOST_GX_TEV_MODE_CUSTOM
                                  ? std::string("custom")
                                  : std::to_string(tev.stages[stage].mode))
                          << " map=" << tev.stages[stage].texmap
                          << " chan=" << tev.stages[stage].color_channel
                          << " cin=" << tev.stages[stage].color_input[0] << ','
                          << tev.stages[stage].color_input[1] << ','
                          << tev.stages[stage].color_input[2] << ','
                          << tev.stages[stage].color_input[3]
                          << " cop=" << tev.stages[stage].color_op
                          << " cbias=" << tev.stages[stage].color_bias
                          << " cscale=" << tev.stages[stage].color_scale
                          << " creg=" << tev.stages[stage].color_out_reg
                          << " ain=" << tev.stages[stage].alpha_input[0] << ','
                          << tev.stages[stage].alpha_input[1] << ','
                          << tev.stages[stage].alpha_input[2] << ','
                          << tev.stages[stage].alpha_input[3]
                          << " areg=" << tev.stages[stage].alpha_out_reg
                          << ']';
            }
            const auto unmodelled = melee::gx::tev_unmodelled_features(tev);
            if (unmodelled != melee::gx::kTevUnmodelledNone) {
                std::cout << " unmodelled="
                          << melee::gx::describe_tev_unmodelled(unmodelled);
            }
            std::cout << '\n';
        }

        /* The state each group of triangles ran under, which is what a viewer
         * has to follow instead of assuming fixed passes. */
        for (mh_u32 id = 0; id < drawn.draw_states && id < 8; ++id) {
            MeleeHostGxDrawState state{};
            if (!melee_host_gx_captured_draw_state_at(id, &state)) {
                continue;
            }
            std::cout << "  state " << id << ": cull=" << state.cull_mode
                      << " ztest=" << state.z_compare_enable
                      << " zwrite=" << state.z_update_enable
                      << " zfunc=" << state.z_func
                      << " blend=" << state.blend_mode << '('
                      << state.blend_src_factor << ','
                      << state.blend_dst_factor << ')'
                      << " alpha=" << state.alpha_compare_0 << '@'
                      << static_cast<unsigned>(state.alpha_ref_0) << " op="
                      << state.alpha_op << ' ' << state.alpha_compare_1 << '@'
                      << static_cast<unsigned>(state.alpha_ref_1) << '\n';
        }
        if (drawn.display_list_errors != 0 || drawn.rejected_indices != 0) {
            return 1;
        }
    }

    if (melee_host_scene_graphics_release(model) != MELEE_HOST_OK) {
        std::cerr << "scene release failed: "
                  << melee_host_scene_graphics_last_error() << '\n';
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
#if defined(_WIN32)
        std::vector<std::string> startup_storage;
        std::vector<char*> startup_argv;
        if (argc == 1) {
            const auto project_root = melee::windows::find_project_root();
            if (!melee::windows::has_local_assets(project_root)) {
                if (!melee::windows::show_missing_assets_window(project_root)) {
                    return 0;
                }
            }
#if defined(MELEE_HOST_SDL_RENDERER)
            startup_storage = { argv[0], "--play",
                                (project_root / "assets-local").string() };
            startup_argv.reserve(startup_storage.size());
            for (auto& argument : startup_storage) {
                startup_argv.push_back(argument.data());
            }
            argc = static_cast<int>(startup_argv.size());
            argv = startup_argv.data();
#else
            std::cerr << "this build does not include the SDL renderer\n";
            return 2;
#endif
        }
#endif
        if (argc == 2 && std::string(argv[1]) == "--diagnose") {
            return diagnose();
        }
        if (argc == 2 && std::string(argv[1]) == "--inspect-match-rules") {
            return inspect_match_rules();
        }
        if (argc == 2 && std::string(argv[1]) == "--reset-match-rules") {
            return reset_match_rules();
        }
        if (argc == 2 && std::string(argv[1]) == "--diagnose-native-menu") {
            return diagnose_native_menu();
        }
        if (argc == 2 && std::string(argv[1]) == "--diagnose-local-match") {
            return diagnose_local_match();
        }
        if (argc >= 2 && std::string(argv[1]) == "--diagnose-scene-runtime") {
            unsigned frames = 60;
            if (argc == 3) {
                frames = static_cast<unsigned>(std::stoul(argv[2]));
            } else if (argc != 2) {
                print_usage(argv[0]);
                return 2;
            }
            return diagnose_scene_runtime(frames);
        }
        if (argc == 3 && std::string(argv[1]) == "--inspect-hsd") {
            return inspect_hsd(argv[2]);
        }
        if (argc >= 3 && std::string(argv[1]) == "--load-archive") {
            return load_archive(
                argv[2], std::vector<const char*>(argv + 3, argv + argc));
        }
        if (argc == 3 && std::string(argv[1]) == "--decode-sound-bank") {
            return decode_sound_bank(argv[2]);
        }
        if (argc == 3 && std::string(argv[1]) == "--sweep-archives") {
            return sweep_archives(argv[2]);
        }
        if (argc == 3 && std::string(argv[1]) == "--boot-title-archive") {
            return boot_title_archive(argv[2]);
        }
        if (argc == 3 && std::string(argv[1]) == "--boot-title-scene") {
            MeleeHostContext* context = nullptr;
            const std::string root = argv[2];
            const MeleeHostConfig config{ .resource_root = root.c_str(),
                                          .headless = true };
            if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
                melee_host_activate_dvd_backend(context) != MELEE_HOST_OK ||
                melee_host_boot_memory_init(0) != MELEE_HOST_OK ||
                melee_host_boot_load_dol_data(
                    (root + "/sys/main.dol").c_str()) != MELEE_HOST_OK) {
                std::cerr << "boot failed\n";
                melee_host_destroy(context);
                return 1;
            }
            melee_host_title_scene_enter();
            MeleeHostTitleSceneReport scene{};
            const MeleeHostStatus reported =
                melee_host_title_scene_report(&scene);
            melee_host_destroy(context);
            if (reported != MELEE_HOST_OK) {
                std::cerr << "the scene built no GObj lists\n";
                return 1;
            }
            std::cout << "entered the title screen through gm_801A4BD4 and "
                         "gm_Scene_Title_OnEnter\n"
                      << "  GObjs: " << scene.gobjs << " ("
                      << scene.rendered << " with a render callback)\n"
                      << "  cameras: " << scene.cameras
                      << ", lights: " << scene.lights
                      << ", fogs: " << scene.fogs
                      << ", models: " << scene.models << '\n'
                      << "  processes: " << scene.procs << '\n'
                      << "  JObjs: " << scene.jobjs
                      << ", LObjs: " << scene.lobjs << '\n';
            /* What gmtitle.c builds: two cameras (clear and draw), a light
             * list, a fog, the title logo and the background.  The scene
             * manager adds the third camera, the one DevText_CreateCObj gives
             * the debug text overlay. */
            const bool complete = scene.cameras == 3 && scene.lights == 1 &&
                                  scene.fogs == 1 && scene.models == 2;
            return complete ? 0 : 1;
        }
        if (argc == 3 && std::string(argv[1]) == "--run-title-scene") {
            MeleeHostContext* context = nullptr;
            const std::string root = argv[2];
            const MeleeHostConfig config{ .resource_root = root.c_str(),
                                          .headless = true };
            /* Frozen before boot, so the disc waits take the same pad samples
             * on every run as well. */
            freeze_scripted_clock();
            if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
                melee_host_activate_dvd_backend(context) != MELEE_HOST_OK ||
                melee_host_boot_memory_init(0) != MELEE_HOST_OK ||
                melee_host_boot_load_dol_data(
                    (root + "/sys/main.dol").c_str()) != MELEE_HOST_OK) {
                std::cerr << "boot failed\n";
                melee_host_destroy(context);
                return 1;
            }
            melee_host_title_scene_enter();
            MeleeHostTitleRunReport run{};
            const MeleeHostStatus ran =
                melee_host_title_scene_run(nullptr, nullptr, &run);
            melee_host_destroy(context);
            if (ran != MELEE_HOST_OK) {
                std::cerr << "the title scene did not run\n";
                return 1;
            }
            std::cout << "ran the title screen through gm_801A4D34\n"
                      << "  scene frames: " << run.scene_frames << '\n'
                      << "  drawn frames: " << run.drawn_frames << " ("
                      << run.last_frame_triangles
                      << " triangles in the last)\n"
                      << "  retraces: " << run.retraces << '\n'
                      << "  exit buttons: 0x" << std::hex << run.exit_buttons
                      << std::dec << '\n'
                      << "  OS time: " << run.elapsed_ticks << " ticks\n";
            /* gm_Scene_Title_OnFrame counts down 20 frames, then leaves with
             * no buttons on the frame its counter passes 600. */
            const bool timed_out = run.scene_frames == 621 &&
                                   run.exit_buttons == 0 &&
                                   run.drawn_frames > 0 &&
                                   run.last_frame_triangles > 0;
            return timed_out ? 0 : 1;
        }
        /* --play ROOT [ENTRY...] is --run-modes from the title on, presented
         * in a window at 60 frames a second with the keyboard and the first
         * gamepad as pad 1, for as long as the window stays open.  The OS
         * clock starts at the host's time, as on the console.  Script entries
         * still apply over the window's pad; MELEE_HOST_PLAY_HIDDEN draws
         * into a hidden presenter instead, which lets BMP= entries save
         * frames. */
        std::vector<std::string> play_storage;
        std::vector<char*> play_argv;
        bool play = false;
        if (argc >= 3 && std::string(argv[1]) == "--play") {
#if !defined(MELEE_HOST_SDL_RENDERER)
            std::cerr << "--play needs the SDL renderer\n";
            return 2;
#endif
            play = true;
            play_storage = { argv[0], "--run-modes", argv[2], "0",
                             "0xFFFFFFFF" };
            for (int i = 3; i < argc; ++i) {
                play_storage.emplace_back(argv[i]);
            }
            for (std::string& argument : play_storage) {
                play_argv.push_back(argument.data());
            }
            argc = static_cast<int>(play_argv.size());
            argv = play_argv.data();
        }
        if (argc >= 5 && std::string(argv[1]) == "--run-modes") {
            /* FRAME[-LAST]:INPUT[+INPUT][@PORT].  Without LAST a press is
             * held for three drawn frames, so a scene sees the buttons go down
             * and come back up; with LAST it is held through that frame.  An
             * input is a button name, SX=N or SY=N for the main stick, or
             * the headless diagnostics SHADOW, EFBCOPY, FIGHTERS, MOVE, ACTION,
             * FALLS, RULES, RESULT, CLOCK[=SECONDS], MATCHES[=TOTAL],
             * TROPHY=ID, STOP and TRACE=PATH. PORT is 1 to 4,
             * 1 when omitted; a port the script names is
             * connected from the start.  Frames count across modes. */
            struct ScriptedPress {
                mh_u32 first;
                mh_u32 last;
                mh_u32 port;
                mh_u16 buttons;
                mh_s8 stick_x;
                mh_s8 stick_y;
            };
            /* FRAME:BMP=PATH writes that drawn frame to a BMP file through a
             * hidden presenter, as --view-title-scene does.  FRAME:SHADOW
             * instead checks its 256x256 I4 shadow texture headlessly. */
            struct ScriptedShot {
                mh_u32 frame;
                std::string path;
                bool written;
            };
            /* FIRST[-LAST]:TRACE=PATH writes one line per drawn frame in
             * the range to PATH: the scene, the random seed and, for each
             * fighter, its motion, animation frame, position, velocity,
             * facing, ground or air, damage and stocks, with floats as the
             * hex of their bits.  tools/compare_match_trace.py reports the
             * first field two traces differ on. */
            struct ScriptedTrace {
                mh_u32 first;
                mh_u32 last;
                std::FILE* file;
            };
            /* FIRST-LAST:WAV=PATH writes what the AX mixer plays while the
             * drawn frames are in the range to PATH, 16-bit stereo at
             * 32 kHz.  MELEE_HOST_AUDIO=0 keeps the game's voices off. */
            struct ScriptedWav {
                mh_u32 first;
                mh_u32 last;
                std::FILE* file;
                mh_u64 pairs;
            };
            struct ModesInput {
                MeleeHostContext* context = nullptr;
                std::vector<ScriptedPress> presses;
                std::vector<ScriptedShot> shots;
                std::vector<ScriptedTrace> traces;
                std::vector<ScriptedWav> wavs;
                std::vector<mh_u32> shadow_checks;
                std::vector<mh_u32> efb_copy_checks;
                std::vector<mh_u32> fighter_traces;
                std::vector<mh_u32> action_traces;
                std::vector<mh_s32> action_samples;
                std::vector<mh_u32> falls_traces;
                std::vector<mh_s32> falls_samples;
                std::vector<mh_u32> rules_traces;
                std::vector<mh_u32> result_traces;
                std::vector<mh_u32> clock_traces;
                std::vector<std::array<mh_u32, 2>> clock_sets;
                std::vector<mh_u32> matches_traces;
                std::vector<std::array<mh_u32, 2>> matches_sets;
                std::vector<mh_u32> stop_frames;
                std::vector<std::array<mh_u32, 2>> trophy_awards;
                /* The running mode's report, to print each scene as it
                 * starts, with the frame it starts on. */
                MeleeHostGameModeReport* report = nullptr;
                mh_u32 reported_scenes = 0;
                std::vector<std::array<mh_f32, 2>> movement_samples;
                bool connected[4] = { true, false, false, false };
                mh_u32 frames = 0;
#if defined(MELEE_HOST_SDL_RENDERER)
                TitleTextureCache* textures = nullptr;
                melee::render::FramePresenter* presenter = nullptr;
                bool presenter_open = false;
                bool audio_open = false;
                /* --play: the presenter shows every frame, and the keyboard
                 * and the gamepad it reads become pad 1. */
                bool play = false;
                MeleeHostPadState window_pad{};
                std::chrono::steady_clock::time_point play_mark{};
#endif
                bool shot_failed = false;
                bool shadow_check_failed = false;
                bool efb_copy_check_failed = false;
                bool movement_check_requested = false;
                std::string shot_error;
            };
            struct NamedButton {
                const char* name;
                mh_u16 bit;
            };
            static constexpr NamedButton kButtons[] = {
                { "A", PAD_BUTTON_A },       { "B", PAD_BUTTON_B },
                { "X", PAD_BUTTON_X },       { "Y", PAD_BUTTON_Y },
                { "Z", PAD_TRIGGER_Z },      { "L", PAD_TRIGGER_L },
                { "R", PAD_TRIGGER_R },      { "START", PAD_BUTTON_START },
                { "UP", PAD_BUTTON_UP },     { "DOWN", PAD_BUTTON_DOWN },
                { "LEFT", PAD_BUTTON_LEFT }, { "RIGHT", PAD_BUTTON_RIGHT },
            };
            const auto hex_byte = [](mh_u32 value) {
                constexpr const char* kDigits = "0123456789abcdef";
                std::string text = "0x";
                text += kDigits[(value >> 4) & 0xF];
                text += kDigits[value & 0xF];
                return text;
            };

            ModesInput input;
#if defined(MELEE_HOST_SDL_RENDERER)
            /* The presenter keeps a GL texture per decoded image, so the cache
             * that owns the images is declared first and outlives it. */
            TitleTextureCache shot_textures;
            melee::render::FramePresenter shot_presenter;
            input.textures = &shot_textures;
            input.presenter = &shot_presenter;
#endif
            const auto first_mode =
                static_cast<mh_u32>(std::stoul(argv[3], nullptr, 0));
            const auto max_modes =
                static_cast<mh_u32>(std::stoul(argv[4], nullptr, 0));
            for (int i = 5; i < argc; ++i) {
                const std::string entry = argv[i];
                const auto colon = entry.find(':');
                if (colon == std::string::npos) {
                    std::cerr << "expected FRAME[-LAST]:INPUT[+INPUT][@PORT], "
                                 "got "
                              << entry << '\n';
                    return 2;
                }
                const std::string frames = entry.substr(0, colon);
                std::string inputs = entry.substr(colon + 1);
                if (inputs == "EFBCOPY") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:EFBCOPY, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.efb_copy_checks.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    continue;
                }
                if (inputs == "SHADOW") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:SHADOW, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.shadow_checks.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    continue;
                }
                if (inputs == "FIGHTERS") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:FIGHTERS, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.fighter_traces.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    continue;
                }
                if (inputs == "MOVE") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:MOVE, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.fighter_traces.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    input.movement_check_requested = true;
                    continue;
                }
                if (inputs == "ACTION") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:ACTION, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.action_traces.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    continue;
                }
                if (inputs == "FALLS") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:FALLS, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.falls_traces.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    continue;
                }
                if (inputs == "RULES") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:RULES, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.rules_traces.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    continue;
                }
                if (inputs.rfind("TROPHY=", 0) == 0) {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:TROPHY=ID, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.trophy_awards.push_back(
                        { static_cast<mh_u32>(std::stoul(frames)),
                          static_cast<mh_u32>(
                              std::stoul(inputs.substr(7), nullptr, 0)) });
                    continue;
                }
                if (inputs == "STOP") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:STOP, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.stop_frames.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    continue;
                }
                if (inputs == "MATCHES" || inputs.rfind("MATCHES=", 0) == 0)
                {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:MATCHES[=TOTAL], got "
                                  << entry << '\n';
                        return 2;
                    }
                    const auto frame =
                        static_cast<mh_u32>(std::stoul(frames));
                    if (inputs.size() > 7) {
                        input.matches_sets.push_back(
                            { frame, static_cast<mh_u32>(std::stoul(
                                         inputs.substr(8))) });
                    } else {
                        input.matches_traces.push_back(frame);
                    }
                    continue;
                }
                if (inputs == "CLOCK" || inputs.rfind("CLOCK=", 0) == 0) {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:CLOCK[=SECONDS], got "
                                  << entry << '\n';
                        return 2;
                    }
                    const auto frame =
                        static_cast<mh_u32>(std::stoul(frames));
                    if (inputs.size() > 5) {
                        input.clock_sets.push_back(
                            { frame, static_cast<mh_u32>(std::stoul(
                                         inputs.substr(6))) });
                    } else {
                        input.clock_traces.push_back(frame);
                    }
                    continue;
                }
                if (inputs == "RESULT") {
                    if (frames.find('-') != std::string::npos) {
                        std::cerr << "expected FRAME:RESULT, got " << entry
                                  << '\n';
                        return 2;
                    }
                    input.result_traces.push_back(
                        static_cast<mh_u32>(std::stoul(frames)));
                    continue;
                }
                if (inputs.rfind("WAV=", 0) == 0) {
                    ScriptedWav wav{};
                    const auto dash = frames.find('-');
                    wav.first = static_cast<mh_u32>(
                        std::stoul(frames.substr(0, dash)));
                    wav.last = dash == std::string::npos
                                   ? wav.first
                                   : static_cast<mh_u32>(std::stoul(
                                         frames.substr(dash + 1)));
                    if (inputs.size() == 4 || wav.last < wav.first) {
                        std::cerr << "expected FIRST[-LAST]:WAV=PATH, got "
                                  << entry << '\n';
                        return 2;
                    }
                    wav.file = std::fopen(inputs.c_str() + 4, "wb");
                    if (wav.file == nullptr) {
                        std::cerr << "cannot write " << inputs.substr(4)
                                  << '\n';
                        return 2;
                    }
                    /* The header is written again once the size is known. */
                    const std::array<char, 44> header{};
                    std::fwrite(header.data(), 1, header.size(), wav.file);
                    input.wavs.push_back(wav);
                    continue;
                }
                if (inputs.rfind("TRACE=", 0) == 0) {
                    ScriptedTrace trace{};
                    const auto dash = frames.find('-');
                    trace.first = static_cast<mh_u32>(
                        std::stoul(frames.substr(0, dash)));
                    trace.last = dash == std::string::npos
                                     ? trace.first
                                     : static_cast<mh_u32>(std::stoul(
                                           frames.substr(dash + 1)));
                    if (inputs.size() == 6 || trace.last < trace.first) {
                        std::cerr << "expected FIRST[-LAST]:TRACE=PATH, got "
                                  << entry << '\n';
                        return 2;
                    }
                    trace.file = std::fopen(inputs.c_str() + 6, "w");
                    if (trace.file == nullptr) {
                        std::cerr << "cannot write " << inputs.substr(6)
                                  << '\n';
                        return 2;
                    }
                    input.traces.push_back(trace);
                    continue;
                }
                if (inputs.rfind("BMP=", 0) == 0) {
                    if (frames.find('-') != std::string::npos ||
                        inputs.size() == 4)
                    {
                        std::cerr << "expected FRAME:BMP=PATH, got " << entry
                                  << '\n';
                        return 2;
                    }
#if defined(MELEE_HOST_SDL_RENDERER)
                    input.shots.push_back(
                        { static_cast<mh_u32>(std::stoul(frames)),
                          inputs.substr(4), false });
                    continue;
#else
                    std::cerr << "BMP needs the SDL renderer, got " << entry
                              << '\n';
                    return 2;
#endif
                }
                ScriptedPress press{};
                const auto dash = frames.find('-');
                press.first =
                    static_cast<mh_u32>(std::stoul(frames.substr(0, dash)));
                press.last = dash == std::string::npos
                                 ? press.first + 2
                                 : static_cast<mh_u32>(
                                       std::stoul(frames.substr(dash + 1)));
                const auto at = inputs.find('@');
                if (at != std::string::npos) {
                    press.port = static_cast<mh_u32>(
                                     std::stoul(inputs.substr(at + 1))) -
                                 1;
                    inputs.resize(at);
                }
                if (press.port >= 4 || press.last < press.first) {
                    std::cerr << "bad frames or port in " << entry << '\n';
                    return 2;
                }
                input.connected[press.port] = true;
                std::size_t start = 0;
                while (true) {
                    const auto plus = inputs.find('+', start);
                    const std::string name = inputs.substr(
                        start, plus == std::string::npos ? std::string::npos
                                                         : plus - start);
                    if (name.size() > 3 && name[0] == 'S' &&
                        (name[1] == 'X' || name[1] == 'Y') && name[2] == '=')
                    {
                        const long value = std::stol(name.substr(3));
                        if (value < -128 || value > 127) {
                            std::cerr << "stick value out of range in "
                                      << entry << '\n';
                            return 2;
                        }
                        (name[1] == 'X' ? press.stick_x : press.stick_y) =
                            static_cast<mh_s8>(value);
                    } else {
                        const auto* const found = std::find_if(
                            std::begin(kButtons), std::end(kButtons),
                            [&](const NamedButton& button) {
                                return name == button.name;
                            });
                        if (found == std::end(kButtons)) {
                            std::cerr << "unknown button " << name << '\n';
                            return 2;
                        }
                        press.buttons =
                            static_cast<mh_u16>(press.buttons | found->bit);
                    }
                    if (plus == std::string::npos) {
                        break;
                    }
                    start = plus + 1;
                }
                input.presses.push_back(press);
            }
            MeleeHostContext* context = nullptr;
            const std::string root = argv[2];
            const MeleeHostConfig config{ .resource_root = root.c_str(),
                                          .headless = !play };
            if (play) {
                melee_host_os_time_freeze();
            } else {
                freeze_scripted_clock();
            }
            if (melee_host_create(&config, &context) != MELEE_HOST_OK ||
                melee_host_activate_dvd_backend(context) != MELEE_HOST_OK ||
                melee_host_activate_pad_backend(context) != MELEE_HOST_OK ||
                melee_host_boot_memory_init(0) != MELEE_HOST_OK ||
                melee_host_boot_load_dol_data(
                    (root + "/sys/main.dol").c_str()) != MELEE_HOST_OK) {
                std::cerr << "boot failed\n";
                melee_host_destroy(context);
                return 1;
            }
            input.context = context;
            {
                const char* const audio = std::getenv("MELEE_HOST_AUDIO");
                melee_host_ax_set_voices_enabled(
                    audio == nullptr || std::string(audio) != "0");
                /* MELEE_HOST_AUDIO_AUX=0 leaves the game's reverb and delay
                 * out of the mix, which is how a route's sound is compared
                 * with reference decoders. */
                const char* const aux = std::getenv("MELEE_HOST_AUDIO_AUX");
                melee_host_ax_set_aux_enabled(aux == nullptr ||
                                              std::string(aux) != "0");
                /* MELEE_HOST_FOG=0 captures every draw with GX_FOG_NONE, so
                 * the same frames can be drawn without the fog the game
                 * asked for. */
                const char* const fog = std::getenv("MELEE_HOST_FOG");
                melee_host_gx_set_fog_enabled(fog == nullptr ||
                                              std::string(fog) != "0");
            }
            /* What the AX mixer plays goes to the WAV entries whose range
             * holds the current frame and, in a play window, to the sound
             * device. */
            melee_host_ax_set_output_sink(
                [](const mh_s16* stereo, mh_u32 pairs, void* user_data) {
                    auto* const state = static_cast<ModesInput*>(user_data);
                    for (ScriptedWav& wav : state->wavs) {
                        if (state->frames < wav.first ||
                            state->frames > wav.last)
                        {
                            continue;
                        }
                        /* WAV is little-endian, like the host. */
                        std::fwrite(stereo, sizeof(mh_s16), pairs * 2U,
                                    wav.file);
                        wav.pairs += pairs;
                    }
#if defined(MELEE_HOST_SDL_RENDERER)
                    if (state->audio_open) {
                        state->presenter->queue_audio(stereo, pairs);
                    }
#endif
                },
                &input);
#if defined(MELEE_HOST_SDL_RENDERER)
            if (play) {
                const char* const hidden = std::getenv("MELEE_HOST_PLAY_HIDDEN");
                if (!shot_presenter.open(hidden != nullptr && hidden[0] != '\0',
                                         &input.shot_error))
                {
                    std::cerr << "could not open the presenter: "
                              << input.shot_error << '\n';
                    melee_host_destroy(context);
                    return 1;
                }
                input.presenter_open = true;
                input.play = true;
                input.play_mark = std::chrono::steady_clock::now();
                const char* const audio = std::getenv("MELEE_HOST_AUDIO");
                if ((hidden == nullptr || hidden[0] == '\0') &&
                    (audio == nullptr || std::string(audio) != "0"))
                {
                    std::string audio_error;
                    input.audio_open = shot_presenter.open_audio(&audio_error);
                    if (!input.audio_open) {
                        std::cerr << "no sound device: " << audio_error
                                  << '\n';
                    }
                }
            }
#endif
            /* The connected pads, holding whatever the script presses on this
             * frame.  The state reaches PADRead at the host step before the
             * next pad sample. */
            const MeleeHostGxFrameSink scripted_pad = [](void* user_data) {
                auto* const state = static_cast<ModesInput*>(user_data);
                state->frames += 1;
                if (state->report != nullptr &&
                    state->report->scene_count != state->reported_scenes &&
                    state->report->scene_count <=
                        MELEE_HOST_GAME_MODE_MAX_SCENES)
                {
                    state->reported_scenes = state->report->scene_count;
                    constexpr const char* kDigits = "0123456789abcdef";
                    const mh_u32 scene =
                        state->report->scenes[state->reported_scenes - 1]
                            .scene;
                    std::cout << "scene 0x" << kDigits[(scene >> 4) & 0xF]
                              << kDigits[scene & 0xF] << " from frame "
                              << state->frames << '\n';
                }
#if defined(MELEE_HOST_SDL_RENDERER)
                if (state->play) {
                    state->presenter->present(
                        state->textures->images_for_frame());
                    if (!state->presenter->poll(&state->window_pad)) {
                        /* The game has no way to quit: closing the window
                         * ends the process where it is. */
                        std::cout << "window closed at frame "
                                  << state->frames << '\n'
                                  << std::flush;
                        std::_Exit(0);
                    }
                    if (state->frames % 600 == 0) {
                        const auto now = std::chrono::steady_clock::now();
                        const std::chrono::duration<double> elapsed =
                            now - state->play_mark;
                        std::cout << "frame " << state->frames << ": "
                                  << 600.0 / elapsed.count()
                                  << " frames a second\n";
                        state->play_mark = now;
                    }
                }
#endif
                /* A WAV is complete once its range has passed, even if the
                 * process is stopped later. */
                for (ScriptedWav& wav : state->wavs) {
                    if (state->frames == wav.last + 1U) {
                        static_cast<void>(write_wav_header(wav.file, wav.pairs));
                        std::fseek(wav.file, 0, SEEK_END);
                        std::fflush(wav.file);
                    }
                }
                for (const ScriptedTrace& trace : state->traces) {
                    if (state->frames < trace.first ||
                        state->frames > trace.last)
                    {
                        continue;
                    }
                    const auto bits = [](mh_f32 value) {
                        mh_u32 out = 0;
                        std::memcpy(&out, &value, sizeof out);
                        return out;
                    };
                    const mh_u32 scene_count =
                        state->report == nullptr ? 0
                                                 : state->report->scene_count;
                    const mh_u32 scene =
                        scene_count == 0 ||
                                scene_count > MELEE_HOST_GAME_MODE_MAX_SCENES
                            ? 0xFF
                            : state->report->scenes[scene_count - 1].scene;
                    std::fprintf(trace.file, "%u scene=%02x seed=%08x",
                                 state->frames, scene,
                                 melee_host_match_random_seed());
                    for (mh_u32 slot = 0; slot < 4; ++slot) {
                        MeleeHostMatchFighterSample sample{};
                        if (!melee_host_match_fighter_sample(slot, &sample)) {
                            continue;
                        }
                        std::fprintf(
                            trace.file,
                            " P%u motion=%d anim=%08x x=%08x y=%08x vx=%08x"
                            " vy=%08x facing=%08x air=%d percent=%08x"
                            " stocks=%d",
                            slot + 1, sample.motion, bits(sample.anim_frame),
                            bits(sample.x), bits(sample.y),
                            bits(sample.vel_x), bits(sample.vel_y),
                            bits(sample.facing), sample.airborne,
                            bits(sample.percent), sample.stocks);
                    }
                    std::fputc('\n', trace.file);
                }
                /* FRAME:EFBCOPY decodes the colour textures EFB copies wrote
                 * that the frame drew with, and wants one of them to hold a
                 * picture: at least 16 colours, where an empty copy is the
                 * erase colour alone. */
                for (const mh_u32 frame : state->efb_copy_checks) {
                    if (frame != state->frames) {
                        continue;
                    }
                    std::size_t copies = 0;
                    std::size_t most_colours = 0;
                    for (std::size_t index = 0;
                         index < melee_host_gx_captured_texture_count();
                         ++index)
                    {
                        MeleeHostGxTextureDesc texture{};
                        if (!melee_host_gx_captured_texture_at(index,
                                                               &texture) ||
                            texture.image == nullptr ||
                            texture.color_indexed || texture.format == 0 ||
                            melee_host_gx_texture_copy_generation(
                                texture.image) == 0)
                        {
                            continue;
                        }
                        try {
                            const std::size_t bytes =
                                melee::assets::gx_texture_data_size(
                                    texture.width, texture.height,
                                    texture.format);
                            const melee::assets::DecodedTexture decoded =
                                melee::assets::decode_gx_texture(
                                    { static_cast<const std::byte*>(
                                          texture.image),
                                      bytes },
                                    texture.width, texture.height,
                                    texture.format);
                            std::vector<mh_u32> colours;
                            for (std::size_t p = 0;
                                 p + 3 < decoded.rgba.size(); p += 4)
                            {
                                colours.push_back(
                                    (static_cast<mh_u32>(decoded.rgba[p])
                                     << 24U) |
                                    (static_cast<mh_u32>(decoded.rgba[p + 1])
                                     << 16U) |
                                    (static_cast<mh_u32>(decoded.rgba[p + 2])
                                     << 8U) |
                                    decoded.rgba[p + 3]);
                            }
                            std::sort(colours.begin(), colours.end());
                            const auto distinct = static_cast<std::size_t>(
                                std::unique(colours.begin(), colours.end()) -
                                colours.begin());
                            ++copies;
                            most_colours = std::max(most_colours, distinct);
                        } catch (const std::exception&) {
                            continue;
                        }
                    }
                    const bool pictured = copies > 0 && most_colours >= 16;
                    std::cout << "efb copy frame " << frame << ": " << copies
                              << " copied texture(s), at most "
                              << most_colours << " colour(s), "
                              << (pictured ? "ok" : "empty") << '\n';
                    if (!pictured) {
                        state->efb_copy_check_failed = true;
                    }
                }
                for (const mh_u32 frame : state->shadow_checks) {
                    if (frame != state->frames) {
                        continue;
                    }
                    std::size_t textures = 0;
                    std::size_t non_white_bytes = 0;
                    for (std::size_t index = 0;
                         index < melee_host_gx_captured_texture_count();
                         ++index)
                    {
                        MeleeHostGxTextureDesc texture{};
                        if (!melee_host_gx_captured_texture_at(index,
                                                               &texture) ||
                            texture.image == nullptr || texture.format != 0 ||
                            texture.width != 256 || texture.height != 256)
                        {
                            continue;
                        }
                        ++textures;
                        const auto* const bytes =
                            static_cast<const mh_u8*>(texture.image);
                        constexpr std::size_t kI4Bytes = 256U * 256U / 2U;
                        non_white_bytes += static_cast<std::size_t>(std::count_if(
                            bytes, bytes + kI4Bytes, [](mh_u8 value) {
                                return value != 0xFFU;
                            }));
                    }
                    std::cout << "shadow copy frame " << frame << ": "
                              << textures << " I4 texture(s), "
                              << non_white_bytes << " non-white byte(s)\n";
                    if (textures == 0 || non_white_bytes == 0) {
                        state->shadow_check_failed = true;
                    }
                }
                for (const mh_u32 frame : state->fighter_traces) {
                    if (frame != state->frames) {
                        continue;
                    }
                    std::cout << "fighters frame " << frame << ':';
                    std::array<mh_f32, 2> positions{};
                    std::array<bool, 2> present{};
                    for (mh_u32 slot = 0; slot < 2; ++slot) {
                        mh_f32 x = 0;
                        mh_f32 y = 0;
                        mh_f32 z = 0;
                        if (melee_host_match_fighter_position(slot, &x, &y,
                                                              &z))
                        {
                            positions[slot] = x;
                            present[slot] = true;
                            std::cout << " P" << slot + 1 << '=' << x << ','
                                      << y << ',' << z;
                        } else {
                            std::cout << " P" << slot + 1 << "=none";
                        }
                    }
                    std::cout << '\n';
                    if (state->movement_check_requested && present[0] &&
                        present[1]) {
                        state->movement_samples.push_back(positions);
                    }
                }
                for (const mh_u32 frame : state->action_traces) {
                    if (frame != state->frames) {
                        continue;
                    }
                    mh_s32 motion = -1;
                    if (melee_host_match_fighter_motion(0, &motion)) {
                        std::cout << "action frame " << frame << ": P1="
                                  << motion << '\n';
                        state->action_samples.push_back(motion);
                    } else {
                        std::cout << "action frame " << frame
                                  << ": P1=none\n";
                    }
                }
                for (const mh_u32 frame : state->falls_traces) {
                    if (frame != state->frames) {
                        continue;
                    }
                    std::cout << "falls frame " << frame << ':';
                    for (mh_u32 slot = 0; slot < 2; ++slot) {
                        mh_s32 falls = 0;
                        if (melee_host_match_player_falls(slot, &falls)) {
                            std::cout << " P" << slot + 1 << '=' << falls;
                            if (slot == 0) {
                                state->falls_samples.push_back(falls);
                            }
                        } else {
                            std::cout << " P" << slot + 1 << "=none";
                        }
                    }
                    std::cout << '\n';
                }
                for (const mh_u32 frame : state->rules_traces) {
                    if (frame != state->frames) {
                        continue;
                    }
                    mh_u32 mode = 0;
                    mh_u32 time_limit = 0;
                    mh_u32 stock_count = 0;
                    melee_host_match_rules(&mode, &time_limit, &stock_count);
                    std::cout << "rules frame " << frame << ": mode "
                              << mode << " time " << time_limit << " stock "
                              << stock_count << '\n';
                }
                /* The clock the match counts down, and the entry that
                 * moves it: a time-up is a minute of match away under the
                 * shortest rule the menu offers, and the scene keeps
                 * counting from wherever the clock is left. */
                for (const auto& entry : state->clock_sets) {
                    if (entry[0] != state->frames) {
                        continue;
                    }
                    if (melee_host_match_set_clock(entry[1])) {
                        std::cout << "clock frame " << entry[0] << ": set "
                                  << entry[1] << "s\n";
                    } else {
                        std::cout << "clock frame " << entry[0]
                                  << ": no timer\n";
                    }
                }
                for (const mh_u32 frame : state->clock_traces) {
                    if (frame != state->frames) {
                        continue;
                    }
                    mh_u32 seconds = 0;
                    mh_u32 clock_frames = 0;
                    if (melee_host_match_clock(&seconds, &clock_frames)) {
                        std::cout << "clock frame " << frame << ": "
                                  << seconds << "s+" << clock_frames << '\n';
                    } else {
                        std::cout << "clock frame " << frame
                                  << ": no timer\n";
                    }
                }
                /* The VS matches the save data counts, and the entry that
                 * sets it: the smallest play total that unlocks a fighter is
                 * 50 matches, which no route can play. */
                for (const auto& entry : state->matches_sets) {
                    if (entry[0] != state->frames) {
                        continue;
                    }
                    melee_host_match_set_vs_total(entry[1]);
                    std::cout << "matches frame " << entry[0] << ": set "
                              << entry[1] << '\n';
                }
                for (const mh_u32 frame : state->matches_traces) {
                    if (frame != state->frames) {
                        continue;
                    }
                    std::cout << "matches frame " << frame << ": "
                              << melee_host_match_vs_total() << '\n';
                }
                /* A trophy the game's own award path marks as new, which
                 * is what leaves a prize notice pending. */
                for (const auto& entry : state->trophy_awards) {
                    if (entry[0] != state->frames) {
                        continue;
                    }
                    std::cout << "trophy frame " << entry[0] << ": "
                              << entry[1] << (melee_host_match_award_trophy(
                                                  entry[1])
                                                  ? " new\n"
                                                  : " already had\n");
                }
                /* FRAME:STOP ends the route there, for a scene that waits
                 * for input the route has no reason to give: the mode loop
                 * only returns between modes, and a scene that never ends
                 * would hold the route for ever. */
                for (const mh_u32 frame : state->stop_frames) {
                    if (frame != state->frames) {
                        continue;
                    }
                    std::cout << "stopped at frame " << frame << '\n';
                    std::cout.flush();
                    std::exit(0);
                }
                for (const mh_u32 frame : state->result_traces) {
                    if (frame != state->frames) {
                        continue;
                    }
                    mh_u32 outcome = 0;
                    mh_u32 winners = 0;
                    mh_u32 first_winner = 0;
                    mh_s32 stocks_p1 = 0;
                    mh_s32 stocks_p2 = 0;
                    if (melee_host_match_result(&outcome, &winners,
                                                &first_winner, &stocks_p1,
                                                &stocks_p2))
                    {
                        std::cout << "result frame " << frame << ": outcome "
                                  << outcome << " winners " << winners
                                  << " first " << first_winner
                                  << " stocks P1=" << stocks_p1
                                  << " P2=" << stocks_p2 << '\n';
                        /* The places the results screen shows, which after a
                         * sudden death are the only part of the match end
                         * that the tie-breaking match decides. */
                        std::cout << "places frame " << frame << ':';
                        for (mh_u32 slot = 0; slot < 2; ++slot) {
                            mh_s32 place = 0;
                            std::cout << " P" << slot + 1 << '=';
                            if (melee_host_match_place(slot, &place)) {
                                std::cout << place;
                            } else {
                                std::cout << "none";
                            }
                        }
                        std::cout << '\n';
                    } else {
                        std::cout << "result frame " << frame << ": none\n";
                    }
                }
#if defined(MELEE_HOST_SDL_RENDERER)
                for (ScriptedShot& shot : state->shots) {
                    if (shot.frame != state->frames || state->shot_failed) {
                        continue;
                    }
                    if (!state->presenter_open) {
                        state->presenter_open = state->presenter->open(
                            true, &state->shot_error);
                        if (!state->presenter_open) {
                            state->shot_failed = true;
                            continue;
                        }
                    }
                    if (!state->play) {
                        state->presenter->present(
                            state->textures->images_for_frame());
                    }
                    shot.written = state->presenter->save_bmp(
                        shot.path.c_str(), &state->shot_error);
                    state->shot_failed = !shot.written;
                    if (shot.written) {
                        std::cout << "frame " << shot.frame << " written to "
                                  << shot.path << ": "
                                  << melee_host_gx_triangle_count()
                                  << " triangles, "
                                  << melee_host_gx_captured_view_state_count()
                                  << " views, "
                                  << melee_host_gx_captured_texture_count()
                                  << " textures\n";
                        /* The views and the runs in the game's order, with
                         * the box each covers, to match a region of the
                         * image to what drew it. */
                        report_title_capture();
                    }
                }
#endif
                MeleeHostPadState pads[4]{};
#if defined(MELEE_HOST_SDL_RENDERER)
                if (state->play) {
                    pads[0] = state->window_pad;
                }
#endif
                for (const ScriptedPress& press : state->presses) {
                    if (state->frames < press.first ||
                        state->frames > press.last)
                    {
                        continue;
                    }
                    MeleeHostPadState& pad = pads[press.port];
                    pad.buttons =
                        static_cast<mh_u16>(pad.buttons | press.buttons);
                    if (press.stick_x != 0) {
                        pad.stick_x = press.stick_x;
                    }
                    if (press.stick_y != 0) {
                        pad.stick_y = press.stick_y;
                    }
                }
                for (mh_u32 port = 0; port < 4; ++port) {
                    if (!state->connected[port]) {
                        continue;
                    }
                    pads[port].connected = true;
                    static_cast<void>(melee_host_submit_pad_state(
                        state->context, port, &pads[port]));
                }
#if defined(MELEE_HOST_SDL_RENDERER)
                if (state->play) {
                    /* present() owns the finite-rate presentation schedule
                     * and does not return before the next 60 Hz game tick. */
                }
#endif
            };
            if (melee_host_game_begin(first_mode) != MELEE_HOST_OK) {
                std::cerr << "mode " << hex_byte(first_mode)
                          << " is not in the host's mode table\n";
                melee_host_destroy(context);
                return 1;
            }
            /* `route` lists the modes and `scenes` the scene of every state
             * they ran, each with the frames it drew. */
            std::string route = hex_byte(first_mode);
            std::string scenes;
            bool failed = false;
            for (mh_u32 i = 0; i < max_modes; ++i) {
                MeleeHostGameModeReport report{};
                input.report = &report;
                input.reported_scenes = 0;
                const MeleeHostStatus ran = melee_host_game_run_current_mode(
                    scripted_pad, &input, &report);
                input.report = nullptr;
                if (ran == MELEE_HOST_UNSUPPORTED) {
                    /* Left alone on the title the game moves to the opening
                     * movie; --play starts over at the title instead of
                     * ending there. */
                    if (play && melee_host_game_begin(0) == MELEE_HOST_OK) {
                        std::cout << "mode " << hex_byte(report.mode)
                                  << " is not in the host's mode table; "
                                     "back to the title\n";
                        route += " " + hex_byte(0);
                        continue;
                    }
                    std::cout << "stopped: mode " << hex_byte(report.mode)
                              << " is not in the host's mode table\n";
                    break;
                }
                if (ran != MELEE_HOST_OK) {
                    std::cerr << "mode " << hex_byte(report.mode)
                              << " did not run: "
                              << melee_host_status_string(ran) << '\n';
                    failed = true;
                    break;
                }
                const mh_u32 recorded = std::min<mh_u32>(
                    report.scene_count, MELEE_HOST_GAME_MODE_MAX_SCENES);
                for (mh_u32 s = 0; s < recorded; ++s) {
                    const MeleeHostGameSceneReport& scene = report.scenes[s];
                    std::cout << "  scene " << hex_byte(scene.scene)
                              << " after " << scene.drawn_frames
                              << " frames\n";
                    scenes += (scenes.empty() ? "" : " ") +
                              hex_byte(scene.scene) + " (" +
                              std::to_string(scene.drawn_frames) + ")";
                }
                route += " (" + std::to_string(report.drawn_frames) +
                         " frames) ";
                if (report.stopped_at_missing_scene && play &&
                    melee_host_game_begin(0) == MELEE_HOST_OK)
                {
                    std::cout << "scene " << hex_byte(report.missing_scene)
                              << " is not in the host's scene table; back "
                                 "to the title\n";
                    route += "back to 0x00";
                    continue;
                }
                if (report.stopped_at_missing_scene) {
                    std::cout << "stopped: scene "
                              << hex_byte(report.missing_scene)
                              << " is not in the host's scene table\n";
                    route +=
                        "stopped at scene " + hex_byte(report.missing_scene);
                    break;
                }
                std::cout << "mode " << hex_byte(report.mode) << " -> "
                          << hex_byte(report.next_mode) << " after "
                          << report.drawn_frames << " frames\n";
                route += hex_byte(report.next_mode);
            }
            /* What character and stage select left in the VS mode's data:
             * the stage and each slot that is not Gm_PKind_NA (3), with its
             * character. */
            std::string selection = "vs selection: stage ";
            MeleeHostPreparedMatch vs{};
            if (melee_host_vs_selection_get(&vs) == MELEE_HOST_OK) {
                selection += std::to_string(vs.stage_kind);
                for (mh_u32 slot = 0; slot < MELEE_HOST_LOCAL_MATCH_MAX_PLAYERS;
                     ++slot)
                {
                    if (vs.player_kinds[slot] != 3) {
                        selection +=
                            " " + std::to_string(slot) + "=" +
                            std::to_string(static_cast<int>(vs.characters[slot]));
                    }
                }
            }
            melee_host_ax_set_output_sink(nullptr, nullptr);
            melee_host_destroy(context);
            for (const ScriptedWav& wav : input.wavs) {
                if (!write_wav_header(wav.file, wav.pairs) ||
                    std::fclose(wav.file) != 0)
                {
                    std::cerr << "a WAV file could not be written\n";
                    failed = true;
                }
            }
            for (const ScriptedTrace& trace : input.traces) {
                if (std::fclose(trace.file) != 0) {
                    std::cerr << "a trace could not be written\n";
                    failed = true;
                }
            }
            for (const ScriptedShot& shot : input.shots) {
                if (!shot.written) {
                    std::cerr << "frame " << shot.frame << " was not written"
                              << (input.shot_error.empty() ? "" : ": ")
                              << input.shot_error << '\n';
                    failed = true;
                }
            }
            if (input.efb_copy_check_failed) {
                std::cerr << "a requested EFB copy held no picture\n";
                failed = true;
            }
            if (input.shadow_check_failed) {
                std::cerr << "a requested shadow copy was empty\n";
                failed = true;
            }
            if (input.movement_check_requested) {
                bool moved = false;
                if (input.movement_samples.size() >= 2) {
                    const auto& first = input.movement_samples.front();
                    const auto& last = input.movement_samples.back();
                    moved = std::abs(last[0] - first[0]) > 0.1F ||
                            std::abs(last[1] - first[1]) > 0.1F;
                }
                if (!moved) {
                    std::cerr << "a requested movement trace did not move a "
                                 "fighter\n";
                    failed = true;
                }
            }
            if (!input.action_traces.empty() &&
                (input.action_samples.size() < 2 ||
                 input.action_samples.front() == input.action_samples.back()))
            {
                std::cerr << "a requested action trace did not change P1's "
                             "motion state\n";
                failed = true;
            }
            if (!input.falls_traces.empty() &&
                (input.falls_samples.size() < 2 ||
                 input.falls_samples.back() <= input.falls_samples.front()))
            {
                std::cerr << "a requested falls trace saw no KO for P1\n";
                failed = true;
            }
            if (failed) {
                return 1;
            }
            std::cout << selection << '\n'
                      << "scenes: " << scenes << '\n'
                      << "route: " << route << '\n'
                      << std::flush;
            return 0;
        }
        if ((argc == 4 || argc == 5) &&
            std::string(argv[1]) == "--load-scene") {
            mh_u32 model_index = 0;
            if (argc == 5) {
                model_index = static_cast<mh_u32>(std::stoul(argv[4]));
            }
            return load_scene(argv[2], argv[3], true, model_index);
        }
        if (argc == 4 && std::string(argv[1]) == "--load-joint") {
            return load_scene(argv[2], argv[3], false, 0);
        }
        if ((argc == 4 || argc == 5) &&
            std::string(argv[1]) == "--render-scene") {
            mh_u32 model_index = 0;
            if (argc == 5) {
                model_index = static_cast<mh_u32>(std::stoul(argv[4]));
            }
            return load_scene(argv[2], argv[3], true, model_index, true);
        }
        if (argc == 4 && std::string(argv[1]) == "--render-joint") {
            return load_scene(argv[2], argv[3], false, 0, true);
        }
#if defined(MELEE_HOST_SDL_RENDERER)
        if (argc == 6 && std::string(argv[1]) == "--view-animation") {
            return view_animation(argv[2], argv[3], argv[4], argv[5]);
        }
        if ((argc == 3 || argc == 5) &&
            std::string(argv[1]) == "--view-title-scene") {
            const char* const screenshot = argc == 5 ? argv[3] : nullptr;
            const auto frame =
                argc == 5 ? static_cast<mh_u32>(std::stoul(argv[4])) : 0U;
            return view_title_scene(argv[2], screenshot, frame);
        }
#endif
        if (argc == 3 && std::string(argv[1]) == "--list-animations") {
            return list_animations(argv[2]);
        }
        if ((argc == 6 || argc == 7) &&
            std::string(argv[1]) == "--animate-named") {
            const unsigned frames =
                argc == 7 ? static_cast<unsigned>(std::stoul(argv[6])) : 60U;
            return animate_named(argv[2], argv[3], argv[4], argv[5], frames);
        }
        if ((argc == 7 || argc == 8) &&
            std::string(argv[1]) == "--animate-joint") {
            const unsigned frames =
                argc == 8 ? static_cast<unsigned>(std::stoul(argv[7])) : 30U;
            const char* const anim = std::string(argv[5]) == "-" ? nullptr
                                                                 : argv[5];
            const char* const mat = std::string(argv[6]) == "-" ? nullptr
                                                                : argv[6];
            return animate_scene(argv[2], argv[3], false, argv[4], anim, mat,
                                 frames, 1.0F);
        }
#if defined(MELEE_HOST_SDL_RENDERER)
        if ((argc == 4 || argc == 5) &&
            std::string(argv[1]) == "--view-scene") {
            mh_u32 model_index = 0;
            if (argc == 5) {
                model_index = static_cast<mh_u32>(std::stoul(argv[4]));
            }
            return view_scene(argv[2], argv[3], true, model_index);
        }
        if (argc == 4 && std::string(argv[1]) == "--view-joint") {
            return view_scene(argv[2], argv[3], false, 0);
        }
        if ((argc == 4 || argc == 5) &&
            std::string(argv[1]) == "--tev-conformance-scene") {
            mh_u32 model_index = 0;
            if (argc == 5) {
                model_index = static_cast<mh_u32>(std::stoul(argv[4]));
            }
            return tev_conformance(argv[2], argv[3], true, model_index);
        }
        if (argc == 4 && std::string(argv[1]) == "--tev-conformance-joint") {
            return tev_conformance(argv[2], argv[3], false, 0);
        }
#endif
        if (argc == 4 && std::string(argv[1]) == "--inspect-pobj") {
            return inspect_pobj(argv[2], argv[3]);
        }
#if defined(MELEE_HOST_SDL_RENDERER)
        if (argc == 4 && std::string(argv[1]) == "--view-pobj") {
            return inspect_pobj(argv[2], argv[3], true);
        }
#endif
        if (argc == 3 && std::string(argv[1]) == "--inspect-resources") {
            return inspect_resources(argv[2]);
        }
        if (argc == 4 && std::string(argv[1]) == "--read-resource") {
            return read_resource(argv[2], argv[3]);
        }
        print_usage(argv[0]);
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
