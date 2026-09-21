/* Checks the host's AX mixer through the SDK's own AX types, which C++ cannot
 * include (dolphin/os.h is not C++ clean).  Each check returns 1, or 0 with
 * the failed condition in `message`. */

#include <melee_host/ax_mixer.h>

#include <dolphin/ax.h>
#include <dolphin/axfx.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int melee_host_test_ax_decode(char* message, size_t size);
int melee_host_test_ax_prediction(char* message, size_t size);
int melee_host_test_ax_loop(char* message, size_t size);
int melee_host_test_ax_loop_ahead(char* message, size_t size);
int melee_host_test_ax_resample(char* message, size_t size);
int melee_host_test_ax_voices_off(char* message, size_t size);
int melee_host_test_ax_steal(char* message, size_t size);
int melee_host_test_ax_setters(char* message, size_t size);
int melee_host_test_ax_frame(char* message, size_t size);
int melee_host_test_ax_reverb(char* message, size_t size);
int melee_host_test_ax_aux_return(char* message, size_t size);
int melee_host_test_ax_itd_and_surround(char* message, size_t size);

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            snprintf(message, size, "line %d: %s", __LINE__, #condition);     \
            return 0;                                                         \
        }                                                                     \
    } while (0)

/* A DSP ADPCM voice at the output rate and full volume, so each output
 * sample is one decoded sample. */
static AXPB adpcm_voice(u32 current, u32 end)
{
    AXPB pb;

    memset(&pb, 0, sizeof(pb));
    pb.state = 1;
    pb.addr.format = 0;
    pb.addr.currentAddressHi = current >> 16;
    pb.addr.currentAddressLo = current;
    pb.addr.endAddressHi = end >> 16;
    pb.addr.endAddressLo = end;
    pb.src.ratioHi = 1;
    pb.ve.currentVolume = 0x8000;
    pb.mix.vL = 0x8000;
    pb.mix.vR = 0x8000;
    return pb;
}

int melee_host_test_ax_decode(char* message, size_t size)
{
    /* Two frames with zero coefficients, the first at scale 1 and the second
     * at scale 4: each sample is its nibble times the scale, and the frame
     * header at nibble 16 is read, not played. */
    static const u8 aram[16] = { 0x00, 0x01, 0x23, 0x45, 0x67, 0x89,
                                 0xAB, 0xCD, 0x02, 0x7F };
    static const s32 expected[20] = { 0,  1,  2,  3,  4,  5,  6,
                                      7,  -8, -7, -6, -5, -4, -3,
                                      28, -4, 0,  0,  0,  0 };
    s32 left[20] = { 0 };
    s32 right[20] = { 0 };
    AXPB pb = adpcm_voice(2, 19);

    melee_host_ax_render_voice(&pb, aram, sizeof(aram), left, right, 20);
    CHECK(memcmp(left, expected, sizeof(expected)) == 0);
    CHECK(memcmp(right, expected, sizeof(expected)) == 0);
    CHECK(pb.state == 0);
    CHECK(pb.adpcm.pred_scale == 0x02);
    return 1;
}

int melee_host_test_ax_prediction(char* message, size_t size)
{
    /* Pair 1 weighs the previous sample by 1.0 (0x800 in 5.11), so each
     * nibble adds to the running value. */
    static const u8 aram[8] = { 0x10, 0x11, 0x11, 0x11,
                                0xFF, 0xFF, 0x11, 0x11 };
    static const s32 expected[14] = { 1, 2, 3, 4, 5, 6, 5,
                                      4, 3, 2, 3, 4, 5, 6 };
    s32 left[14] = { 0 };
    AXPB pb = adpcm_voice(2, 15);

    /* A voice starts after its first frame header, whose byte the synth's
     * descriptor carries as the starting predictor and scale. */
    pb.adpcm.pred_scale = 0x10;
    pb.adpcm.a[1][0] = 0x0800;
    melee_host_ax_render_voice(&pb, aram, sizeof(aram), left, NULL, 14);
    CHECK(memcmp(left, expected, sizeof(expected)) == 0);
    CHECK((s16) pb.adpcm.yn1 == 6);
    CHECK((s16) pb.adpcm.yn2 == 5);
    return 1;
}

