#ifndef GALE01_00CE50
#define GALE01_00CE50

#include <Runtime/platform.h>

/* The original DOL exports these approximations as expf/powf.  On Windows
 * those names are already exported by the Universal CRT, and COFF cannot
 * interpose the executable's definition as ELF can.  Keep the console names
 * for matching builds while giving host callers an unambiguous symbol. */
#if defined(MELEE_HOST)
float melee_host_expf(float);
float melee_host_powf(float, float);
#define expf melee_host_expf
#define powf melee_host_powf
#endif

/* 00CF74 */ s32 powi(s32, s32);
/* 00D008 */ float lb_8000D008(float, float);
/* 00D148 */ s32 lb_8000D148(float, float, float, float, float, float, float);

#endif
