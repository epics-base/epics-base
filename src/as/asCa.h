/* asCa.h */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#ifndef INCasCah
#define INCasCah

#include "shareLib.h"
epicsShareFunc void epicsShareAPI asCaStart(void);
epicsShareFunc void epicsShareAPI asCaStop(void);
epicsShareExtern int asCaDebug;

#endif /*INCasCah*/
