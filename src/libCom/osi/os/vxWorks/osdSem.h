/* os/vxWorks/osdSem.h */

/* Author:  Marty Kraimer Date:    25AUG99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <vxWorks.h>
#include <semLib.h>

/* If the macro is replaced by inline it is necessary to say
   static __inline__
   but then a warning message appears everywhere osdSem.h is included
*/

#define semBinaryGive(ID) semGive((SEM_ID)(ID))

#define semBinaryTake(ID) \
(semTake((SEM_ID)(ID),WAIT_FOREVER)==OK ? semTakeOK : semTakeError)

#define semMutexGive(ID) semGive((SEM_ID)(ID))

#define semMutexTake(ID) \
(semTake((SEM_ID)(ID),WAIT_FOREVER)==OK ? semTakeOK : semTakeError)
