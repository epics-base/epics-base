/* osi/os/vxWorks/osdInterrupt.c */

/* Author:  Marty Kraimer Date:    28JAN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <vxWorks.h>
#include <intLib.h>
#include <logLib.h>

#include "epicsInterrupt.h"

int  epicsInterruptLock() {return(intLock());}

void  epicsInterruptUnlock(int key) {intUnlock(key);}

int  epicsInterruptIsInterruptContext() {return(intContext());}

void  epicsInterruptContextMessage(const char *message)
{
    logMsg((char *)message,0,0,0,0,0,0);
}
