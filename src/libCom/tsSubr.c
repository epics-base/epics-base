/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	08-09-90
 */

/*+/mod***********************************************************************
* TITLE	tsSubr.c - time stamp routines
*
* DESCRIPTION
*	This module contains routines relating to time stamps.  Capabilities
*	provided include: converting from time stamp to text; converting
*	from text to time stamp; computing the difference between two time
*	stamps; and getting system time in the form of a time stamp.
*
*	A time stamp is a pair of `unsigned long' quantities: the number
*	of seconds past an epoch; and the number of nanoseconds within
*	the second.  Time stamps always reference GMT.  Except as noted in
*	individual routines, the nanosecond part of the time stamp is assumed
*	always to be between 0 and 999999999; these routines always produce
*	time stamps which follow the rule.
*
*	These routines handle leap year issues and daylight savings time
*	issues, and properly handle dates after 1999.  The tsDefs.h header
*	file contains definitions which must be customized for the local
*	time zone.
*
* QUICK REFERENCE
* #include "tsDefs.h"
* TS_STAMP  timeStamp;
*  void  date(								)
*  void  tsAddDouble(      >pStampSum,    pStamp,        secAsDouble	)
*  void  TsAddDouble(      >pStampSum,    pStamp,        secAsDouble	)
*  void  TsAddStamps(      >pStampSum,    pStamp1,       pStamp2	)
*   int  tsCmpStamps(       pStamp1,      pStamp2			)
*   int  TsCmpStampsEQ(     pStamp1,      pStamp2			)
*   int  TsCmpStampsNE(     pStamp1,      pStamp2			)
*   int  TsCmpStampsGT(     pStamp1,      pStamp2			)
*   int  TsCmpStampsGE(     pStamp1,      pStamp2			)
*   int  TsCmpStampsLT(     pStamp1,      pStamp2			)
*   int  TsCmpStampsLE(     pStamp1,      pStamp2			)
*  void  TsDiffAsDouble(   >pSecAsDouble, pStamp1,       pStamp2	)
*  void  TsDiffAsStamp(    >pStampDiff,   pStamp1,       pStamp2	)
*  long  tsLocalTime(      >pStamp					)
*  long  tsRoundDownLocal(<>pStamp,       intervalUL			)
*  long  tsRoundUpLocal(  <>pStamp,       intervalUL			)
*  void  tsStampFromLocal( >pStamp,       detail                        )
*  void  tsStampToLocal(    pStamp,      >detail                        )
*  char *tsStampToText(     pStamp,       textType,      >textBuffer	)
*					 TS_TEXT_MONDDYYYY   buf[32]
*					 TS_TEXT_MMDDYY      buf[28]
*  long  tsTextToStamp(    >pStamp,    <>pTextPointer			)
*  long  tsTimeTextToStamp(>pStamp,    <>pTextPointer			)
*
* BUGS
* o	time stamps on the first day of the epoch are taken to be `delta
*	times', thus making it impossible to represent times on the first
*	day of the epoch as a time stamp.
*
* NOTES
* 1.	The environment variable EPICS_TS_MIN_WEST overrides the default
*	time zone established by TS_MIN_WEST in tsDefs.h
*
* SEE ALSO
*	tsDefs.h	contains definitions needed to use these routines,
*			as well as configuration information
*
* (Some of the code here is based somewhat on time-related routines from
* 4.2 BSD.)
*-***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#define INC_tsDefs_h
#if defined(vxWorks)
#   include <vxWorks.h>
#   include "drvTS.h"
#elif defined(VMS)
#   include <sys/types.h>
#   include <sys/time.h>
#   include <sys/socket.h>
#elif defined(_WIN32)
#   include <winsock2.h>
#   include <time.h>
#else
#   include <sys/time.h>
#endif

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "envDefs.h"
#define createTSSubrGlobals
#undef INC_tsDefs_h
#include "tsDefs.h"



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
#if 0   /* Years up to & including 2006 */
#define TS_DST_BEGIN -90	/* first Sun in Apr (Apr 1 = 90) */
#define TS_DST_END 303		/* last Sun in Oct (Oct 31 = 303) */
#else   /* Years from 2007 */
#define TS_DST_BEGIN (59+14)	/* second Sun in Mar (Mar 1 = 59) */
#define TS_DST_END -304		/* first Sun in Nov (Nov 1 = 304) */
#endif
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


static void tsStampToLocalZone(TS_STAMP *pStamp,struct tsDetail *pT);
static void tsInitMinWest(void);

static int nextAlph1UCField(char **ppText,char **ppField, char *pDelim);
static int nextIntField(char **ppText,char **ppField,char *pDelim);
static int nextIntFieldAsInt(char **ppText,int *pIntVal,char *pDelim);
static int nextIntFieldAsLong(char **ppText,long *pLongVal,char	*pDelim);

static int needToInitMinWest=1;
static long tsMinWest=TS_MIN_WEST;

