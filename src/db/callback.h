/* $Id$ */

/* includes for general purpose callback tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	12-12-91	mrk	Initial version
 * .02	04-05-94	mrk	Remove casts on Lvalues (ANSI forbids)
 * .02	02-09-95	joh	if def'd out typedef CALLBACK for 
 *				windows
 */


#ifndef INCcallbackh
#define INCcallbackh 1

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
#ifdef __STDC__
	void (*callback)(struct callbackPvt*);
#else
	void(*callback)();
#endif
	int		priority;
	void		*user; /*for use by callback user*/
}CALLBACK;

#ifdef __STDC__
typedef void    (*CALLBACKFUNC)(struct callbackPvt*);
#else
typedef void (*CALLBACKFUNC)();
#endif

#define callbackSetCallback(PFUN,PCALLBACK)\
( (PCALLBACK)->callback = (PFUN) )
#define callbackSetPriority(PRIORITY,PCALLBACK)\
( (PCALLBACK)->priority = (PRIORITY) )
#define callbackSetUser(USER,PCALLBACK)\
( (PCALLBACK)->user = (VOID *)(USER) )
#define callbackGetUser(USER,PCALLBACK)\
( (USER) = (void *)((CALLBACK *)(PCALLBACK))->user )

#ifdef __STDC__
long callbackInit();
void callbackRequest(CALLBACK *);
void callbackRequestProcessCallback(CALLBACK *pCallback,
	int Priority, void *pRec);
int callbackSetQueueSize(int size);
#else
long callbackInit();
void callbackRequest();
void callbackRequestProcessCallback();
int callbackSetQueueSize();
#endif /*__STDC__*/


#endif /*INCcallbackh*/
