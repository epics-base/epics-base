/* dbConvert.h */

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#ifndef INCdbConverth
#define INCdbConverth

#include "shareLib.h"
#include "dbFldTypes.h"
epicsShareExtern long (*dbGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1])
    (DBADDR *paddr, void *pbuffer,long nRequest, long no_elements, long offset);
epicsShareExtern long (*dbPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1])
    (DBADDR *paddr, const void *pbuffer,long nRequest, long no_elements,
    long offset);

#endif /*INCdbConverth*/
