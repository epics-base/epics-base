/* iocClock.h */

/* Author:  Marty Kraimer Date:  16JUN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include "epicsTime.h"

typedef int (*pepicsTimeGetCurrent)(epicsTimeStamp *pDest);
typedef int (*pepicsTimeGetEvent)(epicsTimeStamp *pDest,int eventNumber);

void iocClockInit(void);
void iocClockRegister(pepicsTimeGetCurrent getCurrent,pepicsTimeGetEvent getEvent);