static int daysInMonth[] =    {31,28,31,30, 31, 30, 31, 31, 30, 31, 30, 31};
static int dayYear1stOfMon[] = {0,31,59,90,120,151,181,212,243,273,304,334};
static char	monthText[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

#define TEXTBUFSIZE 40

/*/subhead tsTest-------------------------------------------------------------
*    some test code.
*----------------------------------------------------------------------------*/
#if TS_TEST || LINT

#  ifndef vxWorks
int tsTest(void);
int  main()
  {
    return tsTest();
  }
#  endif
int tsTest()
{
    long	stat;
    double	dbl;
    TS_STAMP	stamp1;
    char	stamp1Txt[32];
    TS_STAMP	stamp2;
    char	stamp2Txt[32];
    TS_STAMP	stamp3;
    char	stamp3Txt[32];
    TS_STAMP	stampD;
    char	stampDTxt[32];
    TS_STAMP	stampDbl;
    char	stampDblTxt[32];
    TS_STAMP	stampDblD;
    char	stampDblDTxt[32];
    TS_STAMP	inStamp;
    char	inStampTxt[80];

/*----------------------------------------------------------------------------
*    get current date and print in the various ways possible
*----------------------------------------------------------------------------*/
    (void)printf("Getting current date:\n");
    if ((stat = tsLocalTime(&stamp1)) != S_ts_OK) {
	(void)printf("%s\n", TsStatusToText(stat));
	return 1;
    }
    (void)printf("	%s\n",
			tsStampToText(&stamp1, TS_TEXT_MONDDYYYY, stamp1Txt));
    (void)printf("	%s\n",
			tsStampToText(&stamp1, TS_TEXT_MMDDYY, stamp1Txt));

/*----------------------------------------------------------------------------
*    now, play around a bit with other routines
*----------------------------------------------------------------------------*/
    stamp2.secPastEpoch = 80432;
    stamp2.nsec = 900000000;

    TsAddStamps(&stamp3, &stamp1, &stamp2);
    (void)printf("%s plus %s is %s\n",
			stamp1Txt,
			tsStampToText(&stamp2, TS_TEXT_MMDDYY, stamp2Txt),
    			tsStampToText(&stamp3, TS_TEXT_MMDDYY, stamp3Txt));
    TsDiffAsStamp(&stampD, &stamp3, &stamp1);
    (void)printf("%s minus %s is %s\n",
			stamp3Txt, stamp1Txt,
    			tsStampToText(&stampD, TS_TEXT_MMDDYY, stampDTxt));
    TsDiffAsDouble(&dbl, &stamp3, &stamp1);
    (void)printf("%s Minus %s is %.9f\n", stamp3Txt, stamp1Txt, dbl);
    TsDiffAsDouble(&dbl, &stamp1, &stamp3);
    (void)printf("%s Minus %s is %.9f\n", stamp1Txt, stamp3Txt, dbl);

    tsAddDouble(&stampDbl, &stamp1, 80432.9);
    (void)printf("%s plus %.9f is %s\n",
			stamp1Txt, 80432.9,
    			tsStampToText(&stampDbl, TS_TEXT_MMDDYY, stampDblTxt));
    TsAddDouble(&stampDbl, &stamp1, 80432.9);
    (void)printf("%s Plus %.9f is %s\n",
			stamp1Txt, 80432.9,
    			tsStampToText(&stampDbl, TS_TEXT_MMDDYY, stampDblTxt));
    tsAddDouble(&stampDblD, &stampDbl, -80432.9);
    (void)printf("%s plus %.9f is %s\n",
			stampDblTxt, -80432.9,
    			tsStampToText(&stampDblD,TS_TEXT_MMDDYY, stampDblDTxt));
    TsAddDouble(&stampDblD, &stampDbl, -80432.9);
    (void)printf("%s Plus %.9f is %s\n",
			stampDblTxt, -80432.9,
    			tsStampToText(&stampDblD,TS_TEXT_MMDDYY, stampDblDTxt));

    stamp2.nsec = 100000000;
    TsAddStamps(&stamp3, &stamp1, &stamp2);
    (void)printf("%s plus %s is %s\n",
			stamp1Txt,
			tsStampToText(&stamp2, TS_TEXT_MMDDYY, stamp2Txt),
    			tsStampToText(&stamp3, TS_TEXT_MMDDYY, stamp3Txt));
    TsDiffAsStamp(&stampD, &stamp3, &stamp1);
    (void)printf("%s minus %s is %s\n",
			stamp3Txt, stamp1Txt,
    			tsStampToText(&stampD, TS_TEXT_MMDDYY, stampDTxt));
    TsDiffAsDouble(&dbl, &stamp3, &stamp1);
    (void)printf("%s Minus %s is %.9f\n", stamp3Txt, stamp1Txt, dbl);
    TsDiffAsDouble(&dbl, &stamp1, &stamp3);
    (void)printf("%s Minus %s is %.9f\n", stamp1Txt, stamp3Txt, dbl);

    tsAddDouble(&stampDbl, &stamp1, 80432.1);
    (void)printf("%s plus %.9f is %s\n",
			stamp1Txt, 80432.1,
    			tsStampToText(&stampDbl, TS_TEXT_MMDDYY, stampDblTxt));
    TsAddDouble(&stampDbl, &stamp1, 80432.1);
    (void)printf("%s Plus %.9f is %s\n",
			stamp1Txt, 80432.1,
    			tsStampToText(&stampDbl, TS_TEXT_MMDDYY, stampDblTxt));
    tsAddDouble(&stampDblD, &stampDbl, -80432.1);
    (void)printf("%s plus %.9f is %s\n",
			stampDblTxt, -80432.1,
    			tsStampToText(&stampDblD,TS_TEXT_MMDDYY, stampDblDTxt));
    TsAddDouble(&stampDblD, &stampDbl, -80432.1);
    (void)printf("%s Plus %.9f is %s\n",
			stampDblTxt, -80432.1,
    			tsStampToText(&stampDblD,TS_TEXT_MMDDYY, stampDblDTxt));

    (void)printf("comparisons for %s vs %s\n", stamp1Txt, stamp3Txt);
    (void)printf("	tsCmpStamps:  A,B %2d    B,A %2d    A,A %2d\n",
	    tsCmpStamps(&stamp1, &stamp3), tsCmpStamps(&stamp3, &stamp1),
	    tsCmpStamps(&stamp1, &stamp1));
    (void)printf(
	"	macro A vs B: EQ %d  NE %d  GT %d  GE %d  LT %d  LE %d\n",
	    TsCmpStampsEQ(&stamp1, &stamp3), TsCmpStampsNE(&stamp1, &stamp3),
	    TsCmpStampsGT(&stamp1, &stamp3), TsCmpStampsGE(&stamp1, &stamp3),
	    TsCmpStampsLT(&stamp1, &stamp3), TsCmpStampsLE(&stamp1, &stamp3));
    (void)printf(
	"	macro B vs A: EQ %d  NE %d  GT %d  GE %d  LT %d  LE %d\n",
	    TsCmpStampsEQ(&stamp3, &stamp1), TsCmpStampsNE(&stamp3, &stamp1),
	    TsCmpStampsGT(&stamp3, &stamp1), TsCmpStampsGE(&stamp3, &stamp1),
	    TsCmpStampsLT(&stamp3, &stamp1), TsCmpStampsLE(&stamp3, &stamp1));
    (void)printf(
	"	macro A vs A: EQ %d  NE %d  GT %d  GE %d  LT %d  LE %d\n",
	    TsCmpStampsEQ(&stamp1, &stamp1), TsCmpStampsNE(&stamp1, &stamp1),
	    TsCmpStampsGT(&stamp1, &stamp1), TsCmpStampsGE(&stamp1, &stamp1),
	    TsCmpStampsLT(&stamp1, &stamp1), TsCmpStampsLE(&stamp1, &stamp1));

    stamp2.secPastEpoch = 0;
    stamp2.nsec = 10000000;
    TsAddStamps(&stamp3, &stamp1, &stamp2);
    (void)printf("comparisons for %s vs %s\n", stamp1Txt,
    			tsStampToText(&stamp3, TS_TEXT_MMDDYY, stamp3Txt));
    (void)printf("	tsCmpStamps:  A,B %2d    B,A %2d    A,A %2d\n",
	    tsCmpStamps(&stamp1, &stamp3), tsCmpStamps(&stamp3, &stamp1),
	    tsCmpStamps(&stamp1, &stamp1));
    (void)printf(
	"	macro A vs B: EQ %d  NE %d  GT %d  GE %d  LT %d  LE %d\n",
	    TsCmpStampsEQ(&stamp1, &stamp3), TsCmpStampsNE(&stamp1, &stamp3),
	    TsCmpStampsGT(&stamp1, &stamp3), TsCmpStampsGE(&stamp1, &stamp3),
	    TsCmpStampsLT(&stamp1, &stamp3), TsCmpStampsLE(&stamp1, &stamp3));
    (void)printf(
	"	macro B vs A: EQ %d  NE %d  GT %d  GE %d  LT %d  LE %d\n",
	    TsCmpStampsEQ(&stamp3, &stamp1), TsCmpStampsNE(&stamp3, &stamp1),
	    TsCmpStampsGT(&stamp3, &stamp1), TsCmpStampsGE(&stamp3, &stamp1),
	    TsCmpStampsLT(&stamp3, &stamp1), TsCmpStampsLE(&stamp3, &stamp1));

/*----------------------------------------------------------------------------
*    exercise tsTextToStamp
*----------------------------------------------------------------------------*/
    inStampTxt[0] = '\0';
    while (strncmp(inStampTxt, "quit", 4) != 0) {
	(void)printf("type text date/time or quit: ");
	if (fgets(inStampTxt, 80, stdin) == NULL) {
	    (void)strcpy(inStampTxt, "quit");
	    (void)printf("\n");
	    clearerr(stdin);
	}
	else if (strncmp(inStampTxt, "quit", 4) == 0)
	    ;		/* no action */
	else {
	    char	*pText;
	    pText = inStampTxt;
	    if ((stat = tsTextToStamp(&inStamp, &pText)) != S_ts_OK) {
		(void)printf("%s\n", TsStatusToText(stat));
	    }
	    else {
		(void)printf("stamp is: %u %u %s\n", inStamp.secPastEpoch,
			inStamp.nsec,
			tsStampToText(&inStamp, TS_TEXT_MONDDYYYY, inStampTxt));
	    }
	}
    }

    clearerr(stdin);
    return 0;
}
#endif

#ifdef vxWorks
/*+/subr**********************************************************************
* NAME	date - print present time and date
*
* DESCRIPTION
*	Prints the present date and time when running under VxWorks.
*
* RETURNS
*	void
*
*-*/
void date()
{
    TS_STAMP	now;
    char	nowText[32];

    (void)tsLocalTime(&now);
    (void)printf("%s\n", tsStampToText(&now, TS_TEXT_MONDDYYYY, nowText));
}
#endif

/*+/internal******************************************************************
* NAME	sunday - find closest Sunday
*
* DESCRIPTION
*	Finds the day of the year for the closest Sunday to the specified
*	day of the year; caller can specify whether the preceding or
*	following Sunday is desired.
*
* RETURNS
*	day of year for the desired Sunday
*
*-*/
static int sunday(
int	day,		/* I day of year to find closest Sunday */
int	leap,		/* I 0, 1 for not leap year, leap year, respectively */
int	dayYear,	/* I known day of year */
int	dayOfWeek)	/* I day of week for dayYear */
{
    int		offset;		/* controls direction of offset */

/*----------------------------------------------------------------------------
*    The principle of this routine is: if I know the day of the week for a
*    particular day in this year, then the day of the week for the caller's
*    day is (myDOW - (myDay - callersDay) % 7)  .  (This needs to be
*    elaborated a little, since it can generate numbers outside 0 to 6  .)
*    Once the day of the week is known for the caller's day, the previous
*    Sunday is obtained by subtracting the caller's day of week from the
*    caller's day; if the following Sunday is desired, then finish up by
*    adding 7 to the date for the prior Sunday.
*
*    'offset' takes care of the 'outside 0 to 6' issue and also takes care
*    of the direction for 'prior' or 'following'.  There isn't any magic
*    about the choice of 700 as the value--it must be a multiple of 7 and
*    must be larger than the number of days in a year.
*----------------------------------------------------------------------------*/
    if (day < 0) {	/* look for prior Sunday */
	offset = -700;
	day = -day;
	assert(day > 5);	/* day can't be too early in year */
    }
    else {		/* look for following Sunday */
	offset = 700;
	assert(day < 360);	/* day can't be too late in year */
    }
/*----------------------------------------------------------------------------
*    adjust caller's day of year to compensate for leap year.  If the day is
*    after Feb 28, add 1 in leap years.  If the day is Feb 28, then also add
*    1, so that the day stays "last day in Feb".
*----------------------------------------------------------------------------*/
    if (day >= 58)
	day += leap;

    return (day - (day - dayYear + dayOfWeek + offset) % 7 );
}

/*+/macro*********************************************************************
* NAME	TsAddXxx - addition for time stamps
*
* DESCRIPTION
*	These routines and macros produce a time stamp which is the sum of a
*	time stamp and another quantity.  For xxAddDouble(), the `other
*	quantity' is a positive or negative double precision value.  For
*	TsAddStamps, the `other quantity' is another time stamp.
*
*	    tsAddDouble(pStampSum, pStamp1, secAsDouble)	function
*	    TsAddDouble(pStampSum, pStamp1, secAsDouble)	macro
*	    TsAddStamps(pStampSum, pStamp1, pStamp2)		macro
*
*	pStampSum can be the same as one of the other pStamp arguments.
*
* RETURNS
*	void
*
* NOTES
* 1.	These tools properly handle the case where pStamp1->nsec and/or
*	pStamp2->nsec are greater then 1000000000, so long as the sum is
*	less than 4294967295 (i.e., 0xffffffff).
* 2.	For xxAddDouble(), if secAsDouble is negative, its absolute value
*	is assumed to be smaller than pStamp, so that the sum will be positive.
* 3.	Using xxAddDouble() can produce time stamps whose resolution differs
*	from the resolution of the addend time stamp; this can be confusing
*	for comparisons and for tsStampToText().  For example,
*	9/10/90 10:23:06.940 plus 80432.9 (equivalent to 22:20:32.900)
*	produces 9/11/90 8:43:39.839999999 rather than ...840, as desired.
*
* EXAMPLE
* 1.	Compute the time stamp corresponding to 2 hours past the present
*	time and date.
*
*	TS_STAMP	now, later;
*	double		twoHours=7200.;
*
*	tsLocalTime(&now);
*	TsAddDouble(&later, &now, twoHours);
*
*-*/
void epicsShareAPI
tsAddDouble(TS_STAMP *pSum,TS_STAMP *pStamp, double dbl)
{
    TS_STAMP	stamp;		/* stamp equivalent of the double */

    if (dbl >= 0.) {
	stamp.secPastEpoch = (unsigned long)dbl;
	stamp.nsec = (unsigned long)
			((dbl - (double)stamp.secPastEpoch) * 1000000000.);
	TsAddStamps(pSum, pStamp, &stamp);
    }
    else {
	dbl = -dbl;
	stamp.secPastEpoch = (unsigned long)dbl;
	stamp.nsec = (unsigned long)
			((dbl - (double)stamp.secPastEpoch) * 1000000000.);
	TsDiffAsStamp(pSum, pStamp, &stamp);
    }
}

/*+/subr**********************************************************************
* NAME	tsCmpStamps - compare two time stamps
*
* DESCRIPTION
*	Compare two time stamps and return (similar to strcmp() ) negative,
*	zero, or positive if the first time stamp is, respectively, less
*	than, equal to, or greater than the second.
*
*	Several comparison macros are also available in tsDefs.h  .  These
*	macros return a true condition if the comparison test succeeds:
*
*	    TsCmpStampsEQ(pStamp1, pStamp2) is true if stamp1 == stamp2
*	    TsCmpStampsNE(pStamp1, pStamp2) is true if stamp1 != stamp2
*	    TsCmpStampsGT(pStamp1, pStamp2) is true if stamp1 >  stamp2
*	    TsCmpStampsGE(pStamp1, pStamp2) is true if stamp1 >= stamp2
*	    TsCmpStampsLT(pStamp1, pStamp2) is true if stamp1 <  stamp2
*	    TsCmpStampsLE(pStamp1, pStamp2) is true if stamp1 <= stamp2
*
* RETURNS
*	see description above
*
* EXAMPLES
* 1.	Compare two time stamps.
*	TS_STAMP	stamp1, stamp2;
*
*	if ((i=tsCmpStamps(&stamp1, &stamp2)) == 0)
*	    printf("stamps are equal\n");
*	else if (i < 0)
*	    printf("first stamp is earlier\n");
*	else
*	    printf("first stamp is later\n");
*
* 2.	Compare two time stamps.
*	TS_STAMP	stamp1, stamp2;
*
*	if (TsCmpStampsEQ(&stamp1, &stamp2))
*	    printf("stamps are equal\n");
*	else if (TsCmpStampsLT(&stamp1, &stamp2))
*	    printf("first stamp is earlier\n");
*	else
*	    printf("first stamp is later\n");
*
*-*/
int epicsShareAPI tsCmpStamps(TS_STAMP *pStamp1,TS_STAMP *pStamp2)
{
    if (pStamp1->secPastEpoch < pStamp2->secPastEpoch)
	return -1;
    else if (pStamp1->secPastEpoch == pStamp2->secPastEpoch) {
	if (pStamp1->nsec < pStamp2->nsec)
	    return -1;
	else if (pStamp1->nsec == pStamp2->nsec)
	    return 0;
	else
	    return 1;
    }
    else
	return 1;
}

/*+/macro*********************************************************************
* NAME	TsDiffAsXxx - difference between two time stamps
*
* DESCRIPTION
*	These macros produce the result of subtracting two
*	time stamps, i.e.,
*
*		result = *pStamp1 - *pStamp2;
*
*	The form the result depends on the macro used.  For TsDiffAsDouble(),
*	the result is floating point seconds, represented as double.  For
*	TsDiffAsStamp(), the result is a time stamp.
*
*	    TsDiffAsDouble(pSecAsDouble, pStamp1, pStamp2)
*	    TsDiffAsStamp(pStampDiff, pStamp1, pStamp2)
*
* RETURNS
*	void
*
* NOTES
* 1.	Because time stamps are represented as unsigned quantities, care
*	must be taken that *pStamp1 is >= *pStamp2 for TsDiffAsStamp().  (For
*	TsDiffAsDouble(), this restriction doesn't apply.)
*
* EXAMPLES
* 1.	Compute the difference in seconds between two time stamps.
*	TS_STAMP	stamp1, stamp2;
*
*	printf("difference is %f sec.\n", TsDiffAsDouble(&stamp1, &stamp2);
*
*-*/

/*+/subr**********************************************************************
* NAME	tsLocalTime - get local time as a time stamp
*
* DESCRIPTION
*	The local system time is obtained, converted to the reference time
*	used by time stamps, and then converted into the form used for
*	time stamps.
*
* RETURNS
*	S_ts_OK, or
*	S_ts_sysTimeError	if error occurred getting time of day
*
* BUGS
* o	For SunOS, local system time is truncated at milli-seconds--i.e.,
*	micro-seconds are ignored.
*
* EXAMPLES
* 1.	Get the local time as a time stamp.
*	TS_STAMP	now;
*
*	tsLocalTime(&now);
*
*-*/
long epicsShareAPI tsLocalTime(TS_STAMP *pStamp)
{
    long	retStat=S_ts_OK;/* return status to caller */

    assert(pStamp != NULL);

    {
#   if defined(vxWorks)
	    retStat = TScurrentTimeStamp((struct timespec*)pStamp);
	    if (retStat == 0) {
			return S_ts_OK;
	    }
	    else {
			return S_ts_sysTimeError;
	    }
#   elif defined(_WIN32)
		static long offset_time_s = 0;  /* time diff (sec) from 1990 when EPICS started */
		static LARGE_INTEGER time_prev, time_freq;
		LARGE_INTEGER time_cur, time_sec, time_remainder;

		if (offset_time_s == 0) {
			/*
			 * initialize elapsed time counters
			 *
			 * All CPUs running win32 currently have HR
			 * counters (Intel and Mips processors do)
			 */
			if (QueryPerformanceCounter (&time_prev)==0) {
				return S_ts_sysTimeError;
			}
			if (QueryPerformanceFrequency (&time_freq)==0) {
				return S_ts_sysTimeError;
			}
			offset_time_s = (long)time(NULL) - TS_EPOCH_SEC_PAST_1970 
								- (long)(time_prev.QuadPart/time_freq.QuadPart);
		}

		/*
		 * dont need to check status since it was checked once
		 * during initialization to see if the CPU has HR
		 * counters (Intel and Mips processors do)
		 */
		QueryPerformanceCounter (&time_cur);
		if (time_prev.QuadPart > time_cur.QuadPart)	{	/* must have been a timer roll-over */
			double offset;
			/*
			 * must have been a timer roll-over
			 * It takes 9.223372036855e+18/time_freq sec
			 * to roll over this counter (time_freq is 1193182
			 * sec on my system). This is currently about 245118 years.
			 *
			 * attempt to add number of seconds in a 64 bit integer
			 * in case the timer resolution improves
			 */
			offset = pow(2.0, 63.0)-1.0/time_freq.QuadPart;
			if (offset<=LONG_MAX-offset_time_s) {
				offset_time_s += (long) offset;
			}
			else {
				/*
				 * this problem cant be fixed, but hopefully will never occurr
				 */
				fprintf (stderr, "%s.%d Timer overflowed\n", __FILE__, __LINE__);
				return S_ts_sysTimeError;
			}
		}
		time_sec.QuadPart = time_cur.QuadPart / time_freq.QuadPart;
		time_remainder.QuadPart = time_cur.QuadPart % time_freq.QuadPart;
		if (time_sec.QuadPart > LONG_MAX-offset_time_s) {
			/*
			 * this problem cant be fixed, but hopefully will never occurr
			 */
			fprintf (stderr, "%s.%d Timer value larger than storage\n", __FILE__, __LINE__);
			return S_ts_sysTimeError;
		}

		/* add time (sec) since 1990 */
		pStamp->secPastEpoch = offset_time_s + (long)time_sec.QuadPart;
		pStamp->nsec = (long)((time_remainder.QuadPart*1000000000)/time_freq.QuadPart);

		time_prev = time_cur;

#   else
		struct timeval curtime;

		assert(pStamp != NULL);
		if (gettimeofday(&curtime, (struct timezone *)NULL) == -1)
			retStat = S_ts_sysTimeError;
		else {
			pStamp->nsec = curtime.tv_usec * 1000;
			pStamp->secPastEpoch = curtime.tv_sec - TS_EPOCH_SEC_PAST_1970;
		}
#	endif
    }

    pStamp->nsec = pStamp->nsec - (pStamp->nsec % TS_TRUNC);
    return retStat;
}

/*+/subr**********************************************************************
* NAME	tsRoundDownLocal - round stamp down to interval, based on local time
*
* DESCRIPTION
*	The time stamp is rounded down to the prior interval, based on
*	local time.  The rounding interval must be between 1 second and
*	86400 seconds (a full day).
*
* RETURNS
*	S_ts_OK, or
*	S_ts_badRoundInterval if the rounding interval is invalid
*
* EXAMPLES
* 1.	Round a time stamp back to 0000 hours, at the beginning of the day.
*	TS_STAMP	now;
*
*	tsRoundDownLocal(&now, 86400);
*
*-*/
long epicsShareAPI tsRoundDownLocal(TS_STAMP *pStamp, unsigned long interval)
{
    long	retStat=S_ts_OK;/* return status to caller */
    struct tsDetail detail;
    unsigned long seconds;

    tsStampToLocal(*pStamp, &detail);
    if (interval < 1. || interval > 86400.)
	retStat = S_ts_badRoundInterval;
    else {
	seconds = detail.seconds + 60*detail.minutes + 3600*detail.hours;
	seconds -= seconds % interval;
	detail.hours = seconds / 3600;
	detail.minutes = (seconds % 3600) / 60;
	detail.seconds = seconds % 60;
	tsStampFromLocal(pStamp, &detail);
	pStamp->nsec = 0;
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	tsRoundUpLocal - round stamp up to interval, based on local time
*
* DESCRIPTION
*	The time stamp is rounded up to the following interval, based on
*	local time.  The rounding interval must be between 1 second and
*	86400 seconds (a full day).
*
* RETURNS
*	S_ts_OK, or
*	S_ts_badRoundInterval if the rounding interval is invalid
*
* EXAMPLES
* 1.	Round a time stamp forward to 0000 hours, at the beginning of the 
*	following day.
*	TS_STAMP	now;
*
*	tsRoundUpLocal(&now, 86400);
*
*-*/
long epicsShareAPI tsRoundUpLocal(TS_STAMP *pStamp, unsigned long interval)
{
    long	retStat=S_ts_OK;/* return status to caller */
    struct tsDetail detail;
    unsigned long seconds;

    tsStampToLocal(*pStamp, &detail);
    if (interval < 1. || interval > 86400.)
	retStat = S_ts_badRoundInterval;
    else {
	seconds = detail.seconds + 60*detail.minutes + 3600*detail.hours;
	if (pStamp->nsec > 0)
	    seconds++;
	seconds += interval - 1;
	seconds -= seconds % interval;
	detail.hours = seconds / 3600;
	detail.minutes = (seconds % 3600) / 60;
	detail.seconds = seconds % 60;
	tsStampFromLocal(pStamp, &detail);
	pStamp->nsec = 0;
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	tsStampFromLocal - convert time stamp to local time
*
* DESCRIPTION
*	Converts a tsDetail structure for local time into an EPICS time
*	stamp, taking daylight savings time into consideration.
*
* RETURNS
*	void
*
* BUGS
* o	doesn't handle 0 time stamps for time zones west of Greenwich
*
*-*/
void epicsShareAPI tsStampFromLocal(TS_STAMP *pStamp, struct tsDetail *pT)
{
    long	retStat=S_ts_OK;/* return status to caller */

    TS_STAMP	stamp;		/* temp for building time stamp */
    int		year;		/* temp for year number */
    int		days;		/* temp for days since epoch */
    int		dstBegin;	/* day DST begins */
    int		dstEnd;		/* day DST ends */
    int		dst;		/* (0, 1) for DST (isn't, is) in effect */

    if (needToInitMinWest)
	tsInitMinWest();

    year = TS_EPOCH_YEAR;
    days = pT->dayYear;
    while (year < pT->year) {
	if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))
	    days += 366;
	else
	    days += 365;
	year++;
    }
    pT->dayOfWeek = (days + TS_EPOCH_WDAY_NUM) % 7;
    stamp.secPastEpoch = days * 86400;	/* converted to seconds */
    stamp.secPastEpoch += pT->hours*3600 + pT->minutes*60 + pT->seconds;
/*----------------------------------------------------------------------------
*	we now have a stamp which corresponds to the text time, BUT with
*	the assumption that the text time is standard time.  Three daylight
*	time issues must be dealt with: was the text time during DST; was
*	the text time during the `skipped' time when standard ends and DST
*	begins; and was it during the `limbo' time when DST ends and standard
*	begins.
*
*	If, for example, DST ends at 2 a.m., and 1 hour is added during DST,
*	then the limbo time begins at 1 a.m. DST and ends at 2 a.m. standard.
*----------------------------------------------------------------------------*/
    dstBegin = sunday(TS_DST_BEGIN, pT->leapYear, pT->dayYear, pT->dayOfWeek);
    dstEnd = sunday(TS_DST_END, pT->leapYear, pT->dayYear, pT->dayOfWeek);
    assert(dstBegin != dstEnd);
    dst = 0;
    if (dstBegin < dstEnd && (pT->dayYear < dstBegin || pT->dayYear > dstEnd))
	    ;		/* not DST; no action */
    else if (dstBegin > dstEnd && pT->dayYear < dstBegin && pT->dayYear>dstEnd)
	    ;		/* not DST; no action */
    else if (pT->dayYear == dstBegin) {
	if (pT->hours < TS_DST_HOUR_ON)
	    ;		/* not DST; no action */
	else if (pT->hours < TS_DST_HOUR_ON + TS_DST_HRS_ADD)
	    retStat = S_ts_timeSkippedDST;
	else
	    dst = 1;
    }
    else if (pT->dayYear != dstEnd)
	dst = 1;
    else {
	if (pT->hours >= TS_DST_HOUR_OFF + TS_DST_HRS_ADD)
	    ;	/* not DST */
	else if (pT->hours < TS_DST_HOUR_OFF)
	    dst = 1;
	else if (pT->dstOverlapChar == 'd')
	    dst = 1;
    }
/*----------------------------------------------------------------------------
*    assume no DST if tsMinWest is 600 (includes Hawaii and may be
*    exclusively Hawaii?). (WFL, 95/08/15)
*----------------------------------------------------------------------------*/
	if (tsMinWest == 600)
		dst = 0;

    if (dst)
	stamp.secPastEpoch -= TS_DST_HRS_ADD * 3600;
    stamp.secPastEpoch += tsMinWest*60;
    *pStamp = stamp;
}

/*+/subr**********************************************************************
* NAME	tsStampToLocal - convert time stamp to local time
*
* DESCRIPTION
*	Converts an EPICS time stamp into a tsDetail structure for local
*	time, taking daylight savings time into consideration.
*
* RETURNS
*	void
*
* BUGS
* o	doesn't handle 0 time stamps for time zones west of Greenwich
*
*-*/
void epicsShareAPI tsStampToLocal(TS_STAMP stamp,struct tsDetail *pT)
{
    int		dstBegin;	/* day DST begins */
    int		dstEnd;		/* day DST ends */
    int		dst;		/* (0, 1) for DST (isn't, is) in effect */

/*----------------------------------------------------------------------------
*    convert stamp to local time.  Since no dst compensation has been
*    done, this is local STANDARD time.  All dst compensation uses standard
*    time for the tests and adjustments.
*----------------------------------------------------------------------------*/
    tsStampToLocalZone(&stamp, pT);

    dstBegin = sunday(TS_DST_BEGIN, pT->leapYear, pT->dayYear, pT->dayOfWeek);
    dstEnd = sunday(TS_DST_END, pT->leapYear, pT->dayYear, pT->dayOfWeek);
    dst = 0;
    pT->dstOverlapChar = ':';
    if (dstBegin < dstEnd && (pT->dayYear < dstBegin || pT->dayYear > dstEnd))
	    ;		/* not DST; no action */
    else if (dstBegin > dstEnd && pT->dayYear < dstBegin && pT->dayYear>dstEnd)
	    ;		/* not DST; no action */
    else if (pT->dayYear == dstBegin && pT->hours < TS_DST_HOUR_ON)
	;		/* not DST; no action */
    else if (pT->dayYear != dstEnd)
	dst = 1;
    else {
	if (pT->hours >= TS_DST_HOUR_OFF) {
	    ;	/* not DST; may be 'limbo' time */
	    if (pT->hours < TS_DST_HOUR_OFF + TS_DST_HRS_ADD)
		pT->dstOverlapChar = 's';/* standard time, in 'limbo' */
	}
	else if (pT->hours >= TS_DST_HOUR_OFF - TS_DST_HRS_ADD) {
	    dst = 1;
	    pT->dstOverlapChar = 'd';/* daylight time, in 'limbo' */
	}
	else
	    dst = 1;
    }

/*----------------------------------------------------------------------------
*    assume no DST if tsMinWest is 600 (includes Hawaii and may be
*    exclusively Hawaii?). (WFL, 95/08/15)
*----------------------------------------------------------------------------*/
	if (tsMinWest == 600)
		dst = 0;

/*----------------------------------------------------------------------------
*    now, if necessary, change the time stamp to daylight and then convert
*    the resultant stamp to local time.
*----------------------------------------------------------------------------*/
    if (dst) {
	stamp.secPastEpoch += TS_DST_HRS_ADD * 3600;
	tsStampToLocalZone(&stamp, pT);
    }

    return;
}

static void tsInitMinWest()
{
    int		error=0;

    if (envGetLongConfigParam(&EPICS_TS_MIN_WEST, &tsMinWest) != 0)
	error = 1;
    else
	if (tsMinWest >  720 || tsMinWest < -720 )
	    error = 1;
    if (error) {
	    (void)printf(
"tsSubr: illegal value for %s\n\
tsSubr: default value of %ld will be used\n",
EPICS_TS_MIN_WEST.name, tsMinWest);
    }
    needToInitMinWest = 0;
}

static void tsStampToLocalZone(TS_STAMP *pStamp,struct tsDetail *pT)
{
    int		ndays;		/* number of days in this month or year */
    unsigned long secPastEpoch;	/* time from stamp, in local zone */
    int		days;		/* temp for count of days */
    int		hms;		/* left over hours, min, sec in stamp */

    if (needToInitMinWest)
	tsInitMinWest();

    assert(pStamp != NULL);
	assert( (tsMinWest >= -720 && tsMinWest <=  720 ));
	assert( (tsMinWest >= 0 && (pStamp->secPastEpoch >= (unsigned long)(tsMinWest * 60))) ||
            (tsMinWest < 0) );
    assert(pT != NULL);

/*----------------------------------------------------------------------------
*    move the time stamp from GMT to local time, then break it down into its
*    component parts
*----------------------------------------------------------------------------*/
    secPastEpoch = pStamp->secPastEpoch - tsMinWest * 60;
    hms = secPastEpoch % 86400;
    days = secPastEpoch / 86400;
    pT->seconds = hms % 60;
    pT->minutes = (hms/60) % 60;
    pT->hours = hms / 3600;
    pT->dayOfWeek = (days + TS_EPOCH_WDAY_NUM) % 7;

    pT->year=TS_EPOCH_YEAR;
    if (pT->year % 400 == 0 || (pT->year % 4 == 0 && pT->year % 100 != 0))
        ndays = 366;
    else
        ndays = 365;
    while (days > 0) {
	if (pT->year % 400 == 0 || (pT->year % 4 == 0 && pT->year % 100 != 0))
	    ndays = 366;
	else
	    ndays = 365;
	if (days >= ndays)
	    pT->year++;
	days -= ndays;
    }
    if (days < 0)
	days += ndays;
    if (ndays == 366)
	pT->leapYear = 1;
    else
	pT->leapYear = 0;
    pT->dayYear = days;

    pT->monthNum = 0;
    pT->dayMonth = days;
    while (pT->dayMonth > 0) {
	ndays = daysInMonth[pT->monthNum];
	if (pT->monthNum==1 && pT->leapYear)
	    ndays++;
	if (pT->dayMonth >= ndays)
	    pT->monthNum++;
	pT->dayMonth -= ndays;
    }
    if (pT->dayMonth < 0)
	pT->dayMonth += ndays;

    return;
}

/*+/subr**********************************************************************
* NAME	tsStampToText - convert a time stamp to text
*
* DESCRIPTION
*	A EPICS standard time stamp is converted to text.  The text
*	contains the time stamp's representation in the local time zone,
*	taking daylight savings time into account.
*
*	The required size of the caller's text buffer depends on the type
*	of conversion being requested.  The conversion types, buffer sizes,
*	and conversion formats are:
*
*	TS_TEXT_MONDDYYYY  32	Mon dd, yyyy hh:mm:ss.nano-secs
*	TS_TEXT_MMDDYY	   28	mm/dd/yy hh:mm:ss.nano-secs
*
*	If the time in the time stamp is less than or equal to the number
*	of seconds in the day, then the time stamp is assumed to be a
*	delta time.  In this case, only time is stored in the buffer,
*	omitting the date.
*
*	For any of the formats, the fractional part of the time (represented
*	as `nano-secs' above) is variable length.  If the usec and nsec part
*	of the time are both zero, only milli-seconds will be printed; or
*	else if the nsec part is zero, only milli-seconds and micro-seconds
*	will be printed.
*
*	During the period when "time repeats itself" at the switch from
*	daylight time to standard time, the ':' following the "hh" will be
*	replaced with either 'd' or 's' before or after the switch,
*	respectively.
*
* RETURNS
*	pointer to buffer
*
* NOTES
* 1.	There were several motivations for writing this routine:  First, the
*	UNIX routine wasn't particularly portable to VxWorks.  Second, the
*	UNIX routine wasn't re-entrant.  And, last, the UNIX routine didn't
*	provide special handling for "time repeating itself".
*
* EXAMPLES
* 1.	Get the local time as a time stamp and print it.
*	TS_STAMP	now;
*	char		nowText[28];
*
*	tsLocalTime(&now);
*	printf("time is %s\n", tsStampToText(&now, TS_TEXT_MMDDYY, nowText));
*
*-*/
char * epicsShareAPI
tsStampToText(pStamp, textType, textBuffer)
TS_STAMP *pStamp;	/* I pointer to time stamp */
enum tsTextType textType;/* I type of conversion desired; one of TS_TEXT_xxx */
char	*textBuffer;	/* O buffer to receive text */
{

    struct tsDetail t;		/* detailed breakdown of time stamp */
    char	fracPart[10];	/* fractional part of time */

    assert(textBuffer != NULL);

    (void)sprintf(fracPart, "%09d", pStamp->nsec);
    if (pStamp->nsec % 1000000 == 0)
	fracPart[3] = '\0';
    else if (pStamp->nsec % 1000 == 0)
	fracPart[6] = '\0';

    if (pStamp->secPastEpoch <= 86400) {
	t.hours = pStamp->secPastEpoch / 3600;
	t.minutes = (pStamp->secPastEpoch / 60) % 60;
	t.seconds = pStamp->secPastEpoch % 60;
	(void)sprintf(textBuffer, "%02d:%02d:%02d.%s",
    					t.hours, t.minutes, t.seconds,
					fracPart);
    }
    else {
	tsStampToLocal(*pStamp, &t);

	switch (textType) {
	    case TS_TEXT_MMDDYY:
		(void)sprintf(textBuffer, "%02d/%02d/%02d %02d%c%02d:%02d.%s",
					t.monthNum+1, t.dayMonth+1, t.year%100,
    					t.hours, t.dstOverlapChar, t.minutes,
					t.seconds, fracPart);
		break;
	    case TS_TEXT_MONDDYYYY:
		(void)sprintf(textBuffer, "%3.3s %02d, %4d %02d%c%02d:%02d.%s",
					&monthText[t.monthNum*3], t.dayMonth+1,
					t.year,
    					t.hours, t.dstOverlapChar, t.minutes,
					t.seconds, fracPart);
		break;
	    default:
		return NULL;
	}
    }

    return textBuffer;
}

/*+/subr**********************************************************************
* NAME	tsTextToStamp - convert text time and date into a time stamp
*
* DESCRIPTION
*	A text string is scanned to produce an EPICS standard time stamp.
*	The text can contain either a time and date (in the local time zone)
*	or a `delta' time from the present local time and date.
*
*	Several different forms of dates and times are accepted by this
*	routine, as outlined below.  In general, this routine accepts
*	strings of the form:
*
*		date time
*
*	If the date is omitted, then today's date is assumed.  (See the
*	exception for times beginning with `-'.)
*
*   Date representations:
*
*	Dates may appear in several variations.  In the forms where the
*	month is represented by letters, case is ignored; only the first
*	3 letters are checked if the month name is spelled out.  In the
*	forms where year is shown as yy, either 2 or 4 digit year can be
*	specified.
*
*	    mm/dd/yy		(yy less than epoch is taken as 20yy)
*	    mon dd, yyyy	(3 or more letters of month)
*	    dd-mon-yy		(3 or more letters of month; yy<epoch is 20yy)
*
*   Time representations:
*
*	Times may appear in several variations on a single form.  One type of
*	variation controls the units for fractional seconds.  Other
*	variations include:
*
*	    a '-' in front of a time indicates a delta time, to be subtracted
*		from the present time and date.  A date isn't allowed to
*		appear with this form of time.
*	    a 'd' before or after a time indicates that the time is to be taken
*		as `daylight savings time'.  This is generally not needed,
*		but is useful for times during the switchover FROM daylight
*		savings time; during the switchover, there is a period when
*		clock times repeat.  (The 'd' can also appear following
*		the "hh", either before the ':' or in place of the ':'.)
*	    an 's' can be similarly used to indicate that the time is to
*		be taken as standard time.  If neither 'd' nor 's' is used
*		for times during the switchover, then 's' is assumed.
*
*	    hh:mm:ss.nano-secs	(9 trailing digits give nano-seconds)
*	    hh:mm:ss.micros	(6 trailing digits give micro-seconds)
*	    hh:mm:ss.mse	(3 trailing digits give milli-seconds)
*	    hh:mm:ss
*
* RETURNS
*	S_ts_OK, or
*	S_ts_inputTextError	if the text doesn't match one of the forms
*				outlined above
*
* BUGS
* o	-time isn't implemented yet
* o	only a single daylight savings time `rule' can be specified
*
* EXAMPLES
* 1.	Get the time stamp equivalent of a text time
*	char		text[]="7/4/91 8:15:30";
*	TS_STAMP	equiv;
*
*	tsTextToStamp(&equiv, &text);
*
*-*/
long epicsShareAPI tsTextToStamp(
TS_STAMP *pStamp,	/* O time stamp corresponding to text */
char	**pCallerText)	/* IO ptr to ptr to string containing time and date */
{
    long	retStat=S_ts_OK;/* status return to caller */
    long	stat;		/* status from calls */
    struct tsDetail t;		/* detailed breakdown of text */
    TS_STAMP	stamp;		/* temp for building time stamp */
    char	*pField;	/* pointer to field */
    char	delim;		/* delimiter character */
    long	nsec;		/* temp for nano-seconds */
    int		count;		/* count from scan of next field */
    char	textbuf[TEXTBUFSIZE];
    char	*ptextbuf;
    char	**pText;

    if (needToInitMinWest)
	tsInitMinWest();
    assert(pStamp != NULL);
    assert(pCallerText != NULL);
    assert(*pCallerText != NULL);
    /*Lets be nice and not modify *pCallerText or **pCallerText */
    strncpy(textbuf,*pCallerText,TEXTBUFSIZE);
    textbuf[TEXTBUFSIZE-1] = 0;
    ptextbuf = textbuf;
    pText = &ptextbuf;
/*----------------------------------------------------------------------------
*    skip over leading white space
*----------------------------------------------------------------------------*/
    while (isspace(**pText))
	(*pText)++;

/*----------------------------------------------------------------------------
* date -- first, figure out which format of date is present.  Process each
*	format mostly on its own--final validation of day of month and
*	computing dayYear is done in common for all formats.
*----------------------------------------------------------------------------*/
    if (**pText == '-') {			/* no date--delta time */
	
    }
    else if (strchr(*pText, '/') != 0) {	/* mm/dd/yy[yy] */
	count = nextIntFieldAsInt(pText, &t.monthNum, &delim);
	if (count <= 1 || delim != '/' || t.monthNum <=0 || t.monthNum > 12)
	    retStat = S_ts_inputTextError;
	else
	    t.monthNum--;
	if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.dayMonth, &delim);
	    if (count <= 1 || delim != '/' || t.dayMonth <= 0)
		retStat = S_ts_inputTextError;
	}
	if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.year, &delim);
            if (count <= 1 || t.year < 0 || (count > 3 && t.year < TS_EPOCH_YEAR))
		retStat = S_ts_inputTextError;
	    else if (!isspace(delim))
		retStat = S_ts_inputTextError;
	}
    }
    else if (strchr(*pText, ',') != 0) {	/* mon[th] dd, yyyy */
	count = nextAlph1UCField(pText, &pField, &delim);
	if (count < 4 || delim != ' ')
	    retStat = S_ts_inputTextError;
	else {
	    for (t.monthNum=0; t.monthNum<12; t.monthNum++) {
		if (strncmp(pField, monthText+3*t.monthNum, 3) == 0)
		    break;
	    }
	    if (t.monthNum > 11)
		retStat = S_ts_inputTextError;
	}
	if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.dayMonth, &delim);
	    if (count <= 1 || delim != ',' || t.dayMonth <= 0)
		retStat = S_ts_inputTextError;
	}
	if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.year, &delim);
	    if (count < 5 || t.year < TS_EPOCH_YEAR)
		retStat = S_ts_inputTextError;
	    else if (!isspace(delim))
		retStat = S_ts_inputTextError;
	}
    }
    else if (strchr(*pText, '-') != 0) {	/* dd-mon-yy[yy] */
	count = nextIntFieldAsInt(pText, &t.dayMonth, &delim);
	if (count <= 1 || delim != '-' || t.dayMonth <= 0)
	    retStat = S_ts_inputTextError;
	if (retStat == S_ts_OK) {
	    count = nextAlph1UCField(pText, &pField, &delim);
	    if (count < 4 || delim != '-')
		retStat = S_ts_inputTextError;
	    else {
		for (t.monthNum=0; t.monthNum<12; t.monthNum++) {
		    if (strncmp(pField, monthText+3*t.monthNum, 3) == 0)
			break;
		}
		if (t.monthNum > 11)
		    retStat = S_ts_inputTextError;
	    }
	}
	if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.year, &delim);
            if (count <= 1 || t.year < 0 || (count > 3 && t.year < TS_EPOCH_YEAR))
		retStat = S_ts_inputTextError;
	    else if (!isspace(delim))
		retStat = S_ts_inputTextError;
	}
    }
    else {					/* assume date omitted */
	if ((stat = tsLocalTime(&stamp)) != S_ts_OK)
	    retStat = stat;
	else {
	    tsStampToLocal(stamp, &t);
	    t.dayMonth++;
	}
    }

