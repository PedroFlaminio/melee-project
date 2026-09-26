#include <string.h>
#include "memory.h"

#include <Runtime/platform.h>

#ifdef MELEE_HOST
#include <melee_host/memory.h>
#else
#include "debug.h"
#include "initialize.h"
#include <dolphin/os/OSAlloc.h>
#endif

void HSD_Free(void* ptr)
{
#ifdef MELEE_HOST
    melee_host_aligned_free(ptr, 32);
#else
    OSFreeToHeap(HSD_GetHeap(), ptr);
#endif
}

void* HSD_MemAlloc(ssize_t size)
{
    if (size <= 0) {
        return NULL;
    }

#ifdef MELEE_HOST
    {
        /* The console carves these out of an arena the boot leaves zeroed,
         * and code reads fields it never wrote (a new JObj's matrix, for
         * one) as zero; fresh host memory holds whatever was there. */
        void* adr = melee_host_aligned_alloc((size_t) size, 32);
        if (adr != NULL) {
            memset(adr, 0, (size_t) size);
        }
        return adr;
    }
#else
    void* adr;
    adr = OSAllocFromHeap(HSD_GetHeap(), size);
    HSD_ASSERT(52, adr);

    return adr;
#endif
}
