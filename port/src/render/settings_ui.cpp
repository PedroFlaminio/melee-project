#include "settings_ui.hpp"

#include <imgui.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>

namespace melee::render {

    void init_settings_ui(SDL_Window* window, SDL_GLContext gl_context)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();
        
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        
        // Base Melee Violet: #4B237D (75, 35, 125) -> (0.29f, 0.14f, 0.49f)
        ImVec4 melee_violet        = ImVec4(0.294f, 0.137f, 0.490f, 1.000f);
        ImVec4 melee_violet_light  = ImVec4(0.420f, 0.239f, 0.616f, 1.000f);
        ImVec4 melee_violet_lighter= ImVec4(0.545f, 0.365f, 0.741f, 1.000f);
        ImVec4 melee_violet_dark   = ImVec4(0.169f, 0.075f, 0.302f, 1.000f);
        ImVec4 melee_violet_darker = ImVec4(0.100f, 0.040f, 0.180f, 1.000f);

        colors[ImGuiCol_WindowBg]           = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
        colors[ImGuiCol_TitleBg]            = melee_violet_dark;
        colors[ImGuiCol_TitleBgActive]      = melee_violet;
        colors[ImGuiCol_TitleBgCollapsed]   = melee_violet_darker;
        
        colors[ImGuiCol_Header]             = melee_violet;
        colors[ImGuiCol_HeaderHovered]      = melee_violet_light;
        colors[ImGuiCol_HeaderActive]       = melee_violet_lighter;
        
        colors[ImGuiCol_Button]             = melee_violet;
        colors[ImGuiCol_ButtonHovered]      = melee_violet_light;
        colors[ImGuiCol_ButtonActive]       = melee_violet_lighter;
        
        colors[ImGuiCol_FrameBg]            = melee_violet_dark;
        colors[ImGuiCol_FrameBgHovered]     = melee_violet;
        colors[ImGuiCol_FrameBgActive]      = melee_violet_light;
        
        colors[ImGuiCol_CheckMark]          = melee_violet_lighter;
        colors[ImGuiCol_SliderGrab]         = melee_violet_light;
        colors[ImGuiCol_SliderGrabActive]   = melee_violet_lighter;
        
        colors[ImGuiCol_SeparatorHovered]   = melee_violet_light;
        colors[ImGuiCol_SeparatorActive]    = melee_violet_lighter;
        colors[ImGuiCol_ResizeGrip]         = melee_violet;
        colors[ImGuiCol_ResizeGripHovered]  = melee_violet_light;
        colors[ImGuiCol_ResizeGripActive]   = melee_violet_lighter;
        
        colors[ImGuiCol_Tab]                = melee_violet_dark;
        colors[ImGuiCol_TabHovered]         = melee_violet_light;
        colors[ImGuiCol_TabActive]          = melee_violet;
        colors[ImGuiCol_TabUnfocused]       = melee_violet_darker;
        colors[ImGuiCol_TabUnfocusedActive] = melee_violet_dark;

        ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 130");
    }

    void destroy_settings_ui()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    bool process_settings_event(const SDL_Event* event)
    {
        ImGui_ImplSDL3_ProcessEvent(event);
        ImGuiIO& io = ImGui::GetIO();
        switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
            return io.WantCaptureKeyboard;
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
            return io.WantCaptureMouse;
        default:
            return false;
        }
    }

    bool draw_settings_ui(VideoSettings* settings, bool* open,
                          double current_fps)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        bool changed = false;

        if (settings->show_fps) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGuiWindowFlags window_flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoMove;
            if (ImGui::Begin("FPS", nullptr, window_flags)) {
                ImGui::Text("%.1f FPS", current_fps);
            }
            ImGui::End();
        }

        if (*open) {
            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Video Settings", open)) {
                int res = static_cast<int>(settings->resolution);
                const char* res_items[] = { "1x Native", "2x", "3x", "4x",
                                            "5x" };
                if (ImGui::Combo("Internal Resolution", &res, res_items, 5)) {
                    settings->resolution =
                        static_cast<PresentationResolution>(res);
                    changed = true;
                }

                int aspect = static_cast<int>(settings->aspect);
                const char* aspect_items[] = { "4:3", "16:9", "21:9" };
                if (ImGui::Combo("Aspect Ratio", &aspect, aspect_items, 3)) {
                    settings->aspect = static_cast<PresentationAspect>(aspect);
                    changed = true;
                }

                int filter = static_cast<int>(settings->filter);
                const char* filter_items[] = { "Nearest", "Linear" };
                if (ImGui::Combo("Upscaling Filter", &filter, filter_items, 2))
                {
                    settings->filter = static_cast<PresentationFilter>(filter);
                    changed = true;
                }

                int aa = static_cast<int>(settings->anti_aliasing);
                const char* aa_items[] = { "Off", "2x MSAA", "4x MSAA", "8x MSAA" };
                if (ImGui::Combo("Anti-Aliasing", &aa, aa_items, 4))
                {
                    settings->anti_aliasing = static_cast<PresentationAntiAliasing>(aa);
                    changed = true;
                }

                int aniso = static_cast<int>(settings->anisotropy);
                const char* aniso_items[] = { "Off", "2x", "4x", "8x", "16x" };
                if (ImGui::Combo("Anisotropic Filter", &aniso, aniso_items, 5))
                {
                    settings->anisotropy = static_cast<PresentationAnisotropy>(aniso);
                    changed = true;
                }

                int window_mode = static_cast<int>(settings->window_mode);
                const char* window_mode_items[] = { "Windowed", "Fullscreen",
                                                    "Borderless" };
                if (ImGui::Combo("Window Mode", &window_mode,
                                 window_mode_items, 3))
                {
                    settings->window_mode =
                        static_cast<PresentationWindowMode>(window_mode);
                    changed = true;
                }

                int rate_idx = 0;
                const PresentationRate rates[] = {
                    PresentationRate::Fps60,  PresentationRate::Fps120,
                    PresentationRate::Fps144, PresentationRate::Fps165,
                    PresentationRate::Fps240
                };
                const char* rate_items[] = {
                    "60 FPS",  "120 FPS", "144 FPS",
                    "165 FPS", "240 FPS"
                };
                for (int i = 0; i < 5; ++i) {
                    if (rates[i] == settings->rate) {
                        rate_idx = i;
                    }
                }
                if (ImGui::Combo("Presentation Rate", &rate_idx, rate_items, 6)) {
                    settings->rate = rates[rate_idx];
                    changed = true;
                }

                if (ImGui::Checkbox("Show FPS", &settings->show_fps)) {
                    changed = true;
                }
                if (ImGui::Button("Unlock Everything")) {
                    settings->unlock_requested = true;
                    changed = true;
                }
                if (ImGui::Checkbox("Enable Custom Textures", &settings->custom_textures)) {
                    changed = true;
                }
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return changed;
    }

}
