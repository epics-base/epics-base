/*sdr2device.c*/
/* Convert device support to new ascii formats*/
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
#include <devSup.h>
DBBASE	*pdbbase;

#define NUM_LINK_TYPES 16

static char *plink_type_name[NUM_LINK_TYPES] = {
	"CONSTANT",
	"PV_LINK",
	"VME_IO",
	"CAMAC_IO",
	"AB_IO",
	"GPIB_IO",
	"BITBUS_IO",
	0,0,0,
	"DB_LINK",
	"CA_LINK",
	"INST_IO",
	"BBGPIB_IO",
	"RF_IO",
	"VXI_IO"
};

static char *defdctsdr = "default.dctsdr";

int main(int argc,char **argv)
{
    FILE		*fp;
    long		status;
    char 		*sdr_filename;
    char		filename[40];
    char		*pfilename = filename;
    struct recType	*precType;
    struct devChoiceRec *pdevChoiceRec;
    struct devChoiceSet *pdevChoiceSet;
    struct devChoice	*pdevChoice;
    struct recDevSup	*precDevSup;
    struct devSup	*pdevSup;
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
    strcpy(pfilename,"device.ascii");
    fp = fopen(pfilename,"w");
    if(!fp) {
	printf("Error opening file %s\n",pfilename);
	exit(-1);
    }
    precType = pdbbase->precType;
    precDevSup = pdbbase->precDevSup;
    pdevChoiceRec = pdbbase->pchoiceDev;
    for(i=0; i<precType->number; i++) {
	pdevSup = precDevSup->papDevSup[i];
	if(!pdevSup) continue;
	pdevChoiceSet = pdevChoiceRec->papDevChoiceSet[i];
	if(!pdevChoiceSet) continue;
	for(j=0; j<pdevSup->number; j++) {
	    if(!pdevSup->papDsetName[j]) continue;
	    pdevChoice = pdevChoiceSet->papDevChoice[j];
	    if(!pdevChoice) continue;
	    fprintf(fp,"device(%s,%s,%s,\"%s\")\n",
		precType->papName[i],
		plink_type_name[pdevChoice->link_type],
		pdevSup->papDsetName[j],
		pdevChoice->pchoice);
	}
    }
    fclose(fp);
    return(0);
}
