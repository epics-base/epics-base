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
( (PCALLBACK)->user = (void *)(USER) )
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
