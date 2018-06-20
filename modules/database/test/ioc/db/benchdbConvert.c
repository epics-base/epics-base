/*************************************************************************\
* Copyright (c) 2013 Brookhaven Science Assoc, as Operator of Brookhaven
*     National Laboratory.
\*************************************************************************/
#include "string.h"

#include "cantProceed.h"
#include "dbAddr.h"
#include "dbConvert.h"
#include "dbDefs.h"
#include "epicsTime.h"
#include "epicsMath.h"
#include "epicsAssert.h"

#include "epicsUnitTest.h"
#include "testMain.h"

typedef struct {
    size_t nelem, niter;

    short *output;
    short *input;

    GETCONVERTFUNC getter;

    DBADDR addr;
} testData;

static long runRep(testData *D)
{
    size_t i;

    for(i=0; i<D->niter; i++) {
        D->getter(&D->addr, D->output, D->nelem, D->nelem, 0);
    }
    return 0;
}

static void runBench(size_t nelem, size_t niter, size_t nrep)
{
    size_t i;
    testData tdat;
    double *reptimes;
    testDiag("Using %lu element arrays.",(unsigned long)nelem);
    testDiag("run %lu reps with %lu iterations each",
             (unsigned long)nrep, (unsigned long)niter);

    reptimes = callocMustSucceed(nrep, sizeof(*reptimes), "runBench");
    tdat.output = callocMustSucceed(nelem, sizeof(*tdat.output), "runBench");
    tdat.input = callocMustSucceed(nelem, sizeof(*tdat.input), "runBench");

    tdat.nelem = nelem;
    tdat.niter = niter;

    tdat.getter = dbGetConvertRoutine[DBF_SHORT][DBF_SHORT];

    memset(&tdat.addr, 0, sizeof(tdat.addr));
    tdat.addr.field_type = DBF_SHORT;
    tdat.addr.field_size = nelem*sizeof(*tdat.input);
    tdat.addr.no_elements = nelem;
    tdat.addr.pfield = (void*)tdat.input;

    for(i=0; i<nelem; i++)
        tdat.input[i] = (short)i;

    for(i=0; i<nrep; i++)
    {
        epicsTimeStamp start, stop;

        if(epicsTimeGetCurrent(&start)!=epicsTimeOK) {
            testAbort("Failed to get timestamp");
            goto done;
        }

        if(runRep(&tdat))
            goto done;

        if(epicsTimeGetCurrent(&stop)!=epicsTimeOK) {
            testAbort("Failed to get timestamp");
            goto done;
        }

        reptimes[i] = epicsTimeDiffInSeconds(&stop, &start);

        testDiag("%lu bytes in %.03f ms.  %.1f MB/s",
                 (unsigned long)(nelem*niter),
                 reptimes[i]*1e3,
                 (nelem*niter)/reptimes[i]/1e6);
    }

    {
        double sum=0, sum2=0, mean;
        for(i=0; i<nrep; i++) {
            sum += reptimes[i];
            sum2 += reptimes[i]*reptimes[i];
        }

        mean = sum/nrep;
        testDiag("Final: %.04f ms +- %.05f ms.  %.1f MB/s  (for %lu elements)",
                 mean*1e3,
                 sqrt(sum2/nrep - mean*mean)*1e3,
                 (nelem*niter)/mean/1e6,
                 (unsigned long)nelem);
    }

done:
    free(reptimes);
    free(tdat.input);
    free(tdat.output);
}

MAIN(benchdbConvert)
{
    testPlan(0);
    runBench(1, 10000000, 10);
    runBench(2,  5000000, 10);
    runBench(10, 1000000, 10);
    runBench(100, 100000, 10);
    runBench(10000, 1000, 10);
    runBench(100000, 100, 10);
    runBench(1000000, 10, 10);
    runBench(10000000, 1, 10);
    return testDone();
}
