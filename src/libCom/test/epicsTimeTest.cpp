/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Authors: Jeff HIll and Marty Kraimer
 */
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <limits.h>

#include "epicsTime.h"
#include "epicsThread.h"
#include "errlog.h"

extern "C" {
int epicsTimeTest (void);
}

struct l_fp { /* NTP time stamp */
    epicsUInt32 l_ui; /* sec past NTP epoch */
    epicsUInt32 l_uf; /* fractional seconds */
};

epicsTime useSomeCPU;

static const unsigned mSecPerSec = 1000u;
static const unsigned uSecPerSec = 1000u * mSecPerSec;
static const unsigned nSecPerSec = 1000u * uSecPerSec;

void testStringConversion()
{
    char buf[64];
    epicsTime uninit;

    const char * pFormat = "%a %b %d %Y %H:%M:%S.%f";
    uninit.strftime ( buf, sizeof ( buf ), pFormat );
    printf ("Uninitialized using \"%s\" %s\n", pFormat, buf );

    epicsTime current = epicsTime::getCurrent();

    pFormat = "%a %b %d %Y %H:%M:%S.%f";
    current.strftime ( buf, sizeof ( buf ), pFormat );
    printf ("using \"%s\" %s\n", pFormat, buf );

    pFormat = "%a %b %d %Y %H:%M:%S.%4f";
    current.strftime ( buf, sizeof ( buf ), pFormat );
    printf ("using \"%s\" %s\n", pFormat, buf );

    pFormat = "%a %b %d %Y %H:%M:%S.%05f";
    current.strftime ( buf, sizeof ( buf ), pFormat );
    printf ("using \"%s\" %s\n", pFormat, buf );

}

int epicsTimeTest (void)
{
    unsigned errors, sum_errors=0, sum_errloops=0;
    const epicsTime begin = epicsTime::getCurrent();
    const unsigned wasteTime = 10000000u;
    const int nTimes = 100;
    epicsTimeStamp stamp;
    struct timespec ts;
    struct tm tmAnsi;
    local_tm_nano_sec ansiDate;
    gm_tm_nano_sec ansiDateGMT;
    unsigned long nanoSec;

    {
        epicsTime tsi = epicsTime::getCurrent ();
        l_fp ntp = tsi;
        epicsTime tsf = ntp;
        const double diff = fabs ( tsf - tsi );
        // the difference in the precision of the two time formats
        static const double precisionNTP = 1.0 / ( 1.0 + 0xffffffff );
        static const double precisionEPICS = 1.0 / nSecPerSec;
        assert ( diff <= precisionEPICS + precisionNTP );
    }

    printf ("epicsTime Test (%3d loops)\n========================\n\n", nTimes);

    for (int iTimes=0; iTimes < nTimes; ++iTimes) {
        for (unsigned i=0; i<wasteTime; i++) {
            useSomeCPU = epicsTime::getCurrent();
        }

        const epicsTime end = epicsTime::getCurrent();

        const double diff = end - begin;

        if (iTimes == 0) {
            printf ("Time per call to epicsTime::getCurrent() "
                "(%d calls) = %6.3f usec\n\n", wasteTime,
                diff*1e6/wasteTime);
            stamp = begin;
            ansiDate = begin;
            ansiDateGMT = begin;
            ts = begin;
            printf ("The following should be your local time\ndisplayed using "
                    "four different internal representations:\n\n");
            epicsTimeToTM (&tmAnsi, &nanoSec, &stamp);

            printf ("epicsTimeStamp = %s %lu nSec \n", asctime(&tmAnsi), nanoSec);
            printf ("local time zone struct tm = %s %f\n", asctime(&ansiDate.ansi_tm), 
                ansiDate.nSec/(double)nSecPerSec);
            printf ("struct timespec = %s %f\n", asctime(&ansiDate.ansi_tm), 
                ts.tv_nsec/(double)nSecPerSec);
            printf ("UTC struct tm = %s %f\n", asctime(&ansiDateGMT.ansi_tm), 
                ansiDateGMT.nSec/(double)nSecPerSec);
            begin.show (100);
            printf ("\n");
        } else {
            if (iTimes % 10 == 0)
                printf (" ... now at loop %3d\n", iTimes);
        }   

        epicsTime copy = end;
        errors = 0;

        if (!(copy==end)) {
            printf ("#%3d: Failed copy==end by %12.9f\n",
                iTimes, fabs(copy-end));
            errors += 1;
        }
        if (!(copy<=end)) {
            printf ("#%3d: Failed copy<=end by %12.9f\n",
                iTimes, (copy-end));
            errors += 1;
        }
        if (!(copy>=end)) {
            printf ("#%3d: Failed copy>=end by %12.9f\n",
                iTimes, (end-copy));
            errors += 1;
        }
        if (!(end>begin)) {
            printf ("#%3d: Failed end>begin by %12.9f\n",
                    iTimes, (begin-end));
            errors += 1;
        }
        if (!(end>=begin)) {
            printf ("#%3d: Failed end>=begin by %12.9f\n",
                    iTimes, (begin-end));
            errors += 1;
        }
        if (!(begin<end)) {
            printf ("#%3d: Failed begin<end by %12.9f\n",
                    iTimes, (begin-end));
            errors += 1;
        }
        if (!(begin<=end)) {
            printf ("#%3d: Failed begin<=end by %12.9f\n",
                    iTimes, (begin-end));
            errors += 1;
        }
        if (!(begin!=end)) {
            printf ("#%3d: Failed begin!=end\n",iTimes);
            errors += 1;
        }
        const double diff2 = end - begin;
        if (!(diff2==diff)) {
            printf ("#%3d: Failed end-begin==diff by %g\n",
                    iTimes, fabs(diff2-diff));
            errors += 1;
        }

        epicsTime end2 = begin;
	    end2 += diff;
        if (!(end2==end)) {
            printf ("#%3d: Failed (begin+=diff)==end by %12.9f\n",
                    iTimes, fabs(begin-end));
            errors += 1;
        }

        end2 -= diff;
        if (!(end2+diff==end)) {
            printf ("#%3d: Failed begin+diff==end by %12.9f\n",
                    iTimes, fabs(begin+diff-end));
            errors += 1;
        }

        //
        // test struct tm conversions
        //
        ansiDate = begin;
	    epicsTime beginANSI = ansiDate;
        if (!(beginANSI+diff==end)) {
            printf ("#%3d: Failed begin+diff==end "
                    "after tm conversions by %12.9f\n",
                    iTimes, fabs(begin+diff-end));
            errors += 1;
        }

        //
        // test struct timespec conversion
        //
        ts = begin;
        epicsTime beginTS = ts;
        if (!(beginTS+diff==end)) {
            printf ("#%3d: Failed begin+diff==end "
                    "after timespec conversions by %12.9f\n",
                    iTimes, fabs(begin+diff-end));
            errors += 1;
        }
        
        if (errors) {
            printf ("#%3d:   begin ", iTimes); begin.show(0);
            printf ("#%3d:   end   ", iTimes); end.show(0);
            printf ("#%3d:   diff  %12.9f\n\n", iTimes, diff);
            sum_errors += errors;
            sum_errloops += 1;
        }
    }

    printf ("epicsTime test complete. Summary: %d errors found "
            "in %d out of %d loops.\n",
            sum_errors, sum_errloops, nTimes);

    return (sum_errors?1:0);
}
