#ifndef INC_tsDefs_h
#define INC_tsDefs_h
/*	$Id$	
 *	Author:	Roger A. Cole
 *	Date:	08-09-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991-92, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 *  .01 08-09-90 rac	initial version
 *  .02 06-18-91 rac	installed in SCCS
 *  .03 08-03-92 rac	added tsRound routines
 *  .04 07-02-96 joh	added ANSI prototypes	
 *
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

#include <shareLib.h>

#include <errMdef.h>	/* get M_ts for this subsystem's `number' */

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
typedef struct {
    unsigned    secPastEpoch;   /* seconds since 0000 Jan 1, 1990 */
    unsigned    nsec;           /* nanoseconds within second */
} TS_STAMP;

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

/*/subhead configuration------------------------------------------------------
*		C O N F I G U R A T I O N   D E F I N I T I O N S
*
* TS_DST_BEGIN	the day number for starting DST
* TS_DST_END	the day number for ending DST
* TS_MIN_WEST	the number of minutes west of GMT for time zone (zones east
*		will have negative values)
* TS_DST_HOUR_ON the hour (standard time) when DST turns on
* TS_DST_HOUR_OFF the hour (standard time) when DST turns off
* TS_DST_HRS_ADD hours to add when DST is on
*
* day numbers start with 0 for Jan 1; day numbers in these defines are
* based on a NON-leap year.  The start and end days for DST are assumed to be
* Sundays.  A negative day indicates that the following Sunday is to be used;
* a positive day indicates the prior Sunday.  If the begin date is larger than
* the end date, then DST overlaps the change of the year (e.g., for southern
* hemisphere).
*
* Note well that TS_DST_HOUR_ON and TS_DST_HOUR_OFF are both STANDARD time.
* So, if dst begins at 2 a.m. (standard time) and ends at 2 a.m. (daylight
* time), the two values would be 2 and 1, respectively (assuming
* TS_DST_HRS_ADD is 1).
*----------------------------------------------------------------------------*/
#define TS_DST_BEGIN -90	/* first Sun in Apr (Apr 1 = 90) */
#define TS_DST_END 303		/* last Sun in Oct (Oct 31 = 303) */
#define TS_DST_HOUR_ON 2	/* 2 a.m. (standard time) */
#define TS_DST_HOUR_OFF 1	/* 2 a.m. (1 a.m. standard time) */
#define TS_DST_HRS_ADD 1	/* add one hour */
#define TS_MIN_WEST 7 * 60	/* USA mountain time zone */
#if 0		/* first set is for testing only */
#define TS_EPOCH_YEAR 1989
#define TS_EPOCH_SEC_PAST_1970 6940*86400 /* 1/1/89 19 yr (5 leap) of seconds */
#define TS_EPOCH_WDAY_NUM 0	/* Jan 1 1989 was Sun (wkday num = 0) */
#else
#define TS_EPOCH_YEAR 1990
#define TS_EPOCH_SEC_PAST_1970 7305*86400 /* 1/1/90 20 yr (5 leap) of seconds */
#define TS_EPOCH_WDAY_NUM 1	/* Jan 1 1990 was Mon (wkday num = 1) */
#endif
#define TS_MAX_YEAR TS_EPOCH_YEAR+134	/* ULONG can handle 135 years */
#define TS_TRUNC 1000000	/* truncate to milli-second significance */

/*/subhead struct tsDetail----------------------------------------------------
*    breakdown structure for working with secPastEpoch
*----------------------------------------------------------------------------*/

struct tsDetail {
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
};

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

#ifndef TS_PRIVATE_DATA
    epicsShareExtern char *glTsStatText[7];
#else
    char *glTsStatText[] = {
	/* S_ts_OK                */ "success",
	/* S_ts_sysTimeError      */ "error getting system time",
	/* S_ts_badTextCode       */ "invalid TS_TEXT_xxx code",
	/* S_ts_inputTextError    */ "error in text date or time",
	/* S_ts_timeSkippedDST    */ "time skipped on switch to DST",
	/* S_ts_badRoundInterval  */ "rounding interval is invalid",

	/* TS_S_PAST              */ "illegal TS status code",
    };
#endif

