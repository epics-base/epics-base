/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devWfSymb.c */
/* base/src/dev $Id$ */

/* devWfSymb.c - Device Support for vxWorks Global Symbol Waveform Records*/
/*
 *      Author:   Andrew Johnson
 *      Date:     15-10-96
 */


#include	<vxWorks.h>
#include 	<sysSymTbl.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>
#include	<intLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<waveformRecord.h>
#include	<devSymb.h>


/* Create the dset for devWfSymb */
static long init_record();
static long read_wf();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
}devWfSymb={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf};

static int sizeofTypes[] = {MAX_STRING_SIZE,1,1,2,2,4,4,4,8,2};

double testwf[] = {1.1, 2.2, 4.4, 8.8, 16.16, 32.32};


static long init_record(pwf)
    struct waveformRecord	*pwf;
{
    /* determine address of record value */
    if (devSymbFind(pwf->name, &pwf->inp, &pwf->dpvt))
    {
	recGblRecordError(S_db_badField,(void *)pwf,
		"devWfSymb (init_record) Illegal NAME or INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
    long status, typesize;
    struct vxSym *private = (struct vxSym *) pwf->dpvt;
    int lockKey;

    if (private)
    {
        typesize = sizeofTypes[pwf->ftvl];
        
        /* Copy the data */
        lockKey = intLock();
        memcpy(pwf->bptr, 
               (char *)(*private->ppvar) + typesize * private->index, 
               pwf->nelm * typesize);
        intUnlock(lockKey);

        pwf->nord = pwf->nelm;	/* We always get it all */
        pwf->udf = FALSE;	/* Record also does this (but shouldn't) */
        status = 0;
    }
    else
        status = 1;
    
    return(status);	/* Record actually ignores this... */
}
