/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*dbStaticLibRun.c*/

#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "cvtFast.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsPrint.h"
#include "epicsStdlib.h"
#include "epicsTypes.h"
#include "errMdef.h"

#define epicsExportSharedSymbols
#include "dbBase.h"
#include "dbCommon.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "devSup.h"
#include "special.h"

static long do_nothing(struct dbCommon *precord) { return 0; }

/* Dummy DSXT used for soft device supports */
struct dsxt devSoft_DSXT = {
    do_nothing,
    do_nothing
};

static devSup *pthisDevSup = NULL;

void dbInitDevSup(devSup *pdevSup, dset *pdset)
{
    pdevSup->pdset = pdset;
    if (pdevSup->link_type == CONSTANT)
        pdevSup->pdsxt = &devSoft_DSXT;

    if (pdset->init) {
        pthisDevSup = pdevSup;
        pdset->init(0);
        pthisDevSup = NULL;
    }
}

void devExtend(dsxt *pdsxt)
{
    if (!pthisDevSup)
        errlogPrintf("devExtend() called outside of dbInitDevSup()\n");
    else {
        pthisDevSup->pdsxt = pdsxt;
    }
}

long dbAllocRecord(DBENTRY *pdbentry,const char *precordName)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes		*pflddes;
    int			i;
    dbCommon		*precord;
    char		*pfield;
    
    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(pdbRecordType->rec_size == 0) {
	printf("\t*** Did you run x_RegisterRecordDeviceDriver(pdbbase) yet? ***\n");
	epicsPrintf("dbAllocRecord(%s) with %s rec_size = 0\n",
	    precordName, pdbRecordType->name);
	return(S_dbLib_noRecSup);
    }
    precord = dbCalloc(1, pdbRecordType->rec_size);
    precnode->precord = precord;
    pflddes = pdbRecordType->papFldDes[0];
    if(!pflddes) {
	epicsPrintf("dbAllocRecord pflddes for NAME not found\n");
	return(S_dbLib_flddesNotFound);
    }
    assert(pflddes->offset == 0);
    assert(pflddes->size == sizeof(precord->name));
    if(strlen(precordName) >= sizeof(precord->name)) {
	epicsPrintf("dbAllocRecord: NAME(%s) too long\n",precordName);
	return(S_dbLib_nameLength);
    }
    strcpy(precord->name, precordName);
    for(i=1; i<pdbRecordType->no_fields; i++) {

	pflddes = pdbRecordType->papFldDes[i];
	if(!pflddes) continue;
	pfield = (char*)precord + pflddes->offset;
	pdbentry->pfield = (void *)pfield;
	pdbentry->pflddes = pflddes;
	pdbentry->indfield = i;
	switch(pflddes->field_type) {
	case DBF_STRING:
	    if(pflddes->initial)  {
		if(strlen(pflddes->initial) >= pflddes->size) {
		    epicsPrintf("initial size > size for %s.%s\n",
			pdbRecordType->name,pflddes->name);
		} else {
		    strcpy(pfield,pflddes->initial);
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
	case DBF_MENU:
	    if(pflddes->initial) {
		long status;

		status = dbPutStringNum(pdbentry,pflddes->initial);
		if(status)
		    epicsPrintf("Error initializing %s.%s initial %s\n",
			pdbRecordType->name,pflddes->name,pflddes->initial);
	    }
	    break;
	case DBF_DEVICE:
	    if(!pflddes->ftPvt) dbGetDeviceMenu(pdbentry);
	    break;
	case DBF_INLINK:
	case DBF_OUTLINK:
	case DBF_FWDLINK: {
		DBLINK *plink = (DBLINK *)pfield;

		plink->type = CONSTANT;
		if(pflddes->initial) {
		    plink->text =
			dbCalloc(strlen(pflddes->initial)+1,sizeof(char));
		    strcpy(plink->text,pflddes->initial);
		}
	    }
	    break;
	case DBF_NOACCESS:
	    break;
	default:
	    epicsPrintf("dbAllocRecord: Illegal field type\n");
	}
    }
    return(0);
}

long dbFreeRecord(DBENTRY *pdbentry)
{
    dbRecordType *pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    free(precnode->precord);
    precnode->precord = NULL;
    return(0);
}

long dbGetFieldAddress(DBENTRY *pdbentry)
{
    dbRecordType *pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!precnode->precord) return(0);
    pdbentry->pfield = ((char *)precnode->precord) + pflddes->offset;
    return(0);
}

