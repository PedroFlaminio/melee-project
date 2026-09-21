#ifndef MELEE_RENDER_PLAY_WINDOW_HPP
#define MELEE_RENDER_PLAY_WINDOW_HPP

#include <cstdint>
#include <cstdio>
#include <string>

/* The pieces of the play window that do not need SDL, so the unit tests reach
 * them. */
namespace melee::render {

    inline constexpr const char* kPlayWindowName = "Melee PC";
    inline constexpr const char* kPlayWindowControls =
        "Enter is START, WASD is stick; Esc opens Video";

    /* Number of interactive rows in the video settings menu. */
    inline constexpr std::uint8_t kVideoMenuRows = 8;

    /* The simulation remains 60 Hz for all rates.  Presentation is paced at
     * one of these finite target frequencies; keeping the set finite makes
     * the reported rate meaningful and avoids an unbounded swap loop. */
    enum class PresentationRate : std::uint16_t {
        Fps60 = 60,
        Fps120 = 120,
        Fps144 = 144,
        Fps165 = 165,
        Fps240 = 240
    };

    enum class PresentationAspect : std::uint8_t {
        Original4x3,
        Wide16x9,
        Ultrawide21x9
    };

    /* Internal render resolution as a multiplier of the GameCube's native
     * framebuffer (640x528).  Higher multipliers sharpen geometry and
     * textures the same way Ship of Harkinian lets the user scale N64
     * resolution.  The image is then scaled to the window with the chosen
     * filter, so no stretching occurs. */
    enum class PresentationResolution : std::uint8_t {
        x1,
        x2,
        x3,
        x4,
        x5
    };

    /* The blit filter applied when compositing the internal FBO onto the
     * window backbuffer. */
    enum class PresentationAntiAliasing : std::uint8_t {
        Off,
        Msaa2x,
        Msaa4x,
        Msaa8x
    };

    enum class PresentationAnisotropy : std::uint8_t {
        Off,
        x2,
        x4,
        x8,
        x16
    };

    enum class PresentationFilter : std::uint8_t {
        Nearest,
        Linear
    };

    enum class PresentationWindowMode : std::uint8_t {
        Windowed,
        Fullscreen,
        Borderless
    };

    bool is_custom_textures_enabled();

    struct VideoSettings {
        PresentationResolution resolution = PresentationResolution::x1;
        PresentationAspect aspect = PresentationAspect::Original4x3;
        PresentationFilter filter = PresentationFilter::Nearest;
        PresentationWindowMode window_mode = PresentationWindowMode::Windowed;
        PresentationRate rate = PresentationRate::Fps60;
        bool show_fps = false;
        PresentationAntiAliasing anti_aliasing = PresentationAntiAliasing::Off;
        PresentationAnisotropy anisotropy = PresentationAnisotropy::Off;
        bool custom_textures = true;
        bool unlock_requested = false;
    };

    /* Returns the integer scale factor for the given resolution enum. */
    [[nodiscard]] constexpr int resolution_scale(PresentationResolution value)
    {
        return static_cast<int>(value) + 1;
    }

    [[nodiscard]] constexpr const char* label(PresentationRate value)
    {
        switch (value) {
        case PresentationRate::Fps60:
            return "60 FPS";
        case PresentationRate::Fps120:
            return "120 FPS";
        case PresentationRate::Fps144:
            return "144 FPS";
        case PresentationRate::Fps165:
            return "165 FPS";
        case PresentationRate::Fps240:
            return "240 FPS";
        }
        return "60 FPS";
    }

    [[nodiscard]] constexpr const char* label(PresentationAspect value)
    {
        switch (value) {
        case PresentationAspect::Original4x3:
            return "4:3";
        case PresentationAspect::Wide16x9:
            return "16:9";
        case PresentationAspect::Ultrawide21x9:
            return "21:9";
        }
        return "4:3";
    }

    [[nodiscard]] constexpr float ratio(PresentationAspect value)
    {
        switch (value) {
        case PresentationAspect::Original4x3:
            return 4.0F / 3.0F;
        case PresentationAspect::Wide16x9:
            return 16.0F / 9.0F;
        case PresentationAspect::Ultrawide21x9:
            return 21.0F / 9.0F;
        }
        return 4.0F / 3.0F;
    }

    [[nodiscard]] constexpr const char* label(PresentationResolution value)
    {
        switch (value) {
        case PresentationResolution::x1:
            return "1x Native";
        case PresentationResolution::x2:
            return "2x";
        case PresentationResolution::x3:
            return "3x";
        case PresentationResolution::x4:
            return "4x";
        case PresentationResolution::x5:
            return "5x";
        }
        return "1x Native";
    }

    [[nodiscard]] constexpr const char* label(PresentationAntiAliasing value)
    {
        switch (value) {
        case PresentationAntiAliasing::Off: return "Off";
        case PresentationAntiAliasing::Msaa2x: return "2x MSAA";
        case PresentationAntiAliasing::Msaa4x: return "4x MSAA";
        case PresentationAntiAliasing::Msaa8x: return "8x MSAA";
        }
        return "Off";
    }

    [[nodiscard]] constexpr const char* label(PresentationAnisotropy value)
    {
        switch (value) {
        case PresentationAnisotropy::Off: return "Off";
        case PresentationAnisotropy::x2: return "2x";
        case PresentationAnisotropy::x4: return "4x";
        case PresentationAnisotropy::x8: return "8x";
        case PresentationAnisotropy::x16: return "16x";
        }
        return "Off";
    }

    [[nodiscard]] constexpr const char* label(PresentationFilter value)
    {
        switch (value) {
        case PresentationFilter::Nearest:
            return "Nearest";
        case PresentationFilter::Linear:
            return "Linear";
        }
        return "Nearest";
    }

