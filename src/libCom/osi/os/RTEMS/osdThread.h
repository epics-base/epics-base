#ifndef osdThreadh
#define osdThreadh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

/*
 * RTEMS-specific implementation of osi thread variable routines
 */
extern void *rtemsTaskVariable;
epicsShareFunc INLINE threadVarId epicsShareAPI threadPrivateCreate ()
{
	return NULL;
}
epicsShareFunc INLINE void epicsShareAPI threadPrivateDelete ()
{
}
epicsShareFunc INLINE void epicsShareAPI threadPrivateSet (threadVarId id, void *ptr)
{
	rtems_task_variable_add (RTEMS_SELF, &rtemsTaskVariable, NULL);
	rtemsTaskVariable = ptr;
}
epicsShareFunc INLINE void * epicsShareAPI threadPrivateGet (threadVarId id)
{
	return rtemsTaskVariable;
}

#ifdef __cplusplus
}
#endif

#endif /* osdThreadh */