char *dbRecordName(DBENTRY *pdbentry)
{
    dbRecordType *pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes;
    char	*precord;

    if(!pdbRecordType) return(0);
    if(!precnode) return(0);
    if(!precnode->precord) return(0);
    precord = (char *)precnode->precord;
    pflddes = pdbRecordType->papFldDes[0];
    if(!pflddes) return(NULL);
    return(precord + pflddes->offset);
}

int dbIsMacroOk(DBENTRY *pdbentry) { return(FALSE); }

epicsShareFunc int dbIsDefaultValue(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void        *pfield = pdbentry->pfield;

    if(!pflddes) return(FALSE);
    if(!pfield) return(FALSE);
    switch (pflddes->field_type) {
	case (DBF_STRING) : {
	    char *p = (char *)pfield;
	
	    if(!pflddes->initial) return((strlen(p)==0) ? TRUE : FALSE);
	    return((strcmp(pflddes->initial,p)==0) ? TRUE : FALSE);
	}
	case DBF_CHAR: {
	    char	field = *(char *)pfield;
	    long	ltemp;
	    if(pflddes->initial)  {
		ltemp = strtol(pflddes->initial,NULL,0);
		return((field==ltemp));
	    }
	    return((field==0));
	}
	case DBF_UCHAR: {
	    unsigned char	field = *(unsigned char *)pfield;
	    unsigned long	ltemp;

	    if(pflddes->initial)  {
		ltemp = strtoul(pflddes->initial,NULL,0);
		return((field==ltemp));
	    }
	    return((field==0));
	}
	case DBF_SHORT: {
	    short	field = *(short *)pfield;
	    long	ltemp;

	    if(pflddes->initial)  {
		ltemp = strtol(pflddes->initial,NULL,0);
		return((field==ltemp));
	    }
	    return((field==0));
	}
	case DBF_USHORT: {
	    unsigned short	field = *(unsigned short *)pfield;
	    unsigned long	ltemp;

	    if(pflddes->initial)  {
		ltemp = strtoul(pflddes->initial,NULL,0);
		return((field==ltemp));
	    }
	    return((field==0));
	}
	case DBF_LONG: {
	    long	field = *(long *)pfield;
	    long	ltemp;

	    if(pflddes->initial)  {
		ltemp = strtol(pflddes->initial,NULL,0);
		return((field==ltemp));
	    }
	    return((field==0));
	}
	case DBF_ULONG: {
	    unsigned long	field = *(unsigned long *)pfield;
	    unsigned long	ltemp;

	    if(pflddes->initial)  {
		ltemp = strtoul(pflddes->initial,NULL,0);
		return((field==ltemp));
	    }
	    return((field==0));
	}
	case DBF_FLOAT: {
	    float	field = *(float *)pfield;
	    double	dtemp;

	    if(pflddes->initial)  {
		dtemp = epicsStrtod(pflddes->initial,NULL);
		return((field==dtemp));
	    }
	    return((field==0.0));
	}
	case DBF_DOUBLE: {
	    double	field = *(double *)pfield;
	    double	dtemp;

	    if(pflddes->initial)  {
		dtemp = epicsStrtod(pflddes->initial,NULL);
		return((field==dtemp));
	    }
	    return((field==0.0));
	}
	case DBF_ENUM: {
	    unsigned short	field = *(unsigned short *)pfield;
	    unsigned long	ltemp;

	    if(pflddes->initial)  {
		ltemp = strtoul(pflddes->initial,NULL,0);
		return((field==ltemp));
	    }
	    return((field==0));
	}
	case DBF_MENU: {
	    unsigned short	field = *(unsigned short *)pfield;
	    long 		value;
	    char 		*endp;

	    if(pflddes->initial) {
		value = dbGetMenuIndexFromString(pdbentry,pflddes->initial);
		if(value==-1) {
		    value = strtol(pflddes->initial,&endp,0);
		    if(*endp!='\0') return(FALSE);
		}
	    } else {
		value = 0;
	    }
	    if((unsigned short)value == field) return(TRUE);
	    return(FALSE);
	}
	case DBF_DEVICE: {
		dbRecordType	*precordType = pdbentry->precordType;

		if(!precordType) {
		    epicsPrintf("dbIsDefaultValue: pdbRecordType is NULL??\n");
		    return(FALSE);
		}
		if(ellCount(&precordType->devList)==0) return(TRUE);
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
	default:
	    return(TRUE);
    }
    return(FALSE);
}

long dbPutStringNum(DBENTRY *pdbentry,const char *pstring)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    long	status=0;

    if(!pfield) return(S_dbLib_fieldNotFound);
    switch (pflddes->field_type) {
    case DBF_CHAR :
    case DBF_SHORT :
    case DBF_LONG:{
	    long  value;
	    char  *endp;

	    value = strtol(pstring,&endp,0);
	    if(*endp!=0) status = S_dbLib_badField;
	    switch (pflddes->field_type) {
        case DBF_CHAR : *(char *)pfield = (char)value; break;
        case DBF_SHORT : *(short *)pfield = (short)value; break;
        case DBF_LONG : *(epicsInt32 *)pfield = (epicsInt32)value; break;
	    default: epicsPrintf("Logic error in dbPutStringNum\n");
	    }
	}
	break;
    case DBF_UCHAR:
    case DBF_USHORT:
    case DBF_ULONG:
    case DBF_ENUM:{
	    unsigned long  value;
	    char  *endp;

	    value = strtoul(pstring,&endp,0);
	    if(*endp!=0) status = S_dbLib_badField;
	    switch (pflddes->field_type) {
        case DBF_UCHAR : *(unsigned char *)pfield = (unsigned char)value; break;
	    case DBF_USHORT:
        case DBF_ENUM: *(unsigned short *)pfield = (unsigned short)value; break;
        case DBF_ULONG : *(epicsUInt32 *)pfield = (epicsUInt32)value; break;
	    default: epicsPrintf("Logic error in dbPutStringNum\n");
	    }
	}
	break;
    case DBF_FLOAT:
    case DBF_DOUBLE: {	
	    double value;
	    char  *endp;

	    value = epicsStrtod(pstring,&endp);
	    if(*endp!=0) status = S_dbLib_badField;
	    if(pflddes->field_type == DBF_FLOAT)
	        *(float *)pfield = (float)value;
	    else
		*(double *)pfield = value;
	}
	break;
    case DBF_MENU:
    case DBF_DEVICE: {/*Must allow value that is choice or index*/
	    unsigned short	*field= (unsigned short*)pfield;
	    int			ind;
	    long		value;
	    char		*endp;

	    ind = dbGetMenuIndexFromString(pdbentry,pstring);
	    if(ind==-1) {
		value = strtol(pstring,&endp,0);
		if(*endp!='\0') return(S_dbLib_badField);
		ind = value;
		/*Check that ind is withing range*/
		if(!dbGetMenuStringFromIndex(pdbentry,ind))
		    return(S_dbLib_badField);
	    }
	    *field = (unsigned short)ind;
    	}
	return (0);
    default:
	return (S_dbLib_badField);
    }
    return(status);
}