/*----------------------------------------------------------------------------
*	finish processing on date.  t.year has 2 or 4 digit year, t.monthNum
*	has month (0-11), and t.dayMonth has day of month, beginning with 1.
*----------------------------------------------------------------------------*/
    if (retStat == S_ts_OK) {
	if (t.year < 100) {
	    t.year += 1900;
	    if (t.year < TS_EPOCH_YEAR)
		t.year += 100;
	}
	else if (t.year < TS_EPOCH_YEAR)
	    retStat = S_ts_inputTextError;
    }
    if (retStat == S_ts_OK) {
	if (t.year > TS_MAX_YEAR)
	    retStat = S_ts_inputTextError;
    }
    if (retStat == S_ts_OK) {
	if (t.year % 400 == 0 || (t.year % 4 == 0 && t.year % 100 != 0))
	    t.leapYear = 1;
	else
	    t.leapYear = 0;
	if (t.dayMonth <= daysInMonth[t.monthNum] ||
	     (t.monthNum == 1 && t.dayMonth <= daysInMonth[1] + t.leapYear)) {
	    t.dayMonth--;
	    t.dayYear = dayYear1stOfMon[t.monthNum] + t.dayMonth;
	    if (t.monthNum > 1)
		t.dayYear += t.leapYear;
	}
	else
	    retStat = S_ts_inputTextError;
    }

