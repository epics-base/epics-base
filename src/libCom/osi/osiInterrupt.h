#ifndef osiInterrupth
#define osiInterrupth

/*THIS MAY BE A BIG PROBLEM */

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc int epicsShareAPI interruptLock(void);
epicsShareFunc void epicsShareAPI interruptUnlock(int key);
epicsShareFunc int epicsShareAPI interruptIsInterruptContext(void);
epicsShareFunc void epicsShareAPI interruptContextMessage(const char *message);

#ifdef __cplusplus
}
#endif

#include "osdInterrupt.h"

#endif /* osiInterrupth */
