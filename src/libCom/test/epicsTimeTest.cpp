#include <stdio.h>
#include <time.h>
#include <math.h>

#include "epicsTime.h"
#include "epicsThread.h"
#include "errlog.h"

extern "C" {
int epicsTimeTest (void);
}

int epicsTimeTest (void)
{
    unsigned i, errors, sum_errors=0, sum_errloops=0;
    epicsTime begin = epicsTime::getCurrent();
    const unsigned wasteTime = 100000u;
    const int nTimes = 100;
    epicsTimeStamp stamp;
    struct timespec ts;
    struct tm tmAnsi;
    tm_nano_sec ansiDate;
    unsigned long nanoSec;
    double diff;

    printf ("epicsTime Test (%3d loops)\n========================\n\n", nTimes);

    for (int iTimes=0; iTimes < nTimes; ++iTimes) {
        for (i=0; i<wasteTime; i++) {
            epicsTime tmp = epicsTime::getCurrent();
        }

        epicsTime end = epicsTime::getCurrent();

        diff = end - begin;

        if (iTimes == 0) {
            printf ("Time per call to epicsTime::getCurrent() "
                "(%d calls) = %6.3f usec\n\n", wasteTime,
                diff*1e6/wasteTime);
            stamp = begin;
            ansiDate = begin;
            ts = begin;
            printf ("The following should be your local time\ndisplayed using "
                    "four different internal representations:\n\n");
            epicsTimeToTM (&tmAnsi, &nanoSec, &stamp);

            printf ("epicsTimeStamp = %s %lu nSec \n", asctime(&tmAnsi), nanoSec);
            printf ("struct tm = %s %f\n", asctime(&ansiDate.ansi_tm), 
                ansiDate.nSec/(double)epicsTime::nSecPerSec);
            tmAnsi = *localtime (&ts.tv_sec);
            printf ("struct timespec = %s %f\n", asctime(&ansiDate.ansi_tm), 
                ts.tv_nsec/(double)epicsTime::nSecPerSec);
            begin.show (0);
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
        if (!(end-begin==diff)) {
            printf ("#%3d: Failed end-begin==diff by %12.9f\n",
                    iTimes, fabs(end-begin-diff));
            errors += 1;
        }

        begin += diff;
        if (!(begin==end)) {
            printf ("#%3d: Failed (begin+=diff)==end by %12.9f\n",
                    iTimes, fabs(begin-end));
            errors += 1;
        }

        begin -= diff;
        if (!(begin+diff==end)) {
            printf ("#%3d: Failed begin+diff==end by %12.9f\n",
                    iTimes, fabs(begin+diff-end));
            errors += 1;
        }

        //
        // test struct tm conversions
        //
        ansiDate = begin;
        begin = ansiDate;
        if (!(begin+diff==end)) {
            printf ("#%3d: Failed begin+diff==end "
                    "after tm conversions by %12.9f\n",
                    iTimes, fabs(begin+diff-end));
            errors += 1;
        }

        //
        // test struct timespec conversion
        //
        ts = begin;
        begin = ts;
        if (!(begin+diff==end)) {
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

    printf ("epicsTime Test Summary: %d errors found "
            "in %d out of %d loops.\n",
            sum_errors, sum_errloops, nTimes);

    return (sum_errors?1:0);
}