/*----------------------------------------------------------------------------
* time -- first, suppress leading white space and then check for 'd' or 's'
*	as the initial character.
*----------------------------------------------------------------------------*/
    nsec = 0;
    if (retStat == S_ts_OK) {
	while (isspace(**pText))
	    (*pText)++;
	t.dstOverlapChar = ':';
	if (**pText == 'd' || **pText == 's') {
	    t.dstOverlapChar = **pText;
	    (*pText)++;
	}
	count = nextIntFieldAsInt(pText, &t.hours, &delim);
	if (count <= 1 || t.hours < 0 || t.hours > 23)
	    retStat = S_ts_inputTextError;
	else if (delim == 'd' || delim == 's') {
	    t.dstOverlapChar = delim;
	    if (**pText == ':')
		(*pText)++;		/* skip the 'extra' delimiter */
	}
	else if (delim != ':')
	    retStat = S_ts_inputTextError;
        if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.minutes, &delim);
	    if (count <= 1 || t.minutes < 0 || t.minutes > 59)
		retStat = S_ts_inputTextError;
	    else if (delim != ':')
		retStat = S_ts_inputTextError;
	}
        if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.seconds, &delim);
	    if (count <= 1 || t.seconds < 0 || t.seconds > 59)
		retStat = S_ts_inputTextError;
	}
        if (retStat == S_ts_OK && delim == '.') {
	    count = nextIntFieldAsLong(pText, &nsec, &delim);
	    if (count <= 1 || nsec < 0)
		retStat = S_ts_inputTextError;
	    else if (count == 4) {
		if (nsec > 999)
		    retStat = S_ts_inputTextError;
		else
		    nsec *= 1000000;
	    }
	    else if (count == 7) {
		if (nsec > 999999)
		    retStat = S_ts_inputTextError;
		else
		    nsec *= 1000;
	    }
	    else if (count == 10) {
		if (nsec > 999999999)
		    retStat = S_ts_inputTextError;
	    }
	    else
		retStat = S_ts_inputTextError;
	}
	if (retStat == S_ts_OK) {
	    if (delim == 'd' || delim == 's')
		t.dstOverlapChar = delim;
	}
    }

    if (retStat == S_ts_OK) {
	tsStampFromLocal(&stamp, &t);
	stamp.nsec = nsec;
    }
    else {
	stamp.secPastEpoch = 0;
	stamp.nsec = nsec;
    }

    *pStamp = stamp;
    return retStat;
}

