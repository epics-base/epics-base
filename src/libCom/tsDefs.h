/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef INC_tsDefs_h
#define INC_tsDefs_h
/*	$Id$	
 *	Author:	Roger A. Cole
 *	Date:	08-09-90
 */
/*+/mod***********************************************************************
* TITLE	tsDefs.h - time-stamp related definitions
*
* DESCRIPTION
*
* BUGS
* o	only a single, manually configured, conversion between local
*	time and GMT is supported.  Thus, if a new `rule' for daylight
*	savings time were invented, then this code would be `broken'
*	until this file were configured for the new rule.  BUT, then
*	this code is `broken' for the period prior to the new rule's
*	effect.
* o	only dates following Jan 1, 1990 are handled; stamps on Jan 1,
*	1990 are treated as `dateless' times.
*
*-***************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include "epicsTypes.h"
#include "errMdef.h"	/* get M_ts for this subsystem's `number' */
#include "shareLib.h"

/*---------------------------------------------------------------------------
-
* TS_STAMP
*
*    This is the form taken by time stamps in GTACS, and is the form used
*    by the various tools used for dealing with time stamps.
*
*    The time stamp represents the number of nanoseconds past 0000 Jan 1, 199
0,
*    GMT (or UTC, if you prefer).
*
*----------------------------------------------------------------------------
*/
typedef struct TS_STAMP {
    epicsUInt32    secPastEpoch;   /* seconds since 0000 Jan 1, 1990 */
    epicsUInt32    nsec;           /* nanoseconds within second */
} TS_STAMP;

/*---------------------------------------------------------------------------
*    tsDetail
*    breakdown structure for working with secPastEpoch
*----------------------------------------------------------------------------*/

typedef struct tsDetail {
    int         year;           /* 4 digit year */
    int         dayYear;        /* day number in year; 0 = Jan 1 */
    int         monthNum;       /* month number; 0 = Jan */
    int         dayMonth;       /* day number; 0 = 1st of month */
    int         hours;          /* hours within day */
    int         minutes;        /* minutes within hour */
    int         seconds;        /* seconds within minute */
    int         dayOfWeek;      /* weekday number; 0 = Sun */
    int         leapYear;       /* (0, 1) for year (isn't, is) a leap year */
    char	dstOverlapChar;	/* indicator for distinguishing duplicate
				times in the `switch to standard' time period:
				':'--time isn't ambiguous;
				's'--time is standard time
				'd'--time is daylight time */
} TS_DETAIL;

/*----------------------------------------------------------------------------
* TS_TEXT_xxx text type codes for converting between text and time stamp
*
*    TS_TEXT_MONDDYYYY	Mon dd, yyyy hh:mm:ss.nano-secs
*    TS_TEXT_MMDDYY	mm/dd/yy hh:mm:ss.nano-secs
*			123456789012345678901234567890123456789
*			0        1         2         3
*----------------------------------------------------------------------------*/
enum tsTextType{
    TS_TEXT_MONDDYYYY,
    TS_TEXT_MMDDYY
};

/*/subhead macros-------------------------------------------------------------
*    arithmetic macros
*	care has been taken in writing these macros that the result can
*	be stored back into either operand time stamp.  For example,
*	both of the following (as well as other variations) are legal:
*		TsAddStamps(pS1, pS1, pS2);
*		TsDiffAsStamp(pS2, pS1, pS2);
*----------------------------------------------------------------------------*/
/*TsAddDouble was originally a macro. Now just use tsAddDouble */
#define TsAddDouble(pSum, pS1, dbl) tsAddDouble(pSum, pS1, dbl)
#define TsAddStamps(pSum, pS1, pS2) \
    (void)( \
	((*pSum).secPastEpoch = (*pS1).secPastEpoch + (*pS2).secPastEpoch) , \
	( ((*pSum).nsec = (*pS1).nsec + (*pS2).nsec) >= 1000000000 \
	    ?((*pSum).secPastEpoch += (*pSum).nsec/1000000000, \
	      (*pSum).nsec %= 1000000000) \
	    :0)  )