/*/subhead macros-------------------------------------------------------------
*    arithmetic macros
*
*	care has been taken in writing these macros that the result can
*	be stored back into either operand time stamp.  For example,
*	both of the following (as well as other variations) are legal:
*		TsAddStamps(pS1, pS1, pS2);
*		TsDiffAsStamp(pS2, pS1, pS2);
*----------------------------------------------------------------------------*/
#if WIN32 /* the microsoft compiler will not compile the TsAddDouble() macro */ 
#define TsAddDouble(pSum, pS1, dbl) tsAddDouble(pSum, pS1, dbl)
void TsAddDouble(TS_STAMP *pSum, TS_STAMP *pS1, double dbl);
#else /*WIN32*/
#define TsAddDouble(pSum, pS1, dbl) \
  (void)( \
    dbl >= 0. \
      ? ((*pSum).secPastEpoch = (*pS1).secPastEpoch + (unsigned long)dbl, \
        ( ((*pSum).nsec = (*pS1).nsec + (unsigned long)(1000000000. * \
	    			(dbl - (double)((unsigned long)dbl))) ) ) \
		>= 1000000000 \
	    ?((*pSum).secPastEpoch += (*pSum).nsec/1000000000, \
	      (*pSum).nsec %= 1000000000) \
	    :0)  \
      : ((*pSum).secPastEpoch = (*pS1).secPastEpoch - (unsigned long)(-dbl), \
	(*pS1).nsec >= (unsigned long)(1000000000. * \
			((-dbl) - (double)((unsigned long)(-dbl)))) \
	  ?( (*pSum).nsec = (*pS1).nsec - (unsigned long)(1000000000.* \
			((-dbl) - (double)((unsigned long)(-dbl))))) \
	  :( (*pSum).nsec = (*pS1).nsec + 1000000000 - \
			(unsigned long)(1000000000.* \
			((-dbl) - (double)((unsigned long)(-dbl)))), \
	     (*pSum).secPastEpoch -= 1) )  )
#endif /*WIN32*/
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

int epicsShareAPI nextIntFieldAsInt(
char    **ppText,       /* I/O pointer to pointer to text to scan */
int     *pIntVal,       /* O pointer to return field's value */
char    *pDelim         /* O pointer to return field's delimiter */
);

int epicsShareAPI nextAlph1UCField(
char    **ppText,       /* I/O pointer to pointer to text to scan */
char    **ppField,      /* O pointer to pointer to field */
char    *pDelim         /* O pointer to return field's delimiter */
);

int epicsShareAPI nextIntFieldAsLong(
char    **ppText,       /* I/O pointer to pointer to text to scan */
long    *pLongVal,      /* O pointer to return field's value */
char    *pDelim        /* O pointer to return field's delimiter */
);

long epicsShareAPI tsLocalTime (TS_STAMP *pStamp);

void epicsShareAPI tsAddDouble(
TS_STAMP *pSum,         /* O sum time stamp */
TS_STAMP *pStamp,       /* I addend time stamp */
double  dbl             /* I number of seconds to add */
);

int epicsShareAPI tsCmpStamps(
TS_STAMP *pStamp1,      /* pointer to first stamp */
TS_STAMP *pStamp2       /* pointer to second stamp */
);

long epicsShareAPI tsRoundDownLocal(
TS_STAMP *pStamp,       /* IO pointer to time stamp buffer */
unsigned long interval  /* I rounding interval, in seconds */
);

long epicsShareAPI tsRoundUpLocal(
TS_STAMP *pStamp,       /* IO pointer to time stamp buffer */
unsigned long interval  /* I rounding interval, in seconds */
);

char * epicsShareAPI tsStampToText(
TS_STAMP *pStamp,       /* I pointer to time stamp */
enum tsTextType textType,/* I type of conversion desired; one of TS_TEXT_xxx */
char    *textBuffer     /* O buffer to receive text */
);

long epicsShareAPI tsTextToStamp(
TS_STAMP *pStamp,       /* O time stamp corresponding to text */
char    **pText         /* IO ptr to ptr to string containing time and date */
);

long epicsShareAPI tsTimeTextToStamp(
TS_STAMP *pStamp,       /* O time stamp corresponding to text */
char    **pText         /* IO ptr to ptr to string containing time and date */
);

#else /* !defined(__STDC__) && !defined(__cplusplus) */

int nextIntFieldAsInt();
int nextAlph1UCField();
int nextIntFieldAsLong();
long epicsShareAPI tsLocalTime ();
void epicsShareAPI tsAddDouble();
int epicsShareAPI tsCmpStamps();
long epicsShareAPI tsRoundDownLocal();
long epicsShareAPI tsRoundUpLocal();
char epicsShareAPI *tsStampToText();
long epicsShareAPI tsTextToStamp();
long epicsShareAPI tsTimeTextToStamp();

#endif

#ifdef __cplusplus
}
#endif

#endif
