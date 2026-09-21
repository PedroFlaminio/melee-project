#include "test.hpp"

#include <cstddef>
#include <cstdio>

/* The checks live in ax_mixer_check.c, which can include the SDK's AX
 * header. */
extern "C" {
int melee_host_test_ax_decode(char* message, std::size_t size);
int melee_host_test_ax_prediction(char* message, std::size_t size);
int melee_host_test_ax_loop(char* message, std::size_t size);
int melee_host_test_ax_loop_ahead(char* message, std::size_t size);
int melee_host_test_ax_resample(char* message, std::size_t size);
int melee_host_test_ax_voices_off(char* message, std::size_t size);
int melee_host_test_ax_steal(char* message, std::size_t size);
int melee_host_test_ax_setters(char* message, std::size_t size);
int melee_host_test_ax_frame(char* message, std::size_t size);
int melee_host_test_ax_reverb(char* message, std::size_t size);
int melee_host_test_ax_aux_return(char* message, std::size_t size);
int melee_host_test_ax_itd_and_surround(char* message, std::size_t size);
}

TEST_CASE("the host mixer decodes DSP ADPCM through the voice's end address")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_decode(message, sizeof(message)) == 1);
}

TEST_CASE("the host mixer predicts from the frame's coefficient pair")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_prediction(message, sizeof(message)) == 1);
}

TEST_CASE("a looping voice returns to its loop address")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_loop(message, sizeof(message)) == 1);
}

TEST_CASE("a voice looped past its end address plays on from the loop")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_loop_ahead(message, sizeof(message)) == 1);
}

TEST_CASE("the host mixer resamples between neighbouring samples")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_resample(message, sizeof(message)) == 1);
}

TEST_CASE("AX voices stay off until the host turns them on")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_voices_off(message, sizeof(message)) == 1);
}

TEST_CASE("a full voice pool gives the oldest lower priority voice away")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_steal(message, sizeof(message)) == 1);
}

TEST_CASE("AX setters keep the SDK's ratio cap and mixer control bits")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_setters(message, sizeof(message)) == 1);
}

TEST_CASE("an AX frame mixes the playing voices, then runs the frame callback")
{
    char message[256] = {};
    REQUIRE(melee_host_test_ax_frame(message, sizeof(message)) == 1);
}

TEST_CASE("the game's reverb returns its impulse once its combs come round")
{
    char message[256] = {};
    const int passed = melee_host_test_ax_reverb(message, sizeof(message));
    if (passed != 1) {
        std::fprintf(stderr, "%s\n", message);
    }
    REQUIRE(passed == 1);
}

TEST_CASE("an aux bus returns what a voice sends one frame later")
{
    char message[256] = {};
    const int passed = melee_host_test_ax_aux_return(message, sizeof(message));
    if (passed != 1) {
        std::fprintf(stderr, "%s\n", message);
    }
    REQUIRE(passed == 1);
}

TEST_CASE("AX interaural delay and surround reaches the stereo host output")
{
    char message[256] = {};
    const int passed = melee_host_test_ax_itd_and_surround(message,
                                                            sizeof(message));
    if (passed != 1) {
        std::fprintf(stderr, "%s\n", message);
    }
    REQUIRE(passed == 1);
}