epicsShareFunc int dbGetMenuIndex(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;

    if(!pflddes) return(-1);
    if(!pfield) return(-1);
    switch (pflddes->field_type) {
	case (DBF_MENU):
	case (DBF_DEVICE):
    	    return((int)(*(unsigned short *)pfield));
	default:
	    errPrintf(-1,__FILE__, __LINE__,"Logic Error\n");
    }
    return(-1);
}

epicsShareFunc long dbPutMenuIndex(DBENTRY *pdbentry,int index)
{
    dbFldDes  		*pflddes = pdbentry->pflddes;
    unsigned short	*pfield = pdbentry->pfield;

    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!pfield) return(S_dbLib_fieldNotFound);
    switch (pflddes->field_type) {
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;

	    if(!pdbMenu) return(S_dbLib_menuNotFound);
	    if(index<0 || index>=pdbMenu->nChoice) return(S_dbLib_badField);
	    *pfield = (unsigned short)index;
	    return(0);
	}
    case DBF_DEVICE: {
	    dbDeviceMenu *pdbDeviceMenu;

	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(S_dbLib_menuNotFound);
	    if(index<0 || index>=pdbDeviceMenu->nChoice)
		return(S_dbLib_badField);
	    return(dbPutString(pdbentry,pdbDeviceMenu->papChoice[index]));
	}
    default:
	break;
    }
    return (S_dbLib_badField);
}
