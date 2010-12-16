/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>

#include "errlog.h"

#include "directoryServer.h"
#include "fdManager.h"

static int parseDirectoryFile (const char *pFileName);
static int parseDirectoryFP (FILE *pf, const char *pFileName);

#ifndef INADDR_NONE
#define INADDR_NONE (~0ul)
#endif

//
// main()
// (example single threaded ca server tool main loop)
//
extern int main ( int argc, const char **argv )
{
    epicsTime       begin ( epicsTime::getCurrent() );
    directoryServer *pCAS;
    unsigned        debugLevel = 0u;
    double          executionTime = 0.0;
    char            pvPrefix[128] = "";
    char            fileName[128] = "pvDirectory.txt";
    unsigned        aliasCount = 0u;
    bool            forever = true;
    int             nPV;
    int             i;

    for (i=1; i<argc; i++) {
        if (sscanf(argv[i], "-d %u", &debugLevel)==1) {
            continue;
        }
        if (sscanf(argv[i],"-t %lf", &executionTime)==1) {
            forever = false;
            continue;
        }
        if (sscanf(argv[i],"-p %127s", pvPrefix)==1) {
            continue;
        }
        if (sscanf(argv[i],"-c %u", &aliasCount)==1) {
            continue;
        }
        if (sscanf(argv[i], "-f %127s", fileName)==1) {
            continue;
        }
        fprintf (stderr,
"usage: %s [-d<debug level> -t<execution time> -p<PV name prefix> -c<numbered alias count> -f<PV directory file>]\n", 
            argv[0]);

        return (1);
    }

    nPV = parseDirectoryFile (fileName);
    if (nPV<=0) {
        fprintf(stderr, 
"No PVs in directory file=%s. Exiting because there is no useful work to do.\n",
            fileName);
        return (-1);
    }

    try {
        pCAS = new directoryServer(pvPrefix, aliasCount);
        if (!pCAS) {
            return (-1);
        }
    }
    catch ( ... ) {
        errlogPrintf ( "Unable to create a directory service\n" );
        exit ( -1 );
    }

    pCAS->setDebugLevel(debugLevel);

    if (forever) {
        //
        // loop here forever
        //
        while (true) {
            fileDescriptorManager.process (1000.0);
        }
    }
    else {
        double  delay = epicsTime::getCurrent() - begin;
        //
        // loop here untime the specified execution time
        // expires
        //
        while (delay < executionTime) {
            fileDescriptorManager.process (delay);
            delay = epicsTime::getCurrent() - begin;
        }
    }
    pCAS->show(2u);
    delete pCAS;
    return (0);
}

//
// parseDirectoryFile()
//
// PV directory file is expected to have entries of the form:
// <PV name> <host name or dotted ip address> [<optional IP port number>]
//
//
static int parseDirectoryFile (const char *pFileName)
{

    FILE *pf;
    int nPV;

    //
    // open a file that contains the PV directory
    //
    pf = fopen(pFileName, "r");
    if (!pf) {
        fprintf(stderr, "Directory file access probems with file=\"%s\" because \"%s\"\n",
            pFileName, strerror(errno));
        return (-1);
    }

    nPV =  parseDirectoryFP (pf, pFileName);
    
    fclose (pf);

    return nPV;
}

//
// parseDirectoryFP()
//
// PV directory file is expected to have entries of the form:
// <PV name> <host name or dotted ip address> [<optional IP port number>]
//
//
static int parseDirectoryFP (FILE *pf, const char *pFileName)
{
    pvInfo *pPVI;
    char    pvNameStr[128];
    struct sockaddr_in ipa;
    char hostNameStr[128];
    int nPV=0;
    
    ipa.sin_family = AF_INET;
    while ( true ) {
    
        //
        // parse the PV name entry from the file
        //
        if (fscanf(pf, " %127s ", pvNameStr) != 1) {
            return nPV; // success
        }

        //  
        // parse out server address
        //
        if (fscanf(pf, " %s ", hostNameStr) == 1) {
            int status;

            status = aToIPAddr (hostNameStr, 0u, &ipa);
            if (status) {
                fprintf (pf, "Unknown host name=\"%s\" (or bad dotted ip addr) in \"%s\" with PV=\"%s\"?\n", 
                    hostNameStr, pFileName, pvNameStr);
                return -1;
            }
        }
        else {
            fprintf (stderr,"No host name (or dotted ip addr) after PV name in \"%s\" with PV=\"%s\"?\n", 
                    pFileName, pvNameStr);
            return -1;
        }
        
        //
        // parse out optional IP port number
        //
        unsigned portNumber;
        if (fscanf(pf, " %u ", &portNumber) == 1) {
            if (portNumber>0xffff) {
                fprintf (stderr,"Port number supplied is to large in \"%s\" with PV=\"%s\" host=\"%s\" port=%u?\n", 
                    pFileName, pvNameStr, hostNameStr, portNumber);
                return -1;
            }
            
            ipa.sin_port = htons((aitUint16) portNumber);
        }
        else {
            ipa.sin_port = 0u; // use the default CA server port
        }

        pPVI = new pvInfo ( pvNameStr, ipa );
        if ( ! pPVI ) {
            fprintf (stderr, "Unable to allocate space for a new PV in \"%s\" with PV=\"%s\" host=\"%s\"\n",
                    pFileName, pvNameStr, hostNameStr);
            return -1;
        }
        nPV++;
    }
#if defined ( __HP_aCC ) && ( _HP_aCC <= 033300 )
    return 0; // Make HP compiler happy
#endif
}