int melee_host_test_ax_loop(char* message, size_t size)
{
    static const u8 aram[8] = { 0x00, 0x12, 0x30 };
    static const s32 expected[10] = { 1, 2, 3, 0, 1, 2, 3, 0, 1, 2 };
    s32 left[10] = { 0 };
    AXPB pb = adpcm_voice(2, 5);

    pb.addr.loopFlag = 1;
    pb.addr.loopAddressLo = 2;
    melee_host_ax_render_voice(&pb, aram, sizeof(aram), left, NULL, 10);
    CHECK(memcmp(left, expected, sizeof(expected)) == 0);
    CHECK(pb.state == 1);
    return 1;
}

int melee_host_test_ax_loop_ahead(char* message, size_t size)
{
    /* A music stream loops into its next chunk, past its end address, and
     * the end address moves only at the next frame callback.  Until then the
     * voice plays on from the loop address instead of returning to it on
     * every sample. */
    static const u8 aram[16] = { 0x00, 0x12, 0x34, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x56, 0x71 };
    static const s32 expected[8] = { 1, 2, 3, 4, 5, 6, 7, 1 };
    s32 left[8] = { 0 };
    AXPB pb = adpcm_voice(2, 5);

    pb.addr.loopFlag = 1;
    pb.addr.loopAddressLo = 18;
    melee_host_ax_render_voice(&pb, aram, sizeof(aram), left, NULL, 8);
    CHECK(memcmp(left, expected, sizeof(expected)) == 0);
    CHECK(pb.state == 1);
    return 1;
}

int melee_host_test_ax_resample(char* message, size_t size)
{
    /* Nibbles 0, 2, 4 and 6 at half speed; the last sample holds for its
     * second half before the voice stops. */
    static const u8 aram[8] = { 0x00, 0x02, 0x46 };
    static const s32 expected[10] = { 0, 1, 2, 3, 4, 5, 6, 6, 0, 0 };
    s32 left[10] = { 0 };
    AXPB pb = adpcm_voice(2, 5);

    pb.src.ratioHi = 0;
    pb.src.ratioLo = 0x8000;
    melee_host_ax_render_voice(&pb, aram, sizeof(aram), left, NULL, 10);
    CHECK(memcmp(left, expected, sizeof(expected)) == 0);
    CHECK(pb.state == 0);
    return 1;
}

int melee_host_test_ax_voices_off(char* message, size_t size)
{
    melee_host_ax_set_voices_enabled(false);
    AXInit();
    CHECK(AXAcquireVoice(10, NULL, 0) == NULL);
    CHECK(melee_host_ax_acquired_voices() == 0);
    return 1;
}

static int dropped_calls;
static AXVPB* dropped_voice;

static void record_drop(void* voice)
{
    dropped_calls++;
    dropped_voice = voice;
}

int melee_host_test_ax_steal(char* message, size_t size)
{
    AXVPB* voices[AX_MAX_VOICES];
    AXVPB* taken;
    u32 i;

    melee_host_ax_set_voices_enabled(true);
    AXInit();
    for (i = 0; i < AX_MAX_VOICES; i++) {
        voices[i] = AXAcquireVoice(5, record_drop, i);
        CHECK(voices[i] != NULL);
        CHECK(voices[i]->index == i);
    }
    CHECK(melee_host_ax_acquired_voices() == AX_MAX_VOICES);
    /* A request no higher than every voice finds nothing to take. */
    CHECK(AXAcquireVoice(5, record_drop, 99) == NULL);

    dropped_calls = 0;
    voices[0]->pb.state = 1;
    taken = AXAcquireVoice(10, NULL, 100);
    CHECK(taken == voices[0]);
    CHECK(dropped_calls == 1);
    CHECK(dropped_voice == voices[0]);
    CHECK(taken->depop == 1);
    CHECK(taken->pb.state == 0);
    CHECK(taken->userContext == 100);

    AXFreeVoice(voices[1]);
    CHECK(melee_host_ax_acquired_voices() == AX_MAX_VOICES - 1);
    CHECK(AXAcquireVoice(3, NULL, 0) == voices[1]);
    for (i = 0; i < AX_MAX_VOICES; i++) {
        AXFreeVoice(voices[i]);
    }
    CHECK(melee_host_ax_acquired_voices() == 0);
    melee_host_ax_set_voices_enabled(false);
    return 1;
}

