/*dbStaticNoRun.c*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/*
 * Modification Log:
 * -----------------
 * .01	06-JUN-95	mrk	Initial Version
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include <dbDefs.h>
#include <dbFldTypes.h>
#include <epicsPrint.h>
#include <errMdef.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>

long dbAllocRecord(DBENTRY *pdbentry,char *precordName)
{
    dbRecDes		*precdes = pdbentry->precdes;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes		*pflddes;
    void		**papField;
    int			i;
    char		*pstr;

    if(!precdes) return(S_dbLib_recdesNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    precnode->precord = dbCalloc(precdes->no_fields,sizeof(void *));
    papField = (void **)precnode->precord;
    for(i=0; i<precdes->no_fields; i++) {
	pflddes = precdes->papFldDes[i];
	if(!pflddes) continue;
	switch(pflddes->field_type) {
	case DBF_STRING:
	    if(pflddes->size <= 0) {
		fprintf(stderr,"size=0 for %s.%s\n",precdes->name,pflddes->name);
		pflddes->size = 1;
	    }
	    papField[i] = dbCalloc(pflddes->size,sizeof(char));
	    if(pflddes->initial)  {
		if(strlen(pflddes->initial) >= pflddes->size) {
		    fprintf(stderr,"initial size > size for %s.%s\n",
			precdes->name,pflddes->name);
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
	case DBF_DEVICE:
	    papField[i] = dbCalloc(1,sizeof(unsigned short));
	    if(pflddes->initial) sscanf(pflddes->initial,"%hu",papField[i]);
	    break;
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		struct link *plink;

		papField[i] = plink = dbCalloc(1,sizeof(struct link));
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
    dbRecDes	*precdes = pdbentry->precdes;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	**pap;
    int		i,field_type;

    if(!precdes) return(S_dbLib_recdesNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    pap = (void **)precnode->precord;
    precnode->precord = NULL;
    for(i=0; i<precdes->no_fields; i++) {
	pflddes = precdes->papFldDes[i];
	field_type = pflddes->field_type;
	if(field_type==DBF_INLINK
	|| field_type==DBF_OUTLINK
	|| field_type==DBF_FWDLINK) {
	    struct link *plink = (struct link *)pap[i];

	    if(plink->type== CONSTANT)
		free((void *)plink->value.constantStr);
	    if(plink->type==PV_LINK)
		free((void *)plink->value.pv_link.pvname);
	}
	free(pap[i]);
    }
    free((void *)pap);
    return(0);
}

long dbGetFieldAddress(DBENTRY *pdbentry)
{
    dbRecDes	*precdes = pdbentry->precdes;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	**pap;

    if(!precdes) return(S_dbLib_recdesNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!precnode->precord) return(0);
    pap = (void **)precnode->precord;
    pdbentry->pfield = pap[pflddes->indRecDes];
    return(0);
}

char *dbRecordName(DBENTRY *pdbentry)
{
    dbRecDes	*precdes = pdbentry->precdes;
    dbRecordNode *precnode = pdbentry->precnode;
    void	**pap;

    if(!precdes) return(0);
    if(!precnode) return(0);
    if(!precnode->precord) return(0);
    pap = (void **)precnode->precord;
    return((char *)pap[0]);
}

int  dbIsDefaultValue(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void       	*pfield = pdbentry->pfield;

    if(!pflddes) return(FALSE);
    switch (pflddes->field_type) {
	case DBF_STRING: 
	    if(!pfield) return(FALSE);
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
	    if(!pfield) return(TRUE);
	    if(!pflddes->initial) return(FALSE);
	    return(strcmp((char *)pfield,(char *)pflddes->initial)==0);
	case DBF_MENU:
	case DBF_DEVICE: {
		unsigned short val,ival;

		if(!pfield) return(FALSE);
		val = *(unsigned short *)pfield;
		if(pflddes->initial == 0) return((val==0)?TRUE:FALSE);
		sscanf(pflddes->initial,"%hu",&ival);
		return((val==ival)?TRUE:FALSE);
	    }
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		struct link *plink = (struct link *)pfield;

		if(!plink) return(FALSE);
		if(plink->type!=CONSTANT) return(FALSE);
		if(plink->value.constantStr == 0) return(TRUE);
		if(strcmp(plink->value.constantStr,pflddes->initial)==0)
		   return(TRUE);
		return(FALSE);
	    }
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
    pap = (void **)precnode->precord;
    if(!pfield) return(zero);
    return((char *)pap[pflddes->indRecDes]);
}

long dbPutStringNum(DBENTRY *pdbentry,char *pstring)
{
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes  	*pflddes = pdbentry->pflddes;
    char	*pfield = (char *)pdbentry->pfield;
    void	**pap;

    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(pfield) {
	if(strlen(pfield) < strlen(pstring)) {
	    free((void *)pfield);
	    pfield = NULL;
	}
    }
    if(!pfield)  {
	pfield = dbCalloc(strlen(pstring)+1,sizeof(char));
	strcpy(pfield,pstring);
	pdbentry->pfield = pfield;
	pap = (void **)precnode->precord;
	pap[pflddes->indRecDes] = pfield;
    }
    strcpy(pfield,pstring);
    return(0);
}

void dbGetRecordtypeSizeOffset(dbRecDes *pdbRecDes)
{
    /*For no run cant and dont need to set size and offset*/
    return;
}
