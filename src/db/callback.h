/* $Id$ */

/* includes for general purpose callback tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

/* Modification Log:
 * -----------------
 * .01	12-12-91	mrk	Initial version
 * .02	04-05-94	mrk	Remove casts on Lvalues (ANSI forbids)
 * .02	02-09-95	joh	if def'd out typedef CALLBACK for 
 *				windows
 */


#ifndef INCcallbackh
#define INCcallbackh 1

#include "shareLib.h"

/*
 * WINDOWS also has a "CALLBACK" type def
 */
#ifdef _WIN32
#	ifdef CALLBACK
#		undef CALLBACK
#	endif /*CALLBACK*/
#endif /*_WIN32*/

#define NUM_CALLBACK_PRIORITIES 3
#define priorityLow     0
#define priorityMedium  1
#define priorityHigh    2

typedef struct callbackPvt {
	void (*callback)(struct callbackPvt*);
	int		priority;
	void		*user; /*for use by callback user*/
        void            *timer; /*for use by callback itself*/
}CALLBACK;

typedef void    (*CALLBACKFUNC)(struct callbackPvt*);

#define callbackSetCallback(PFUN,PCALLBACK)\
( (PCALLBACK)->callback = (PFUN) )
#define callbackSetPriority(PRIORITY,PCALLBACK)\
( (PCALLBACK)->priority = (PRIORITY) )
#define callbackSetUser(USER,PCALLBACK)\
( (PCALLBACK)->user = (void *)(USER) )
#define callbackGetUser(USER,PCALLBACK)\
( (USER) = (void *)((CALLBACK *)(PCALLBACK))->user )

epicsShareFunc long epicsShareAPI callbackInit();
epicsShareFunc void epicsShareAPI callbackRequest(CALLBACK *pCallback);
epicsShareFunc void epicsShareAPI callbackRequestProcessCallback(
    CALLBACK *pCallback,int Priority, void *pRec);
epicsShareFunc void epicsShareAPI callbackRequestDelayed(
    CALLBACK *pCallback,double seconds);
epicsShareFunc void epicsShareAPI callbackRequestProcessCallbackDelayed(
    CALLBACK *pCallback, int Priority, void *pRec,double seconds);
epicsShareFunc int epicsShareAPI callbackSetQueueSize(int size);

#endif /*INCcallbackh*/
