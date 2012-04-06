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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char	*pbuffer = (char *)pto;
    char	*psrc=(char *)paddr->pfield;
    short	value;

    if(nRequest==1 && offset==0) {
	if(sscanf(psrc,"%hd",&value) == 1) {
		*pbuffer = (char)value;
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
	    *pbuffer = (char)value;
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

static long getStringUchar(
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char	*pbuffer = (unsigned char *)pto;
    char		*psrc=(char *)paddr->pfield;
    unsigned short  	value;

    if(nRequest==1 && offset==0) {
	if(sscanf(psrc,"%hu",&value) == 1) {
		*pbuffer = (unsigned char)value;
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
	    *pbuffer = (unsigned char)value;
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

static long getStringShort(
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    float  *pbuffer = (float *)pto;
    char   *psrc=(char *)paddr->pfield;
    float  value;

    if(nRequest==1 && offset==0) {
	if(epicsScanFloat(psrc, &value) == 1) {
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
	if(epicsScanFloat(psrc, &value) == 1) {
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

static long getStringDouble(
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    return(getStringUshort(paddr,pto,nRequest,no_elements,offset));
}

static long getCharString(
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *pbuffer = (char *)pto;
    char *psrc=(char *)(paddr->pfield);

    if (paddr->pfldDes && paddr->pfldDes->field_type == DBF_STRING) {
        /* This is a DBF_STRING field being read as a long string.
         * The buffer we return must be zero-terminated.
         */
        pbuffer[--nRequest] = 0;
        if (nRequest == 0) return(0);
    }
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    unsigned char *pbuffer = (unsigned char *)pto;
    char *psrc=(char *)(paddr->pfield);

    if (paddr->pfldDes && paddr->pfldDes->field_type == DBF_STRING) {
        /* This is a DBF_STRING field being read as a long string.
         * The buffer we return must be zero-terminated.
         */
        pbuffer[--nRequest] = 0;
        if (nRequest == 0) return(0);
    }
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *pbuffer = (epicsInt32 *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = (epicsInt32)*psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = (epicsInt32)*psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUlong(
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *pbuffer = (epicsUInt32 *)pto;
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = (epicsUInt32)*psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = (epicsUInt32)*psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleFloat(
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
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

static long getMenuString(
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char		*pbuffer = (char *)pto;
    dbFldDes		*pdbFldDes = paddr->pfldDes;
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

static long getDeviceString(
    const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char		*pbuffer = (char *)pto;
    dbFldDes		*pdbFldDes = paddr->pfldDes;
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *pbuffer = (const char *)pfrom;
    epicsInt32  *pdest=(epicsInt32 *)paddr->pfield;
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *pbuffer = (const char *)pfrom;
    epicsUInt32 *pdest=(epicsUInt32 *)paddr->pfield;
    double  	value;

    /*Convert to double first so that numbers like 1.0e3 convert properly*/
    /*Problem was old database access said to get unsigned long as double*/
    if(nRequest==1 && offset==0) {
	if(epicsScanDouble(pbuffer, &value) == 1) {
		*pdest = (epicsUInt32)value;
		return(0);
	}
	else return(-1);
    }
    pdest += offset;
    while (nRequest) {
	if(epicsScanDouble(pbuffer, &value) == 1) {
	    *pdest = (epicsUInt32)value;
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
		nargs = sscanf(pbuffer,"%u%n",&ind,&nchars);
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *pbuffer = (const char *)pfrom;
    dbFldDes		*pdbFldDes = paddr->pfldDes;
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
	nargs = sscanf(pbuffer,"%u%n",&ind,&nchars);
	if(nargs==1 && nchars==strlen(pbuffer) && ind<nChoice) {
	    *pfield = ind;
	    return(0);
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringMenu)");
    return(S_db_badChoice);
}

static long putStringDevice(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *pbuffer = (const char *)pfrom;
    dbFldDes		*pdbFldDes = paddr->pfldDes;
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
	nargs = sscanf(pbuffer,"%u%n",&ind,&nchars);
	if(nargs==1 && nchars==strlen(pbuffer) && ind<nChoice) {
	    *pfield = ind;
	    return(0);
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringDevice)");
    return(S_db_badChoice);
}

static long putCharString(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *pbuffer = (const char *)pfrom;
    epicsInt32 *pdest=(epicsInt32 *)(paddr->pfield);

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

static long putCharUlong(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *pbuffer = (const char *)pfrom;
    epicsUInt32 *pdest=(epicsUInt32 *)(paddr->pfield);

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

static long putCharFloat(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
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

static long putUcharUlong(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const unsigned char *pbuffer = (const unsigned char *)pfrom;
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

static long putUcharFloat(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const short *pbuffer = (const short *)pfrom;
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

static long putShortUlong(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const short *pbuffer = (const short *)pfrom;
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

static long putShortFloat(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const double *pbuffer = (const double *)pfrom;
    epicsInt32  *pdest=(epicsInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = (epicsInt32)*pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = (epicsInt32)*pbuffer++;
	if(++offset==no_elements) pdest=(epicsInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUlong(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const double *pbuffer = (const double *)pfrom;
    epicsUInt32  *pdest=(epicsUInt32 *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = (epicsUInt32)*pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = (epicsUInt32)*pbuffer++;
	if(++offset==no_elements) pdest=(epicsUInt32 *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleFloat(
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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
    dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
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

epicsShareDef GETCONVERTFUNC dbGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1] = {

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

epicsShareDef PUTCONVERTFUNC dbPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1] = {
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
