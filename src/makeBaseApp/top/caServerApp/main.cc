/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "envDefs.h"

#include "exServer.h"
#include "fdManager.h"

//
// main()
// (example single threaded ca server tool main loop)
//
extern int main (int argc, const char **argv)
{
    epicsTime   begin (epicsTime::getCurrent());
    exServer    *pCAS;
    unsigned    debugLevel = 0u;
    double      executionTime = 0.0;
    char        pvPrefix[128] = "";
    unsigned    aliasCount = 1u;
    unsigned    scanOn = true;
    unsigned    syncScan = true;
    char        arraySize[64] = "";
    bool        forever = true;
    int         i;

    for ( i = 1; i < argc; i++ ) {
        if ( sscanf(argv[i], "-d %u", & debugLevel ) == 1 ) {
            continue;
        }
        if ( sscanf ( argv[i],"-t %lf", & executionTime ) == 1 ) {
            forever = false;
            continue;
        }
        if ( sscanf ( argv[i], "-p %127s", pvPrefix ) == 1 ) {
            continue;
        }
        if ( sscanf ( argv[i],"-c %u", & aliasCount ) == 1 ) {
            continue;
        }
        if ( sscanf ( argv[i],"-s %u", & scanOn ) == 1 ) {
            continue;
        }
        if ( sscanf ( argv[i],"-a %63s", arraySize ) == 1 ) {
            continue;
        }
        if ( sscanf ( argv[i],"-ss %u", & syncScan ) == 1 ) {
            continue;
        }
        printf ("\"%s\"?\n", argv[i]);
        printf (
            "usage: %s [-d<debug level> -t<execution time> -p<PV name prefix> " 
            "-c<numbered alias count> -s<1=scan on (default), 0=scan off> "
            "-ss<1=synchronous scan (default), 0=asynchronous scan>]\n", 
            argv[0]);

        return (1);
    }

    if ( arraySize[0] != '\0' ) {
        epicsEnvSet ( "EPICS_CA_MAX_ARRAY_BYTES", arraySize );
    }

    try {
        pCAS = new exServer ( pvPrefix, aliasCount, 
            scanOn != 0, syncScan == 0 );
    }
    catch ( ... ) {
        errlogPrintf ( "Server initialization error\n" );
        errlogFlush ();
        return (-1);
    }

    pCAS->setDebugLevel(debugLevel);

    if ( forever ) {
        //
        // loop here forever
        //
        while (true) {
            fileDescriptorManager.process(1000.0);
        }
    }
    else {
        double delay = epicsTime::getCurrent() - begin;

        //
        // loop here untill the specified execution time
        // expires
        //
        while ( delay < executionTime ) {
            fileDescriptorManager.process ( executionTime - delay );
            delay = epicsTime::getCurrent() - begin;
        }
    }
    //pCAS->show(2u);
    delete pCAS;
    errlogFlush ();
    return (0);
}