int melee_host_test_ax_setters(char* message, size_t size)
{
    AXVPB voice;
    AXPBMIX mix;
    AXPBADDR pcm;

    memset(&voice, 0, sizeof(voice));
    AXSetVoiceSrcRatio(&voice, 0.5F);
    CHECK(voice.pb.src.ratioHi == 0);
    CHECK(voice.pb.src.ratioLo == 0x8000);
    AXSetVoiceSrcRatio(&voice, 9.0F);
    CHECK(voice.pb.src.ratioHi == 4);
    CHECK(voice.pb.src.ratioLo == 0);

    memset(&mix, 0, sizeof(mix));
    mix.vL = 0x8000;
    AXSetVoiceMix(&voice, &mix);
    CHECK(voice.pb.mixerCtrl == 0);
    mix.vAuxBL = 1;
    mix.vDeltaR = 2;
    AXSetVoiceMix(&voice, &mix);
    CHECK(voice.pb.mixerCtrl == (2 | 8));

    memset(&pcm, 0, sizeof(pcm));
    pcm.format = 10;
    voice.pb.adpcm.pred_scale = 0x55;
    AXSetVoiceAddr(&voice, &pcm);
    CHECK(voice.pb.adpcm.gain == 0x0800);
    CHECK(voice.pb.adpcm.pred_scale == 0);
    AXSetVoiceItdTarget(&voice, 99, 2);
    CHECK(voice.pb.itd.targetShiftL == 31 && voice.pb.itd.targetShiftR == 2);
    return 1;
}

static int frame_callbacks;

static void count_frame(void)
{
    frame_callbacks++;
}

int melee_host_test_ax_frame(char* message, size_t size)
{
    s16 stereo[MELEE_HOST_AX_FRAME_SAMPLES * 2];
    u32 i;

    melee_host_ax_set_voices_enabled(false);
    AXInit();
    AXRegisterCallback(count_frame);
    frame_callbacks = 0;
    for (i = 0; i < MELEE_HOST_AX_FRAME_SAMPLES * 2; i++) {
        stereo[i] = 7;
    }
    melee_host_ax_run_frame(stereo);
    CHECK(frame_callbacks == 1);
    for (i = 0; i < MELEE_HOST_AX_FRAME_SAMPLES * 2; i++) {
        CHECK(stereo[i] == 0);
    }
    AXRegisterCallback(NULL);
    return 1;
}

static void* reverb_alloc(unsigned long size)
{
    return malloc(size);
}

static void reverb_free(void* block)
{
    free(block);
}

/* The left channel of 20 frames of the game's standard reverb after an
 * impulse, or 0 when a frame breaks the expectations below. */
static int run_reverb(s32 left[3200], char* message, size_t size)
{
    struct AXFX_REVERBSTD reverb;
    s32 buffer[480];
    struct AXFX_BUFFERUPDATE update;
    u32 frame;
    u32 i;

    memset(&reverb, 0, sizeof(reverb));
    /* AXDriver_8038E37C's defaults, with lbAudioAx_8002838C's time. */
    reverb.tempDisableFX = 0;
    reverb.time = 1.88F;
    reverb.preDelay = 0.002F;
    reverb.damping = 0.64F;
    reverb.coloration = 0.5F;
    reverb.mix = 1.0F;
    AXFXSetHooks(reverb_alloc, reverb_free);
    CHECK(AXFXReverbStdInit(&reverb) == 1);
    update.left = buffer;
    update.right = buffer + 160;
    update.surround = buffer + 320;
    for (frame = 0; frame < 20; frame++) {
        memset(buffer, 0, sizeof(buffer));
        if (frame == 0) {
            buffer[0] = 16384;
        }
        AXFXReverbStdCallback(&update, &reverb);
        for (i = 0; i < 160; i++) {
            left[frame * 160 + i] = buffer[i];
            CHECK(buffer[160 + i] == 0 && buffer[320 + i] == 0);
            CHECK(buffer[i] < (1 << 20) && buffer[i] > -(1 << 20));
        }
    }
    CHECK(AXFXReverbStdShutdown(&reverb) == 1);
    return 1;
}

