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

#include "dbDefs.h"
#include "errMdef.h"
#include "epicsPrint.h"
#include "ellLib.h"
#include "cvtFast.h"
#include "epicsTypes.h"
#include "epicsStdlib.h"

#define epicsExportSharedSymbols
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "devSup.h"
#include "special.h"


static char hex_digit_to_ascii[16]={'0','1','2','3','4','5','6','7','8','9',
		'a','b','c','d','e','f'};

static void ulongToHexString(epicsUInt32 source,char *pdest)
{
    epicsUInt32  val,temp;
    char  digit[10];
    int	  i,j;

    if(source==0) {
	strcpy(pdest,"0x0");
	return;
    }
    *pdest++ = '0'; *pdest++ = 'x';
    val = source;
    for(i=0; val!=0; i++) {
	temp = val/16;
	digit[i] = hex_digit_to_ascii[val - temp*16];
	val = temp;
    }
    for(j=i-1; j>=0; j--) {
	*pdest++ = digit[j];
    }
    *pdest = 0;
    return;
}

static double delta[2] = {1e-6, 1e-15};
static int precision[2] = {6, 14};
static void realToString(double value, char *preturn, int isdouble)
{
    double	absvalue;
    int		logval,prec,end;
    char	tstr[30];
    char	*ptstr = &tstr[0];
    int		round;
    int		ise = FALSE;
    char	*loce = NULL;

    if (value == 0) {
        strcpy(preturn, "0");
        return;
    }

    absvalue = value < 0 ? -value : value;
    if (absvalue < (double)INT_MAX) {
        epicsInt32 intval = (epicsInt32) value;
        double diff = value - intval;

        if (diff < 0) diff = -diff;
        if (diff < absvalue * delta[isdouble]) {
            cvtLongToString(intval, preturn);
            return;
        }
    }

    /*Now starts the hard cases*/
    if (value < 0) {
        *preturn++ = '-';
        value = -value;
    }

    logval = (int)log10(value);
    if (logval > 6 || logval < -2) {
        int nout;

        ise = TRUE;
        prec = precision[isdouble];
        nout = sprintf(ptstr, "%.*e", prec, value);
        loce = strchr(ptstr, 'e');

        if (!loce) {
            ptstr[nout] = 0;
            strcpy(preturn, ptstr);
            return;
        }

        *loce++ = 0;
    } else {
        prec = precision[isdouble] - logval;
        if ( prec < 0) prec = 0;
        sprintf(ptstr, "%.*f", prec, value);
    }

    if (prec > 0) {
        end = strlen(ptstr) - 1;
        round = FALSE;
        while (end > 0) {
            if (tstr[end] == '.') {end--; break;}
            if (tstr[end] == '0') {end--; continue;}
            if (!round && end < precision[isdouble]) break;
            if (!round && tstr[end] < '8') break;
            if (tstr[end-1] == '.') {
                if (round) end = end-2;
                break;
            }
            if (tstr[end-1] != '9') break;
            round = TRUE;
            end--;
        }
        tstr[end+1] = 0;
        while (round) {
            if (tstr[end] < '9') {tstr[end]++; break;}
            if (end == 0) { *preturn++ = '1'; tstr[end] = '0'; break;}
            tstr[end--] = '0';
        }
    }
    strcpy(preturn, &tstr[0]);
    if (ise) {
        if (!(strchr(preturn, '.'))) strcat(preturn, ".0");
        strcat(preturn, "e");
        strcat(preturn, loce);
    }
}

static void floatToString(float value,char *preturn)
{
    realToString((double)value,preturn,0);
    return;
}