/*+/subr**********************************************************************
* NAME	tsTimeTextToStamp - convert text time into a time stamp
*
* DESCRIPTION
*	A text string is scanned to produce an EPICS standard time stamp.
*	The text must contain a time representation as described for
*	tsTextToStamp().  (If either 'd' or 's' occurs in the text time,
*	it is ignored.)  (Since EPICS time stamps are unsigned, the text
*	may not be signed.)
*
* RETURNS
*	S_ts_OK, or
*	S_ts_inputTextError	if the text isn't in a legal form
*
* EXAMPLES
* 1.	Get the time stamp equivalent for an elapsed time.
*	char		text[]="15:31:22.888";
*	TS_STAMP	equiv;
*
*	tsTimeTextToStamp(&equiv, &text);
*
*-*/
long epicsShareAPI
tsTimeTextToStamp(pStamp, pCallerText)
TS_STAMP *pStamp;	/* O time stamp corresponding to text */
char	**pCallerText;	/* IO ptr to ptr to string containing time and date */
{
    long	retStat=S_ts_OK;/* status return to caller */
    struct tsDetail t;		/* detailed breakdown of text */
    TS_STAMP	stamp;		/* temp for building time stamp */
    char	delim;		/* delimiter character */
    int		count;		/* count from scan of next field */
    long	nsec;		/* temp for nano-seconds */
    char	textbuf[TEXTBUFSIZE];
    char	*ptextbuf;
    char	**pText;

    assert(pStamp != NULL);
    assert(pCallerText != NULL);
    assert(*pCallerText != NULL);
    /*Lets be nice and not modify *pCallerText or **pCallerText */
    strncpy(textbuf,*pCallerText,TEXTBUFSIZE);
    textbuf[TEXTBUFSIZE-1] = 0;
    ptextbuf = textbuf;
    pText = &ptextbuf;
/*----------------------------------------------------------------------------
*    skip over leading white space
*----------------------------------------------------------------------------*/
    while (isspace(**pText))
	(*pText)++;

/*----------------------------------------------------------------------------
* time -- first, suppress leading white space and then check for 'd' or 's'
*	as the initial character.
*----------------------------------------------------------------------------*/
    stamp.secPastEpoch = 0;
    nsec = 0;
    if (retStat == S_ts_OK) {
	while (isspace(**pText))
	    (*pText)++;
	t.dstOverlapChar = ':';
	if (**pText == 'd' || **pText == 's') {
	    t.dstOverlapChar = **pText;
	    (*pText)++;
	}
	count = nextIntFieldAsInt(pText, &t.hours, &delim);
	if (count <= 1 || t.hours < 0 || t.hours > 23)
	    retStat = S_ts_inputTextError;
	else if (delim == 'd' || delim == 's') {
	    t.dstOverlapChar = delim;
	    if (**pText == ':')
		(*pText)++;		/* skip the 'extra' delimiter */
	}
	else if (delim != ':')
	    retStat = S_ts_inputTextError;
        if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.minutes, &delim);
	    if (count <= 1 || t.minutes < 0 || t.minutes > 59)
		retStat = S_ts_inputTextError;
	    else if (delim != ':')
		retStat = S_ts_inputTextError;
	}
        if (retStat == S_ts_OK) {
	    count = nextIntFieldAsInt(pText, &t.seconds, &delim);
	    if (count <= 1 || t.seconds < 0 || t.seconds > 59)
		retStat = S_ts_inputTextError;
	}
        if (retStat == S_ts_OK && delim == '.') {
	    count = nextIntFieldAsLong(pText, &nsec, &delim);
	    if (count <= 1 || nsec < 0)
		retStat = S_ts_inputTextError;
	    else if (count == 4) {
		if (nsec > 999)
		    retStat = S_ts_inputTextError;
		else
		    nsec *= 1000000;
	    }
	    else if (count == 7) {
		if (nsec > 999999)
		    retStat = S_ts_inputTextError;
		else
		    nsec *= 1000;
	    }
	    else if (count == 10) {
		if (nsec > 999999999)
		    retStat = S_ts_inputTextError;
	    }
	    else
		retStat = S_ts_inputTextError;
	}
    }