#define TsDiffAsDouble(pDbl, pS1, pS2) \
  (void)( \
    *pDbl = ((double)(*pS1).nsec - (double)(*pS2).nsec)/1000000000., \
    *pDbl += (double)(*pS1).secPastEpoch - (double)(*pS2).secPastEpoch )
/* TsDiffAsStamp() assumes that stamp1 >= stamp2 */
#define TsDiffAsStamp(pDiff, pS1, pS2) \
  (void)( \
    (*pDiff).secPastEpoch = (*pS1).secPastEpoch - (*pS2).secPastEpoch , \
    (*pS1).nsec >= (*pS2).nsec \
      ?( (*pDiff).nsec = (*pS1).nsec - (*pS2).nsec ) \
      :( (*pDiff).nsec = ((*pS1).nsec + 1000000000) - (*pS2).nsec, \
	 (*pDiff).secPastEpoch -= 1 )  )
/*----------------------------------------------------------------------------
*    comparison macros
*----------------------------------------------------------------------------*/
#define TsCmpStampsEQ(pS1, pS2) \
	(((*pS1).secPastEpoch) == ((*pS2).secPastEpoch) && \
	 ((*pS1).nsec) == ((*pS2).nsec))
#define TsCmpStampsNE(pS1, pS2) \
	(((*pS1).secPastEpoch) != ((*pS2).secPastEpoch) || \
	 ((*pS1).nsec) != ((*pS2).nsec))
#define TsCmpStampsGT(pS1, pS2) \
	((((*pS1).secPastEpoch) > ((*pS2).secPastEpoch)) \
	    ?1 \
	    :((((*pS1).secPastEpoch) == ((*pS2).secPastEpoch)) \
		?((((*pS1).nsec) > ((*pS2).nsec)) ? 1 : 0) \
		:0  ))
#define TsCmpStampsGE(pS1, pS2) \
	((((*pS1).secPastEpoch) > ((*pS2).secPastEpoch)) \
	    ?1 \
	    :((((*pS1).secPastEpoch) == ((*pS2).secPastEpoch)) \
		?((((*pS1).nsec) >= ((*pS2).nsec)) ? 1 : 0) \
		:0  ))
#define TsCmpStampsLT(pS1, pS2) \
	((((*pS1).secPastEpoch) < ((*pS2).secPastEpoch)) \
	    ?1 \
	    :((((*pS1).secPastEpoch) == ((*pS2).secPastEpoch)) \
		?((((*pS1).nsec) < ((*pS2).nsec)) ? 1 : 0) \
		:0  ))
#define TsCmpStampsLE(pS1, pS2) \
	((((*pS1).secPastEpoch) < ((*pS2).secPastEpoch)) \
	    ?1 \
	    :((((*pS1).secPastEpoch) == ((*pS2).secPastEpoch)) \
		?((((*pS1).nsec) <= ((*pS2).nsec)) ? 1 : 0) \
		:0  ))

/*----------------------------------------------------------------------------
*    `prototypes'
*----------------------------------------------------------------------------*/

#if defined(__STDC__) || defined(__cplusplus)

epicsShareFunc void epicsShareAPI tsStampFromLocal(
TS_STAMP *pStamp,
struct tsDetail *pT
);

epicsShareFunc void epicsShareAPI tsStampToLocal(
TS_STAMP stamp,
struct tsDetail *pT
);

epicsShareFunc long epicsShareAPI tsLocalTime (TS_STAMP *pStamp);

epicsShareFunc void epicsShareAPI tsAddDouble(
TS_STAMP *pSum,         /* O sum time stamp */
TS_STAMP *pStamp,       /* I addend time stamp */
double  dbl             /* I number of seconds to add */
);

epicsShareFunc int epicsShareAPI tsCmpStamps(
TS_STAMP *pStamp1,      /* pointer to first stamp */
TS_STAMP *pStamp2       /* pointer to second stamp */
);

