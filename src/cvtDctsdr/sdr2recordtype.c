/*sdr2recordtype.c*/
/* Convert recordtype and record menus from .dctsdr file to new ascii formats */
/* sdr2recordtype <recordtype> <recordtype.h> <file.dctsdr> */
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
 * .01	05-15-95	mrk	Original Version
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#include <errMdef.h>
#include <dbDefs.h>
#include <special.h>
#include <dbStaticLib.h>
DBBASE	*pdbbase;

static char *defdctsdr = "default.dctsdr";

static char *ptypename[DBF_NTYPES] = {
	"DBF_STRING",
	"DBF_CHAR",
	"DBF_UCHAR",
	"DBF_SHORT",
	"DBF_USHORT",
	"DBF_LONG",
	"DBF_ULONG",
	"DBF_FLOAT",
	"DBF_DOUBLE",
	"DBF_ENUM",
	"DBF_MENU",
	"DBF_MENU",
	"DBF_MENU",
	"DBF_DEVICE",
	"DBF_INLINK",
	"DBF_OUTLINK",
	"DBF_FWDLINK",
	"DBF_NOACCESS"
};

#define NGUIDEFS 26
static char *pguigroup[NGUIDEFS] = {
	"",
	"GUI_COMMON",
	"GUI_ALARMS",
	"GUI_BITS1",
	"GUI_BITS2",
	"GUI_CALC",
	"GUI_CLOCK",
	"GUI_COMPRESS",
	"GUI_CONVERT",
	"GUI_DISPLAY",
	"GUI_HIST",
	"GUI_INPUTS",
	"GUI_LINKS",
	"GUI_MBB",
	"GUI_MOTOR",
	"GUI_OUTPUT",
	"GUI_PID",
	"GUI_PULSE",
	"GUI_SELECT",
	"GUI_SEQ1",
	"GUI_SEQ2",
	"GUI_SEQ3",
	"GUI_SUB",
	"GUI_TIMER",
	"GUI_WAVE",
	"GUI_SCAN"
};