/*----------------------------------------------------------------------------
* convert tsDetail structure into a time stamp, after doing a little more
*	refinement of the tsDetail structure
*----------------------------------------------------------------------------*/
    if (retStat == S_ts_OK) {
	stamp.secPastEpoch += t.hours*3600 + t.minutes*60 + t.seconds;
	stamp.nsec = nsec;
    }

    *pStamp = stamp;
    return retStat;
}

/*+/mod***********************************************************************
* TITLE	nextFieldSubr.c - text field scanning routines
*
* GENERAL DESCRIPTION
*	The routines in this module provide for scanning fields in text
*	strings.  They can be used as the basis for parsing text input
*	to a program.
*
* QUICK REFERENCE
* char	(*pText)[];
* char	(*pField)[];
* char	*pDelim;
*
*   int  nextAlph1UCField(	<>pText,   >pField,   >pDelim		)
*   int  nextIntField(		<>pText,   >pField,   >pDelim		)
*   int  nextIntFieldAsInt(	<>pText,   >pIntVal,  >pDelim		)
*   int  nextIntFieldAsLong(	<>pText,   >pLongVal, >pDelim		)
*
* DESCRIPTION
*	The input text string is scanned to identify the beginning and
*	end of a field.  At return, the input pointer points to the character
*	following the delimiter and the delimiter has been returned through
*	its pointer; the field contents are `returned' either as a pointer
*	to the first character of the field or as a value returned through
*	a pointer.
*
*	In the input string, a '\0' is stored in place of the delimiter,
*	so that standard string handling tools can be used for text fields.
*
*	nextAlph1UCField	scans the next alphabetic field, changes
*				the first character to upper case, and
*				changes the rest to lower case
*	nextIntField		scans the next integer field
*	nextIntFieldAsInt	scans the next integer field as an int
*	nextIntFieldAsLong	scans the next integer field as a long
*
* RETURNS
*	count of characters in field, including the delimiter.  A special
*	case exists when only '\0' is encountered; in this case 0 is returned.
*	(For quoted alpha strings, the count will include the " characters.)
*
* BUGS
* o	use of type checking macros isn't protected by isascii()
*
* SEE ALSO
*	tsTextToStamp()
*
* NOTES
* 1.	fields involving alpha types consider underscore ('_') to be
*	alphabetic.
*
* EXAMPLE
*	char text[]="process 30 samples"
*	char *pText;	pointer into text string
*	char *pCmd;	pointer to first field, to use as a command
*	int  count;	value of second field, number of items to process
*	char *pUnits;	pointer to third field, needed for command processing
*	int  length;	length of field
*	char delim;	delimiter for field
*
*	pText = text;
*	if (nextIntFieldAsInt(&pText, &count, &delim) <= 1)
*	    error action if empty field
*	printf("command=%s, count=%d, units=%s\n", pCmd, count, pUnits);
*
*-***************************************************************************/

