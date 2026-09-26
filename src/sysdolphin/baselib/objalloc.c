#include "objalloc.h"

#include <string.h>

#include "memory.h"
#ifndef MELEE_HOST
#include "initialize.h"
#include <dolphin/os/OSAlloc.h>
#endif

#ifdef MELEE_HOST
typedef uintptr_t ObjHeapAddress;
typedef size_t ObjHeapSize;
#else
typedef u32 ObjHeapAddress;
typedef u32 ObjHeapSize;
#endif

static objheap obj_heap = { 0, 0, -1, -1 };

#if defined(MELEE_HOST) && defined(__has_feature)
#if __has_feature(address_sanitizer)
#define MELEE_HOST_OBJALLOC_CAN_MALLOC 1
#endif
#endif
#if defined(MELEE_HOST) && defined(__SANITIZE_ADDRESS__)
#define MELEE_HOST_OBJALLOC_CAN_MALLOC 1
#endif

#ifdef MELEE_HOST_OBJALLOC_CAN_MALLOC
#include <sanitizer/asan_interface.h>
#include <stdlib.h>
/* MELEE_HOST_OBJALLOC_POISON=1 keeps the pools but marks a free object's
 * bytes unaddressable, so a write through a stale pointer is reported where
 * it happens, not as a corrupt free list later. */
static bool objalloc_poison(void)
{
    static int mode = -1;
    if (mode < 0) {
        const char* value = getenv("MELEE_HOST_OBJALLOC_POISON");
        mode = value != NULL && value[0] == '1';
    }
    return mode != 0;
}
#define OBJ_POISON(data, obj)                                                 \
    do {                                                                      \
        if (objalloc_poison()) {                                              \
            ASAN_POISON_MEMORY_REGION((obj), (data)->size);                   \
        }                                                                     \
    } while (0)
#define OBJ_UNPOISON(data, obj)                                               \
    do {                                                                      \
        if (objalloc_poison()) {                                              \
            ASAN_UNPOISON_MEMORY_REGION((obj), (data)->size);                 \
        }                                                                     \
    } while (0)
#else
#define OBJ_POISON(data, obj) ((void) 0)
#define OBJ_UNPOISON(data, obj) ((void) 0)
#endif
#ifdef MELEE_HOST_OBJALLOC_CAN_MALLOC
/* MELEE_HOST_OBJALLOC_MALLOC=1, in a sanitized build, gives every object
 * its own malloc block instead of a slot in a pool, so AddressSanitizer
 * sees a write past an object or into one already freed, which inside a
 * pool only shows up later as a corrupt free list. */
static bool objalloc_malloc(void)
{
    static int mode = -1;
    if (mode < 0) {
        const char* value = getenv("MELEE_HOST_OBJALLOC_MALLOC");
        mode = value != NULL && value[0] == '1';
    }
    return mode != 0;
}
#endif

static HSD_ObjAllocData* alloc_datas;

void HSD_ObjSetHeap(u32 size, void* ptr)
{
    obj_heap.curr = (ObjHeapAddress) ptr;
    obj_heap.top = (ObjHeapAddress) ptr;
    obj_heap.remain = size;
    obj_heap.size = size;
}

s32 HSD_ObjAllocAddFree(HSD_ObjAllocData* data, u32 num)
{
    ObjHeapAddress computed_start;
    ObjHeapAddress pool_end;
    ObjHeapSize pool_size;
    u8* pool_start;

    u8 _[4];

    HSD_ASSERT(0xEE, data);
    pool_size = data->size * num;
    if (obj_heap.top != 0) {
        pool_end = obj_heap.top + obj_heap.size;
        computed_start = (obj_heap.curr + data->align) & ~data->align;
        pool_start = (void*) computed_start;
        if (computed_start > pool_end) {
            return 0;
        }
        if (pool_end - (ObjHeapAddress) pool_start < pool_size) {
            pool_size = pool_end - (ObjHeapAddress) pool_start -
                        (pool_end - (ObjHeapAddress) pool_start) % data->size;
        }
        num = pool_size / data->size;
        if (num == 0) {
            return 0;
        }
        obj_heap.curr = (ObjHeapAddress) pool_start + pool_size;
        obj_heap.remain = pool_end - obj_heap.curr;
    } else {
        pool_start = HSD_MemAlloc(pool_size);
        if (pool_start == 0) {
            return 0;
        }
        obj_heap.remain -= pool_size;
    }

    {
        int i;
        for (i = 0; (unsigned) i < num - 1; i++) {
            *(void**) (pool_start + data->size * i) =
                (void*) (pool_start + data->size * (i + 1));
        }
        *(void**) (pool_start + data->size * i) = data->freehead;
        for (i = 0; (unsigned) i < num; i++) {
            OBJ_POISON(data, pool_start + data->size * i);
        }
    }

    data->freehead = (HSD_ObjAllocLink*) pool_start;
    data->free += num;
    return num;
}

