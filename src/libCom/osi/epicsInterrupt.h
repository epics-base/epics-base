#ifndef epicsInterrupth
#define epicsInterrupth

/*THIS MAY BE A BIG PROBLEM */

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc int epicsShareAPI epicsInterruptLock(void);
epicsShareFunc void epicsShareAPI epicsInterruptUnlock(int key);
epicsShareFunc int epicsShareAPI epicsInterruptIsInterruptContext(void);
epicsShareFunc void epicsShareAPI epicsInterruptContextMessage(const char *message);

#ifdef __cplusplus
}
#endif

#include "osdInterrupt.h"

#endif /* epicsInterrupth */
