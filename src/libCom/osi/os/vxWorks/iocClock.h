/* iocClock.h */

/* Author:  Marty Kraimer Date:  16JUN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include "tsStamp.h"

typedef int (*ptsStampGetCurrent)(TS_STAMP *pDest);
typedef int (*ptsStampGetEvent)(TS_STAMP *pDest,unsigned eventNumber);

void iocClockInit(void);
void iocClockRegister(ptsStampGetCurrent getCurrent,ptsStampGetEvent getEvent);
