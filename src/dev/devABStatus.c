/* devABStatus.c */
/*
 *      Original Authors: Marty Kraimer (nagging by Ned Arnold,Bob dalesio)
 *      Date:  3/6/91
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  04-20-95	mrk	Initial version
 * 	...
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<taskLib.h>

#include	<dbDefs.h>
#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<epicsPrint.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<drvAb.h>
#include        <mbbiRecord.h>

typedef struct {
	void		*drvPvt;
	IOSCANPVT	ioscanpvt;
}devPvt;


/* Create the dsets*/
typedef struct {
	long            number;
	DEVSUPFUN       report;
	DEVSUPFUN       init;
	DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_bi;} ABBIDSET;

LOCAL long init_common(struct mbbiRecord *prec);
LOCAL long read_adapter_status(struct mbbiRecord *prec);
LOCAL long read_card_status(struct mbbiRecord *prec);
ABBIDSET devMbbiAbAdapterStat=  { 5, 0, 0, init_common, 0, read_adapter_status};
ABBIDSET devMbbiAbCardStat=  { 5, 0, 0, init_common, 0, read_card_status};

LOCAL long init_common(struct mbbiRecord *prec)
{
    unsigned long	*pstate_values;
    char		*pstate_string;
    unsigned short	*palarm_sevr;
    short		i;

    if(abFailure>=16) {
	epicsPrintf("devMbbiAbLinkStat. > 16 error status values\n");
	taskSuspend(0);
	return(0);
    }
    pstate_values = &(prec->zrvl);
    pstate_string = prec->zrst;
    palarm_sevr = &prec->zrsv;
    /*Following code assumes that abFailure is last status value*/
    for(i=0; i<=abFailure; i++ ,pstate_string += sizeof(prec->zrst)){
	pstate_values[i] = 1<<i;
	if(pstate_string[0]=='\0')
		strncpy(pstate_string,abStatusMessage[i],sizeof(prec->zrst)-1);
	if(i>0) palarm_sevr[i] = MAJOR_ALARM;
    }
    return(0);
}

LOCAL long read_adapter_status(struct mbbiRecord *prec)
{
	abStatus	status;
	struct abio	*pabio;

	pabio = (struct abio *)&(prec->inp.value);
	status = (*pabDrv->adapterStatus)(pabio->link,pabio->adapter);
	prec->rval = 1 << status;;
	return(0);
}
LOCAL long read_card_status(struct mbbiRecord *prec)
{
	abStatus	status;
	struct abio	*pabio;

	pabio = (struct abio *)&(prec->inp.value);
	status = (*pabDrv->cardStatus)(pabio->link,pabio->adapter,pabio->card);
	prec->rval = 1 << status;;
	return(0);
}
