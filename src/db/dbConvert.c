/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbConvert.c */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            11-7-90
*/

#include <stddef.h>
#include <epicsStdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "epicsConvert.h"
#include "dbDefs.h"
#include "errlog.h"
#include "cvtFast.h"
#include "dbBase.h"
#include "link.h"
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "errMdef.h"
#include "recSup.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "dbAccessDefs.h"
#include "recGbl.h"
#include "dbConvert.h"

/* DATABASE ACCESS GET CONVERSION SUPPORT */

static long getStringString (
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char  *pbuffer = (char *)pto;
    char  *psrc=paddr->pfield;
    short size=paddr->field_size;
    short sizeto;

    /* always force result string to be null terminated*/
    sizeto = size;
    if(sizeto>=MAX_STRING_SIZE) sizeto = MAX_STRING_SIZE-1;

    if(nRequest==1 && offset==0) {
	strncpy(pbuffer,psrc,sizeto);
	*(pbuffer+sizeto) = 0;
	return(0);
    }
    psrc+= (size*offset);
    while (nRequest) {
        strncpy(pbuffer,psrc,sizeto);
	*(pbuffer+sizeto) = 0;
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=paddr->pfield;
	else
    		psrc  += size;
	nRequest--;
    }
    return(0);
}

static long getStringChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char	*pbuffer = (char *)pto;
    char	*psrc=(char *)paddr->pfield;
    short	value;

    if(nRequest==1 && offset==0) {
	if(sscanf(psrc,"%hd",&value) == 1) {
		*pbuffer = (char)value;
		return(0);
	} else if(strlen(psrc) == 0) {
		*pbuffer = '0';
		return(0);
	} else {
		return(-1);
	}
    }
    psrc += MAX_STRING_SIZE*offset;
    while (nRequest) {
	if(sscanf(psrc,"%hd",&value) == 1) {
	    *pbuffer = (char)value;
	} else if(strlen(psrc) == 0) {
		*pbuffer = '0';
	} else {
		return(-1);
	}
	pbuffer++;
	if(++offset==no_elements)
	    psrc=paddr->pfield;
	else
	    psrc += MAX_STRING_SIZE;
	nRequest--;
    }
    return(0);
}

static long getStringUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char	*pbuffer = (unsigned char *)pto;
    char		*psrc=(char *)paddr->pfield;
    unsigned short  	value;

    if(nRequest==1 && offset==0) {
	if(sscanf(psrc,"%hu",&value) == 1) {
		*pbuffer = (unsigned char)value;
		return(0);
	} else if(strlen(psrc) == 0) {
		*pbuffer = '0';
		return(0);
	} else {
		return(-1);
	}
    }
    psrc += MAX_STRING_SIZE*offset;
    while (nRequest) {
	if(sscanf(psrc,"%hu",&value) == 1) {
	    *pbuffer = (unsigned char)value;
	} else if(strlen(psrc) == 0) {
		*pbuffer = '0';
	} else {
		return(-1);
	}
	pbuffer++;
	if(++offset==no_elements)
	    psrc=paddr->pfield;
	else
	    psrc += MAX_STRING_SIZE;
	nRequest--;
    }
    return(0);
}

static long getStringShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short  *pbuffer = (short *)pto;
    char   *psrc=(char *)paddr->pfield;
    short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(psrc,"%hd",&value) == 1) {
		*pbuffer = value;
		return(0);
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0;
		return(0);
	} else {
		return(-1);
	}
    }
    psrc += MAX_STRING_SIZE*offset;
    while (nRequest) {
	if(sscanf(psrc,"%hd",&value) == 1) {
	    *pbuffer = value;
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0;
	} else {
		return(-1);
	}
	pbuffer++;
	if(++offset==no_elements)
	    psrc=paddr->pfield;
	else
	    psrc += MAX_STRING_SIZE;
	nRequest--;
    }
    return(0);
}

