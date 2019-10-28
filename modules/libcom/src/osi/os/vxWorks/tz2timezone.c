/*************************************************************************\
* Copyright (c) 2009 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* Copyright (c) 2019 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Authors: Larry Hoff, Andrew Johnson <anj@anl.gov>
 */

/*
 * This file exports a single function "int tz2timezone(void)" which reads
 * the TZ environment variable (defined by POSIX) and converts it to the
 * TIMEZONE environment variable defined by ANSI. The latter is used by
 * VxWorks "time" functions, is largely deprecated in other computing
 * environments, has limitations, and is difficult to maintain. This holds
 * out the possibility of "pretending" that VxWorks supports "TZ" - until
 * such time as it actually does.
 *
 * For simplicity, only the "POSIX standard form" of TZ will be supported.
 * Even that is complicated enough (see following spec). Furthermore,
 * only the "M" form of DST start and stop dates are supported.
 *
 * TZ = zone[-]offset[dst[offset],start[/time],end[/time]]
 *
 * zone
 *     A three or more letter name for the timezone in normal (winter) time.
 *
 * [-]offset
 *     A signed time giving the offset of the time zone westwards from
 *     Greenwich. The time has the form hh[:mm[:ss]] with a one of two
 *     digit hour, and optional two digit minutes and seconds.
 *
 * dst
 *     The name of the time zone when daylight saving is in effect. It may
 *     be followed by an offset giving how big the adjustment is, required
 *     if different than the default of 1 hour.
 *
 * start/time,end/time
 *     Specify the start and end of the daylight saving period. The start
 *     and end fields indicate on what day the changeover occurs, and must
 *     be in this format:
 *
 *     Mm.n.d
 *         This indicates month m, the n-th occurrence of day d, where
 *             1 <= m <= 12,  1 <= n <= 5,  0 <= d <= 6, 0=Sunday
 *         The 5th occurrence means the last occurrence of that day in a
 *         month. So M4.1.0 is the first Sunday in April, M9.5.0 is the
 *         last Sunday in September.
 *
 *     The time field indicates what hour the changeover occurs on the given
 *     day (TIMEZONE only supports switching on the hour).
 *
 */

#include <vxWorks.h>
#include <envLib.h>   /* getenv() */
#include <stdio.h>    /* printf() */
#include <string.h>   /* strchr() */
#include <ctype.h>    /* isalpha() */

#include <epicsTime.h>

/* for reference: TZ syntax, example, and TIMEZONE example
 * std offset dst [offset],start[/time],end[/time]
 *    CST6CDT5,M3.2.0,M11.1.0
 *    EST+5EDT,M4.1.0/2,M10.5.0/2
 *
 * std <unused> offset start stop
 *    TIMEZONE=EST::300:030802:110102
 */

