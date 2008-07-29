/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*dbStaticNoRun.c*/

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "errMdef.h"
 
#define epicsExportSharedSymbols
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
 

long dbAllocRecord(DBENTRY *pdbentry,const char *precordName)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes		*pflddes;
    void		**papField;
    int			i;
    char		*pstr;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    precnode->precord = dbCalloc(pdbRecordType->no_fields,sizeof(void *));
    papField = (void **)precnode->precord;
    for(i=0; i<pdbRecordType->no_fields; i++) {
	pflddes = pdbRecordType->papFldDes[i];
	if(!pflddes) continue;
	pdbentry->pflddes = pflddes;
	switch(pflddes->field_type) {
	case DBF_STRING:
	    if(pflddes->size <= 0) {
		fprintf(stderr,"size=0 for %s.%s\n",
			pdbRecordType->name,pflddes->name);
		pflddes->size = 1;
	    }
	    papField[i] = dbCalloc(pflddes->size,sizeof(char));
	    if(pflddes->initial)  {
		if(strlen(pflddes->initial) >= (unsigned)(pflddes->size)) {
		    fprintf(stderr,"initial size > size for %s.%s\n",
			pdbRecordType->name,pflddes->name);
		} else {
		    strcpy((char *)papField[i],pflddes->initial);
		}
	    }
	    break;
	case DBF_CHAR:
	case DBF_UCHAR:
	case DBF_SHORT:
	case DBF_USHORT:
	case DBF_LONG:
	case DBF_ULONG:
	case DBF_FLOAT:
	case DBF_DOUBLE:
	case DBF_ENUM:
	    if(pflddes->initial) {
		papField[i] =
			dbCalloc(strlen(pflddes->initial)+1,sizeof(char));
		strcpy((char *)papField[i],pflddes->initial);
	    }
	    break;
	case DBF_MENU:
	case DBF_DEVICE: {/*Must allow initial value that is choice or index*/
		long value;
		char *endp;
		char *pchoice = NULL;

		if(!pflddes->initial) break;
		if(dbGetMenuIndexFromString(pdbentry,pflddes->initial)!=-1) {
		    pchoice=pflddes->initial;
		} else {
		    value = strtol(pflddes->initial,&endp,0);
		    if(*endp=='\0') pchoice =
			dbGetMenuStringFromIndex(pdbentry,(int)value);
		}
		if(!pchoice) {
		    epicsPrintf("%s.%s dbAllocRecord. Bad initial value\n",
			    precordName,pflddes->name);
		    break;
		}
		papField[i] = dbCalloc(strlen(pchoice)+1,sizeof(char));
		strcpy(papField[i],pchoice);
	    }
	    break;
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		struct link *plink;

		papField[i] = plink = dbCalloc(1,sizeof(struct link));
		plink->type = CONSTANT;
		if(pflddes->initial) {
		    plink->value.constantStr =
			dbCalloc(strlen(pflddes->initial)+1,sizeof(char));
		    strcpy(plink->value.constantStr,pflddes->initial);
		}
	    }
	    break;
	case DBF_NOACCESS:
	    break;
	default:
	    fprintf(stderr,"dbAllocRecord: Illegal field type\n");
	}
    }
    pstr = (char *)papField[0];
    strcpy(pstr,precordName);
    return(0);
}

long dbFreeRecord(DBENTRY *pdbentry)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	**pap;
    int		i,field_type;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    pap = (void **)precnode->precord;
    precnode->precord = NULL;
    for(i=0; i<pdbRecordType->no_fields; i++) {
	pflddes = pdbRecordType->papFldDes[i];
	field_type = pflddes->field_type;
	if(field_type==DBF_INLINK
	|| field_type==DBF_OUTLINK
	|| field_type==DBF_FWDLINK)
		dbFreeLinkContents((struct link *)pap[i]);
	free(pap[i]);
    }
    free((void *)pap);
    return(0);
}

long dbGetFieldAddress(DBENTRY *pdbentry)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	**pap;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!precnode->precord) return(0);
    pap = (void **)precnode->precord;
    pdbentry->pfield = pap[pflddes->indRecordType];
    return(0);
}

char *dbRecordName(DBENTRY *pdbentry)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    void	**pap;

    if(!pdbRecordType) return(0);
    if(!precnode) return(0);
    if(!precnode->precord) return(0);
    pap = (void **)precnode->precord;
    return((char *)pap[0]);
}

int dbIsMacroOk(DBENTRY *pdbentry) { return(TRUE);}