static long getStringUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short	*pbuffer = (unsigned short *)pto;
    char   		*psrc=(char *)paddr->pfield;
    unsigned short  		value;

    if(nRequest==1 && offset==0) {
	if(sscanf(psrc,"%hu",&value) == 1) {
		*pbuffer = value;
		return(0);
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0;
		return(0);
	} else {
		return(-1);
	}
    }
    psrc += MAX_STRING_SIZE*offset;
    while (nRequest) {
	if(sscanf(psrc,"%hu",&value) == 1) {
	    *pbuffer = value;
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0;
	} else {
		return(-1);
	}
	pbuffer++;
	if(++offset==no_elements)
	    psrc=paddr->pfield;
	else
	    psrc += MAX_STRING_SIZE;
	nRequest--;
    }
    return(0);
}

static long getStringLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32   *pbuffer = (epicsInt32 *)pto;
    char   *psrc=(char *)paddr->pfield;
    epicsInt32  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(psrc,"%d",&value) == 1) {
		*pbuffer = value;
		return(0);
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0;
		return(0);
	} else {
		return(-1);
	}
    }
    psrc += MAX_STRING_SIZE*offset;
    while (nRequest) {
	if(sscanf(psrc,"%d",&value) == 1) {
	    *pbuffer = value;
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0;
	} else {
		return(-1);
	}
	pbuffer++;
	if(++offset==no_elements)
	    psrc=paddr->pfield;
	else
	    psrc += MAX_STRING_SIZE;
	nRequest--;
    }
    return(0);
}

static long getStringUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32	*pbuffer = (epicsUInt32 *)pto;
    char   	*psrc=(char *)paddr->pfield;
    double  	value;

    /*Convert to double first so that numbers like 1.0e3 convert properly*/
    /*Problem was old database access said to get unsigned long as double*/
    if(nRequest==1 && offset==0) {
	if(epicsScanDouble(psrc, &value) == 1) {
		*pbuffer = (epicsUInt32)value;
		return(0);
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0;
		return(0);
	} else {
		return(-1);
	}
    }
    psrc += MAX_STRING_SIZE*offset;
    while (nRequest) {
	if(epicsScanDouble(psrc, &value) == 1) {
	    *pbuffer = (epicsUInt32)value;
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0;
	} else {
		return(-1);
	}
	pbuffer++;
	if(++offset==no_elements)
	    psrc=paddr->pfield;
	else
	    psrc += MAX_STRING_SIZE;
	nRequest--;
    }
    return(0);
}

static long getStringFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float  *pbuffer = (float *)pto;
    char   *psrc=(char *)paddr->pfield;
    float  value;

    if(nRequest==1 && offset==0) {
	if(epicsScanFloat(psrc, &value) == 1) {
		*pbuffer = value;
		return(0);
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0.0;
		return(0);
	} else {
		return(-1);
	}
    }
    psrc += MAX_STRING_SIZE*offset;
    while (nRequest) {
	if(epicsScanFloat(psrc, &value) == 1) {
	    *pbuffer = value;
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0.0;
	} else {
		return(-1);
	}
	pbuffer++;
	if(++offset==no_elements)
	    psrc=paddr->pfield;
	else
	    psrc += MAX_STRING_SIZE;
	nRequest--;
    }
    return(0);
}

static long getStringDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    char   *psrc=(char *)paddr->pfield;
    double  value;

    if(nRequest==1 && offset==0) {
	if(epicsScanDouble(psrc, &value) == 1) {
		*pbuffer = value;
		return(0);
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0.0;
		return(0);
	} else {
		return(-1);
	}
    }
    psrc += MAX_STRING_SIZE*offset;
    while (nRequest) {
	if(epicsScanDouble(psrc, &value) == 1) {
	    *pbuffer = value;
	} else if(strlen(psrc) == 0) {
		*pbuffer = 0.0;
	} else {
		return(-1);
	}
	pbuffer++;
	if(++offset==no_elements)
	    psrc=paddr->pfield;
	else
	    psrc += MAX_STRING_SIZE;
	nRequest--;
    }
    return(0);
}

