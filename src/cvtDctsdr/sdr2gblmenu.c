/*sdr2gblmenu.c*/
/* Convert global menus from old  .dctsdr file to new ascii formats */
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
DBBASE	*pdbbase;

#define NUM_GBL_MENUS 11

static char *aiMenuName[NUM_GBL_MENUS] = {
	"menuYesNo",
	"menuScan",
	"menuAlarmSevr",
	"menuAlarmStat",
	"menuArrType",
	"menuOmsl",
	"menuLinr",
	"menuFtype",
	"menuCompress",
	"menuPriority",
	"menuIvoa"
};

static char *defdctsdr = "default.dctsdr";

int main(int argc,char **argv)
{
    FILE		*fp;
    long		status;
    char 		*sdr_filename;
    char		filename[40];
    char		*pfilename = filename;
    char		choice_name[50];
    char		temp_str[50];
    char		*ptemp;
    struct choiceSet	*pchoiceSet;
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
    for(i=0; i<NUM_GBL_MENUS; i++) {
	strcpy(pfilename,aiMenuName[i]);
	strcat(pfilename,".ascii");
	fp = fopen(pfilename,"w");
	if(!fp) {
	    printf("Error opening file %s\n",pfilename);
	    exit(-1);
	}
	pchoiceSet = pdbbase->pchoiceGbl->papChoiceSet[i];
	fprintf(fp,"menu(%s) {\n",aiMenuName[i]);
	for(j=0; j<pchoiceSet->number; j++) {
	    strcpy(temp_str,pchoiceSet->papChoice[j]);
	    ptemp = temp_str;
	    while(*ptemp) {
		if(!isalnum(*ptemp)) *ptemp = '_';
		ptemp++;
	    }
	    sprintf(choice_name,"%s%s",aiMenuName[i],temp_str);
	    fprintf(fp,"\tchoice(%s,\"%s\")\n",
		choice_name,pchoiceSet->papChoice[j]);
	}
	fprintf(fp,"}\n");
	fclose(fp);
    }
    return(0);
}