static char *pgblMenuNames[11] = {
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

int main(int argc,char **argv)
{
    FILE		*fp,*fpInclude;
    long		status;
    char 		*sdrFilename;
    char		*recordtypeName;
    char		*recordtypeInclude;
    char		filename[40];
    char		*pfilename = filename;
    struct recType	*precType;
    struct recDes	*precDes;
    struct choiceRec	*pchoiceRec;
    struct arrChoiceSet *parrChoiceSet;
    struct choiceSet	*pchoiceSet;
    struct recTypDes	*precTypDes;
    struct fldDes	*pfldDes;
    char		menuname[50];
    char		*pmenuname = menuname;
    char		choiceName[50];
    char		tempStr[50];
    char		*ptemp;
    int			indRectype;
    int			i,j;
    int			indFldDes;
    char		includeLine[80];
    DBENTRY		dbentry;
    DBENTRY		*pdbentry=&dbentry;
    char		valuestring[80];
    char		*pstr;
    char		**paprecChoiceNames;
    int			makeCommon=FALSE;
    
    if(argc<3 || argc>4) {
	fprintf(stderr,"usage: sdr2recordtype <recordtype> <recordtype.h> [<file.dctsdr>]");
	exit(-1);
    }
    recordtypeName = argv[1];
    if(strcmp(recordtypeName,"dbCommon")==0) makeCommon=TRUE;
    recordtypeInclude = argv[2];
    if(argc==4) {
	sdrFilename = argv[3];
    } else {
	sdrFilename = defdctsdr;
    }
    fpInclude = fopen(recordtypeInclude,"r");
    if(!fpInclude) {
	fprintf(stderr,"Error opening file %s\n",recordtypeInclude);
	exit(-1);
    }
    fp = fopen(sdrFilename,"r");
    if(!fp) {
	fprintf(stderr,"Error opening file %s\n",sdrFilename);
	exit(-1);
    }
    pdbbase=dbAllocBase();
    status=dbRead(pdbbase,fp);
    if(status) {
	errMessage(status,"dbRead");
	exit(-1);
    }
    fclose(fp);
    precType = pdbbase->precType;
    for(indRectype=0; indRectype< precType->number; indRectype++) {
	if(!precType->papName[indRectype]) continue;
	if(makeCommon) break;
	if(strcmp(recordtypeName,precType->papName[indRectype]) == 0) break;
    }
    if(indRectype>=precType->number) {
	fprintf(stderr,"%s not found\n",recordtypeName);
	exit(-1);
    }
    if(makeCommon) {
	strcpy(pfilename,"dbCommon");
    } else {
	strcpy(pfilename,recordtypeName);
	strcat(pfilename,"Record");
    }
    strcat(pfilename,".dbd");
    fp = fopen(pfilename,"w");
    if(!fp) {
	fprintf(stderr,"Error opening file %s\n",pfilename);
	exit(-1);
    }
    /*Create record instance with initial values*/
    dbInitEntry(pdbbase,pdbentry);
    status = dbFindRecdes(pdbentry,precType->papName[indRectype]);
    if(status) {printf("No record description\n"); exit(-1);}
    status = dbCreateRecord(pdbentry,"temp");
    if(status) {printf("dbCreateRecord failed\n"); exit(-1);}
    /*For menus let name be <recordtype><fieldname> */
    precTypDes = pdbbase->precDes->papRecTypDes[indRectype];
    parrChoiceSet = pdbbase->pchoiceRec->papArrChoiceSet[indRectype];
    if(!parrChoiceSet) goto gen_rectype;
    if(makeCommon) goto gen_rectype;
    paprecChoiceNames = (char **)calloc(parrChoiceSet->number,sizeof(char *));
    for(i=0; i< parrChoiceSet->number; i++) {
	/*Find first field that uses this choice set */
	for(j=0; j< precTypDes->no_fields; j++) {
	    pfldDes = precTypDes->papFldDes[j];
	    if(pfldDes->field_type==DBF_RECCHOICE
	    && pfldDes->choice_set==i) break;
	}
	if(j>=precTypDes->no_fields) continue;
	strcpy(pmenuname,recordtypeName);
	strcat(pmenuname,pfldDes->fldname);
	for(j=strlen(pmenuname)-1; j>0; j--) {
	    if(pmenuname[j] == ' ') pmenuname[j] = 0;
	}
	paprecChoiceNames[i] = calloc(1,strlen(pmenuname) + 1);
	strcpy(paprecChoiceNames[i],pmenuname);
	fprintf(fp,"menu(%s) {\n",pmenuname);
	pchoiceSet = parrChoiceSet->papChoiceSet[i];
	for(j=0; j<pchoiceSet->number; j++) {
	    strcpy(tempStr,pchoiceSet->papChoice[j]);
	    ptemp = tempStr;
	    while(*ptemp) {
		if(!isalnum(*ptemp)) *ptemp = '_';
		ptemp++;
	    }
	    sprintf(choiceName,"%s_%s",pmenuname,tempStr);
	    fprintf(fp,"\tchoice(%s,\"%s\")\n",
		choiceName,pchoiceSet->papChoice[j]);
	}
	fprintf(fp,"}\n");
    }
gen_rectype:
    if(makeCommon) {
	indFldDes=-1;
    } else {
	fprintf(fp,"recordtype(%s) {\n",recordtypeName);
	fprintf(fp,"\tinclude \"dbCommon.dbd\" \n");
	/*Skip fields in dbCommon*/
	for(indFldDes=0; indFldDes< precTypDes->no_fields; indFldDes++) {
	    if(strcmp(precTypDes->papFldDes[indFldDes]->fldname,"FLNK")==0) break;
	}
    }
    while(++indFldDes < precTypDes->no_fields) {
	pfldDes = precTypDes->papFldDes[indFldDes];
	if(pfldDes->field_type==DBF_NOACCESS) {
	    char	*prtn;
	    char	fldname[5];
	    char	*ptemp;

	    strcpy(fldname,pfldDes->fldname);
	    pstr = fldname;
	    while(*pstr) {
		if(*pstr == ' ') {
		    *pstr = 0;
		    break;
		}
		*pstr = tolower(*pstr);
		pstr++;
	    }
            /*skip all lines up to beginning of structure*/
	    fseek(fpInclude,0,SEEK_SET);
	    while(TRUE) {
		pstr = fgets(includeLine,80,fpInclude);
		if(!pstr) {
	    	    fprintf(stderr,"Does not appear to be include file%s\n",
			recordtypeInclude);
	    	    exit(-1);
		}
		if(strstr(includeLine,"struct ")) break;
	    }
	    while(TRUE) {
		prtn = fgets(includeLine,80,fpInclude);
		if(!prtn) {
		    fprintf(stderr, "field %s not found in %s\n",
			    pfldDes->fldname,recordtypeInclude);
		    break;
		}
		ptemp = &includeLine[0];
		while(TRUE) {
		    if(!ptemp) break;
		    if(!(pstr = strstr(ptemp,fldname))) {
			ptemp = NULL;
			break;
		    }
		    if(isalnum(*(pstr-1))
		    || isalnum(*(pstr + strlen(fldname)))) {
			ptemp = pstr + 1;
			continue;
		    }
		    break;
		}
		if(!ptemp) continue;
		break;
	    }
	    if(!prtn) continue;
	    if(strstr(pstr,"Created Pad")) continue;
	}
	strcpy(tempStr,pfldDes->fldname);
	for(j=strlen(tempStr)-1; j>0; j--) {
	    if(tempStr[j] == ' ') tempStr[j] = 0;
	}
	fprintf(fp,"\tfield(%s,%s) {\n",
		tempStr,ptypename[pfldDes->field_type]);
	if(pfldDes->prompt[0]!=0) {
	    fprintf(fp,"\t\tprompt(\"%s\")\n",pfldDes->prompt);
	}
	if(pfldDes->promptflag) {
	    if(pfldDes->promptflag<0 || pfldDes->promptflag>=NGUIDEFS)
		fprintf(fp,"\t\tpromptgroup(%s)\n",pguigroup[9]);/*GUI_DISPLAY*/
	    else
	        fprintf(fp,"\t\tpromptgroup(%s)\n",pguigroup[pfldDes->promptflag]);
	}
	if(pfldDes->as_level == 0) fprintf(fp,"\t\tasl(ASL0)\n");
	if(pfldDes->special) {
	    fprintf(fp,"\t\tspecial(");
	    switch(pfldDes->special) {
	    case SPC_NOMOD: fprintf(fp,"SPC_NOMOD)\n"); break;
	    case SPC_DBADDR: fprintf(fp,"SPC_DBADDR)\n"); break;
	    case SPC_SCAN: fprintf(fp,"SPC_SCAN)\n"); break;
	    case SPC_HDWLINK: fprintf(fp,"SPC_HDWLINK)\n"); break;
	    case SPC_ALARMACK: fprintf(fp,"SPC_ALARMACK)\n"); break;
	    case SPC_AS: fprintf(fp,"SPC_AS)\n"); break;
	    case SPC_MOD: fprintf(fp,"SPC_MOD)\n"); break;
	    case SPC_RESET: fprintf(fp,"SPC_RESET)\n"); break;
	    case SPC_LINCONV: fprintf(fp,"SPC_LINCONV)\n"); break;
	    case SPC_CALC: fprintf(fp,"SPC_CALC)\n"); break;
	    default : fprintf(fp,"%d)\n",pfldDes->special);
	    }
	}
	if(pfldDes->process_passive) fprintf(fp,"\t\tpp(TRUE)\n");
	if(pfldDes->cvt_type) fprintf(fp,"\t\tbase(HEX)\n");
	if(pfldDes->interest)
	    fprintf(fp,"\t\tinterest(%d)\n",pfldDes->interest);
	switch(pfldDes->field_type) {
	case DBF_STRING: {
	    fprintf(fp,"\t\tsize(%d)\n",pfldDes->size);
	    }
	    break;
	case DBF_CHAR: {
		status = dbFindField(pdbentry,pfldDes->fldname);
    		if(status) {printf("dbFindField failed\n"); exit(-1);}
		pstr = dbGetString(pdbentry);
		if(pfldDes->initial.char_value)
		    fprintf(fp,"\t\tinitial(\"%d\")\n",
			pfldDes->initial.char_value);
	    }
	    break;
	case DBF_UCHAR: {
		if(pfldDes->initial.uchar_value)
		    fprintf(fp,"\t\tinitial(\"%u\")\n",
			pfldDes->initial.uchar_value);
	    }
	    break;
	case DBF_SHORT: {
		if(pfldDes->initial.short_value)
		    fprintf(fp,"\t\tinitial(\"%d\")\n",
			pfldDes->initial.short_value);
	    }
	    break;
	case DBF_USHORT: {
		if(pfldDes->initial.ushort_value)
		    fprintf(fp,"\t\tinitial(\"%u\")\n",
			pfldDes->initial.ushort_value);
	    }
	    break;
	case DBF_LONG: {
		if(pfldDes->initial.long_value)
		    fprintf(fp,"\t\tinitial(\"%d\")\n",
			pfldDes->initial.long_value);
	    }
	    break;
	case DBF_ULONG: {
		if(pfldDes->initial.ulong_value)
		    fprintf(fp,"\t\tinitial(\"%u\")\n",
			pfldDes->initial.ulong_value);
	    }
	    break;
	case DBF_FLOAT: {
		if(pfldDes->initial.float_value)
		    fprintf(fp,"\t\tinitial(\"%g\")\n",
			pfldDes->initial.float_value);
	    }
	    break;
	case DBF_DOUBLE: {
		if(pfldDes->initial.double_value)
		    fprintf(fp,"\t\tinitial(\"%g\")\n",
			pfldDes->initial.double_value);
	    }
	    break;
	case DBF_ENUM: {
		if(pfldDes->initial.enum_value)
		    fprintf(fp,"\t\tinitial(\"%u\")\n",
			pfldDes->initial.enum_value);
	    }
	    break;
	case DBF_GBLCHOICE: {
		fprintf(fp,"\t\tmenu(%s)\n",pgblMenuNames[pfldDes->choice_set]);
		if(pfldDes->initial.ushort_value)
		    fprintf(fp,"\t\tinitial(\"%u\")\n",
			pfldDes->initial.ushort_value);
	    }
	    break;
	case DBF_CVTCHOICE: {
		fprintf(fp,"\t\tmenu(menuConvert)\n");
		if(pfldDes->initial.ushort_value)
		    fprintf(fp,"\t\tinitial(\"%u\")\n",
			pfldDes->initial.ushort_value);
	    }
	    break;
	case DBF_RECCHOICE: {
		fprintf(fp,"\t\tmenu(%s)\n",paprecChoiceNames[pfldDes->choice_set]);
		if(pfldDes->initial.ushort_value)
		    fprintf(fp,"\t\tinitial(\"%u\")\n",
			pfldDes->initial.ushort_value);
	    }
	    break;
	case DBF_DEVCHOICE: {
		if(pfldDes->initial.ushort_value)
		    fprintf(fp,"\t\tinitial(\"%u\")\n",
			pfldDes->initial.ushort_value);
	    }
	    break;
	case DBF_INLINK: {
	    }	
	    break;
	case DBF_OUTLINK: {
	    }	
	    break;
	case DBF_FWDLINK: {
	    }
	    break;
	case DBF_NOACCESS: { /*includeLine holds definition*/
		char	*ptempStr = tempStr;

		ptemp = includeLine;
		while(*ptemp==' ' || *ptemp=='\t') ptemp++;
		while(*ptemp!=';') *ptempStr++ = *ptemp++;
		*ptempStr = 0;
		fprintf(fp,"\t\textra(\"%s\")\n",tempStr);
	    }
	    break;
	}
	fprintf(fp,"\t}\n");
	if(makeCommon
	&& (strcmp(precTypDes->papFldDes[indFldDes]->fldname,"FLNK")==0))
		break;
    }
    if(!makeCommon)fprintf(fp,"}\n");
    fclose(fpInclude);
    fclose(fp);
    return(0);
}
