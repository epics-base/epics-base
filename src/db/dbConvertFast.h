/* dbConvertFast.h */

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#ifndef INCdbConvertFasth
#define INCdbConvertFasth

#include "shareLib.h"
#include "dbFldTypes.h"

epicsShareExtern long (*dbFastGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1])();
epicsShareExtern long (*dbFastPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1])();

#endif /*INCdbConvertFasth*/
