/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbReadTest.c */
/*	Author: Marty Kraimer	Date: 13JUL95	*/

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

int main(int argc,char **argv)
{
    int         i;
    int		strip;
    char	*path = NULL;
    char	*sub = NULL;
    int		pathLength = 0;
    int		subLength = 0;
    char	**pstr;
    char	*psep;
    int		*len;
    long	status;
    static char *pathSep = OSI_PATH_LIST_SEPARATOR;
    static char *subSep = ",";

    /*Look for options*/
    if(argc<2) {
	printf("usage: dbReadTest -Idir -Smacsub file.dbd file.db \n");
	exit(0);
    }
    while((strncmp(argv[1],"-I",2)==0)||(strncmp(argv[1],"-S",2)==0)) {
	if(strncmp(argv[1],"-I",2)==0) {
	    pstr = &path;
	    psep = pathSep;
	    len = &pathLength;
	} else {
	    pstr = &sub;
	    psep = subSep;
	    len = &subLength;
	}
	if(strlen(argv[1])==2) {
	    dbCatString(pstr,len,argv[2],psep);
	    strip = 2;
	} else {
	    dbCatString(pstr,len,argv[1]+2,psep);
	    strip = 1;
	}
	argc -= strip;
	for(i=1; i<argc; i++) argv[i] = argv[i + strip];
    }
    if(argc<2 || (strncmp(argv[1],"-",1)==0)) {
	printf("usage: dbReadTest -Idir -Idir file.dbd file.dbd \n");
	exit(0);
    }
    for(i=1; i<argc; i++) {
	status = dbReadDatabase(&pdbbase,argv[i],path,sub);
	if(!status) continue;
	fprintf(stderr,"For input file %s",argv[i]);
	errMessage(status,"from dbReadDatabase");
    }
/*
    dbDumpRecordType(pdbbase,"ai");
    dbDumpRecordType(pdbbase,NULL);
    dbPvdDump(pdbbase,1);
    gphDump(pdbbase->pgpHash);
    dbDumpMenu(pdbbase,NULL);
    dbDumpRecord(pdbbase,NULL,0);
    dbReportDeviceConfig(pdbbase,stdout);
*/
    dbFreeBase(pdbbase);
    return(0);
}
