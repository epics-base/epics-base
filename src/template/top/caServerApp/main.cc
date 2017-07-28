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
#include "errlog.h"

#include "exServer.h"
#include "fdManager.h"

//
// main()
// (example single threaded ca server tool main loop)
//
extern int main ( int argc, const char **argv )
{
    epicsTime   begin (epicsTime::getCurrent());
    exServer    *pCAS;
    unsigned    debugLevel = 0u;
    double      executionTime = 0.0;
    double      asyncDelay = 0.1;
    char        pvPrefix[128] = "";
    unsigned    aliasCount = 1u;
    unsigned    scanOn = true;
    unsigned    syncScan = true;
    char        arraySize[64] = "";
    bool        forever = true;
    unsigned    maxSimultAsyncIO = 1000u;
    int         i;

    i = 1;
    while ( i < argc ) {
        if ( strcmp ( argv[i], "-d" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%u", & debugLevel ) == 1 ) {
                i += 2;
                continue;
            }
        }
        if ( strcmp ( argv[i],"-t" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%lf", & executionTime ) == 1 ) {
                forever = false;
                i += 2;
                continue;
            }
        }
        if ( strcmp ( argv[i], "-p" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%127s", pvPrefix ) == 1 ) {
                i += 2;
                continue;
            }
        }
        if ( strcmp ( argv[i], "-c" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%u", & aliasCount ) == 1 ) {
                i += 2;
                continue;
            }
        }
        if ( strcmp ( argv[i], "-s" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%u", & scanOn ) == 1 ) {
                i += 2;
                continue;
            }
        }
        if ( strcmp ( argv[i], "-a" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%63s", arraySize ) == 1 ) {
                i += 2;
                continue;
            }
        }
        if ( strcmp ( argv[i],"-ss" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%u", & syncScan ) == 1 ) {
                i += 2;
                continue;
            }        
        }
        if ( strcmp ( argv[i],"-ad" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%lf", & asyncDelay ) == 1 ) {
                i += 2;
                continue;
            }        
        }
        if ( strcmp ( argv[i],"-an" ) == 0 ) {
            if ( i+1 < argc && sscanf ( argv[i+1], "%u", & maxSimultAsyncIO ) == 1 ) {
                i += 2;
                continue;
            }        
        }
        printf ( "\"%s\"?\n", argv[i] );
        if ( i + 1 < argc ) {
            printf ( "\"%s\"?\n", argv[i+1] );
        }
        printf (
            "usage: %s [-d <debug level> -t <execution time> -p <PV name prefix> " 
            "-c <numbered alias count> -s <1=scan on (default), 0=scan off> "
            "-ss <1=synchronous scan (default), 0=asynchronous scan> "
            "-a <max array size> -ad <async delay> "
            "-an <max simultaneous async>\n", 
            argv[0]);

        return (1);
    }

    if ( arraySize[0] != '\0' ) {
        epicsEnvSet ( "EPICS_CA_MAX_ARRAY_BYTES", arraySize );
    }

    try {
        pCAS = new exServer ( pvPrefix, aliasCount, 
            scanOn != 0, syncScan == 0, asyncDelay,
            maxSimultAsyncIO );
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