int melee_host_test_ax_reverb(char* message, size_t size)
{
    static s32 first[3200];
    static s32 second[3200];
    int heard = 0;
    u32 i;

    if (!run_reverb(first, message, size) ||
        !run_reverb(second, message, size))
    {
        return 0;
    }
    /* A fully wet reverb is silent until its shortest comb returns what the
     * pre-delay line (63 samples) handed it 1789 samples before. */
    for (i = 0; i < 1850; i++) {
        CHECK(first[i] == 0);
    }
    for (i = 1850; i < 1900; i++) {
        heard |= first[i] != 0;
    }
    CHECK(heard);
    CHECK(memcmp(first, second, sizeof(first)) == 0);
    return 1;
}

static u32 aux_callbacks;
static s32 aux_send[MELEE_HOST_AX_FRAME_SAMPLES];

/* Leaves the send as it is, so the return is the send, and keeps a copy of
 * its left channel. */
static void record_aux(void* data, void* context)
{
    const struct AX_AUX_DATA* const aux = data;
    (void) context;
    aux_callbacks += 1;
    memcpy(aux_send, aux->l, sizeof(aux_send));
}

int melee_host_test_ax_aux_return(char* message, size_t size)
{
    s16 stereo[MELEE_HOST_AX_FRAME_SAMPLES * 2];
    AXPBADDR addr;
    AXPBADPCM adpcm;
    AXPBSRC src;
    AXPBVE ve;
    AXPBMIX mix;
    AXVPB* voice;
    s32 returned[MELEE_HOST_AX_FRAME_SAMPLES];
    u32 i;

    melee_host_ax_set_voices_enabled(true);
    melee_host_ax_set_aux_enabled(true);
    AXInit();
    AXRegisterAuxACallback(record_aux, NULL);
    aux_callbacks = 0;
    voice = AXAcquireVoice(15, NULL, 0);
    CHECK(voice != NULL);
    /* ARAM's last bytes, which no allocation reaches and no check writes, and
     * a predictor of 1.0 over a history of 1000: every sample is 1000, sent
     * to aux A's left at full volume and nowhere else. */
    {
        const u32 start = (melee_host_aram_size() - 0x100U) * 2U + 2U;
        /* Room for both frames the check plays. */
        const u32 end = start + 0x400U;
        memset(&addr, 0, sizeof(addr));
        addr.currentAddressHi = (u16) (start >> 16);
        addr.currentAddressLo = (u16) start;
        addr.endAddressHi = (u16) (end >> 16);
        addr.endAddressLo = (u16) end;
    }
    memset(&adpcm, 0, sizeof(adpcm));
    adpcm.a[0][0] = 2048;
    adpcm.yn1 = 1000;
    memset(&src, 0, sizeof(src));
    src.ratioHi = 1;
    memset(&ve, 0, sizeof(ve));
    ve.currentVolume = 0x8000;
    memset(&mix, 0, sizeof(mix));
    mix.vAuxAL = 0x8000;
    AXSetVoiceAddr(voice, &addr);
    AXSetVoiceAdpcm(voice, &adpcm);
    AXSetVoiceSrc(voice, &src);
    AXSetVoiceVe(voice, &ve);
    AXSetVoiceMix(voice, &mix);
    AXSetVoiceState(voice, 1);

    /* The send goes to the callback; nothing reaches the output yet. */
    melee_host_ax_run_frame(stereo);
    CHECK(aux_callbacks == 1);
    for (i = 0; i < MELEE_HOST_AX_FRAME_SAMPLES * 2; i++) {
        CHECK(stereo[i] == 0);
    }
    for (i = 0; i < MELEE_HOST_AX_FRAME_SAMPLES; i++) {
        CHECK(aux_send[i] == 1000);
        returned[i] = aux_send[i];
    }
    /* A frame later that return is the left channel. */
    melee_host_ax_run_frame(stereo);
    CHECK(aux_callbacks == 2);
    for (i = 0; i < MELEE_HOST_AX_FRAME_SAMPLES; i++) {
        CHECK(stereo[i * 2] == returned[i] && stereo[i * 2 + 1] == 0);
    }
    /* With the buses off the voice sends nothing and the callback waits. */
    melee_host_ax_set_aux_enabled(false);
    melee_host_ax_run_frame(stereo);
    CHECK(aux_callbacks == 2);
    for (i = 0; i < MELEE_HOST_AX_FRAME_SAMPLES * 2; i++) {
        CHECK(stereo[i] == 0);
    }
    melee_host_ax_set_aux_enabled(true);
    AXRegisterAuxACallback(NULL, NULL);
    AXFreeVoice(voice);
    melee_host_ax_set_voices_enabled(false);
    AXInit();
    return 1;
}

