/* ioccrfRegisterCommon.h */
/* Author:  Marty Kraimer Date: 27APR2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#ifndef INCioccrfRegisterCommonH
#define INCioccrfRegisterCommonH

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* register many useful commands */
epicsShareFunc void epicsShareAPI ioccrfRegisterCommon(void);

#ifdef __cplusplus
}
#endif

#endif /*INCioccrfRegisterCommonH*/