/*-----------------------------------------------------------------------------
*    the preamble skips over leading white space, stopping either at
*    end of string or at a non-white-space character.  If EOS is encountered,
*    then the appropriate return conditions are set up and a return 0 is done.
*----------------------------------------------------------------------------*/
#define NEXT_PREAMBLE \
    char	*pDlm;		/* pointer to field delimiter */ \
    int		count;		/* count of characters (plus delim) */ \
 \
    assert(ppText != NULL); \
    assert(*ppText != NULL); \
    assert(ppField != NULL); \
    assert(pDelim != NULL); \
 \
    if (**ppText == '\0') { \
	*ppField = *ppText; \
	*pDelim = **ppText; \
	return 0; \
    } \
    while (**ppText != '\0' && isspace(**ppText)) \
	(*ppText)++;		/* skip leading white space */ \
    pDlm = *ppField = *ppText; \
    if (*pDlm == '\0') { \
	*pDelim = **ppText; \
	return 0; \
    } \
    count = 1;			/* include delimiter in count */
/*-----------------------------------------------------------------------------
*    the postamble is called for each character in the field.  It moves
*    the pointers and increments the count.  If the loop terminates, then
*    the caller is informed of the delimiter and the source character
*    string has '\0' inserted in place of the delimiter, so that the field
*    is now a proper '\0' terminated string.
*----------------------------------------------------------------------------*/
#define NEXT_POSTAMBLE \
	pDlm++; \
	count++; \
    } \
    *pDelim = *pDlm; \
    *ppText = pDlm; \
    if (*pDlm != '\0') { \
	(*ppText)++;		/* point to next available character */ \
	*pDlm = '\0'; \
    }

