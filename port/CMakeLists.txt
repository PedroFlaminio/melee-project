cmake_minimum_required(VERSION 3.25)
project(melee-pc)

# clang-cl identifies as Clang, but accepts the Visual Studio option syntax.
set(MELEE_HOST_MSVC_FRONTEND OFF)
if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(MELEE_HOST_MSVC_FRONTEND ON)
endif()
add_library(melee_host STATIC
    src/assets/dol_image.cpp
    src/assets/hsd_archive.cpp
    src/assets/hsd_host_archive.cpp
    src/assets/hsd_materialize.cpp
    src/assets/gx_texture.cpp
    src/assets/hsd_runtime_archive.cpp
    src/assets/schemas/db_common.cpp
    src/assets/schemas/scene_graphics.cpp
    src/assets/virtual_disc.cpp
    src/gx/command_recorder.cpp
    src/gx/state_recorder.cpp
    src/gx/tev.cpp
    src/gx/view.cpp
    src/video/vi.cpp
    src/host.cpp
    src/os/clock.cpp
    src/os/absent_devices.c
    src/os/alarm.c
    src/os/baselib_support.c
    src/os/ax_mixer.c
    src/os/sound_bank.c
    src/os/dolphin_mtx.c
    src/os/dolphin_time.cpp
    src/os/interrupts.c
    src/os/memory.cpp
    src/os/pad.cpp
    src/os/dvd.cpp
)

# The GX capture runs for every vertex the game draws, and the EFB copies run
# the TEV for every fragment of a copied rectangle.  Unoptimized, the results
# screen's portrait copies alone took a minute of the cancelled VS route; with
# these two files optimized the route is back under half a minute.  They keep
# their debug information.
if(NOT MELEE_HOST_MSVC_FRONTEND)
    set_source_files_properties(src/gx/command_recorder.cpp src/gx/tev.cpp
        PROPERTIES COMPILE_OPTIONS "-O2")
endif()


target_include_directories(melee_host
    PUBLIC include
    PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/extern/dolphin/include
)

target_compile_definitions(melee_host PUBLIC MELEE_HOST=1)

function(melee_host_set_warnings target)
    if(MELEE_HOST_MSVC_FRONTEND)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(MELEE_HOST_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wconversion
            -Wpedantic
            -Wshadow
        )
        if(MELEE_HOST_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(melee_host_set_legacy_warnings target)
    if(MELEE_HOST_MSVC_FRONTEND)
        target_compile_options(${target} PRIVATE /W3)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wno-deprecated-non-prototype
            -Wno-sign-compare
            -Wno-sign-conversion
        )
    endif()
endfunction()

