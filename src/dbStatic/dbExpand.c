/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbExpand.c */
/*	Author: Marty Kraimer	Date: 30NOV95	*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "errMdef.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "dbBase.h"
#include "gpHash.h"
#include "osiFileName.h"

DBBASE *pdbbase = NULL;

void usage() 
{
    fprintf(stderr, "Usage:\n\tdbExpand -Ipath -ooutfile "
	    "-S macro=value file1.dbd file2.dbd ...\n");
    fprintf(stderr,"Specifying any path will replace the default of '.'\n");
}

int main(int argc,char **argv)
{
    char	*path = NULL;
    char	*sub = NULL;
    int		pathLength = 0;
    int		subLength = 0;
    char        *outFilename = NULL;
    FILE        *outFP = stdout;
    long	status;
    long	returnStatus = 0;
    static char *pathSep = OSI_PATH_LIST_SEPARATOR;
    static char *subSep = ",";

    /* Discard program name argv[0] */
    ++argv;
    --argc;
    
    while ((argc > 1) && (**argv == '-')) {
        char optLtr = (*argv)[1];
        char *optArg;
        
        if (strlen(*argv) > 2) {
            optArg = *argv+2;
            ++argv;
            --argc;
        } else {
            optArg = argv[1];
            argv += 2;
            argc -= 2;
        }
        
        switch (optLtr) {
        case 'o':
            outFilename = optArg;
            break;
            
        case 'I':
            dbCatString(&path, &pathLength, optArg, pathSep);
            break;
            
        case 'S':
            dbCatString(&sub, &subLength, optArg, subSep);
            break;
            
        default:
            fprintf(stderr, "dbExpand: Unknown option '-%c'\n", optLtr);
            usage();
            exit(1);
        }
    }
    
    if (argc < 1) {
	fprintf(stderr, "dbExpand: No input file specified\n");
        usage();
	exit(1);
    }
    
    for (; argc>0; --argc, ++argv) {
	status = dbReadDatabase(&pdbbase,*argv,path,sub);
	if (status) returnStatus = status;
    }
    if (returnStatus) {
        fprintf(stderr, "dbExpand: Input errors, no output generated\n");
        exit(1);
    }
    if (outFilename) {
        outFP = fopen(outFilename, "w");
        if (!outFP) {
            perror("dbExpand");
            exit(1);
        }
    }
    
    dbWriteMenuFP(pdbbase,outFP,0);
    dbWriteRecordTypeFP(pdbbase,outFP,0);
    dbWriteDeviceFP(pdbbase,outFP);
    dbWriteDriverFP(pdbbase,outFP);
    dbWriteRegistrarFP(pdbbase,outFP);
    dbWriteFunctionFP(pdbbase,outFP);
    dbWriteVariableFP(pdbbase,outFP);
    dbWriteBreaktableFP(pdbbase,outFP);
    dbWriteRecordFP(pdbbase,outFP,0,0);
    
    free((void *)path);
    free((void *)sub);
    return 0;
}
