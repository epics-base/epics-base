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
    dbRecDes		*pdbRecDes = pdbentry->precdes;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes		*pflddes;
    void		**papField;
    int			i;
    char		*pstr;

    if(!pdbRecDes) return(S_dbLib_recdesNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    precnode->precord = dbCalloc(pdbRecDes->no_fields,sizeof(void *));
    papField = (void **)precnode->precord;
    for(i=0; i<pdbRecDes->no_fields; i++) {
	pflddes = pdbRecDes->papFldDes[i];
	if(!pflddes) continue;
	switch(pflddes->field_type) {
	case DBF_STRING:
	    if(pflddes->size <= 0) {
		fprintf(stderr,"size=0 for %s.%s\n",pdbRecDes->name,pflddes->name);
		pflddes->size = 1;
	    }
	    papField[i] = dbCalloc(pflddes->size,sizeof(char));
	    if(pflddes->initial)  {
		if(strlen(pflddes->initial) >= (unsigned)(pflddes->size)) {
		    fprintf(stderr,"initial size > size for %s.%s\n",
			pdbRecDes->name,pflddes->name);
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
    dbRecDes	*pdbRecDes = pdbentry->precdes;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	**pap;
    int		i,field_type;

    if(!pdbRecDes) return(S_dbLib_recdesNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    pap = (void **)precnode->precord;
    precnode->precord = NULL;
    for(i=0; i<pdbRecDes->no_fields; i++) {
	pflddes = pdbRecDes->papFldDes[i];
	field_type = pflddes->field_type;
	if(field_type==DBF_INLINK
	|| field_type==DBF_OUTLINK
	|| field_type==DBF_FWDLINK) {
	    struct link *plink = (struct link *)pap[i];

	    switch(plink->type) {
	    case CONSTANT: free((void *)plink->value.constantStr); break;
	    case PV_LINK: free((void *)plink->value.pv_link.pvname); break;
	    case VME_IO: dbFreeParmString(&plink->value.vmeio.parm); break;
	    case CAMAC_IO: dbFreeParmString(&plink->value.camacio.parm); break;
	    case AB_IO: dbFreeParmString(&plink->value.abio.parm); break;
	    case GPIB_IO: dbFreeParmString(&plink->value.gpibio.parm); break;
	    case BITBUS_IO: dbFreeParmString(&plink->value.bitbusio.parm);break;
	    case INST_IO: dbFreeParmString(&plink->value.instio.string); break;
	    case BBGPIB_IO: dbFreeParmString(&plink->value.bbgpibio.parm);break;
	    case VXI_IO: dbFreeParmString(&plink->value.vxiio.parm); break;
	    }
	}
	free(pap[i]);
    }
    free((void *)pap);
    return(0);
}

long dbGetFieldAddress(DBENTRY *pdbentry)
{
    dbRecDes	*pdbRecDes = pdbentry->precdes;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;
    void	**pap;

    if(!pdbRecDes) return(S_dbLib_recdesNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!precnode->precord) return(0);
    pap = (void **)precnode->precord;
    pdbentry->pfield = pap[pflddes->indRecDes];
    return(0);
}

char *dbRecordName(DBENTRY *pdbentry)
{
    dbRecDes	*pdbRecDes = pdbentry->precdes;
    dbRecordNode *precnode = pdbentry->precnode;
    void	**pap;

    if(!pdbRecDes) return(0);
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
	case DBF_MENU: {
		unsigned short val,ival;

		if(!pfield) return(FALSE);
		val = *(unsigned short *)pfield;
		if(pflddes->initial == 0) return((val==0)?TRUE:FALSE);
		sscanf(pflddes->initial,"%hu",&ival);
		return((val==ival)?TRUE:FALSE);
	    }
	case DBF_DEVICE: {
		dbRecDes	*pdbRecDes = pdbentry->precdes;
		devSup		*pdevSup;

		if(!pdbRecDes) {
		    epicsPrintf("dbIsDefaultValue: pdbRecDes is NULL??\n");
		    return(FALSE);
		}
		pdevSup = (devSup *)ellFirst(&pdbRecDes->devList);
		if(!pdevSup) return(TRUE);
		return(FALSE);
	    }
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		struct link *plink = (struct link *)pfield;

		if(!plink) return(FALSE);
		if(plink->type!=CONSTANT) return(FALSE);
		if(plink->value.constantStr == 0) return(TRUE);
		if(!pflddes->initial) return(FALSE);
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