function(melee_host_enable_sanitizers target)
    if(MELEE_HOST_ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
            if(CMAKE_C_COMPILER_ID MATCHES "Clang")
                # ASan describes every global in a table its module
                # constructor registers, and that table would keep each
                # global, and whatever the global points at, alive through
                # --gc-sections.  With this each description sits in a
                # section tied to its global's, so the linker drops both.
                target_compile_options(${target} PRIVATE
                    -fsanitize-address-globals-dead-stripping
                )
            endif()
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
            if(NOT APPLE)
                # The ASan runtime finds those descriptions through
                # __start_asan_globals, and GNU ld keeps every section a
                # __start_ symbol names unless told otherwise.
                target_link_options(${target} PRIVATE -Wl,-z,start-stop-gc)
            endif()
        else()
            message(FATAL_ERROR "Sanitizers are not configured for this compiler")
        endif()
    endif()
endfunction()

melee_host_set_warnings(melee_host)
melee_host_enable_sanitizers(melee_host)

# The first native-compiled slice of the original game. Sources are added here
# only after they compile without changing their PowerPC matching behavior.
# Each fighter's file names, costume strings and costume list, which ftdata.c's
# tables point at.  Most of them live in the character's main file, so the
# character's code is compiled too, and --gc-sections keeps only what is
# reached.  ftkirby.c also holds the copy ability preload.
set(MELEE_HOST_FIGHTER_DATA_SOURCES
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftCaptain/ftcaptain.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftCLink/ftclink.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftCrazyHand/ftcrazyhand.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftDonkey/ftdonkey.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftDrMario/ftdrmario.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftEmblem/ftemblem.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftFalco/ftfalco.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftFox/ftfox.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftGameWatch/ftgamewatch.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftGanon/ftganon.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftGigaKoopa/ftgkoopa.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftKirby/ftkirby.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftKirby/ftkirbydata.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftKoopa/ftkoopa.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftLink/ftlink.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftLuigi/ftluigi.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftMario/ftmariostrings.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftMars/ftmars.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftMasterHand/ftmasterhand.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftMewtwo/ftmewtwo.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftNana/ftnana.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftNess/ftness.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftPeach/ftpeach.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftPichu/ftpichu.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftPikachu/ftpikachu.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftPopo/ftpopo.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftPurin/ftpurin.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftSamus/ftsamus.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftSandbag/ftsandbag.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftSeak/ftseak.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftYoshi/ftyoshi.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftZakoBoy/ftboy.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftZakoGirl/ftgirl.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/kinds/ftZelda/ftzelda.c
)

add_library(melee_game_core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/match_trace.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbtime.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbvector.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbanim.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbarchive.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbfile.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbheap.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lblanguage.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbmemory.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmmain.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmmain_lib.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmscene.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gm_1A36.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmvs.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmvsmelee.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmvsmode.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gm_1B03.c
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mnmain.c
    # The VS route begins with character and stage selection.  Keep their
    # native implementations in the host core while the mode table is being
    # expanded; they currently share the menu/HSD/input substrate above.
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mncharsel.c
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mnstagesel.c
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mnmainrule.c
    ${CMAKE_SOURCE_DIR}/src/melee/gr/ground.c
    ${CMAKE_SOURCE_DIR}/src/melee/pl/player.c
    ${CMAKE_SOURCE_DIR}/src/melee/pl/plattack.c
    ${CMAKE_SOURCE_DIR}/src/melee/pl/plbonus.c
    ${CMAKE_SOURCE_DIR}/src/melee/pl/plstale.c
    ${CMAKE_SOURCE_DIR}/src/melee/pl/pl_040D.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/fighter.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/aobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/controller.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/devcom.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/fobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/class.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/object.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hash.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/cobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/displayfunc.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/dobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/fog.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/jobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/lobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/mobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/pobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/robj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/tobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/wobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/shadow.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/state.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/tev.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/texp.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/texpdag.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/bytecode.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/initialize.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/perf.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/util.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/video.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/gobj.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/gobjinit.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/gobjproc.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/gobjplink.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/gobjgxlink.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/gobjobject.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/gobjuserdata.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/id.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/memory.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/objalloc.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/list.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/mtx.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/rumble.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/spline.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/synth.c
    src/game/baselib_bootstrap.c
    src/game/local_match.c
    src/game/match_rules.c
    src/game/hsd_audio_stubs.c
    src/game/menu_native.c
    src/game/scene_runtime.c
    src/game/scene_graphics.cpp
    src/game/archive_probe.cpp
    src/game/boot_memory.c
    src/game/title_scene.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmtitle.c
    ${CMAKE_SOURCE_DIR}/src/melee/db/dbinit.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gm_1601.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gm_1A3F.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmopening.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmopeningmode.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbaudio_ax.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbdvd.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbmthp.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbspdisplay.c
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mn_22EC.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/sislib.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3A64.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3A76.c
    src/game/dol_data.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/axdriver.c
    ${CMAKE_SOURCE_DIR}/src/melee/gr/stage.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmcameramode.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbsnap.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lb_0195.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lb_013B.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lb_00B0.c
    ${CMAKE_SOURCE_DIR}/src/melee/ty/toy.c
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mnname.c
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mnnamenew.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gm_16F1.c
    ${CMAKE_SOURCE_DIR}/src/melee/if/if_2FF2.c
    ${CMAKE_SOURCE_DIR}/src/melee/if/textdraw.c
    ${CMAKE_SOURCE_DIR}/src/melee/if/textlib_1.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3915.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/sobjlib.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3924.c
    # The title's state table, and what the scene frame loop (gm_801A4D34) and
    # its pad alarm call every frame.
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmtitlemode.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_392C.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3933.c
    ${CMAKE_SOURCE_DIR}/src/melee/db/dbscreenshot.c
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbcardgame.c
    # The end of a game mode's routing reaches the memory card layer, and the
    # title state's exit reaches gm_1736.c.  The host's mode and scene tables
    # stand in for gmscdata.c.
    ${CMAKE_SOURCE_DIR}/src/melee/lb/lbcardnew.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3A94.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3B27.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3B2B.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3B2E.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3B33.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3B34.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_4D11.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gm_1736.c
    ${CMAKE_SOURCE_DIR}/src/melee/ty/tydisplay.c
    # The title state keeps every preload heap, so the title demo's preload
    # registers the demo fighters' files through ftdata.c.
    ${CMAKE_SOURCE_DIR}/src/melee/ft/ftdata.c
    ${MELEE_HOST_FIGHTER_DATA_SOURCES}
    # A fighter's preload also registers its effect archive.
    ${CMAKE_SOURCE_DIR}/src/melee/ef/efasync.c
    # The main menu's mode.  Its state allocates the snapshot buffers and the
    # gallery's rate table before the menu scene loads.
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmmenumode.c
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mngallery.c
    ${CMAKE_SOURCE_DIR}/src/melee/mn/mnsnap.c
    # What the menu scene and the screens its table names call directly: the
    # event table loader, the how-to movie check, home run's name tags, the
    # out-of-line matrix dirty flag and the snapshot loader.
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmevent.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmhowto.c
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmhomerun.c
    ${CMAKE_SOURCE_DIR}/src/melee/ft/ft_0C31.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/hsd_3B5C.c
    src/game/game_tables.c
    src/game/game_data_translators.c
    src/game/unported.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/random.c
    src/os/os_heap.c
    ${CMAKE_SOURCE_DIR}/extern/dolphin/src/dolphin/os/OSAlloc.c
    ${CMAKE_SOURCE_DIR}/extern/dolphin/src/dolphin/os/OSArena.c
    ${CMAKE_SOURCE_DIR}/extern/dolphin/src/dolphin/pad/PadClamp.c
    # The SDK's audio effects are arithmetic on sample buffers and run as
    # they are.
    ${CMAKE_SOURCE_DIR}/extern/dolphin/src/dolphin/axfx/axfx.c
    ${CMAKE_SOURCE_DIR}/extern/dolphin/src/dolphin/axfx/chorus.c
    ${CMAKE_SOURCE_DIR}/extern/dolphin/src/dolphin/axfx/delay.c
    ${CMAKE_SOURCE_DIR}/extern/dolphin/src/dolphin/axfx/reverb_hi.c
    ${CMAKE_SOURCE_DIR}/extern/dolphin/src/dolphin/axfx/reverb_std.c
)
target_compile_definitions(melee_game_core PUBLIC MELEE_HOST=1)
target_include_directories(melee_game_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/extern/dolphin/include
    PRIVATE
        src
)
melee_host_set_legacy_warnings(melee_game_core)
# The decompiled C gives a few functions a plain `inline` definition, which
# MWCC emits out of line when a call is not inlined.  C99 makes that definition
# inline-only, which leaves an undefined reference at -O0; GNU89 semantics emit
# it, as MWCC does.  No header has a plain inline definition, so nothing is
# emitted twice.
if(NOT MELEE_HOST_MSVC_FRONTEND)
    target_compile_options(melee_game_core PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-fgnu89-inline>)
endif()
# The rest of the decompiled game.  --gc-sections drops what nothing reaches,
# in the sanitizer build too, so a module no scene calls adds nothing to a
# link, and what a scene reaches is what gets linked.  gmscdata.c stays out:
# the host's mode and scene tables (src/game/game_tables.c) stand in for it.
file(GLOB_RECURSE MELEE_HOST_REST_OF_GAME CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/melee/*.c)
get_target_property(melee_game_core_listed melee_game_core SOURCES)
list(REMOVE_ITEM MELEE_HOST_REST_OF_GAME
    ${melee_game_core_listed}
    ${CMAKE_SOURCE_DIR}/src/melee/gm/gmscdata.c
)
target_sources(melee_game_core PRIVATE ${MELEE_HOST_REST_OF_GAME})
# The baselib the rest of the game calls into: quaternions, the particle
# system, its generators and their application SRT.
target_sources(melee_game_core PRIVATE
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/particle.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/psdisp.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/psdisptev.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/generator.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/psappsrt.c
    ${CMAKE_SOURCE_DIR}/src/sysdolphin/baselib/quatlib.c
    # The not-a-number and infinity constants lbtrigf.c returns.
    ${CMAKE_SOURCE_DIR}/src/MSL/float.c
)
# Port code keeps the strict warning set even where it shares a target with
# decompiled translation units, which need the legacy relaxations.  Use the
# warning spelling of the active compiler: passing GCC options to cl.exe makes
# an otherwise valid Windows build stop before compiling any source.
if(MELEE_HOST_MSVC_FRONTEND)
    set_source_files_properties(
        src/game/scene_graphics.cpp
        src/game/archive_probe.cpp
        PROPERTIES COMPILE_OPTIONS "/W4"
    )
else()
    set_source_files_properties(
        src/game/scene_graphics.cpp
        src/game/archive_probe.cpp
        PROPERTIES COMPILE_OPTIONS "-Wconversion;-Wpedantic;-Wshadow"
    )
endif()
melee_host_enable_sanitizers(melee_game_core)
target_link_libraries(melee_game_core PUBLIC melee_host)
if(NOT MELEE_HOST_MSVC_FRONTEND)
    target_compile_options(melee_game_core PRIVATE -ffunction-sections -fdata-sections)
endif()

add_executable(melee-pc src/main.cpp)
if(WIN32)
    target_sources(melee-pc PRIVATE src/windows_asset_setup.cpp)
    target_link_libraries(melee-pc PRIVATE Comctl32 Comdlg32 Shell32)
endif()
target_link_libraries(melee-pc PRIVATE melee_game_core)
target_include_directories(melee-pc PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/extern/dolphin/include
)
melee_host_set_warnings(melee-pc)
melee_host_enable_sanitizers(melee-pc)
if(UNIX AND NOT APPLE)
    target_link_options(melee-pc PRIVATE -Wl,--gc-sections)
endif()

option(MELEE_HOST_BUILD_SDL_RENDERER "Build the SDL/OpenGL geometry preview" ON)
if(MELEE_HOST_BUILD_SDL_RENDERER)
    find_package(SDL3 CONFIG QUIET)
    if(NOT SDL3_FOUND)
        include(FetchContent)
        # Keep the Windows executable self-contained instead of requiring a
        # separately installed SDL runtime.
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.4.14
        )
        FetchContent_MakeAvailable(SDL3)
    endif()
    find_package(OpenGL QUIET)
    if(TARGET SDL3::SDL3 AND TARGET OpenGL::GL)
        include(cmake/imgui.cmake)
        target_sources(melee-pc PRIVATE src/render/sdl_gl_renderer.cpp src/render/settings_ui.cpp src/stb_image.cpp src/custom_textures.cpp)
        target_link_libraries(melee-pc PRIVATE SDL3::SDL3 OpenGL::GL imgui)
        target_compile_definitions(melee-pc PRIVATE MELEE_HOST_SDL_RENDERER=1)
    else()
        message(STATUS "SDL3/OpenGL not found; geometry preview is disabled")
    endif()
endif()

if(MELEE_HOST_BUILD_TESTS)
    enable_testing()

    add_executable(melee-host-tests
        tests/game_core_test.cpp
        tests/aobj_test.cpp
        tests/gx_command_recorder_test.cpp
        tests/hsd_archive_test.cpp
        tests/gx_texture_test.cpp
        tests/hsd_schema_test.cpp
        tests/interrupts_test.cpp
        tests/os_clock_test.cpp
        tests/virtual_disc_test.cpp
        tests/dvd_test.cpp
        tests/host_test.cpp
        tests/input_test.cpp
        tests/id_test.cpp
        tests/memory_test.cpp
        tests/dolphin_mtx_test.cpp
        tests/fobj_test.cpp
        tests/list_test.cpp
        tests/local_match_test.cpp
        tests/scene_runtime_test.cpp
        tests/spline_test.cpp
        tests/ax_mixer_test.cpp
        tests/synth_test.cpp
        tests/gx_state_test.cpp
        tests/gx_tev_test.cpp
        tests/gx_view_test.cpp
        tests/video_test.cpp
        tests/os_heap_test.cpp
        tests/hsd_graphics_test.cpp
        tests/hsd_materialize_test.cpp
        tests/hsd_host_archive_test.cpp
        tests/lb_memory_test.cpp
        tests/dol_test.cpp
        tests/baselib_bootstrap_test.cpp
        tests/pad_test.cpp
        tests/play_window_test.cpp
        tests/command_layout_test.cpp
        tests/command_layout_check.c
        tests/command_stream_run.c
        tests/item_data_check.c
        tests/stage_data_check.c
        tests/fighter_data_check.c
        tests/fighter_fox_data_check.c
        tests/scene_data_check.c
        tests/ax_mixer_check.c
        tests/test_main.cpp
    )
    target_link_libraries(melee-host-tests PRIVATE melee_game_core)
    target_include_directories(melee-host-tests PRIVATE src tests)
    melee_host_set_warnings(melee-host-tests)
    # The C checks include the game's own headers, which need the relaxations
    # the decompiled code builds with.
    if(NOT MELEE_HOST_MSVC_FRONTEND)
        set_source_files_properties(
            tests/command_layout_check.c
            tests/command_stream_run.c
            tests/item_data_check.c
            tests/stage_data_check.c
            tests/fighter_data_check.c
            tests/fighter_fox_data_check.c
            tests/scene_data_check.c
            tests/ax_mixer_check.c
            PROPERTIES COMPILE_OPTIONS
                "-Wno-pedantic;-Wno-conversion;-Wno-shadow;-Wno-sign-conversion;-Wno-sign-compare;-Wno-deprecated-non-prototype;-Wno-unused-parameter;-Wno-missing-field-initializers"
        )
    endif()
    melee_host_enable_sanitizers(melee-host-tests)
    if(UNIX AND NOT APPLE)
        target_link_options(melee-host-tests PRIVATE -Wl,--gc-sections)
    endif()

    add_test(NAME melee-host-tests COMMAND melee-host-tests)
    # host_command_layout.h and command_layout_check.c are generated from
    # src/melee/lb/types.h; this fails when they no longer match it.
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME melee-host-command-layout-generated
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_host_command_layout.py
                    --check
        )
    endif()
    # Local game assets are intentionally untracked.  When available, retain
    # an end-to-end regression for the TLUT material path without making CI
    # depend on proprietary data.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/GmRegClr.dat")
        add_test(
            NAME melee-host-tlut-asset
            COMMAND melee-pc --inspect-pobj
                    ${CMAKE_SOURCE_DIR}/assets-local/GmRegClr.dat
                    ScGamRegClear_scene_data
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_tests_properties(melee-host-tlut-asset PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    # The host's AX mixer against the reference decoder in
    # tools/ssm_to_wav.py, sample for sample, over every voice of two sound
    # banks: short effects at 32 and 8 kHz, some looping, and the title's long
    # announcer lines at 12 kHz.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/audio/bigblue.ssm")
        foreach(bank bigblue nr_title)
            add_test(
                NAME melee-host-sound-bank-${bank}-asset
                COMMAND ${Python3_EXECUTABLE}
                        ${CMAKE_SOURCE_DIR}/port/tools/ssm_to_wav.py
                        ${CMAKE_SOURCE_DIR}/assets-local/audio/${bank}.ssm
                        --compare-host $<TARGET_FILE:melee-pc>
            )
            if(MELEE_HOST_ENABLE_SANITIZERS)
                set_tests_properties(melee-host-sound-bank-${bank}-asset
                    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
                )
            endif()
        endforeach()
    endif()
    # What the game plays, end to end: from the title to the main menu and
    # back, START's effect and the menu music recorded from the host's mixer
    # and compared with decoders that do not use it.  The music is checked
    # window by window across its chunk joins, and the effect with the music
    # taken out.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/audio/menu01.hps" AND
       EXISTS "${CMAKE_SOURCE_DIR}/assets-local/audio/main.ssm" AND
       EXISTS "${CMAKE_SOURCE_DIR}/assets-local/sys/main.dol")
        add_test(
            NAME melee-host-route-audio-asset
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/port/tools/check_route_audio.py
                    $<TARGET_FILE:melee-pc> ${CMAKE_SOURCE_DIR}/assets-local
        )
        set_tests_properties(melee-host-route-audio-asset PROPERTIES
            PASS_REGULAR_EXPRESSION
                "the host's music and effect match the reference decoders"
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_tests_properties(melee-host-route-audio-asset PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    # The same data, loaded through the original object layer instead of the
    # read-only schema.  One menu scene and one character model, which is the
    # deeper tree and the one carrying envelopes and shape sets.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/GmPause.dat")
        add_test(
            NAME melee-host-scene-load-asset
            COMMAND melee-pc --render-scene
                    ${CMAKE_SOURCE_DIR}/assets-local/GmPause.dat
                    ScGamPause_scene_data
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_tests_properties(melee-host-scene-load-asset PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/PlMrNr.dat")
        add_test(
            NAME melee-host-model-load-asset
            COMMAND melee-pc --render-joint
                    ${CMAKE_SOURCE_DIR}/assets-local/PlMrNr.dat
                    PlyMario5K_Share_joint
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_tests_properties(melee-host-model-load-asset PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    # The exact symbol list gmTitle_801A1AC0 hands lbArchive_LoadSymbols, which
    # makes every one of them fatal if missing.  The US file is the one the
    # game opens for an English setting.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/GmTtAll.usd")
        add_test(
            NAME melee-host-title-archive-asset
            COMMAND melee-pc --load-archive
                    ${CMAKE_SOURCE_DIR}/assets-local/GmTtAll.usd
                    TtlMoji_Top_joint TtlMoji_Top_animjoint
                    TtlMoji_Top_matanim_joint TtlMoji_Top_shapeanim_joint
                    ScTitle_cam_int1_camera ScTitle_scene_lights ScTitle_fog
                    TtlBg_Top_joint TtlBg_Top_animjoint TtlBg_Top_matanim_joint
                    TtlBg_Top_shapeanim_joint TitleMark_sobjdesc
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_tests_properties(melee-host-title-archive-asset PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    # The same archive through the game's own loader: gmMain's memory sequence,
    # lbArchive_LoadSymbols, and lbFile over devcom and the host DVD.  It runs
    # in its own process because it moves the OS arena.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/dvd-index.bin")
        add_test(
            NAME melee-host-boot-title-archive-asset
            COMMAND melee-pc --boot-title-archive
                    ${CMAKE_SOURCE_DIR}/assets-local
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_tests_properties(melee-host-boot-title-archive-asset PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    # Fighter records are character-specific.  Keep Mario and Link covered
    # separately from Fox so a new playable fighter cannot silently lose its
    # archive translator.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/PlMr.dat")
        add_test(
            NAME melee-host-mario-data-asset
            COMMAND melee-pc --load-archive
                    ${CMAKE_SOURCE_DIR}/assets-local/PlMr.dat ftDataMario
        )
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/PlLk.dat")
        add_test(
            NAME melee-host-link-data-asset
            COMMAND melee-pc --load-archive
                    ${CMAKE_SOURCE_DIR}/assets-local/PlLk.dat ftDataLink
        )
    endif()
    # The title screen entered by its own code: the scene manager's setup
    # (gm_801A4BD4) and gm_Scene_Title_OnEnter, after gmMain's boot.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/sys/main.dol")
        add_test(
            NAME melee-host-title-scene-asset
            COMMAND melee-pc --boot-title-scene
                    ${CMAKE_SOURCE_DIR}/assets-local
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_tests_properties(melee-host-title-scene-asset PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
        # The title screen's own frame loop, gm_801A4D34, on a frozen OS clock:
        # the scene has to draw its frames and leave by itself when its
        # countdown and 600 frames run out.
        add_test(
            NAME melee-host-title-frames-asset
            COMMAND melee-pc --run-title-scene
                    ${CMAKE_SOURCE_DIR}/assets-local
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_tests_properties(melee-host-title-frames-asset PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
        # Whole game modes through the scene manager's routing, each with its
        # state's preload, its frame loop and the exit that picks the next
        # mode.  Left alone the title times out to the opening movie.  START
        # leads to the main menu, where DOWN, A and A pick VS Melee.  The route
        # line is printed only when every mode returned.
        add_test(
            NAME melee-host-title-mode-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 1
        )
        add_test(
            NAME melee-host-main-menu-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 2
                    120:START 160:DOWN 200:A 240:A
        )
        set_tests_properties(melee-host-title-mode-asset PROPERTIES
            PASS_REGULAR_EXPRESSION "route: 0x00 \\(621 frames\\) 0x18\n"
        )
        set_tests_properties(melee-host-main-menu-asset PROPERTIES
            PASS_REGULAR_EXPRESSION
                "route: 0x00 \\(122 frames\\) 0x01 \\(120 frames\\) 0x02\n"
        )
        # A VS match with two pads.  Each pad raises its cursor to its door's
        # HMN button, grabs its coin on the way to Fox and drops it there;
        # START leaves for stage select, where pad 1 picks Hyrule Temple.  The
        # match runs Fox against Fox until pausing is allowed (frame 655),
        # pad 1 pauses and quits with L+R+A+START, which the match takes as no
        # contest.  The results screen follows: a button on pad 1 moves past
        # its opening, and START on each human pad marks that player ready (a
        # second START would unmark it), which returns to character select,
        # where holding B returns to the main menu.  The selection is read back
        # from the VS mode's data: stage 14, Fox (2) in slots 0 and 1.
        add_test(
            NAME melee-host-vs-match-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 3
                    120:START 160:DOWN 200:A 240:A
                    300-315:SY=127 300-315:SY=127@2 320:A 320:A@2
                    330-337:SX=127 330-333:SX=-127@2
                    345-354:SY=127 345-354:SY=127@2 360:A 360:A@2
                    380:START 420-421:SX=-127 425-439:SY=127 445:A
                    640:SHADOW 640:MOVE 640:ACTION 641:A 641-645:SX=127
                    650:ACTION 660:MOVE
                    680:START 700-715:L+R+A 705-715:START
                    960:START 1100:START 1100:START@2 1200-1400:B
        )
        set_tests_properties(melee-host-vs-match-asset PROPERTIES
            PASS_REGULAR_EXPRESSION
                "vs selection: stage 14 0=2 1=2\nscenes: 0x00 \\(122\\) 0x01 \\(120\\) 0x08 \\(141\\) 0x09 \\(149\\) 0x02 \\(175\\) 0x05 \\(406\\) 0x08 \\(120\\)\nroute: 0x00 \\(122 frames\\) 0x01 \\(120 frames\\) 0x02 \\(991 frames\\) 0x01\n"
        )
        # A VS match that ends by itself.  In character select, after both
        # pads take Fox, pad 1 takes its cursor to the corner and back under
        # the rules button; the rules menu turns Time into Stock and lowers the
        # stock count from 3 to 1, and B returns to character select with both
        # choices kept.  In the match pad 1 runs left, jumps over the temple's
        # wall and falls off the stage.  Its only stock gone, the match ends by
        # elimination and the results screen shows a finished match: a button
        # moves past the winner, START on each human pad marks that player
        # ready, and holding B in character select returns to the main menu.
        # The route is long, and under the sanitizers it takes most of the
        # default timeout.
        add_test(
            NAME melee-host-vs-stock-match-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 3
                    120:START 160:DOWN 200:A 240:A
                    300-315:SY=127 300-315:SY=127@2 320:A 320:A@2
                    330-337:SX=127 330-333:SX=-127@2
                    345-354:SY=127 345-354:SY=127@2 360:A 360:A@2
                    365-404:SX=-127 365-404:SY=127 406-423:SX=127 430:A
                    460:RIGHT 480:DOWN 490:LEFT 510:LEFT 520:RULES 540:B
                    600:START 640-641:SX=-127 645-659:SY=127 665:A
                    860:FALLS 861-1120:SX=-127 925:X 1150:FALLS
                    1300:RESULT 1450:EFBCOPY
                    1466:START 1606:START 1606:START@2 1706-1906:B
        )
        set_tests_properties(melee-host-vs-stock-match-asset PROPERTIES
            TIMEOUT 3600
            PASS_REGULAR_EXPRESSION
                "rules frame 520: mode 1 time 2 stock 1\n.*result frame 1300: outcome 2 winners 1 first 1 stocks P1=0 P2=1\n.*efb copy frame 1450: [0-9]+ copied texture\\(s\\), at most [0-9]+ colour\\(s\\), ok\n.*vs selection: stage 14 0=2 1=2\nscenes: 0x00 \\(122\\) 0x01 \\(120\\) 0x08 \\(361\\) 0x09 \\(149\\) 0x02 \\(460\\) 0x05 \\(407\\) 0x08 \\(120\\)\nroute: 0x00 \\(122 frames\\) 0x01 \\(120 frames\\) 0x02 \\(1497 frames\\) 0x01\n"
        )
        # A match that ends tied, which the game breaks with a sudden death.
        # The default rules are a two minute Time match, so the route leaves
        # the rules alone and only moves the clock: CLOCK=3 puts three
        # seconds on it once the HUD is up, and the scene counts them down
        # itself, with the time-up at 0s+59 the way the console's match does.
        # Neither Fox scored, so the match ends with two winners and the mode
        # goes to its sudden death state (scene 0x03): one stock each at 300%,
        # where pad 1 runs off the stage again.  The results screen keeps the
        # tied Time match's end - outcome 1 (time-up), two winners, three
        # stocks each - and shows the places the sudden death decided, 2nd for
        # pad 1 and 1st for pad 2.
        add_test(
            NAME melee-host-vs-sudden-death-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 3
                    120:START 160:DOWN 200:A 240:A
                    300-315:SY=127 300-315:SY=127@2 320:A 320:A@2
                    330-337:SX=127 330-333:SX=-127@2
                    345-354:SY=127 345-354:SY=127@2 360:A 360:A@2
                    380:START 420-421:SX=-127 425-439:SY=127 445:A
                    660:RULES 680:CLOCK=3 950:CLOCK
                    1100:FALLS 1150-1420:SX=-127 1215:X 1450:FALLS
                    1600:RESULT 1700:EFBCOPY
                    1756:START 1896:START 1896:START@2 1996-2196:B
        )
        set_tests_properties(melee-host-vs-sudden-death-asset PROPERTIES
            TIMEOUT 3600
            PASS_REGULAR_EXPRESSION
                "rules frame 660: mode 0 time 2 stock 3\nclock frame 680: set 3s\nclock frame 950: 0s\\+59\n.*falls frame 1450: P1=1 P2=0\n.*result frame 1600: outcome 1 winners 2 first 0 stocks P1=3 P2=3\nplaces frame 1600: P1=2 P2=1\n.*efb copy frame 1700: [0-9]+ copied texture\\(s\\), at most [0-9]+ colour\\(s\\), ok\n.*vs selection: stage 14 0=2 1=2\nscenes: 0x00 \\(122\\) 0x01 \\(120\\) 0x08 \\(141\\) 0x09 \\(149\\) 0x02 \\(501\\) 0x03 \\(469\\) 0x05 \\(407\\) 0x08 \\(120\\)\nroute: 0x00 \\(122 frames\\) 0x01 \\(120 frames\\) 0x02 \\(1787 frames\\) 0x01\n"
        )
        # The challenger the results screen leads to.  A fighter is
        # unlocked by the VS matches the save data counts, and the smallest
        # total that unlocks one is 50, which no route can play, so MATCHES=50
        # leaves the save data where 50 matches would.  The stock match is the
        # same as above; on leaving the results the game finds Jigglypuff
        # (unlock index 4) still locked, and the mode enters the challenger
        # scene (0x29), which shows "A new foe has appeared!" over its
        # silhouette and waits for the loser's pad.  The route stops there:
        # the match that would follow is on the challenger's own stage, whose
        # data the host does not translate.
        add_test(
            NAME melee-host-vs-challenger-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 3
                    120:START 160:DOWN 200:A 240:A
                    300-315:SY=127 300-315:SY=127@2 320:A 320:A@2
                    330-337:SX=127 330-333:SX=-127@2
                    345-354:SY=127 345-354:SY=127@2 360:A 360:A@2
                    365-404:SX=-127 365-404:SY=127 406-423:SX=127 430:A
                    460:RIGHT 480:DOWN 490:LEFT 510:LEFT 520:RULES 540:B
                    600:START 640-641:SX=-127 645-659:SY=127 665:A
                    900:MATCHES=50 905:MATCHES
                    861-1120:SX=-127 925:X 1150:FALLS 1160:FALLS
                    1300:RESULT 1466:START 1606:START 1606:START@2
                    1700:MATCHES 1800:STOP
        )
        set_tests_properties(melee-host-vs-challenger-asset PROPERTIES
            TIMEOUT 3600
            PASS_REGULAR_EXPRESSION
                "matches frame 905: 50\n.*result frame 1300: outcome 2 winners 1 first 1 stocks P1=0 P2=1\nplaces frame 1300: P1=2 P2=1\nscene 0x29 from frame 1620\nmatches frame 1700: 51\nstopped at frame 1800\n"
        )
        # The prize notice, the other place the results screen leads to.
        # The game hands out a trophy when a play total crosses one of its
        # thresholds, and TROPHY=0x55 takes that path (fn_80172C78) for the
        # Maxim Tomato, which leaves the notice pending.  After the same stock
        # match the mode enters the prize scene (0x27), which says what the
        # trophy was; a button closes it and the mode carries on to character
        # select, where the route stops.
        add_test(
            NAME melee-host-vs-prize-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 3
                    120:START 160:DOWN 200:A 240:A
                    300-315:SY=127 300-315:SY=127@2 320:A 320:A@2
                    330-337:SX=127 330-333:SX=-127@2
                    345-354:SY=127 345-354:SY=127@2 360:A 360:A@2
                    365-404:SX=-127 365-404:SY=127 406-423:SX=127 430:A
                    460:RIGHT 480:DOWN 490:LEFT 510:LEFT 520:RULES 540:B
                    600:START 640-641:SX=-127 645-659:SY=127 665:A
                    900:TROPHY=0x55
                    861-1120:SX=-127 925:X 1150:FALLS 1160:FALLS
                    1300:RESULT 1466:START 1606:START 1606:START@2
                    1900:A 1990:STOP
        )
        set_tests_properties(melee-host-vs-prize-asset PROPERTIES
            TIMEOUT 3600
            PASS_REGULAR_EXPRESSION
                "trophy frame 900: 85 new\n.*result frame 1300: outcome 2 winners 1 first 1 stocks P1=0 P2=1\nplaces frame 1300: P1=2 P2=1\nscene 0x27 from frame 1620\nscene 0x08 from frame 1910\nstopped at frame 1990\n"
        )
        # The same stock match with Mario and Link, both chosen from the
        # character select rather than forced: pad 1 holds up five frames
        # longer than the Fox route to reach Mario's icon in the top row, and
        # pad 2 walks right along the middle row to Link.  Their own data,
        # items and animations carry the match: Mario falls off the stage, the
        # results screen shows Link as the winner and copies both portraits
        # from the EFB.  The selection reads back as Mario (8) and Link (6).
        add_test(
            NAME melee-host-vs-mario-link-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 3
                    120:START 160:DOWN 200:A 240:A
                    300-315:SY=127 300-315:SY=127@2 320:A 320:A@2
                    330-337:SX=127 330-359:SX=127@2
                    345-359:SY=127 345-358:SY=127@2 362:A 362:A@2
                    365-404:SX=-127 365-404:SY=127 406-423:SX=127 430:A
                    460:RIGHT 480:DOWN 490:LEFT 510:LEFT 520:RULES 540:B
                    600:START 640-641:SX=-127 645-659:SY=127 665:A
                    860:FALLS 861-1120:SX=-127 925:X 1150:FALLS
                    1300:RESULT 1450:EFBCOPY
                    1466:START 1606:START 1606:START@2 1706-1906:B
        )
        set_tests_properties(melee-host-vs-mario-link-asset PROPERTIES
            TIMEOUT 3600
            PASS_REGULAR_EXPRESSION
                "rules frame 520: mode 1 time 2 stock 1\n.*falls frame 1150: P1=1 P2=0\n.*result frame 1300: outcome 2 winners 1 first 1 stocks P1=0 P2=1\n.*efb copy frame 1450: [0-9]+ copied texture\\(s\\), at most [0-9]+ colour\\(s\\), ok\n.*vs selection: stage 14 0=8 1=6\nscenes: 0x00 \\(122\\) 0x01 \\(120\\) 0x08 \\(361\\) 0x09 \\(149\\) 0x02 \\(489\\) 0x05 \\(378\\) 0x08 \\(120\\)\nroute: 0x00 \\(122 frames\\) 0x01 \\(120 frames\\) 0x02 \\(1497 frames\\) 0x01\n"
        )
        # Mario's and Link's own moves, the ones that need their character
        # items: Mario throws a fireball, swings the cape, spins in the air
        # and rises with the super jump punch; Link charges an arrow, throws
        # the boomerang, pulls a bomb and throws it, spins his sword and
        # grabs with the hookshot, which both fighters do on the same frame.
        # ACTION reads pad 1's motion state back: SpecialN (343), SpecialS
        # (345), SpecialAirLw (350), SpecialHi (347) and Catch (212).  Pad 1
        # then pauses and quits the match, and both pads leave the results.
        add_test(
            NAME melee-host-mario-link-specials-asset
            COMMAND melee-pc --run-modes
                    ${CMAKE_SOURCE_DIR}/assets-local 0 3
                    120:START 160:DOWN 200:A 240:A
                    300-315:SY=127 300-315:SY=127@2 320:A 320:A@2
                    330-337:SX=127 330-359:SX=127@2
                    345-359:SY=127 345-358:SY=127@2 362:A 362:A@2
                    380:START 420-421:SX=-127 425-439:SY=127 445:A
                    660:B 665:ACTION 730:SX=127+B 735:ACTION
                    800:SY=-128+B 805:ACTION 880:SY=127+B 885:ACTION
                    985:Z 990:ACTION
                    665-700:B@2 740:SX=-127+B@2 790:SY=-128+B@2 830:A@2
                    870:SY=127+B@2 985:Z@2
                    1010:START 1030-1045:L+R+A 1035-1045:START
                    1290:START 1430:START 1430:START@2 1530-1730:B
        )
        set_tests_properties(melee-host-mario-link-specials-asset PROPERTIES
            TIMEOUT 3600
            PASS_REGULAR_EXPRESSION
                "action frame 665: P1=343\naction frame 735: P1=345\naction frame 805: P1=350\naction frame 885: P1=347\naction frame 990: P1=212\n.*vs selection: stage 14 0=8 1=6\nscenes: 0x00 \\(122\\) 0x01 \\(120\\) 0x08 \\(141\\) 0x09 \\(149\\) 0x02 \\(505\\) 0x05 \\(406\\) 0x08 \\(120\\)\nroute: 0x00 \\(122 frames\\) 0x01 \\(120 frames\\) 0x02 \\(1321 frames\\) 0x01\n"
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_property(TEST melee-host-title-mode-asset
                melee-host-main-menu-asset melee-host-vs-match-asset
                melee-host-vs-stock-match-asset
                melee-host-vs-sudden-death-asset
                melee-host-vs-challenger-asset
                melee-host-vs-prize-asset
                melee-host-vs-mario-link-asset
                melee-host-mario-link-specials-asset
                APPEND PROPERTY
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    # A model and its animation from the same archive, advanced far enough that
    # a joint actually travels.  This is the end-to-end check that keyframe
    # streams survived translation and the original interpreter read them.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/GmTtAll.dat")
        add_test(
            NAME melee-host-animation-asset
            COMMAND melee-pc --animate-joint
                    ${CMAKE_SOURCE_DIR}/assets-local/GmTtAll.dat
                    TtlMoji_Top_joint
                    ${CMAKE_SOURCE_DIR}/assets-local/GmTtAll.dat
                    TtlMoji_Top_animjoint TtlMoji_Top_matanim_joint 200
        )
        set_tests_properties(melee-host-animation-asset PROPERTIES
            PASS_REGULAR_EXPRESSION "AObj frame: 199 of 1600"
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_property(TEST melee-host-animation-asset APPEND PROPERTY
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    # A character's own walk cycle, out of the file that holds one archive per
    # action.  This is the whole chain: the concatenated container, the
    # FigaTree format, and the original applier driving a real skeleton.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets-local/PlMrAJ.dat")
        add_test(
            NAME melee-host-character-animation-asset
            COMMAND melee-pc --animate-named
                    ${CMAKE_SOURCE_DIR}/assets-local/PlMrNr.dat
                    PlyMario5K_Share_joint
                    ${CMAKE_SOURCE_DIR}/assets-local/PlMrAJ.dat
                    PlyMario5K_Share_ACTION_WalkMiddle_figatree 30
        )
        set_tests_properties(melee-host-character-animation-asset PROPERTIES
            PASS_REGULAR_EXPRESSION "AObj frame 29 of 45"
        )
        if(MELEE_HOST_ENABLE_SANITIZERS)
            set_property(TEST melee-host-character-animation-asset APPEND
                PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
            )
        endif()
    endif()
    if(MELEE_HOST_ENABLE_SANITIZERS)
        # LeakSanitizer cannot run under the traced execution environment used
        # by some CI runners. AddressSanitizer and UBSan remain enabled.
        set_tests_properties(melee-host-tests PROPERTIES
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
        )
    endif()
endif()
