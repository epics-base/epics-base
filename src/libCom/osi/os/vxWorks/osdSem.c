/* os/vxWorks/osiSem.c */

/* Author:  Marty Kraimer Date:    25AUG99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include "osiSem.h"
#include "cantProceed.h"


semBinaryId semBinaryMustCreate(int initialState)
{
    semBinaryId id;
    id = semBinaryCreate(initialState);
    if(!id) cantProceed("semBinaryMustCreate");
    return(id);
}

semMutexId semMutexMustCreate(void)
{
    semMutexId id;
    id = semMutexCreate();
    if(!id) cantProceed("semMutexMustCreate");
    return(id);
}