/* A short PCM8 voice at the end of ARAM, away from the allocations that the
 * rest of the test binary uses.  PCM8 makes the delay and phase relationship
 * easy to read: 1, 2, 3 ... become 256, 512, 768 ... at full gain. */
static AXVPB* pcm8_test_voice(void)
{
    AXPBADDR address;
    AXPBSRC source;
    AXPBVE envelope;
    u8* aram = (u8*) (uintptr_t) melee_host_aram_bytes();
    const u32 start = melee_host_aram_size() - 0x80U;
    AXVPB* voice;

    aram[start] = 1;
    aram[start + 1] = 2;
    aram[start + 2] = 3;
    aram[start + 3] = 4;
    voice = AXAcquireVoice(15, NULL, 0);
    if (voice == NULL) {
        return NULL;
    }
    memset(&address, 0, sizeof(address));
    address.format = 25;
    address.currentAddressHi = (u16) (start >> 16);
    address.currentAddressLo = (u16) start;
    address.endAddressHi = (u16) ((start + 3U) >> 16);
    address.endAddressLo = (u16) (start + 3U);
    memset(&source, 0, sizeof(source));
    source.ratioHi = 1;
    memset(&envelope, 0, sizeof(envelope));
    envelope.currentVolume = 0x8000;
    AXSetVoiceAddr(voice, &address);
    AXSetVoiceSrc(voice, &source);
    AXSetVoiceVe(voice, &envelope);
    AXSetVoiceState(voice, 1);
    return voice;
}

int melee_host_test_ax_itd_and_surround(char* message, size_t size)
{
    AXPBMIX mix;
    AXVPB* voice;
    s16 stereo[MELEE_HOST_AX_FRAME_SAMPLES * 2];

    melee_host_ax_set_voices_enabled(true);
    AXInit();
    voice = pcm8_test_voice();
    CHECK(voice != NULL);
    memset(&mix, 0, sizeof(mix));
    mix.vL = 0x8000;
    mix.vR = 0x8000;
    AXSetVoiceMix(voice, &mix);
    AXSetVoiceItdOn(voice);
    AXSetVoiceItdTarget(voice, 2, 0);
    melee_host_ax_run_frame(stereo);
    /* Left approaches its two-sample target while right remains immediate. */
    CHECK(stereo[0] == 0 && stereo[1] == 256);
    CHECK(stereo[2] == 0 && stereo[3] == 512);
    CHECK(stereo[4] == 256 && stereo[5] == 768);
    CHECK(voice->pb.itd.shiftL == 2 && voice->pb.itd.shiftR == 0);
    AXFreeVoice(voice);

    AXInit();
    voice = pcm8_test_voice();
    CHECK(voice != NULL);
    memset(&mix, 0, sizeof(mix));
    mix.vS = 0x8000;
    AXSetVoiceMix(voice, &mix);
    melee_host_ax_run_frame(stereo);
    /* Stereo sinks carry the third AX channel in the standard Lt/Rt phase
     * pair, so a surround-only source remains audible and decodable. */
    CHECK(stereo[0] == 256 && stereo[1] == -256);
    CHECK(stereo[2] == 512 && stereo[3] == -512);
    AXFreeVoice(voice);
    melee_host_ax_set_voices_enabled(false);
    AXInit();
    return 1;
}
