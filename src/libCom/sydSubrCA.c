/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	06-12-91
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
 * .00	06-12-91	rac	initial version
 * .01	06-18-91	rac	installed in SCCS
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	sydSubrCA.c - acquire synchronous samples from Channel Access
*
* DESCRIPTION
*	
* BUGS
* o	doesn't support asynchronous `search' for the initial connection
*	to a channel
*-***************************************************************************/
#include <genDefs.h>
#define SYD_PRIVATE
#include <sydDefs.h>
#include <fields.h>
#ifndef INCLcadefh
#   include <cadef.h>
#endif

#ifdef vxWorks
#   include <vxWorks.h>
#   include <stdioLib.h>
#else
#   include <stdio.h>
#endif

long sydCAFunc();
void sydCAFuncConnHandler();
void sydCAFuncMonHandler();
#define sydCA_searchNOW 1	/* NOTE!!! sydChanOpen presently needs the
				search to complete before returning to it */
#if !sydCA_searchNOW
void sydCAFuncGetGR(), sydCAFuncInitGR();
#endif	/* sydCA_searchNOW */

long
sydOpenCA(ppSspec, pHandle)
SYD_SPEC **ppSspec;	/* O pointer to synchronous set spec pointer */
void	*pHandle;	/* I NULL; someday might implement a callback
			routine to be called when a sample is available */
{
    long	stat;

    assert(ppSspec != NULL);

    if ((*ppSspec = (SYD_SPEC *)GenMalloc(sizeof(SYD_SPEC))) == NULL)
	return S_syd_noMem;
    (*ppSspec)->pFunc = sydCAFunc;
    (*ppSspec)->type = SYD_TY_CA;
    if ((stat = sydCAFunc(*ppSspec, NULL, SYD_FC_INIT, pHandle)) != S_syd_OK){
	GenFree((char *)*ppSspec);
	return stat;
    }
    (*ppSspec)->nInBufs = SYD_CHAN_MAX_IN;

    return sydOpen(ppSspec, pHandle);
}
/*+/subr**********************************************************************
* NAME	sydCAFunc - handle "Channel Access" data interactions
*
* DESCRIPTION
*	Handle acquisition of synchronous samples from Channel Access.
*
*	The caller must periodically call ca_pend_event in order for these
*	routines to make connections to channels, detect connect/disconnect,
*	and receive data.  The frequency with which ca_pend_event is called
*	should be about the same as the maximum frequency that data values
*	will change.
*
* sydCAFunc(pSspec, NULL, SYD_FC_INIT, pCallback)
* sydCAFunc(pSspec, pSChan, SYD_FC_OPEN, NULL)  chanName already in pSChan
* sydCAFunc(pSspec, pSChan, SYD_FC_READ, NULL)
* sydCAFunc(pSspec, pSChan, SYD_FC_POSITION, &stamp)  no operation performed
* sydCAFunc(pSspec, pSChan, SYD_FC_CLOSE, NULL)
* sydCAFunc(pSspec, NULL, SYD_FC_FILEINFO, outStream)
* sydCAFunc(pSspec, NULL, SYD_FC_WRAPUP, NULL)  no operation performed
*
* RETURNS
*	S_syd_OK, or
*	S_syd_EOF, or
*	S_syd_ERROR, or
*	other code indicating error
*
* BUGS
* o	doesn't do anything (or even detect) overwriting un-sampled
*	buffers (for SYD_FC_READ)
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydCAFunc(pSspec, pStruct, funcCode, pArg)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
void	*pStruct;	/* IO pointer to data struct used by funcCode */
enum sydFuncCode funcCode;/* I function code */
void	*pArg;		/* I pointer to arg, as required by funcCode */
{
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    chid	pCh;		/* channel pointer */
    long	retStat=S_syd_OK;
    long	stat;
    int		i;

    pSChan = (SYD_CHAN *)pStruct;

    if (funcCode ==					SYD_FC_INIT) {
	pSspec->pHandle = (void *)NULL;
    }
    else if (funcCode ==				SYD_FC_OPEN) {
#ifdef sydCA_searchNOW
	stat = ca_search(pSChan->name, &pCh);
#else
	stat = ca_build_and_connect(pSChan->name, TYPENOTCONN, 0, &pCh,
			NULL, sydCAFuncConnHandler, pSChan);
#endif
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_ERROR;
	    (void)printf("sydCAFunc: error on ca_search for %s :\n%s\n",
			pSChan->name, ca_message(stat));
	    return retStat;
	}
#ifdef sydCA_searchNOW
        stat = ca_pend_io(2.);
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_chanNotFound;
	    ca_clear_channel(pCh);
	    return retStat;
	}
	ca_puser(pCh) = pSChan;
	stat = ca_change_connection_event(pCh, sydCAFuncConnHandler);
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_ERROR;
	    (void)printf("sydCAFunc: error on ca_search for %s :\n%s\n",
			pSChan->name, ca_message(stat));
	    ca_clear_channel(pCh);
	    return retStat;
	}
	pSChan->dbrType = dbf_type_to_DBR_TIME(ca_field_type(pCh));
	pSChan->elCount = ca_element_count(pCh);
	pSChan->dbrGrType = dbf_type_to_DBR_GR(ca_field_type(pCh));
	stat = ca_get(pSChan->dbrGrType, pCh, &pSChan->grBuf);
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_ERROR;
	    (void)printf("sydCAFunc: error on ca_get(GR) for %s :\n%s\n",
			pSChan->name, ca_message(stat));
	    ca_clear_channel(pCh);
	    return retStat;
	}
        stat = ca_pend_io(2.);
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_ERROR;
	    (void)printf("sydCAFunc: error on ca_get(GR) for %s :\n%s\n",
			pSChan->name, ca_message(stat));
	    ca_clear_channel(pCh);
	    return retStat;
	}
	pSChan->gotGr = 1;
	stat = ca_add_array_event(pSChan->dbrType, pSChan->elCount, pCh,
			sydCAFuncMonHandler, NULL, 0., 0., 0., &pSChan->evid);
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_ERROR;
	    (void)printf("sydCAFunc: error on ca_get(GR) for %s :\n%s\n",
			pSChan->name, ca_message(stat));
	    ca_clear_channel(pCh);
	    return retStat;
	}