void* HSD_ObjAlloc(HSD_ObjAllocData* data)
{
    HSD_ObjAllocLink* cur;
    u32 size;

    if (data->num_limit_flag && data->used >= data->num_limit) {
        return NULL;
    }
    if (data->heap_limit_flag) {
        if (data->heap_limit_num == (unsigned) -1) {
            if (obj_heap.top != 0) {
                size = obj_heap.remain;
            } else {
#ifdef MELEE_HOST
                size = U32_MAX;
#else
                size = OSCheckHeap(HSD_GetHeap());
#endif
            }
            if (size <= data->heap_limit_size) {
                data->heap_limit_num = data->used + data->free;
            }
        } else {
            if (obj_heap.top != 0) {
                size = obj_heap.remain;
            } else {
#ifdef MELEE_HOST
                size = U32_MAX;
#else
                size = OSCheckHeap(HSD_GetHeap());
#endif
            }
            if (size > data->heap_limit_size) {
                data->heap_limit_num = -1;
            }
        }
        if (data->used >= data->heap_limit_num) {
            return NULL;
        }
    }
#ifdef MELEE_HOST_OBJALLOC_CAN_MALLOC
    if (objalloc_malloc()) {
        data->used += 1;
        if (data->used > data->peak) {
            data->peak = data->used;
        }
        return malloc(data->size);
    }
#endif
    if (data->free == 0) {
        HSD_ObjAllocAddFree(data, 1);
        if (data->free == 0) {
            return NULL;
        }
    }
    cur = data->freehead;
    OBJ_UNPOISON(data, cur);
    data->freehead = cur->next;
    data->used += 1;
    data->free -= 1;
    if (data->used > data->peak) {
        data->peak = data->used;
    }
    return cur;
}

void HSD_ObjFree(HSD_ObjAllocData* data, void* obj)
{
    HSD_ObjAllocLink* link = obj;
#ifdef MELEE_HOST_OBJALLOC_CAN_MALLOC
    if (objalloc_malloc()) {
        free(obj);
        data->used -= 1;
        return;
    }
#endif
    link->next = data->freehead;
    data->freehead = link;
    OBJ_POISON(data, link);
    data->free += 1;
    data->used -= 1;
}

static inline void removeAll(HSD_ObjAllocData* data)
{
    HSD_ObjAllocData** cur = &alloc_datas;
    while (*cur != NULL) {
        if (*cur == data) {
            *cur = (*cur)->next;
        } else {
            cur = &(*cur)->next;
        }
    }
}

void HSD_ObjAllocInit(HSD_ObjAllocData* data, size_t size, u32 align)
{
    HSD_ASSERT(0x185, data);
    if (data != NULL) {
        removeAll(data);
    } else {
        alloc_datas = NULL;
    }
    memset(data, 0, sizeof(HSD_ObjAllocData));
    data->num_limit = -1;
    data->heap_limit_size = 0;
    data->heap_limit_num = -1;
    data->align = align - 1;
    data->size = (size + data->align) & ~data->align;
    data->next = alloc_datas;
    alloc_datas = data;
}

void _HSD_ObjAllocForgetMemory(void* low, void* high)
{
    alloc_datas = NULL;
}
