include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    # A branch makes every CMake reconfigure contact GitHub, even when the
    # dependency was already populated.  Keep builds reproducible and usable
    # offline by tracking a released, known-good revision instead.  A clean
    # build still clones this tag normally; updating it is an intentional
    # source change rather than a side effect of configuring.
    GIT_TAG v1.92.9b
    UPDATE_DISCONNECTED TRUE
)
# FetchContent otherwise runs `git fetch` on every reconfigure of an existing
# source directory.  That is neither needed for the pinned tag nor acceptable
# for an offline build.  Updating it is a deliberate source change.
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PRIVATE SDL3::SDL3 OpenGL::GL)