#else
	pSChan->dbrType = TYPENOTCONN;
	pSChan->elCount = 0;
	pSChan->evid = NULL;
	pSChan->gotGr = 0;
#endif
	pSChan->restart = 1;
	pSChan->pHandle = (void *)pCh;
    }
    else if (funcCode ==				SYD_FC_READ) {
	;
    }
    else if (funcCode ==				SYD_FC_POSITION) {
	;
    }
    else if (funcCode ==				SYD_FC_CLOSE) {
	if (pSChan->pHandle != NULL) {
	    stat = ca_clear_channel((chid)pSChan->pHandle);
	    if (stat != ECA_NORMAL)
		retStat = S_syd_ERROR;
	}
    }
    else if (funcCode ==				SYD_FC_FILEINFO) {
	(void)fprintf((FILE *)pArg, "no info for Channel Access\n");
    }
    else if (funcCode ==				SYD_FC_WRAPUP) {
	;
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	sydCAFuncConnHandler - connection handler for sync CA data routines
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
static void
sydCAFuncConnHandler(arg)
struct connection_handler_args arg;
{
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    chid	pCh;		/* channel pointer */
    long	retStat=S_syd_OK;
    long	stat;
    int		i;

    pCh = arg.chid;
    pSChan = (SYD_CHAN *)ca_puser(pCh);
    if (arg.op == CA_OP_CONN_UP) {
	pSChan->dbrType = dbf_type_to_DBR_TIME(ca_field_type(pCh));
	pSChan->restart = 1;
	if (pSChan->evid == NULL) {
	    pSChan->elCount = ca_element_count(pCh);
	    stat = ca_add_array_event(pSChan->dbrType, pSChan->elCount, pCh,
			sydCAFuncMonHandler, NULL, 0., 0., 0., &pSChan->evid);
	    if (stat != ECA_NORMAL) {
		assertAlways(0);
	    }
	}
#if !sydCA_searchNOW
	if (pSChan->gotGr == 0) {
	    sydCAFuncInitGr(pSChan);
	    pSChan->dbrGrType = dbf_type_to_DBR_GR(ca_field_type(pCh));
	    stat = ca_array_get_callback(pSChan->dbrGrType, 1, pCh,
			sydCAFuncMonHandler, NULL);
	}
#endif	/* sydCA_searchNOW */
    }
    else {
	pSChan->dbrType = TYPENOTCONN;
    }
}

/*+/subr**********************************************************************
* NAME	sydCAFuncMonHandler - handle monitor and get_callback
*
* DESCRIPTION
*	Receive data from Channel Access and store in the input buffers
*	of channels' synchronous data descriptors.  The data will be
*	retrieved later and assembled into synchronous samples by
*	sydSampleGet.
*
*	If data are received from Channel Access faster than the program
*	removes it with sydSampleGet, then the oldest unretrieved data
*	is lost.
*
* RETURNS
*	void
*
* BUGS
* o	no indication is given of lost data
* o	no precautions are taken to protect against the case where a
*	monitor comes in (following a disconnect/connect) with an old, old
*	time stamp.
*
* SEE ALSO
*	sydSampleGet, sydSampleStore
*
* EXAMPLE
*       sydResetBufKeepNewest(pSspec);
*       sydSampleSetAlloc(pSspec, nSamp);
*       pSspec->sampleCount = 0;
*       pSspec->refTs->secPastEpoch = 0; 
*       pSspec->refTs->nsec = 0;
*
*       while (sydSampleGet(pSspec) != S_syd_noDataNow)
*           sydSampleStore(pSspec);
*
*
*-*/
void
sydCAFuncMonHandler(arg)
struct event_handler_args arg;
{
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    SYD_SPEC	*pSspec;	/* pointer to syncSet spec */
    chid	pCh;		/* channel pointer */
    long	retStat=S_syd_OK;
    long	stat;
    int		i, i1, j;
    long	inStatus;
    union db_access_val *pBuf;

    pCh = arg.chid;
    pBuf = arg.dbr;
    pSChan = (SYD_CHAN *)ca_puser(pCh);
    pSspec = pSChan->pSspec;

#if !sydCA_searchNOW
    if (dbr_type_is_GR(arg.type)) {
	pSChan->gotGr = 1;
	sydCAFuncGetGR(pSChan, (union db_access_val *)arg.dbr);
	return;
    }
#endif
    if (!dbr_type_is_TIME(arg.type))
	return;

    if (pSChan->lastInBuf == pSChan->firstInBuf &&
		pBuf->tfltval.stamp.secPastEpoch != 0) {
	if (pSspec->refTs.secPastEpoch == 0 ||
	    TsCmpStampsLE(&pBuf->tfltval.stamp, &pSspec->refTs)) {
	    pSChan->lastInBuf = pSChan->firstInBuf = -1;
	    pSspec->refTs = pBuf->tfltval.stamp;
	}
    }
    i = NEXT_INBUF(pSChan, pSChan->lastInBuf);
    i1 = NEXT_INBUF(pSChan, i);
#if 0
    if (i == pSChan->firstInBuf)
        return;                         /* don't overwrite old data */
    else if (pSChan->restart) {
#endif
    if (pSChan->restart) {
	pSChan->inStatus[i] = SYD_B_RESTART;
	pSChan->restart = 0;
    }
    else
	pSChan->inStatus[i] = SYD_B_FULL;

    if (pSChan->dbrType == DBR_TIME_FLOAT) {
	pSChan->pInBuf[i]->tfltval = pBuf->tfltval;
	if (pSChan->elCount > 1) {
	    float	*pDest = &pSChan->pInBuf[i]->tfltval.value + 1;
	    float	*pSrc = &pBuf->tfltval.value + 1;
	    for (j=1; j<pSChan->elCount; j++)
		*pDest++ = *pSrc++;
	}
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
	pSChan->pInBuf[i]->tshrtval = pBuf->tshrtval;
	if (pSChan->elCount > 1) {
	    short	*pDest = &pSChan->pInBuf[i]->tshrtval.value + 1;
	    short	*pSrc = &pBuf->tshrtval.value + 1;
	    for (j=1; j<pSChan->elCount; j++)
		*pDest++ = *pSrc++;
	}
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
	pSChan->pInBuf[i]->tdblval = pBuf->tdblval;
	if (pSChan->elCount > 1) {
	    double	*pDest = &pSChan->pInBuf[i]->tdblval.value + 1;
	    double	*pSrc = &pBuf->tdblval.value + 1;
	    for (j=1; j<pSChan->elCount; j++)
		*pDest++ = *pSrc++;
	}
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
	pSChan->pInBuf[i]->tlngval = pBuf->tlngval;
	if (pSChan->elCount > 1) {
	    long	*pDest = &pSChan->pInBuf[i]->tlngval.value + 1;
	    long	*pSrc = &pBuf->tlngval.value + 1;
	    for (j=1; j<pSChan->elCount; j++)
		*pDest++ = *pSrc++;
	}
    }
    else if (pSChan->dbrType == DBR_TIME_STRING) {
	pSChan->pInBuf[i]->tstrval = pBuf->tstrval;
	if (pSChan->elCount > 1) {
	    char *pDest = pSChan->pInBuf[i]->tstrval.value + db_strval_dim;
	    char	*pSrc = pBuf->tstrval.value + db_strval_dim;
	    for (j=1; j<pSChan->elCount; j++) {
		strcpy(pDest, pSrc);
		pDest += db_strval_dim;
		pSrc += db_strval_dim;
	    }
	}
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
	pSChan->pInBuf[i]->tenmval = pBuf->tenmval;
	if (pSChan->elCount > 1) {
	    short	*pDest = &pSChan->pInBuf[i]->tenmval.value + 1;
	    short	*pSrc = &pBuf->tenmval.value + 1;
	    for (j=1; j<pSChan->elCount; j++)
		*pDest++ = *pSrc++;
	}
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
	pSChan->pInBuf[i]->tchrval = pBuf->tchrval;
	if (pSChan->elCount > 1) {
	    unsigned char	*pDest = &pSChan->pInBuf[i]->tchrval.value + 1;
	    unsigned char	*pSrc = &pBuf->tchrval.value + 1;
	    for (j=1; j<pSChan->elCount; j++)
		*pDest++ = *pSrc++;
	}
    }
    pSChan->lastInBuf = i;
    if (pSChan->firstInBuf < 0)
	pSChan->firstInBuf = i;
#if 0
	need to check if about to overflow buffers, and signal handler if so
#endif

}

/*/subhead sydCAFuncGetGR-----------------------------------------------------
*
*----------------------------------------------------------------------------*/
#if !sydCA_searchNOW

static void
sydCAFuncGetGR(pSChan, pGrBuf)
SYD_CHAN *pSChan;	/* pointer to syncSet channel descriptor */
union db_access_val *pGrBuf;/* pointer to buffer with graphics info */
{
    if (pSChan->dbrType == DBR_GR_FLOAT)
	pSChan->grBuf.gfltval = pGrBuf->gfltval;
    else if (pSChan->dbrType == DBR_GR_SHORT)
	pSChan->grBuf.gshrtval = pGrBuf->gshrtval;
    else if (pSChan->dbrType == DBR_GR_DOUBLE)
	pSChan->grBuf.gdblval = pGrBuf->gdblval;
    else if (pSChan->dbrType == DBR_GR_LONG)
	pSChan->grBuf.glngval = pGrBuf->glngval;
    else if (pSChan->dbrType == DBR_GR_STRING)
	pSChan->grBuf.gstrval = pGrBuf->gstrval;
    else if (pSChan->dbrType == DBR_GR_ENUM)
	pSChan->grBuf.genmval = pGrBuf->genmval;
    else if (pSChan->dbrType == DBR_GR_CHAR)
	pSChan->grBuf.gchrval = pGrBuf->gchrval;
}

/*/subhead sydCAFuncInitGR-----------------------------------------------------
*
*----------------------------------------------------------------------------*/
static void
sydCAFuncInitGR(pSChan)
SYD_CHAN *pSChan;	/* pointer to syncSet channel descriptor */
{
    if (pSChan->dbrType == DBR_TIME_FLOAT) {
#define FLT_DEST pSChan->grBuf.gfltval
	FLT_DEST.status = NO_ALARM;
	FLT_DEST.severity = NO_ALARM;
	FLT_DEST.precision = 3;
	(void)strcpy(FLT_DEST.units, " ");
	FLT_DEST.upper_disp_limit = 0.;
	FLT_DEST.lower_disp_limit = 0.;
	FLT_DEST.upper_alarm_limit = 0.;
	FLT_DEST.lower_alarm_limit = 0.;
	FLT_DEST.upper_warning_limit = 0.;
	FLT_DEST.lower_warning_limit = 0.;
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
#define SHRT_DEST pSChan->grBuf.gshrtval
	SHRT_DEST.status = NO_ALARM;
	SHRT_DEST.severity = NO_ALARM;
	(void)strcpy(SHRT_DEST.units, " ");
	SHRT_DEST.upper_disp_limit = 0;
	SHRT_DEST.lower_disp_limit = 0;
	SHRT_DEST.upper_alarm_limit = 0;
	SHRT_DEST.lower_alarm_limit = 0;
	SHRT_DEST.upper_warning_limit = 0;
	SHRT_DEST.lower_warning_limit = 0;
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
#define DBL_DEST pSChan->grBuf.gdblval
	DBL_DEST.status = NO_ALARM;
	DBL_DEST.severity = NO_ALARM;
	DBL_DEST.precision = 3;
	(void)strcpy(DBL_DEST.units, " ");
	DBL_DEST.upper_disp_limit = 0.;
	DBL_DEST.lower_disp_limit = 0.;
	DBL_DEST.upper_alarm_limit = 0.;
	DBL_DEST.lower_alarm_limit = 0.;
	DBL_DEST.upper_warning_limit = 0.;
	DBL_DEST.lower_warning_limit = 0.;
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
#define LNG_DEST pSChan->grBuf.glngval
	LNG_DEST.status = NO_ALARM;
	LNG_DEST.severity = NO_ALARM;
	(void)strcpy(LNG_DEST.units, " ");
	LNG_DEST.upper_disp_limit = 0;
	LNG_DEST.lower_disp_limit = 0;
	LNG_DEST.upper_alarm_limit = 0;
	LNG_DEST.lower_alarm_limit = 0;
	LNG_DEST.upper_warning_limit = 0;
	LNG_DEST.lower_warning_limit = 0;
    }
    else if (pSChan->dbrType == DBR_TIME_STRING) {
#define STR_DEST pSChan->grBuf.gstrval
	STR_DEST.status = NO_ALARM;
	STR_DEST.severity = NO_ALARM;
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
#define ENM_DEST pSChan->grBuf.genmval
	ENM_DEST.status = NO_ALARM;
	ENM_DEST.severity = NO_ALARM;
	ENM_DEST.no_str = 0;
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
#define CHR_DEST pSChan->grBuf.gchrval
	CHR_DEST.status = NO_ALARM;
	CHR_DEST.severity = NO_ALARM;
	(void)strcpy(CHR_DEST.units, " ");
	CHR_DEST.upper_disp_limit = 0;
	CHR_DEST.lower_disp_limit = 0;
	CHR_DEST.upper_alarm_limit = 0;
	CHR_DEST.lower_alarm_limit = 0;
	CHR_DEST.upper_warning_limit = 0;
	CHR_DEST.lower_warning_limit = 0;
    }
}
#endif	/* sydCA_searchNOW */
