/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*sdr2driver.c*/
/* Convert driver support to new ascii formats*/
/*
 *     Author:	Marty Kraimer
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <errMdef.h>
#include <dbDefs.h>
#include <dbStaticLib.h>
#include <drvSup.h>
DBBASE	*pdbbase;

static char *defdctsdr = "default.dctsdr";

int main(int argc,char **argv)
{
    FILE		*fp;
    long		status;
    char 		*sdr_filename;
    char		filename[40];
    char		*pfilename = filename;
    struct drvSup	*pdrvSup;
    int			i,j;
    
    if(argc==2) {
	sdr_filename = argv[1];
    } else {
	sdr_filename = defdctsdr;
    }
    fp = fopen(sdr_filename,"r");
    if(!fp) {
	printf("Error opening file %s\n",sdr_filename);
	exit(-1);
    }
    pdbbase=dbAllocBase();
    status=dbRead(pdbbase,fp);
    if(status) {
	errMessage(status,"dbRead");
	exit(-1);
    }
    fclose(fp);
    strcpy(pfilename,"driver.dbd");
    fp = fopen(pfilename,"w");
    if(!fp) {
	printf("Error opening file %s\n",pfilename);
	exit(-1);
    }
    pdrvSup = pdbbase->pdrvSup;
    for(i=0; i<pdrvSup->number; i++) {
	fprintf(fp,"driver(%s)\n",pdrvSup->papDrvName[i]);
    }
    fclose(fp);
    return(0);
}
