/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*sdr2device.c*/
/* Convert device support to new ascii formats*/
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
    strcpy(pfilename,"device.dbd");
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
