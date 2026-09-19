/* Game functions the host links against but does not run yet.
 *
 * Each one is reachable from code that is ported, so the link needs it, but
 * the path that would call it is not part of what the host executes.  Rather
 * than return something plausible, each stops with its own name, so the day
 * the path is taken the report says which module has to come next.
 */

#include <melee/gm/gm_16F1.h>
#include <melee/gm/gmresultplayer.h>
#include <melee/gm/gmtoulib.h>
#include <melee/gr/grdatfiles.h>
#include <melee/mn/mncharsel.h>
#include <melee/mn/mncount.h>
#include <melee/mn/mndatadel.h>
#include <melee/mn/mndeflicker.h>
#include <melee/mn/mndiagram.h>
#include <melee/mn/mnevent.h>
#include <melee/mn/mnhyaku.h>
#include <melee/mn/mninfo.h>
#include <melee/mn/mninfobonus.h>
#include <melee/mn/mnlanguage.h>
#include <melee/mn/mnmainrule.h>
#include <melee/mn/mnsound.h>
#include <melee/mn/mnsoundtest.h>
#include <melee/mn/mnvibration.h>
#include <sysdolphin/baselib/hsd_3982.h>
#include <sysdolphin/baselib/leak.h>
#include <sysdolphin/baselib/particle.h>

#include <dolphin/os.h>
#include <dolphin/os/OSThread.h>
#include <dolphin/thp/thp.h>

#define MELEE_HOST_UNPORTED(name)                                             \
    OSPanic(__FILE__, __LINE__, "%s is not ported to the host yet", name)

/* lb_8001B14C lists the save files on a mounted memory card and keeps the ones
 * whose company and game code match the disc's.  The host never mounts a card,
 * and it does not read the disc header yet. */
struct DVDDiskID* DVDGetCurrentDiskID(void)
{
    MELEE_HOST_UNPORTED("DVDGetCurrentDiskID");
    return NULL;
}

/* gm_801A4D34 runs gm_801A4970, the debug pause and report handler, and checks
 * the thread list only at the debug levels, which the host does not select.
 * These are what the report and the check call. */
int HSD_Leak_80387DF8(int arg0)
{
    (void) arg0;
    MELEE_HOST_UNPORTED("HSD_Leak_80387DF8");
    return 0;
}

HSD_GObj* hsd_80398310(u16 arg0, u8 arg1, u8 arg2, u32 arg3)
{
    (void) arg0;
    (void) arg1;
    (void) arg2;
    (void) arg3;
    MELEE_HOST_UNPORTED("hsd_80398310");
    return NULL;
}

long OSCheckActiveThreads(void)
{
    MELEE_HOST_UNPORTED("OSCheckActiveThreads");
    return 0;
}

/* THP video, which lbmthp.c decodes for the movies the menus can play.  The
 * host has no THP decoder yet. */
void THPInit(void)
{
    MELEE_HOST_UNPORTED("THPInit");
}

s32 THPDec_8032FD40(THPDec_8032FD40_Data* arg0, u16 arg1)
{
    (void) arg0;
    (void) arg1;
    MELEE_HOST_UNPORTED("THPDec_8032FD40");
    return 0;
}

void THPDec_80331340(s32 arg0, void* arg1, void* arg2, void* arg3)
{
    (void) arg0;
    (void) arg1;
    (void) arg2;
    (void) arg3;
    MELEE_HOST_UNPORTED("THPDec_80331340");
}

void THPDec_803313D0(s32 arg0, void* arg1, void* arg2, void* arg3, u32 arg4)
{
    (void) arg0;
    (void) arg1;
    (void) arg2;
    (void) arg3;
    (void) arg4;
    MELEE_HOST_UNPORTED("THPDec_803313D0");
}

s32 THPVideoDecode(void* file, void* tileY, void* tileU, void* tileV,
                   void* work)
{
    (void) file;
    (void) tileY;
    (void) tileU;
    (void) tileV;
    (void) work;
    MELEE_HOST_UNPORTED("THPVideoDecode");
    return 0;
}

/* The screens the menus open other than VS Melee.  mnmain.c's menu table and
 * the name entry screens name each one's entry, so the link needs them.  The
 * host runs the main menu and the VS submenu up to Melee, and stops here when
 * another screen is chosen. */
void gm_80190FE4(int arg0)
{
    (void) arg0;
    MELEE_HOST_UNPORTED("gm_80190FE4");
}

/* Rule, item-switch and tournament submenus are reachable from the CSS menu
 * but are outside the first local-VS path. */
void mnItemSw_802358C0(void)
{
    MELEE_HOST_UNPORTED("mnItemSw_802358C0");
}

void mn_802339FC(void)
{
    MELEE_HOST_UNPORTED("mn_802339FC");
}

void gm_80190EA4(void)
{
    MELEE_HOST_UNPORTED("gm_80190EA4");
}
