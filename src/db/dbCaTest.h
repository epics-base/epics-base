/* dbCaTest.h */

/* Author:  Marty Kraimer Date:    25FEB2000 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/
#ifndef INCdbTesth
#define INCdbTesth 1

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc long epicsShareAPI dbcar(char *recordname,int level);

#ifdef __cplusplus
}
#endif

#endif /*INCdbTesth */
