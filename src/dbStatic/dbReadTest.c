/* dbReadTest.c */
/*	Author: Marty Kraimer	Date: 13JUL95	*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/* Modification Log:
 * -----------------
 * .01	13JUL95	mrk	Initial Implementation
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <epicsPrint.h>
#include <errMdef.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <dbBase.h>
#include <gpHash.h>

DBBASE *pdbbase = NULL;
#define MAX_PATH_LENGTH 256

static void addPath(char *path,char *newdir)
{
    if((strlen(path)+strlen(newdir)+2) > (size_t)MAX_PATH_LENGTH) {
        fprintf(stderr,"path > 256 characters\n");
        exit(-1);
    }
    if(strlen(path) > (size_t)0) strcat(path,":");
    strcat(path,newdir);
}
 

int main(int argc,char **argv)
{
    long        status;
    int         strip;
    char        path[MAX_PATH_LENGTH];
    int         i;

    /*Look for path, i.e. -I dir or -Idir*/
    path[0] = 0;
    while(strncmp(argv[1],"-I",2)==0) {
        if(strlen(argv[1])==2) {
            addPath(path,argv[2]);
            strip = 2;
        } else {
            addPath(path,argv[1]+2);
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
	status = dbReadDatabase(&pdbbase,argv[i],path);
	if(!status) continue;
	fprintf(stderr,"For input file %s",argv[i]);
	errMessage(status,"from dbReadDatabase");
    }
/*
    dbDumpRecordType(pdbbase,"ai");
    dbDumpRecordType(pdbbase,NULL);
    dbPvdDump(pdbbase);
    gphDump(pdbbase->pgpHash);
    dbDumpMenu(pdbbase,NULL);
    dbDumpRecord(pdbbase,NULL,0);
*/
    dbFreeBase(pdbbase);
    return(0);
}