epicsShareFunc long epicsShareAPI tsRoundDownLocal(
TS_STAMP *pStamp,       /* IO pointer to time stamp buffer */
unsigned long interval  /* I rounding interval, in seconds */
);

epicsShareFunc long epicsShareAPI tsRoundUpLocal(
TS_STAMP *pStamp,       /* IO pointer to time stamp buffer */
unsigned long interval  /* I rounding interval, in seconds */
);

epicsShareFunc char * epicsShareAPI tsStampToText(
TS_STAMP *pStamp,       /* I pointer to time stamp */
enum tsTextType textType,/* I type of conversion desired; one of TS_TEXT_xxx */
char    *textBuffer     /* O buffer to receive text */
);

epicsShareFunc long epicsShareAPI tsTextToStamp(
TS_STAMP *pStamp,       /* O time stamp corresponding to text */
char    **pText         /* IO ptr to ptr to string containing time and date */
);

epicsShareFunc long epicsShareAPI tsTimeTextToStamp(
TS_STAMP *pStamp,       /* O time stamp corresponding to text */
char    **pText         /* IO ptr to ptr to string containing time and date */
);

#else /* !defined(__STDC__) && !defined(__cplusplus) */

epicsShareFunc void epicsShareAPI tsStampFromLocal();
epicsShareFunc void epicsShareAPI tsStampToLocal();
epicsShareFunc long epicsShareAPI tsLocalTime ();
epicsShareFunc void epicsShareAPI tsAddDouble();
epicsShareFunc int epicsShareAPI tsCmpStamps();
epicsShareFunc long epicsShareAPI tsRoundDownLocal();
epicsShareFunc long epicsShareAPI tsRoundUpLocal();
epicsShareFunc char * epicsShareAPI tsStampToText();
epicsShareFunc long epicsShareAPI tsTextToStamp();
epicsShareFunc long epicsShareAPI tsTimeTextToStamp();

#endif /*defined(__STDC__) || defined(__cplusplus) */

/*/subhead status codes-------------------------------------------------------
*		S T A T U S   C O D E S
*
*	
*    Status codes for time stamp routines have the form S_ts_briefMessage  .
*
*    The macro TsStatusToText(stat) can be used to obtain a text string
*    corresponding to a status code.
*
*----------------------------------------------------------------------------*/
#define S_ts_OK		     0 
#define S_ts_sysTimeError    (M_ts|1| 1<<1) /* error getting system time */
#define S_ts_badTextCode     (M_ts|1| 2<<1) /* invalid TS_TEXT_xxx code */
#define S_ts_inputTextError  (M_ts|1| 3<<1) /* error in text date or time */
#define S_ts_timeSkippedDST  (M_ts|1| 4<<1) /* time skipped on switch to DST */
#define S_ts_badRoundInterval (M_ts|1| 5<<1) /* invalid rounding interval */

#define TS_S_PAST		 6	/* one past last legal code */

#define TsStatusToIndex(status) \
	(  ((status&0xffff0000)!=M_ts)  \
		? TS_S_PAST  \
		: (  (((status&0xffff)>>1)>=TS_S_PAST)  \
		     ? TS_S_PAST  \
		     : ((status&0xffff)>>1)  \
		  )  \
	)

#define TsStatusToText(status) \
	(glTsStatText[TsStatusToIndex(status)])

epicsShareExtern char *glTsStatText[7];
#ifdef createTSSubrGlobals
    epicsShareDef char *glTsStatText[] = {
        /* S_ts_OK                */ "success",
        /* S_ts_sysTimeError      */ "error getting system time",
        /* S_ts_badTextCode       */ "invalid TS_TEXT_xxx code",
        /* S_ts_inputTextError    */ "error in text date or time",
        /* S_ts_timeSkippedDST    */ "time skipped on switch to DST",
        /* S_ts_badRoundInterval  */ "rounding interval is invalid",

        /* TS_S_PAST              */ "illegal TS status code",
    };
#endif

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*INC_tsDefs_h*/