epicsShareFunc int epicsShareAPI dbIsDefaultValue(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void       	*pfield = pdbentry->pfield;

    if(!pflddes) return(FALSE);
    if(pflddes->field_type==DBF_DEVICE) return(FALSE);
    if(!pfield) return(TRUE);
    switch (pflddes->field_type) {
	case DBF_STRING: 
	    if(!pflddes->initial)
		return((*(char *)pfield =='\0') ? TRUE : FALSE);
	    return(strcmp((char *)pfield,(char *)pflddes->initial)==0);
	case DBF_CHAR:
	case DBF_UCHAR:
	case DBF_SHORT:
	case DBF_USHORT:
	case DBF_LONG:
	case DBF_ULONG:
	case DBF_FLOAT:
	case DBF_DOUBLE:
	case DBF_ENUM: 
	    if(!pflddes->initial) {
		return((strlen((char *)pfield)==0)?TRUE:FALSE);
	    }
	    return(strcmp((char *)pfield,(char *)pflddes->initial)==0);
	case DBF_MENU: {
		long value;
		char *endp;
		char *pinitial = NULL;

		if(pflddes->initial) {
		    if(dbGetMenuIndexFromString(pdbentry,pflddes->initial)!=-1){
			pinitial=pflddes->initial;
		    } else {
			value = strtol(pflddes->initial,&endp,0);
			if(*endp=='\0') pinitial =
			    dbGetMenuStringFromIndex(pdbentry,(int)value);
		    }
		} else {
		    pinitial = dbGetMenuStringFromIndex(pdbentry,0);
		}
		if(!pinitial) return(FALSE);
		return(strcmp(pinitial,pfield)==0);
	    }
	case DBF_DEVICE: /*Should never reach this state*/
	    return(FALSE);
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		struct link *plink = (struct link *)pfield;

		if(!plink) return(FALSE);
		if(plink->type!=CONSTANT) return(FALSE);
		if(!plink->value.constantStr) return(TRUE);
		if(!pflddes->initial)
		    return((strlen((char *)plink->value.constantStr)==0)?TRUE:FALSE);
		if(strcmp(plink->value.constantStr,pflddes->initial)==0)
		   return(TRUE);
		return(FALSE);
	    }
	case DBF_NOACCESS:
	    return(TRUE);
    }
    return(FALSE);
}

char *dbGetStringNum(DBENTRY *pdbentry)
{
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    void	**pap;
    static char	zero[] = "0";

    if(!precnode) return(0);
    if(!precnode->precord) return(0);
    if(!pflddes) return(0);
    if(!pfield) switch(pflddes->field_type) {
	case DBF_CHAR:
	case DBF_UCHAR:
	case DBF_SHORT:
	case DBF_USHORT:
	case DBF_LONG:
	case DBF_ULONG:
	case DBF_FLOAT:
	case DBF_DOUBLE:
	case DBF_ENUM:
	    return(zero);
	case DBF_MENU:
	case DBF_DEVICE:
	    return(dbGetMenuStringFromIndex(pdbentry,0));
	default:
	    epicsPrintf("dbGetStringNum. Illegal Field Type\n");
	    return(NULL);
    }
    pap = (void **)precnode->precord;
    return((char *)pap[pflddes->indRecordType]);
}

long dbPutStringNum(DBENTRY *pdbentry,const char *pstring)
{
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes  	*pflddes = pdbentry->pflddes;
    char	*pfield = (char *)pdbentry->pfield;
    void	**pap;

    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(pfield) {
	if((unsigned)strlen(pfield) < (unsigned)strlen(pstring)) {
	    free((void *)pfield);
	    pfield = NULL;
	}
    }
    if(!pfield)  {
	pfield = dbCalloc(strlen(pstring)+1,sizeof(char));
	strcpy(pfield,pstring);
	pdbentry->pfield = pfield;
	pap = (void **)precnode->precord;
	pap[pflddes->indRecordType] = pfield;
    }
    strcpy(pfield,pstring);
    return(0);
}

epicsShareFunc int  epicsShareAPI dbGetMenuIndex(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    int		nChoices,choice;
    char	**menuChoices;
    char	*pfield;

    if(!pflddes) return(-1);
    pfield = dbGetStringNum(pdbentry);
    if(!pfield) return(-1);
    nChoices = dbGetNMenuChoices(pdbentry);
    menuChoices = dbGetMenuChoices(pdbentry);
    if(nChoices<=0 || !menuChoices) return(-1);
    for(choice=0; choice<nChoices; choice++) {
	if(strcmp(menuChoices[choice],pfield)==0) return(choice);
    }
    return(-1);
}

epicsShareFunc long epicsShareAPI dbPutMenuIndex(DBENTRY *pdbentry,int index)
{
    int		nChoices;
    char	**menuChoices;

    nChoices = dbGetNMenuChoices(pdbentry);
    menuChoices = dbGetMenuChoices(pdbentry);
    if(index<0 || index>=nChoices) return(S_dbLib_badField);
    dbPutStringNum(pdbentry,menuChoices[index]);
    return(0);
}
