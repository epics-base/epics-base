/* osi/os/vxWorks/osdInterrupt.h */

/* Author:  Marty Kraimer Date:    28JAN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <vxWorks.h>
#include <intLib.h>
#include <logLib.h>

INLINE int  interruptLock() {return(intLock());}

INLINE void  interruptUnlock(int key) {intUnlock(key);}

INLINE int  interruptIsInterruptContext() {return(intContext());}

INLINE void  interruptContextMessage(const char *message)
{
    logMsg((char *)message,0,0,0,0,0,0);
}