static void doubleToString(double value,char *preturn)
{
    realToString(value,preturn,1);
    return;
}


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
    char		*precord;
    char		*pfield;
    
    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(pdbRecordType->rec_size == 0) {
	printf("\t*** Did you run x_RegisterRecordDeviceDriver(pdbbase) yet? ***\n");
	epicsPrintf("dbAllocRecord(%s) with %s rec_size = 0\n",
	    precordName, pdbRecordType->name);
	return(S_dbLib_noRecSup);
    }
    precnode->precord = dbCalloc(1,pdbRecordType->rec_size);
    precord = (char *)precnode->precord;
    pflddes = pdbRecordType->papFldDes[0];
    if(!pflddes) {
	epicsPrintf("dbAllocRecord pflddes for NAME not found\n");
	return(S_dbLib_flddesNotFound);
    }
    if(strlen(precordName)>=pflddes->size) {
	epicsPrintf("dbAllocRecord: NAME(%s) too long\n",precordName);
	return(S_dbLib_nameLength);
    }
    pfield = precord + pflddes->offset;
    strcpy(pfield,precordName);
    for(i=1; i<pdbRecordType->no_fields; i++) {

	pflddes = pdbRecordType->papFldDes[i];
	if(!pflddes) continue;
	pfield = precord + pflddes->offset;
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
		    plink->value.constantStr =
			dbCalloc(strlen(pflddes->initial)+1,sizeof(char));
		    strcpy(plink->value.constantStr,pflddes->initial);
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

epicsShareFunc int epicsShareAPI dbIsDefaultValue(DBENTRY *pdbentry)
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

char *dbGetStringNum(DBENTRY *pdbentry)
{
    dbFldDes  	*pflddes = pdbentry->pflddes;
    void	*pfield = pdbentry->pfield;
    char	*message;
    unsigned char cvttype;

    if(!pdbentry->message) pdbentry->message = dbCalloc(1,50);
    message = pdbentry->message;
    cvttype = pflddes->base;
    switch (pflddes->field_type) {
    case DBF_CHAR:
	if(cvttype==CT_DECIMAL)
	    cvtCharToString(*(char*)pfield, message);
	else
	    ulongToHexString((epicsUInt32)(*(char*)pfield),message);
	break;
    case DBF_UCHAR:
	if(cvttype==CT_DECIMAL)
	    cvtUcharToString(*(unsigned char*)pfield, message);
	else
	    ulongToHexString((epicsUInt32)(*(unsigned char*)pfield),message);
	break;
    case DBF_SHORT:
	if(cvttype==CT_DECIMAL)
	    cvtShortToString(*(short*)pfield, message);
	else
	    ulongToHexString((epicsUInt32)(*(short*)pfield),message);
	break;
    case DBF_USHORT:
    case DBF_ENUM:
	if(cvttype==CT_DECIMAL)
	    cvtUshortToString(*(unsigned short*)pfield, message); 
	else
	    ulongToHexString((epicsUInt32)(*(unsigned short*)pfield),message);
	break;
    case DBF_LONG:
	if(cvttype==CT_DECIMAL) 
	    cvtLongToString(*(epicsInt32*)pfield, message);
	else
	    ulongToHexString((epicsUInt32)(*(epicsInt32*)pfield), message);
	break;
    case DBF_ULONG:
	if(cvttype==CT_DECIMAL)
	    cvtUlongToString(*(epicsUInt32 *)pfield, message);
	else
	    ulongToHexString(*(epicsUInt32*)pfield, message);
	break;
    case DBF_FLOAT:
	floatToString(*(float *)pfield,message);
	break;
    case DBF_DOUBLE:
	doubleToString(*(double *)pfield,message);
	break;
    case DBF_MENU: {
	    dbMenu	*pdbMenu = (dbMenu *)pflddes->ftPvt;
	    short	choice_ind;
	    char	*pchoice;

	    if(!pfield) {strcpy(message,"Field not found"); return(message);}
	    choice_ind = *((short *) pdbentry->pfield);
	    if(!pdbMenu || choice_ind<0 || choice_ind>=pdbMenu->nChoice)
		return(NULL);
	    pchoice = pdbMenu->papChoiceValue[choice_ind];
	    strcpy(message, pchoice);
	}
	break;
    case DBF_DEVICE: {
	    dbDeviceMenu	*pdbDeviceMenu;
	    char		*pchoice;
	    short		choice_ind;

	    if(!pfield) {strcpy(message,"Field not found"); return(message);}
	    pdbDeviceMenu = dbGetDeviceMenu(pdbentry);
	    if(!pdbDeviceMenu) return(NULL);
	    choice_ind = *((short *) pdbentry->pfield);
	    if(choice_ind<0 || choice_ind>=pdbDeviceMenu->nChoice)
		return(NULL);
	    pchoice = pdbDeviceMenu->papChoice[choice_ind];
	    strcpy(message, pchoice);
	}
	break;
    default:
	return(NULL);
    }
    return (message);
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
	    case DBF_CHAR : *(char *)pfield = value; break;
	    case DBF_SHORT : *(short *)pfield = value; break;
	    case DBF_LONG : *(epicsInt32 *)pfield = value; break;
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
	    case DBF_UCHAR : *(unsigned char *)pfield = value; break;
	    case DBF_USHORT:
	    case DBF_ENUM: *(unsigned short *)pfield=value; break;
	    case DBF_ULONG : *(epicsUInt32 *)pfield = value; break;
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

epicsShareFunc int epicsShareAPI dbGetMenuIndex(DBENTRY *pdbentry)
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

epicsShareFunc long epicsShareAPI dbPutMenuIndex(DBENTRY *pdbentry,int index)
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
