/* asSubRecordFunctions.h */

/* Author:  Marty Kraimer Date:    01MAY2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#ifndef INCasSubRecordFunctionsh
#define INCasSubRecordFunctionsh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct subRecord;

epicsShareFunc long epicsShareAPI asSubInit(struct subRecord *precord,int pass);
epicsShareFunc long epicsShareAPI asSubProcess(struct subRecord *precord);
epicsShareFunc void epicsShareAPI asSubRecordFunctionsRegister(void);

#ifdef __cplusplus
}
#endif

#endif /* INCasSubRecordFunctionsh */
