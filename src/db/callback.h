/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */

/* includes for general purpose callback tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
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