    [[nodiscard]] constexpr const char* label(PresentationWindowMode value)
    {
        switch (value) {
        case PresentationWindowMode::Windowed:
            return "Windowed";
        case PresentationWindowMode::Fullscreen:
            return "Fullscreen";
        case PresentationWindowMode::Borderless:
            return "Borderless";
        }
        return "Windowed";
    }

    /* Row order: 0=Resolution, 1=Aspect, 2=Filter, 3=WindowMode, 4=Rate,
     * 5=ShowFPS */
    constexpr void cycle(VideoSettings* settings, std::uint8_t row,
                         int direction)
    {
        if (settings == nullptr || direction == 0) {
            return;
        }
        const int step = direction > 0 ? 1 : -1;
        const auto next = [step](int value, int count) {
            return (value + step + count) % count;
        };
        if (row == 0) {
            settings->resolution = static_cast<PresentationResolution>(
                next(static_cast<int>(settings->resolution), 5));
        } else if (row == 1) {
            settings->aspect = static_cast<PresentationAspect>(
                next(static_cast<int>(settings->aspect), 3));
        } else if (row == 2) {
            settings->filter = static_cast<PresentationFilter>(
                next(static_cast<int>(settings->filter), 2));
        } else if (row == 3) {
            settings->anti_aliasing = static_cast<PresentationAntiAliasing>(
                next(static_cast<int>(settings->anti_aliasing), 4));
        } else if (row == 4) {
            settings->anisotropy = static_cast<PresentationAnisotropy>(
                next(static_cast<int>(settings->anisotropy), 5));
        } else if (row == 5) {
            settings->window_mode = static_cast<PresentationWindowMode>(
                next(static_cast<int>(settings->window_mode), 3));
        } else if (row == 6) {
            constexpr PresentationRate values[] = { PresentationRate::Fps60,
                                                    PresentationRate::Fps120,
                                                    PresentationRate::Fps144,
                                                    PresentationRate::Fps165,
                                                    PresentationRate::Fps240 };
            int index = 0;
            for (int i = 0; i < 5; ++i) {
                if (values[i] == settings->rate) {
                    index = i;
                }
            }
            settings->rate = values[next(index, 5)];
        } else if (row == 7) {
            settings->show_fps = !settings->show_fps;
        }
    }

    /* The name of each menu row, indexed by row number. */
    inline constexpr const char* kVideoMenuRowNames[] = {
        "RESOLUTION", "ASPECT RATIO", "TEXTURE FILTER", "ANTI-ALIASING", "ANISOTROPIC FILTER", "WINDOW MODE", "TARGET RATE", "SHOW FPS"
    };

    /* The current value label of each menu row. */
    inline const char* video_menu_row_value(const VideoSettings& settings,
                                            std::uint8_t row)
    {
        switch (row) {
        case 0:
            return label(settings.resolution);
        case 1:
            return label(settings.aspect);
        case 2:
            return label(settings.filter);
        case 3:
            return label(settings.anti_aliasing);
        case 4:
            return label(settings.anisotropy);
        case 5:
            return label(settings.window_mode);
        case 6:
            return label(settings.rate);
        case 7:
            return settings.show_fps ? "ON" : "OFF";
        default:
            return "";
        }
    }

    /* SDL reads a gamepad axis from -32768 to 32767 with right and down
     * positive. The GameCube stick, like the keyboard's WASD, has right and up
     * positive, so a vertical axis changes sign on the way to PADStatus. */
    [[nodiscard]] constexpr std::int8_t gamepad_axis_x(std::int16_t value)
    {
        return static_cast<std::int8_t>(static_cast<int>(value) / 258);
    }

    [[nodiscard]] constexpr std::int8_t gamepad_axis_y(std::int16_t value)
    {
        return static_cast<std::int8_t>(-static_cast<int>(value) / 258);
    }

    /* Counts presented frames and gives their rate once per period of
     * wall-clock time.  The first frame only starts the count, so a rate
     * covers the frame intervals inside the period. */
    class FrameRateMeter {
    public:
        explicit constexpr FrameRateMeter(std::uint64_t period_nanoseconds)
            : period_nanoseconds_(period_nanoseconds)
        {
        }

        /* Returns true, with the rate in *frames_per_second, on the frame that
         * closes a period. */
        [[nodiscard]] bool add_frame(std::uint64_t now_nanoseconds,
                                     double* frames_per_second)
        {
            if (!started_ || now_nanoseconds < start_nanoseconds_) {
                started_ = true;
                start_nanoseconds_ = now_nanoseconds;
                frames_ = 0;
                return false;
            }
            ++frames_;
            const std::uint64_t elapsed = now_nanoseconds - start_nanoseconds_;
            if (elapsed < period_nanoseconds_) {
                return false;
            }
            *frames_per_second = static_cast<double>(frames_) * 1e9 /
                                 static_cast<double>(elapsed);
            start_nanoseconds_ = now_nanoseconds;
            frames_ = 0;
            return true;
        }

    private:
        std::uint64_t period_nanoseconds_;
        std::uint64_t start_nanoseconds_ = 0;
        std::uint64_t frames_ = 0;
        bool started_ = false;
    };

    [[nodiscard]] inline std::string play_window_title()
    {
        return std::string(kPlayWindowName) + " — " + kPlayWindowControls;
    }

    [[nodiscard]] inline std::string
    play_window_title(double frames_per_second)
    {
        char rate[32];
        std::snprintf(rate, sizeof(rate), "%.1f FPS", frames_per_second);
        return std::string(kPlayWindowName) + " — " + rate + " — " +
               kPlayWindowControls;
    }

} // namespace melee::render

#endif
