/*sdr2driver.c*/
/* Convert driver support to new ascii formats*/
/*
 *
 *     Author:	Marty Kraimer
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	05-10-95	mrk	Original Version
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
    strcpy(pfilename,"driver.ascii");
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
