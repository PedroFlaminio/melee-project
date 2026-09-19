#include <melee_host/ax_mixer.h>
#include <melee_host/host.h>
#include <melee_host/memory.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#endif
#include <dolphin/ai.h>
#include <dolphin/ar.h>
#include <dolphin/ax.h>
#include <dolphin/os/OSCache.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The host does not emulate the DSP.  ARAM addresses are offsets into a host
 * buffer, deliberately not process pointers: code which stores an ARAM
 * address must not be able to accidentally dereference it on a 64-bit host,
 * and only ARQ transfers move bytes in or out.  Allocations are deterministic
 * and keep the console's 32-byte DMA alignment contract.
 */
/* The retail console has 16 MiB of ARAM.  The host keeps scene archives and
 * asynchronously-read animation files resident longer than the console's
 * loader, so the two-player Link route needs headroom for PlLkAJ.dat after
 * the common banks are loaded.  ARAM addresses remain 32-bit offsets and the
 * matching build never sees this host-only backing store. */
#define MELEE_HOST_ARAM_SIZE (24U * 1024U * 1024U)

/* ARAM is a stack in the SDK: ARFree releases the most recent block and
 * reports its length, so the lengths are kept in allocation order. */
#define MELEE_HOST_ARAM_BLOCKS 64

static u32 melee_host_aram_next;
static u32 melee_host_aram_blocks[MELEE_HOST_ARAM_BLOCKS];
static u32 melee_host_aram_block_count;
static u32 melee_host_dsp_sample_rate;
static u8 melee_host_stream_volume_left;
static u8 melee_host_stream_volume_right;

u32 ARAlloc(u32 length)
{
    const u32 aligned_length = (length + (ARQ_DMA_ALIGNMENT - 1U)) &
                               ~(ARQ_DMA_ALIGNMENT - 1U);
    const u32 result = melee_host_aram_next;

    if (aligned_length > MELEE_HOST_ARAM_SIZE - result) {
        OSPanic(__FILE__, __LINE__, "host ARAM exhausted");
    }
    if (melee_host_aram_block_count == MELEE_HOST_ARAM_BLOCKS) {
        OSPanic(__FILE__, __LINE__, "host ARAM has no free blocks");
    }
    melee_host_aram_blocks[melee_host_aram_block_count++] = aligned_length;
    melee_host_aram_next += aligned_length;
    return result;
}

u32 ARFree(u32* length)
{
    u32 block;

    if (melee_host_aram_block_count == 0) {
        OSPanic(__FILE__, __LINE__, "ARFree with no ARAM block allocated");
    }
    block = melee_host_aram_blocks[--melee_host_aram_block_count];
    if (length != NULL) {
        *length = block;
    }
    melee_host_aram_next -= block;
    return melee_host_aram_next;
}

u32 ARGetSize(void)
{
    return MELEE_HOST_ARAM_SIZE;
}

/* The SDK's ARInit sets up the block stack and reports where user ARAM
 * begins.  The host keeps its own stack in ARAlloc and ARFree, so this only
 * reports the current top. */
u32 ARInit(u32* stack_index_addr, u32 num_entries)
{
    (void) stack_index_addr;
    (void) num_entries;
    return melee_host_aram_next;
}

/* The host's ARQ queue is the backend scheduler, so there is nothing to set
 * up. */
void ARQInit(void) {}

static u8 melee_host_aram_store[MELEE_HOST_ARAM_SIZE];

const mh_u8* melee_host_aram_bytes(void)
{
    return melee_host_aram_store;
}

mh_u32 melee_host_aram_size(void)
{
    return MELEE_HOST_ARAM_SIZE;
}

static bool melee_host_aram_range_valid(ARQAddress offset, u32 length)
{
    return offset <= MELEE_HOST_ARAM_SIZE &&
           length <= MELEE_HOST_ARAM_SIZE - offset;
}

/* The DMA transfer and the callback of one posted request. */
static void melee_host_arq_complete(void* user_data)
{
    ARQRequest* const request = (ARQRequest*) user_data;

    if (request->type == ARQ_TYPE_MRAM_TO_ARAM) {
        if (!melee_host_aram_range_valid(request->dest, request->length)) {
            OSPanic(__FILE__, __LINE__, "ARQ write past the end of ARAM");
        }
        memcpy(melee_host_aram_store + request->dest,
               (const void*) request->source, request->length);
    } else if (request->type == ARQ_TYPE_ARAM_TO_MRAM) {
        if (!melee_host_aram_range_valid(request->source, request->length)) {
            OSPanic(__FILE__, __LINE__, "ARQ read past the end of ARAM");
        }
        memcpy((void*) request->dest, melee_host_aram_store + request->source,
               request->length);
    } else {
        OSPanic(__FILE__, __LINE__, "ARQ request of unknown type %u",
                request->type);
    }
    if (request->callback != NULL) {
        request->callback(request);
    }
}

