#include <stdio.h>
#include <time.h>

#include "osiTime.h"
#include "osiThread.h"
#include "errlog.h"

extern "C" {
int osiTimeTest (void);
}

int osiTimeTest (void)
{
    unsigned i;
    osiTime begin = osiTime::getCurrent();
    const unsigned wasteTime = 100000u;
    const int nTimes = 100;
    TS_STAMP stamp;
    struct timespec ts;
    struct tm tmAnsi;
    tm_nano_sec ansiDate;
    unsigned long nanoSec;
    double diff;

    for(int iTimes=0; iTimes < nTimes; ++iTimes) {
        for (i=0; i<wasteTime; i++) {
        	osiTime tmp = osiTime::getCurrent();
        }

        osiTime end = osiTime::getCurrent();

        diff = end - begin;
        if(iTimes==0)
             printf ("elapsed per call to osiTime::getCurrent() = %f\n", 
        	diff/wasteTime);

        stamp = begin;
        ansiDate = begin;
        ts = begin;

        tsStampToTM (&tmAnsi, &nanoSec, &stamp);
        if(iTimes==0)
            printf ("TS_STAMP = %s %lu nSec \n", asctime(&tmAnsi), nanoSec);

        if(iTimes==0)
            printf ("struct tm = %s %f\n", asctime(&ansiDate.ansi_tm), 
        	ansiDate.nSec/(double)osiTime::nSecPerSec);
        tmAnsi = *localtime (&ts.tv_sec);
        if(iTimes==0)
            printf ("struct timespec = %s %f\n", asctime(&ansiDate.ansi_tm), 
        	ts.tv_nsec/(double)osiTime::nSecPerSec);
        if(iTimes==0) begin.show (0);

        osiTime copy = end;
        if(!(copy==end)) printf("copy==end %d\n",iTimes);
        if(!(copy<=end)) printf("copy<=end %d\n",iTimes);
        if(!(copy>=end)) printf("copy>=end %d\n",iTimes);
        if(!(end>begin)) printf("end>begin %d\n",iTimes);
        if(!(end>=begin)) printf("end>=begin %d\n",iTimes);
        if(!(begin<end)) printf("begin<end %d\n",iTimes);
        if(!(begin<=end)) printf("begin<=end %d\n",iTimes);
        if(!(begin!=end)) printf("begin!=end %d\n",iTimes);
        if(!(end-begin==diff)) printf("end-begin==diff %d\n",iTimes);
        begin += diff;
        if(!(begin==end)) printf("begin==end %d\n",iTimes);
        begin -= diff;
        if(!(begin+diff==end)) printf("begin+diff==end %d\n",iTimes);

        //
        // test struct tm conversions
        //
        ansiDate = begin;
        begin = ansiDate;
        if(!(begin+diff==end)) printf("begin+diff==end %d\n",iTimes);

        //
        // test struct timespec conversion
        //
        ts = begin;
        begin = ts;
        if(!(begin+diff==end)) printf("begin+diff==end %d\n",iTimes);

    }
    return(0);
}