static long getStringEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    return(getStringUshort(paddr,pto,nRequest,no_elements,offset));
}

static long getCharString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtCharToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtCharToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(char *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getCharChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short *pbuffer = (short *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short *pbuffer = (unsigned short *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float *pbuffer = (float *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getCharDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pbuffer = (epicsEnum16 *)pto;
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtUcharToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtUcharToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(unsigned char *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUcharChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short *pbuffer = (short *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short *pbuffer = (unsigned short *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float *pbuffer = (float *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUcharDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pbuffer = (epicsEnum16 *)pto;
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtShortToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtShortToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(short *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getShortChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
static long getShortShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short *pbuffer = (short *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short *pbuffer = (unsigned short *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float *pbuffer = (float *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getShortDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pbuffer = (epicsEnum16 *)pto;
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtUshortToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtUshortToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(unsigned short *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUshortChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
static long getUshortShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short *pbuffer = (short *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short *pbuffer = (unsigned short *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float *pbuffer = (float *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUshortDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pbuffer = (epicsEnum16 *)pto;
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtLongToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtLongToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(epicsInt32 *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getLongChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short *pbuffer = (short *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short *pbuffer = (unsigned short *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float *pbuffer = (float *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getLongDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16  *pbuffer = (epicsEnum16  *)pto;
    epicsInt32 *psrc=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char  *pbuffer = (char  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtUlongToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtUlongToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(epicsUInt32 *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUlongChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char  *pbuffer = (char  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char  *pbuffer = (unsigned char  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short  *pbuffer = (short  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short  *pbuffer = (unsigned short  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32  *pbuffer = (epicsInt32  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32  *pbuffer = (epicsUInt32  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float  *pbuffer = (float  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUlongDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double  *pbuffer = (double  *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pbuffer = (epicsEnum16 *)pto;
    epicsUInt32 *psrc=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    float	*psrc=(float *)(paddr->pfield);
    long	status = 0;
    int		precision = 6;
    struct rset	*prset = 0;

    if(paddr) prset = dbGetRset(paddr);
    if(prset && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    if(nRequest==1 && offset==0) {
	cvtFloatToString(*psrc,pbuffer,precision);
	return(status);
    }
    psrc += offset;
    while (nRequest) {
	cvtFloatToString(*psrc,pbuffer,precision);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(float *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(status);
}

static long getFloatChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short *pbuffer = (short *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short *pbuffer = (unsigned short *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float *pbuffer = (float *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getFloatDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pbuffer = (epicsEnum16 *)pto;
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    double *psrc=(double *)(paddr->pfield);
    long	status = 0;
    int		precision = 6;
    struct rset	*prset = 0;

    if(paddr) prset = dbGetRset(paddr);
    if(prset && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    if(nRequest==1 && offset==0) {
	cvtDoubleToString(*psrc,pbuffer,precision);
	return(status);
    }
    psrc += offset;
    while (nRequest) {
	cvtDoubleToString(*psrc,pbuffer,precision);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(double *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(status);
}

static long getDoubleChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short *pbuffer = (short *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short *pbuffer = (unsigned short *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float *pbuffer = (float *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
        *pbuffer = epicsConvertDoubleToFloat(*psrc);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
        *pbuffer = epicsConvertDoubleToFloat(*psrc);
        ++psrc; ++pbuffer;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getDoubleDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pbuffer = (epicsEnum16 *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumString(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    struct rset	*prset;
    long	status;

    if((prset=dbGetRset(paddr)) && (prset->get_enum_str))
        return( (*prset->get_enum_str)(paddr,pbuffer) );
    status=S_db_noRSET;
    recGblRecSupError(status,paddr,"dbGet","get_enum_str");
    return(S_db_badDbrtype);
}

static long getEnumChar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUchar(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumShort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    short *pbuffer = (short *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUshort(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    unsigned short *pbuffer = (unsigned short *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumLong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUlong(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumFloat(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    float *pbuffer = (float *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getEnumDouble(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    double *pbuffer = (double *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumEnum(
    DBADDR *paddr, void *pto, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pbuffer = (epicsEnum16 *)pto;
    epicsEnum16 *psrc=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getMenuString(DBADDR *paddr, void *pto,
    long nRequest, long no_elements, long offset)
{
    char		*pbuffer = (char *)pto;	
    dbFldDes		*pdbFldDes = (dbFldDes *)paddr->pfldDes;
    dbMenu		*pdbMenu;
    char		**papChoiceValue;
    char		*pchoice;
    epicsEnum16	choice_ind= *((epicsEnum16*)paddr->pfield);

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGet(getMenuString)");
        return(S_db_onlyOne);
    }
    if(!pdbFldDes
    || !(pdbMenu = (dbMenu *)pdbFldDes->ftPvt)
    || (choice_ind>=pdbMenu->nChoice)
    || !(papChoiceValue = pdbMenu->papChoiceValue)
    || !(pchoice=papChoiceValue[choice_ind])) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGet(getMenuString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pchoice,MAX_STRING_SIZE);
    return(0);
}

static long getDeviceString(DBADDR *paddr, void *pto,
    long nRequest, long no_elements, long offset)
{
    char		*pbuffer = (char *)pto;	
    dbFldDes		*pdbFldDes = (dbFldDes *)paddr->pfldDes;
    dbDeviceMenu	*pdbDeviceMenu;
    char		**papChoice;
    char		*pchoice;
    epicsEnum16	choice_ind= *((epicsEnum16*)paddr->pfield);

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGet(getDeviceString)");
        return(S_db_onlyOne);
    }
    if(!pdbFldDes
    || !(pdbDeviceMenu = (dbDeviceMenu *)pdbFldDes->ftPvt)
    || (choice_ind>=pdbDeviceMenu->nChoice )
    || !(papChoice = pdbDeviceMenu->papChoice)
    || !(pchoice=papChoice[choice_ind])) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGet(getDeviceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pchoice,MAX_STRING_SIZE);
    return(0);
}

/* DATABASE ACCESS PUT CONVERSION SUPPORT */

static long putStringString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    char  *pdest=paddr->pfield;
    short size=paddr->field_size;

    if(nRequest==1 && offset==0) {
	strncpy(pdest,pbuffer,size);
	*(pdest+size-1) = 0;
	return(0);
    }
    pdest+= (size*offset);
    while (nRequest) {
        strncpy(pdest,pbuffer,size);
	*(pdest+size-1) = 0;
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest  += size;
	nRequest--;
    }
    return(0);
}

static long putStringChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    char  *pdest=(char *)paddr->pfield;
    short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hd",&value) == 1) {
		*pdest = (char)value;
		return(0);
	}
	else return(-1);
    }
    pdest += offset;
    while (nRequest) {
	if(sscanf(pbuffer,"%hd",&value) == 1) {
	    *pdest = (char)value;
	} else {
	    return(-1);
	}
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
	    pdest=paddr->pfield;
	else
	    pdest++;
	nRequest--;
    }
    return(0);
}

static long putStringUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    unsigned char  *pdest=(unsigned char *)paddr->pfield;
    unsigned short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hu",&value) == 1) {
		*pdest = (unsigned char)value;
		return(0);
	}
	else return(-1);
    }
    pdest += offset;
    while (nRequest) {
	if(sscanf(pbuffer,"%hu",&value) == 1) {
	    *pdest = (unsigned char)value;
	} else {
	    return(-1);
	}
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
	    pdest=paddr->pfield;
	else
	    pdest++;
	nRequest--;
    }
    return(0);
}

static long putStringShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    short  *pdest=(short *)paddr->pfield;
    short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hd",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    pdest += offset;
    while (nRequest) {
	if(sscanf(pbuffer,"%hd",&value) == 1) {
	    *pdest = value;
	} else {
	    return(-1);
	}
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
	    pdest=paddr->pfield;
	else
	    pdest++;
	nRequest--;
    }
    return(0);
}

static long putStringUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    unsigned short  *pdest=(unsigned short *)paddr->pfield;
    unsigned short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hu",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    pdest += offset;
    while (nRequest) {
	if(sscanf(pbuffer,"%hu",&value) == 1) {
	    *pdest = value;
	} else {
	    return(-1);
	}
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
	    pdest=paddr->pfield;
	else
	    pdest++;
	nRequest--;
    }
    return(0);
}

static long putStringLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    long  *pdest=(long *)paddr->pfield;
    long  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%ld",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    pdest += offset;
    while (nRequest) {
	if(sscanf(pbuffer,"%ld",&value) == 1) {
	    *pdest = value;
	} else {
	    return(-1);
	}
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
	    pdest=paddr->pfield;
	else
	    pdest++;
	nRequest--;
    }
    return(0);
}

static long putStringUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    unsigned long  *pdest=(unsigned long *)paddr->pfield;
    double  	value;

    /*Convert to double first so that numbers like 1.0e3 convert properly*/
    /*Problem was old database access said to get unsigned long as double*/
    if(nRequest==1 && offset==0) {
	if(epicsScanDouble(pbuffer, &value) == 1) {
		*pdest = (unsigned long)value;
		return(0);
	}
	else return(-1);
    }
    pdest += offset;
    while (nRequest) {
	if(epicsScanDouble(pbuffer, &value) == 1) {
	    *pdest = (unsigned long)value;
	} else {
	    return(-1);
	}
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
	    pdest=paddr->pfield;
	else
	    pdest++;
	nRequest--;
    }
    return(0);
}

static long putStringFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    float  *pdest=(float *)paddr->pfield;
    float  value;

    if(nRequest==1 && offset==0) {
	if(epicsScanFloat(pbuffer, &value) == 1) {
		*pdest = value;
		return(0);
	} else {
	    return(-1);
	}
    }
    pdest += offset;
    while (nRequest) {
	if(epicsScanFloat(pbuffer, &value) == 1) {
	    *pdest = value;
	} else {
	    return(-1);
	}
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
	    pdest=paddr->pfield;
	else
	    pdest++;
	nRequest--;
    }
    return(0);
}

static long putStringDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    double  *pdest=(double *)paddr->pfield;
    double  value;

    if(nRequest==1 && offset==0) {
	if(epicsScanDouble(pbuffer, &value) == 1) {
		*pdest = value;
		return(0);
	} else {
	    return(-1);
	}
    }
    pdest += offset;
    while (nRequest) {
	if(epicsScanDouble(pbuffer, &value) == 1) {
	    *pdest = value;
	} else {
	    return(-1);
	}
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
	    pdest=paddr->pfield;
	else
	    pdest++;
	nRequest--;
    }
    return(0);
}

static long putStringEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    struct rset 	*prset;
    epicsEnum16      *pfield= (epicsEnum16*)(paddr->pfield);
    long        	status;
    unsigned int	nchoices,ind;
    int			nargs,nchars;
    struct dbr_enumStrs enumStrs;

    if((prset=dbGetRset(paddr))
    && (prset->put_enum_str)) {
	status = (*prset->put_enum_str)(paddr,pbuffer);
	if(!status) return(0);
	if(prset->get_enum_strs) {
	    status = (*prset->get_enum_strs)(paddr,&enumStrs);
	    if(!status) {
		nchoices = enumStrs.no_str;
		nargs = sscanf(pbuffer," %u %n",&ind,&nchars);
		if(nargs==1 && nchars==strlen(pbuffer) && ind<nchoices) {
		    *pfield = ind;
		    return(0);
		}
		status = S_db_badChoice;
	    }
	}else {
	    status=S_db_noRSET;
	}
    } else {
	status=S_db_noRSET;
    }
    if(status == S_db_noRSET) {
	recGblRecSupError(status,paddr,"dbPutField","put_enum_str");
    } else {
	recGblRecordError(status,(void *)paddr->precord,pbuffer);
    }
    return(status);
}

static long putStringMenu(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    dbFldDes		*pdbFldDes = (dbFldDes *)paddr->pfldDes;
    dbMenu		*pdbMenu;
    char		**papChoiceValue;
    char		*pchoice;
    epicsEnum16      *pfield= (epicsEnum16*)(paddr->pfield);
    unsigned int	nChoice,ind;
    int			nargs,nchars;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringMenu)");
        return(S_db_onlyOne);
    }
    if(pdbFldDes
    && (pdbMenu = (dbMenu *)pdbFldDes->ftPvt)
    && (papChoiceValue = pdbMenu->papChoiceValue)) {
	nChoice = pdbMenu->nChoice;
	for(ind=0; ind<nChoice; ind++) {
	    if(!(pchoice=papChoiceValue[ind])) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield = ind;
		return(0);
	    }
	}
	nargs = sscanf(pbuffer," %u %n",&ind,&nchars);
	if(nargs==1 && nchars==strlen(pbuffer) && ind<nChoice) {
	    *pfield = ind;
	    return(0);
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringMenu)");
    return(S_db_badChoice);
}

static long putStringDevice(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    dbFldDes		*pdbFldDes = (dbFldDes *)paddr->pfldDes;
    dbDeviceMenu	*pdbDeviceMenu = (dbDeviceMenu *)pdbFldDes->ftPvt;
    char		**papChoice;
    char		*pchoice;
    epicsEnum16      *pfield= (epicsEnum16*)(paddr->pfield);
    unsigned int	nChoice,ind;
    int			nargs,nchars;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringDevice)");
        return(S_db_onlyOne);
    }
    if(pdbFldDes
    && (pdbDeviceMenu = (dbDeviceMenu *)pdbFldDes->ftPvt)
    && (papChoice = pdbDeviceMenu->papChoice)) {
	nChoice = pdbDeviceMenu->nChoice;
	for(ind=0; ind<nChoice; ind++) {
	    if(!(pchoice=papChoice[ind])) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield = ind;
		return(0);
	    }
	}
	nargs = sscanf(pbuffer," %u %n",&ind,&nchars);
	if(nargs==1 && nchars==strlen(pbuffer) && ind<nChoice) {
	    *pfield = ind;
	    return(0);
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringDevice)");
    return(S_db_badChoice);
}

static long putCharString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtCharToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtCharToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putCharChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putCharDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const char *pbuffer = (const char *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtUcharToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtUcharToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUcharChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUcharDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtShortToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtShortToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putShortChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putShortDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const short *pbuffer = (const short *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtUshortToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtUshortToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUshortChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    epicsInt32  *pdest=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    epicsUInt32  *pdest=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUshortDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const unsigned short *pbuffer = (const unsigned short *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtLongToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtLongToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putLongChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    epicsInt32  *pdest=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    epicsUInt32  *pdest=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putLongDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsInt32 *pbuffer = (const epicsInt32 *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtUlongToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtUlongToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUlongChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    epicsInt32  *pdest=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    epicsUInt32  *pdest=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUlongDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsUInt32 *pbuffer = (const epicsUInt32 *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    char	*pdest=(char *)(paddr->pfield);
    long	status = 0;
    int		precision = 6;
    struct rset	*prset = 0;
    short size=paddr->field_size;

    if(paddr) prset = dbGetRset(paddr);
    if(prset && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    if(nRequest==1 && offset==0) {
	cvtFloatToString(*pbuffer,pdest,precision);
	return(status);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtFloatToString(*pbuffer,pdest,precision);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(status);
}

static long putFloatChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    char   *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    unsigned char   *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    epicsInt32  *pdest=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    epicsUInt32  *pdest=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putFloatDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const float *pbuffer = (const float *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    char	*pdest=(char *)(paddr->pfield);
    long	status = 0;
    int		precision = 6;
    struct rset	*prset = 0;
    short size=paddr->field_size;

    if(paddr) prset = dbGetRset(paddr);
    if(prset && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    if(nRequest==1 && offset==0) {
	cvtDoubleToString(*pbuffer,pdest,precision);
	return(status);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtDoubleToString(*pbuffer,pdest,precision);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(status);
}

static long putDoubleChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    char   *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    unsigned char   *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    epicsInt32  *pdest=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    epicsUInt32  *pdest=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
        *pdest = epicsConvertDoubleToFloat(*pbuffer);
	return(0);
    }
    pdest += offset;
    while (nRequest) {
        *pdest = epicsConvertDoubleToFloat(*pbuffer);
        ++pbuffer; ++pdest;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putDoubleDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const double *pbuffer = (const double *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumString(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtUshortToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtUshortToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putEnumChar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUchar(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumShort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUshort(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumLong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    epicsInt32  *pdest=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUlong(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    epicsUInt32  *pdest=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumFloat(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putEnumDouble(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumEnum(
    DBADDR *paddr,const void *pfrom,long nRequest,long no_elements,long offset)
{
    const epicsEnum16 *pbuffer = (const epicsEnum16 *)pfrom;
    epicsEnum16  *pdest=(epicsEnum16 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(epicsEnum16 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

/* This is the table of routines for converting database fields */
/* the rows represent the field type of the database field */
/* the columns represent the types of the buffer in which they are placed */

/* buffer types are********************************************************
 DBR_STRING,      DBR_CHR,         DBR_UCHAR,       DBR_SHORT,       DBR_USHORT,
 DBR_LONG,        DBR_ULONG,       DBR_FLOAT,       DBR_DOUBLE,      DBR_ENUM
 ***************************************************************************/

epicsShareDef long (*dbGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1])
    (struct dbAddr *paddr, void *pbuffer, long nRequest, long no_elements, long offset) = {

/* source is a DBF_STRING		*/
{getStringString, getStringChar,   getStringUchar,  getStringShort,  getStringUshort,
 getStringLong,   getStringUlong,  getStringFloat,  getStringDouble, getStringEnum},
/* source is a DBF_CHAR 		*/
{getCharString,   getCharChar,     getCharUchar,    getCharShort,    getCharUshort,
 getCharLong,     getCharUlong,    getCharFloat,    getCharDouble,   getCharEnum},
/* source is a DBF_UCHAR		*/
{getUcharString,  getUcharChar,    getUcharUchar,   getUcharShort,   getUcharUshort,
 getUcharLong,    getUcharUlong,   getUcharFloat,   getUcharDouble,  getUcharEnum},
/* source is a DBF_SHORT		*/
{getShortString,  getShortChar,    getShortUchar,   getShortShort,   getShortUshort,
 getShortLong,    getShortUlong,   getShortFloat,   getShortDouble,  getShortEnum},
/* source is a DBF_USHORT		*/
{getUshortString, getUshortChar,   getUshortUchar,  getUshortShort,  getUshortUshort,
 getUshortLong,   getUshortUlong,  getUshortFloat,  getUshortDouble, getUshortEnum},
/* source is a DBF_LONG		*/
{getLongString,   getLongChar,     getLongUchar,    getLongShort,    getLongUshort,
 getLongLong,     getLongUlong,    getLongFloat,    getLongDouble,   getLongEnum},
/* source is a DBF_ULONG		*/
{getUlongString,  getUlongChar,    getUlongUchar,   getUlongShort,   getUlongUshort,
 getUlongLong,    getUlongUlong,   getUlongFloat,   getUlongDouble,  getUlongEnum},
/* source is a DBF_FLOAT		*/
{getFloatString,  getFloatChar,    getFloatUchar,   getFloatShort,   getFloatUshort,
 getFloatLong,    getFloatUlong,   getFloatFloat,   getFloatDouble,  getFloatEnum},
/* source is a DBF_DOUBLE		*/
{getDoubleString, getDoubleChar,   getDoubleUchar,  getDoubleShort,  getDoubleUshort,
 getDoubleLong,   getDoubleUlong,  getDoubleFloat,  getDoubleDouble, getDoubleEnum},
/* source is a DBF_ENUM		*/
{getEnumString,   getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_MENU	*/
{getMenuString,   getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_DEVICE	*/
{getDeviceString,getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum},
};

/* This is the table of routines for converting database fields */
/* the rows represent the buffer types				*/
/* the columns represent the field types			*/

/* field types are********************************************************
 DBF_STRING,      DBF_CHAR,        DBF_UCHAR,       DBF_SHORT,       DBF_USHORT,
 DBF_LONG,        DBF_ULONG,       DBF_FLOAT,       DBF_DOUBLE,      DBF_ENUM
 DBF_MENU,        DBF_DEVICE
 ***************************************************************************/

epicsShareDef long (*dbPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1])() = {
/* source is a DBR_STRING		*/
{putStringString, putStringChar,   putStringUchar,  putStringShort,  putStringUshort,
 putStringLong,   putStringUlong,  putStringFloat,  putStringDouble, putStringEnum,
 putStringMenu,putStringDevice},
/* source is a DBR_CHAR		*/
{putCharString,   putCharChar,     putCharUchar,    putCharShort,    putCharUshort,
 putCharLong,     putCharUlong,    putCharFloat,    putCharDouble,   putCharEnum,
 putCharEnum,     putCharEnum},
/* source is a DBR_UCHAR		*/
{putUcharString,  putUcharChar,    putUcharUchar,   putUcharShort,   putUcharUshort,
 putUcharLong,    putUcharUlong,   putUcharFloat,   putUcharDouble,  putUcharEnum,
 putUcharEnum,    putUcharEnum},
/* source is a DBR_SHORT		*/
{putShortString,  putShortChar,    putShortUchar,   putShortShort,   putShortUshort,
 putShortLong,    putShortUlong,   putShortFloat,   putShortDouble,  putShortEnum,
 putShortEnum,    putShortEnum},
/* source is a DBR_USHORT		*/
{putUshortString, putUshortChar,   putUshortUchar,  putUshortShort,  putUshortUshort,
 putUshortLong,   putUshortUlong,  putUshortFloat,  putUshortDouble, putUshortEnum,
 putUshortEnum,   putUshortEnum},
/* source is a DBR_LONG		*/
{putLongString,   putLongChar,     putLongUchar,    putLongShort,    putLongUshort,
 putLongLong,     putLongUlong,    putLongFloat,    putLongDouble,   putLongEnum,
 putLongEnum,     putLongEnum},
/* source is a DBR_ULONG		*/
{putUlongString,  putUlongChar,    putUlongUchar,   putUlongShort,   putUlongUshort,
 putUlongLong,    putUlongUlong,   putUlongFloat,   putUlongDouble,  putUlongEnum,
 putUlongEnum,    putUlongEnum},
/* source is a DBR_FLOAT		*/
{putFloatString,  putFloatChar,    putFloatUchar,   putFloatShort,   putFloatUshort,
 putFloatLong,    putFloatUlong,   putFloatFloat,   putFloatDouble,  putFloatEnum,
 putFloatEnum,    putFloatEnum},
/* source is a DBR_DOUBLE		*/
{putDoubleString, putDoubleChar,   putDoubleUchar,  putDoubleShort,  putDoubleUshort,
 putDoubleLong,   putDoubleUlong,  putDoubleFloat,  putDoubleDouble, putDoubleEnum,
 putDoubleEnum,   putDoubleEnum},
/* source is a DBR_ENUM		*/
{putEnumString,   putEnumChar,     putEnumUchar,    putEnumShort,    putEnumUshort,
 putEnumLong,     putEnumUlong,    putEnumFloat,    putEnumDouble,   putEnumEnum,
 putEnumEnum,     putEnumEnum}
};
