#include <vxWorks.h>
#include <intLib.h>
#include <logLib.h>


#define interruptLock intLock
#define interruptUnlock intUnlock
#define interruptIsInterruptContext intContext
#define interruptContextMessage(MESSAGE) \
    (logMsg((char *)(MESSAGE),0,0,0,0,0,0))
