/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*sdr2gblmenu.c*/
/* Convert global menus from old  .dctsdr file to new ascii formats */
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
	strcat(pfilename,".dbd");
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