static int extractDate(const char *tz, struct tm *current, char *s)
{
    static const int startdays[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    static const int molengths[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    int month, week, weekday, hour=2; /* default=2AM */
    int jan1wday, wday, mday;

    /* Require 'M' format */
    if (*++tz != 'M') {
        printf("tz2timezone: Unsupported date type, need 'M' format\n");
        return ERROR;
    }
    tz++;

    if (sscanf(tz, "%d.%d.%d/%d", &month, &week, &weekday, &hour) < 3)
        return ERROR; /* something important missing */

    if (month == 0 || month>12 ||
        week < 1 || week > 5 ||
        weekday < 0 || weekday > 6 ||
        hour < 0 || hour > 23)
        return ERROR;

    /* Now for some brute-force calendar calculations... */
    /* start month is in "month", and the day is "weekday", but
       we need to know when that weekday first occurs in that month */
    /* Let's start with weekday on Jan. 1 */
    jan1wday = (7 + current->tm_wday - (current->tm_yday % 7)) % 7;

    /* We need to know if it is a leap year (and if it matters) */
    /* Let's assume that we're working with a date between 1901 and 2099,
       that way we don't have to think about the "century exception".
       If this code is still running (unchanged) in 2100, I'll be stunned
       (and 139 years old) */
    wday = (jan1wday + startdays[month-1] +
        ((month > 2 && (current->tm_year % 4 == 0)) ? 1 : 0)) % 7;

    /* Let's see on what day-of-the-month the first target weekday occurs
       (counting from 1). The result is a number between 1 and 7, inclusive. */
    mday = 1 + ((7 + weekday - wday) % 7);

    /* Next, we add the week offset. If we overflow the month, we subtract
       one week */
    mday += 7 * (week - 1);
    if (mday > molengths[month-1])
        mday -= 7;

    /* Should I handle Feb 29? I'm willing to gamble that no one in their right
       mind would schedule a time change to occur on Feb. 29. If so, we'll be a
       week early */
    sprintf(s, "%02d%02d%02d", month, mday, hour);

    return OK;
}


static const char *getTime(const char *s, int *time)
{
    /* Time format is [+/-]hh[:mm][:ss] followed by the next zone name */

    *time = 0;

    if (!isdigit((int) s[0]))
        return s; /* no number here... */

    if (!isdigit((int) s[1])) { /* single digit form */
        *time = s[0] - '0';
        return s + 1;
    }

    if (isdigit((int) s[1])) { /* two digit form */
        *time = 10 * (s[0] - '0') + (s[1] - '0');
        return s + 2;
    }

    return s; /* does not follow supported form */
}

int tz2timezone(void)
{
    const char *tz = getenv("TZ");
    /* Spec. says that zone names must be at least 3 chars.
     * I've never seen a longer zone name, but I'll allocate
     * 40 chars. If you live in a zone with a longer name,
     * you may want to think about the benefits of relocation.
     */
    char zone[40];
    char start[10], stop[10]; /* only really need 7 bytes now */
    int hours = 0, minutes = 0, sign = 1;
    /* This is more than enough, even with a 40-char zone
     * name, and 4-char offset.
     */
    char timezone[100];
    int i = 0; /* You *always need an "i" :-) */
    epicsTimeStamp now;
    struct tm current;

    /* First let's get the current time. We need the year to
     * compute the start/stop dates for DST.
     */
    if (epicsTimeGetCurrent(&now) ||
        epicsTimeToTM(&current, NULL, &now))
        return ERROR;

    /* Make sure TZ exists.
     * Spec. says that ZONE must be at least 3 chars.
     */
    if ((!tz) || (strlen(tz) < 3))
        return ERROR;

    /* OK, now a bunch of brute-force parsing. My brain hurts if
     * I try to think of an elegant regular expression for the
     * string.
     */

    /* Start extracting zone name, must be alpha */
    while ((i < sizeof(zone) - 1) && isalpha((int) *tz)) {
        zone[i++] = *tz++;
    }
    if (i < 3)
        return ERROR; /* Too short, not a real zone name? */

    zone[i] = 0; /* Nil-terminate (for now) */

    /* Now extract offset time. The format is [+/-]hh[:mm[:ss]]
     * Recall that TIMEZONE doesn't support seconds....
     */
    if (*tz == '-') {
        sign = -1;
        tz++;
    }
    else if (*tz == '+') {
        tz++;
    }

    /* Need a digit now */
    if (!isdigit((int) *tz))
        return ERROR;

    /* First get the hours */
    tz = getTime(tz, &hours);
    if (hours > 24)
        return ERROR;

    if (*tz == ':') { /* There is a minutes part */
        /* Need another digit now */
        if (!isdigit((int) *++tz))
            return ERROR;

        /* Extract the minutes */
        tz = getTime(tz, &minutes);
        if (minutes > 60)
            return ERROR;

        /* Skip any seconds part */
        if (*tz == ':') {
            int seconds;
            tz = getTime(tz + 1, &seconds);
        }
    }

    /* Extract any DST zone name, must be alpha */
    if (isalpha((int) *tz)) {
        zone[i++] = '/'; /* Separate the names */

        while ((i < sizeof(zone) - 1) && isalpha((int) *tz)) {
            zone[i++] = *tz++;
        }
        zone[i] = 0; /* Nil-terminate */
    }

    minutes += hours * 60;
    minutes *= sign;

    /* Look for start/stop dates - require neither or both */
    tz = strchr(tz, ',');
    if (!tz) {  /* No daylight savings time here */
        /* Format the env. variable */
        sprintf(timezone, "TIMEZONE=%s::%d", zone, minutes);
    }
    else {
        if (extractDate(tz, &current, start) != OK)
            return ERROR;

        tz = strchr(tz + 1, ',');
        if (!tz)
            return ERROR;
        if (extractDate(tz, &current, stop) != OK)
            return ERROR;

        /* Format the env. variable */
        sprintf(timezone, "TIMEZONE=%s::%d:%s:%s", zone, minutes, start, stop);
    }

    /* Make it live! */
    putenv(timezone);

    return OK;
}
