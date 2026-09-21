#include <cstdint>
#include <string>

#include "render/play_window.hpp"
#include "test.hpp"

TEST_CASE("the play window's gamepad axes keep the GameCube's up positive")
{
    using melee::render::gamepad_axis_x;
    using melee::render::gamepad_axis_y;
    REQUIRE(gamepad_axis_x(0) == 0);
    REQUIRE(gamepad_axis_x(32767) == 127);
    REQUIRE(gamepad_axis_x(-32768) == -127);
    /* SDL gives up as negative. */
    REQUIRE(gamepad_axis_y(-32768) == 127);
    REQUIRE(gamepad_axis_y(32767) == -127);
    REQUIRE(gamepad_axis_y(0) == 0);
    REQUIRE(gamepad_axis_y(-2580) == 10);
    REQUIRE(gamepad_axis_y(2580) == -10);
}

TEST_CASE("the play window's frame rate meter reports once per period")
{
    constexpr std::uint64_t kSecond = 1'000'000'000ULL;
    melee::render::FrameRateMeter meter(kSecond / 2);
    double rate = -1.0;
    /* 60 Hz for one second: the first frame starts the count, and the frames
     * at 0.5 s and 1 s close a period each. */
    int reports = 0;
    for (std::uint64_t frame = 0; frame <= 60; ++frame) {
        if (meter.add_frame(frame * kSecond / 60, &rate)) {
            ++reports;
            REQUIRE(rate > 59.99);
            REQUIRE(rate < 60.01);
        }
    }
    REQUIRE(reports == 2);

    /* Half the rate over the next period. */
    reports = 0;
    for (std::uint64_t frame = 1; frame <= 15; ++frame) {
        if (meter.add_frame(kSecond + frame * kSecond / 30, &rate)) {
            ++reports;
        }
    }
    REQUIRE(reports == 1);
    REQUIRE(rate > 29.99);
    REQUIRE(rate < 30.01);

    /* A clock that goes back starts the count again. */
    REQUIRE(!meter.add_frame(0, &rate));
    REQUIRE(!meter.add_frame(kSecond / 4, &rate));
    REQUIRE(meter.add_frame(kSecond / 2, &rate));
    REQUIRE(rate > 3.99);
    REQUIRE(rate < 4.01);
}

TEST_CASE("the play window's title carries the frame rate")
{
    REQUIRE(melee::render::play_window_title() ==
            "Melee PC — Enter is START, WASD is stick; Esc opens Video");
    REQUIRE(melee::render::play_window_title(59.94) ==
            "Melee PC — 59.9 FPS — Enter is START, WASD is stick; Esc opens "
            "Video");
}

TEST_CASE("video settings only cycle through supported presentation choices")
{
    melee::render::VideoSettings settings{};
    /* Row 0 is now resolution. */
    melee::render::cycle(&settings, 0, 1);
    REQUIRE(std::string(melee::render::label(settings.resolution)) == "2x");
    melee::render::cycle(&settings, 0, -1);
    REQUIRE(std::string(melee::render::label(settings.resolution)) ==
            "1x Native");
    /* Row 1 is aspect. */
    melee::render::cycle(&settings, 1, 1);
    REQUIRE(std::string(melee::render::label(settings.aspect)) == "16:9");
    melee::render::cycle(&settings, 1, 1);
    REQUIRE(std::string(melee::render::label(settings.aspect)) == "21:9");
    /* Row 2 is filter. */
    melee::render::cycle(&settings, 2, 1);
    REQUIRE(std::string(melee::render::label(settings.filter)) == "Linear");
    melee::render::cycle(&settings, 2, 1);
    REQUIRE(std::string(melee::render::label(settings.filter)) == "Nearest");
    /* Row 5 is window mode. */
    melee::render::cycle(&settings, 5, 1);
    REQUIRE(std::string(melee::render::label(settings.window_mode)) ==
            "Fullscreen");
    /* Row 6 is rate. */
    melee::render::cycle(&settings, 6, -1);
    REQUIRE(settings.rate == melee::render::PresentationRate::Fps240);
    melee::render::cycle(&settings, 6, 1);
    REQUIRE(settings.rate == melee::render::PresentationRate::Fps60);
}

TEST_CASE("video aspect ratios match the presentation menu")
{
    using melee::render::PresentationAspect;
    using melee::render::ratio;
    REQUIRE(ratio(PresentationAspect::Original4x3) == 4.0F / 3.0F);
    REQUIRE(ratio(PresentationAspect::Wide16x9) == 16.0F / 9.0F);
    REQUIRE(ratio(PresentationAspect::Ultrawide21x9) == 21.0F / 9.0F);
}
