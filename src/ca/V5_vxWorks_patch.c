
static char *sccsId = "@(#)V5_vxWorks_patch.c	1.2\t7/27/92";

#include <taskLib.h>
#include <taskVarLib.h>

/*******************************************************************************
*
* joh 2-3-92
* this a modified version of the WRS orig which
* allows tasks variables to be fetched from a 
* task delete hook.
*
*
*
*
* taskVarGetPatch - get the value of a task variable
*
* This routine returns the private value of the task variable for a
* specific task.  The specified task is usually not the calling task,
* which can get its private value by directly accessing the variable.
* This routine is provided primarily for debugging purposes.
*
* RETURNS:
* The private value of the task variable, or
* ERROR if the specified task is not found or it
* does not own the task variable.
*
* SEE ALSO: taskVarAdd(2), taskVarDelete(2), taskVarSet(2)
*/

int taskVarGetPatch (tid, pVar)
    int tid;		/* task id whose task variable is to be retrieved */
    int *pVar;		/* pointer to task variable */
			 
    {
    FAST TASK_VAR *pTaskVar;		/* ptr to next node */
    WIND_TCB *pTcb;
/* 
 * joh 020392
 * dont call taskTcb() here and bypass error check
 * which keeps this from working in a task delete hook
 */
    pTcb = (WIND_TCB *) tid;

    if (pTcb == NULL)
	return (ERROR);

    /* find descriptor for specified task variable */

    for (pTaskVar = pTcb->pTaskVar;
	 pTaskVar != NULL;
	 pTaskVar = pTaskVar->next)
	{
	if (pTaskVar->address == pVar)
	    return (taskIdCurrent == pTcb ? *pVar : pTaskVar->value);
	}


    /* specified address is not a task variable for specified task */

    errnoSet(S_taskLib_TASK_VAR_NOT_FOUND);
    return (ERROR);
    }