/* No audio interface to start: nothing drains the DSP's output yet. */
void AIInit(u8* stack)
{
    (void) stack;
}

void AISetDSPSampleRate(u32 rate)
{
    melee_host_dsp_sample_rate = rate;
}

u32 AIGetDSPSampleRate(void)
{
    return melee_host_dsp_sample_rate;
}

void AISetStreamVolLeft(u8 volume)
{
    melee_host_stream_volume_left = volume;
}

u8 AIGetStreamVolLeft(void)
{
    return melee_host_stream_volume_left;
}

void AISetStreamVolRight(u8 volume)
{
    melee_host_stream_volume_right = volume;
}

u8 AIGetStreamVolRight(void)
{
    return melee_host_stream_volume_right;
}

/* The IPL's sound setting, which the console keeps in SRAM.  A GameCube
 * leaves the factory in stereo, and the host's output is stereo. */
static u32 melee_host_sound_mode = OS_SOUND_MODE_STEREO;

u32 OSGetSoundMode(void)
{
    return melee_host_sound_mode;
}

void OSSetSoundMode(u32 mode)
{
    melee_host_sound_mode = mode == OS_SOUND_MODE_MONO ? OS_SOUND_MODE_MONO
                                                       : OS_SOUND_MODE_STEREO;
}

/* The native host does not expose the GameCube component-video setting. */
u32 OSGetProgressiveMode(void)
{
    return 0;
}

/* CodeWarrior's double-to-u64 helper is emitted as PowerPC assembly in the
 * original runtime.  Native C has the required conversion directly. */
u64 __cvt_dbl_usll(double value)
{
    return value <= 0.0 ? 0U : (u64) value;
}

void DCInvalidateRange(void* address, u32 length)
{
    (void) address;
    (void) length;
}

void DCStoreRange(void* address, u32 length)
{
    (void) address;
    (void) length;
}

void DCStoreRangeNoSync(void* address, u32 length)
{
    (void) address;
    (void) length;
}

/* The host has no CPU cache to write back, like DCStoreRange. */
void DCFlushRange(void* address, u32 length)
{
    (void) address;
    (void) length;
}

BOOL OSGetResetSwitchState(void)
{
    return FALSE;
}

/* The console keeps why it last reset in low memory; zero is a cold power-on,
 * which is the only way the host starts.  gmMainLib_8015FCC0 reads it to
 * decide whether the intro is skipped. */
unsigned long OSGetResetCode(void)
{
    return 0;
}

/* gm_801A4014 resets the console only while the game is resetting, which
 * starts at the reset button; the host has neither the button nor a reset. */
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu)
{
    (void) reset;
    (void) resetCode;
    (void) forceMenu;
    OSPanic(__FILE__, __LINE__, "OSResetSystem is not ported to the host");
}

void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority,
                    ARQAddress source, ARQAddress dest, u32 length,
                    ARQCallback callback)
{
    request->next = NULL;
    request->owner = owner;
    request->type = type;
    request->priority = priority;
    request->source = source;
    request->dest = dest;
    request->length = length;
    request->callback = callback;
    /* The SDK completes a request by DMA interrupt, after ARQPostRequest has
     * returned, and devcom depends on that: it posts the last piece of a
     * transfer and only then unlinks the request that the callback puts back
     * on the free list.  So the host transfers at the next scheduler step, in
     * the order requests were posted.  The SDK's two priority queues and its
     * chunking are not modelled. */
    if (melee_host_dvd_schedule_backend_task(melee_host_arq_complete,
                                             request) != MELEE_HOST_OK)
    {
        OSPanic(__FILE__, __LINE__,
                "an ARAM transfer was posted and no backend is active");
    }
}

void OSReport(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
}

void OSPanic(const char* file, int line, const char* message, ...)
{
    va_list arguments;

    fprintf(stderr, "OS panic at %s:%d: ", file, line);
    va_start(arguments, message);
    vfprintf(stderr, message, arguments);
    va_end(arguments);
    fputc('\n', stderr);
    abort();
}

void __assert(const char* file, u32 line, const char* condition)
{
    fprintf(stderr, "HSD assertion failed at %s:%u: %s\n", file, line,
            condition);
    abort();
}

void HSD_Panic(const char* file, u32 line, const char* message)
{
    fprintf(stderr, "HSD panic at %s:%u: %s\n", file, line, message);
    abort();
}