static int nextAlph1UCField(char **ppText,char **ppField, char *pDelim)
{
    NEXT_PREAMBLE
    while (isalpha(*pDlm) || *pDlm == '_') {
	if (count == 1) {
	    if (islower(*pDlm))
		*pDlm = toupper(*pDlm);
	}
	else {
	    if (isupper(*pDlm))
		*pDlm = tolower(*pDlm);
	}
	NEXT_POSTAMBLE
    return count;
}

static int nextIntField(char **ppText,char **ppField,char *pDelim)
{
    NEXT_PREAMBLE
    while (isdigit(*pDlm) || ((*pDlm=='-' || *pDlm=='+') && count==1)) {
	NEXT_POSTAMBLE
    return count;
}

static int nextIntFieldAsInt(char **ppText,int *pIntVal,char *pDelim)
{
    char	*pField;	/* pointer to field */
    int		count;		/* count of char in field, including delim */

    assert(pIntVal != NULL);

    count = nextIntField(ppText, &pField, pDelim);
    if (count > 1) {
	if (sscanf(pField, "%d", pIntVal) != 1)
	    assert(0);
    }

    return count;
}

static int nextIntFieldAsLong(char **ppText,long *pLongVal,char	*pDelim)
{
    char	*pField;	/* pointer to field */
    int		count;		/* count of char in field, including delim */

    assert(pLongVal != NULL);

    count = nextIntField(ppText, &pField, pDelim);
    if (count > 1) {
	if (sscanf(pField, "%ld", pLongVal) != 1)
	    assert(0);
    }

    return count;
}
