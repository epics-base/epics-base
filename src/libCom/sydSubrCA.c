/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	06-12-91
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991-92, the Regents of the University of California,
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
 *  .00 06-12-91 rac	initial version
 *  .01 06-18-91 rac	installed in SCCS
 *  .02 06-19-91 rac	replace <fields.h> with <alarm.h>
 *  .03 08-15-91 rac	use new call for sydOpen
 *  .04 09-23-91 rac	allow async completion of ca_search
 *  .05 02-13-92 rac	use ADEL for monitoring; perform time-stamp
 *				rounding if requested
 *  .06 03-08-92 rac	use deadband from pSspec for monitoring
 *  .07 03-27-92 rac	fix bug in reconnect--discon wasn't getting reset;
 *			properly handle wrap-around in input buffer; call
 *			user's CA monitor handler if desired; do a flush
 *			after ca_search; handle disconCount and needGrCount
 *			in sampleSetSpec; move copyGr and copyVal into
 *			sydSubr.c
 *  .08 09-14-92 rac	discontinue use of special malloc routines
 *  .09 09-29-92 rac	discard samples with duplicate time stamps
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code
 */
/*+/mod***********************************************************************
* TITLE	sydSubrCA.c - acquire synchronous samples from Channel Access
*
* DESCRIPTION
*	
*-***************************************************************************/
#include <genDefs.h>
#define SYD_PRIVATE
#include <sydDefs.h>
#include <alarm.h>
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
static void sydCAFuncConnHandler();
void sydCAFuncMonHandler();
static long sydFuncCA_finishConn();

#define sydCA_searchNOW	/* force immediate completion of connection */
#undef sydCA_searchNOW	/* asynchronous completion of connection */
static void sydCAFuncInitGR();

long
sydOpenCA(ppSspec, pHandle)
SYD_SPEC **ppSspec;	/* O pointer to synchronous set spec pointer */
void	*pHandle;	/* I NULL; someday might implement a callback
			routine to be called when a sample is available */
{
    long	stat;

    assert(ppSspec != NULL);

    if ((*ppSspec = (SYD_SPEC *)malloc(sizeof(SYD_SPEC))) == NULL)
	return S_syd_noMem;
    (*ppSspec)->pFunc = sydCAFunc;
    (*ppSspec)->type = SYD_TY_CA;
    if ((stat = sydCAFunc(*ppSspec, NULL, SYD_FC_INIT, pHandle)) != S_syd_OK){
	free((char *)*ppSspec);
	return stat;
    }
    (*ppSspec)->nInBufs = SYD_CHAN_MAX_IN;

    return sydOpen(ppSspec);
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
* sydCAFunc(pSspec, pSChan, SYD_FC_READ, NULL)  start monitoring
* sydCAFunc(pSspec, pSChan, SYD_FC_STOP, NULL)  stop monitoring
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
/*-----------------------------------------------------------------------------
*    Initiate a connection to Channel Access for this channel.  The action
*    depends on whether searches are to be completed _now_, or asynchronously.
*
*    If searches are to be completed now, then a ca_pend_io is done.  If
*    the search isn't successful, an error return is given.
*
*    If searches are to be asynchronous, the connection handler is allowed
*    to do the required connection processing.
*----------------------------------------------------------------------------*/
	pSChan->dbfType = pSChan->dbrType = TYPENOTCONN;
	pSspec->disconCount++;
	pSChan->elCount = 1;
	pSChan->evid = NULL;
	pSChan->gotGr = 0;
	pSspec->needGrCount++;
	pSChan->restart = 1;
	sydCAFuncInitGR(pSChan);
#ifdef sydCA_searchNOW
	stat = ca_search(pSChan->name, &pCh);
#else
	stat = ca_build_and_connect(pSChan->name, TYPENOTCONN, 0, &pCh,
			NULL, sydCAFuncConnHandler, pSChan);
        stat = ca_flush_io();
#endif
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_ERROR;
	    (void)printf("sydCAFunc: error on ca_search for %s :\n%s\n",
			pSChan->name, ca_message(stat));
	    return retStat;
	}
	pSChan->pHandle = (void *)pCh;
#ifdef sydCA_searchNOW
        stat = ca_pend_io(2.);
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_chanNotFound;
	    ca_clear_channel(pCh);
	    return retStat;
	}
	ca_puser(pCh) = pSChan;
	retStat = sydFuncCA_finishConn(pSChan, pCh);
	if (retStat != S_syd_OK)
	    return retStat;
	pSChan->pSspec->disconCount--;
	stat = ca_change_connection_event(pCh, sydCAFuncConnHandler);
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_ERROR;
	    (void)printf("sydCAFunc: error on ca_search for %s :\n%s\n",
			pSChan->name, ca_message(stat));
	    ca_clear_channel(pCh);
	    return retStat;
	}
#else
	retStat = S_syd_chanNotConn;
#endif
    }
    else if (funcCode ==				SYD_FC_READ) {
	if ((pCh = (chid)pSChan->pHandle) == NULL)
	    return;
	if (pSChan->evid == NULL) {
	    if (pSChan->pSspec->monFn == NULL) {
		stat = ca_add_masked_array_event(pSChan->dbrType,
		    pSChan->elCount, pCh, sydCAFuncMonHandler, NULL,
		    0., 0., 0., &pSChan->evid, pSChan->pSspec->deadband);
	    }
	    else {
		stat = ca_add_masked_array_event(pSChan->dbrType,
		    pSChan->elCount, pCh,
		    pSChan->pSspec->monFn, pSChan->pArg,
		    0., 0., 0., &pSChan->evid, pSChan->pSspec->deadband);
	    }
	    if (stat != ECA_NORMAL) {
		retStat = S_syd_ERROR;
		(void)printf(
"sydCAFunc: error on ca_add_masked_array_event for %s :\n%s\n",
		    pSChan->name, ca_message(stat));
		ca_clear_channel(pCh);
		return retStat;
	    }
	}
    }
    else if (funcCode ==				SYD_FC_STOP) {
	if ((pCh = (chid)pSChan->pHandle) == NULL)
	    return;
	if (pSChan->evid == NULL) {
	    ca_clear_event(pSChan->evid);
	    pSChan->evid = NULL;
	}
    }
    else if (funcCode ==				SYD_FC_POSITION) {
	;
    }
    else if (funcCode ==				SYD_FC_CLOSE) {
	if ((pCh = (chid)pSChan->pHandle) != NULL) {
	    stat = ca_clear_channel(pCh);
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

/*+/internal******************************************************************
* NAME	sydFuncCA_finishConn
*
* DESCRIPTION
*	Handles connect and disconnect for a channel.
*
*	The first connect results in adding a monitor for the channel using
*	the `syd' monitor handler, unless the user has his own monitor
*	handler.  In the latter case, no monitor is added until the user
*	calls sydInputGet the first time.
*
*-*/
static long
sydFuncCA_finishConn(pSChan, pCh)
SYD_CHAN *pSChan;	/* pointer to syncSet channel descriptor */
chid	pCh;		/* channel pointer */
{
    long	retStat=S_syd_OK;
    long	stat;

    pSChan->dbfType = ca_field_type(pCh);
    pSChan->dbrType = dbf_type_to_DBR_TIME(pSChan->dbfType);
    pSChan->elCount = ca_element_count(pCh);
    pSChan->dbrGrType = dbf_type_to_DBR_GR(pSChan->dbfType);
    stat = ca_array_get_callback(pSChan->dbrGrType, 1, pCh,
		sydCAFuncMonHandler, NULL);
    if (stat != ECA_NORMAL) {
	retStat = S_syd_ERROR;
	(void)printf(
		"sydCAFunc: error on ca_array_get_callback(GR) for %s :\n%s\n",
		pSChan->name, ca_message(stat));
	ca_clear_channel(pCh);
	return retStat;
    }
    if (pSChan->evid == NULL) {
	if (pSChan->pSspec->monFn == NULL) {
	    stat = ca_add_masked_array_event(pSChan->dbrType,
		pSChan->elCount, pCh, sydCAFuncMonHandler, NULL,
		0., 0., 0., &pSChan->evid, pSChan->pSspec->deadband);
	}
	if (stat != ECA_NORMAL) {
	    retStat = S_syd_ERROR;
	    (void)printf(
		"sydCAFunc: error on ca_add_masked_array_event for %s :\n%s\n",
		pSChan->name, ca_message(stat));
	    ca_clear_channel(pCh);
	    return retStat;
	}
    }
    pSChan->conn = 1;
    pSChan->discon = 0;
    retStat = sydChanOpen1(pSChan->pSspec, pSChan);
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
	if (pSChan->conn == 0)
	    retStat = sydFuncCA_finishConn(pSChan, pCh);
	    if (retStat != S_syd_OK) {
		(void)printf(
"sydCAFuncConnHandler: error finishing connection for %s\n", ca_name(pCh));
	    }
	pSChan->restart = 1;
	pSChan->discon = 0;
	pSChan->pSspec->disconCount--;
    }
    else {
	pSChan->discon = 1;
	pSChan->pSspec->disconCount++;
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
*	If time stamp rounding is to be done, it is done here.
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
    int		nbytes;

    pCh = arg.chid;
    pBuf = arg.dbr;
    pSChan = (SYD_CHAN *)ca_puser(pCh);
    pSspec = pSChan->pSspec;

    if (dbr_type_is_GR(arg.type)) {
	pSChan->gotGr = 1;
	pSspec->needGrCount--;
	sydCopyGr(&pSChan->grBuf, pBuf, arg.type);
	stat = sydChanOpenGR(pSChan);
	return;
    }
    if (!dbr_type_is_TIME(arg.type))
	return;

    if (pSspec->roundNsec > 0)
	sydTsRound(&pBuf->tfltval.stamp, pSspec->roundNsec);
/*-----------------------------------------------------------------------------
*	If this monitor has the same time stamp as the prior monitor for
*	this channel, discard this one.
*----------------------------------------------------------------------------*/
    if ((i = pSChan->lastInBuf) >= 0) {
	if (TsCmpStampsLE(&pBuf->tfltval.stamp,
					&pSChan->pInBuf[i]->tfltval.stamp)) {
	    return;
	}
    }

/*-----------------------------------------------------------------------------
*	If the reference time stamp for the sync set hasn't been set yet,
*	or if it has, but the time stamp in this monitor is earlier, set it.
*----------------------------------------------------------------------------*/
    if (pSspec->refTs.secPastEpoch == 0 || pSspec->sampleCount == 0) {
	if (pBuf->tfltval.stamp.secPastEpoch != 0 &&
			TsCmpStampsLE(&pBuf->tfltval.stamp, &pSspec->refTs)) {
	    pSspec->refTs = pBuf->tfltval.stamp;
	}
    }

    i = NEXT_INBUF(pSChan, pSChan->lastInBuf);
    i1 = NEXT_INBUF(pSChan, i);
    if (pSChan->restart) {
	pSChan->inStatus[i] = SYD_B_RESTART;
	pSChan->restart = 0;
    }
    else
	pSChan->inStatus[i] = SYD_B_FULL;

    nbytes = dbr_size_n(arg.type, pSChan->elCount);
    bcopy((char *)pBuf, (char *)pSChan->pInBuf[i], nbytes);

    pSChan->lastInBuf = i;
    if (pSChan->firstInBuf == i) {
	pSChan->firstInBuf = i1;
	pSChan->inStatus[i1] = SYD_B_RESTART;
    }
    else if (pSChan->firstInBuf < 0)
	pSChan->firstInBuf = i;
}

/*+/internal******************************************************************
* NAME	sydCAFuncInitGR
*
*-*/
static void
sydCAFuncInitGR(pSChan)
SYD_CHAN *pSChan;	/* pointer to syncSet channel descriptor */
{
    if (pSChan->dbrType == DBR_TIME_FLOAT) {
#define FLT_DEST pSChan->grBuf.gfltval
	FLT_DEST.status = FLT_DEST.severity = NO_ALARM;
	FLT_DEST.precision = 3;
	(void)strcpy(FLT_DEST.units, " ");
	FLT_DEST.upper_disp_limit = FLT_DEST.lower_disp_limit = 0.;
	FLT_DEST.upper_alarm_limit = FLT_DEST.lower_alarm_limit = 0.;
	FLT_DEST.upper_warning_limit = FLT_DEST.lower_warning_limit = 0.;
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
#define SHRT_DEST pSChan->grBuf.gshrtval
	SHRT_DEST.status = SHRT_DEST.severity = NO_ALARM;
	(void)strcpy(SHRT_DEST.units, " ");
	SHRT_DEST.upper_disp_limit = SHRT_DEST.lower_disp_limit = 0;
	SHRT_DEST.upper_alarm_limit = SHRT_DEST.lower_alarm_limit = 0;
	SHRT_DEST.upper_warning_limit = SHRT_DEST.lower_warning_limit = 0;
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
#define DBL_DEST pSChan->grBuf.gdblval
	DBL_DEST.status = DBL_DEST.severity = NO_ALARM;
	DBL_DEST.precision = 3;
	(void)strcpy(DBL_DEST.units, " ");
	DBL_DEST.upper_disp_limit = DBL_DEST.lower_disp_limit = 0.;
	DBL_DEST.upper_alarm_limit = DBL_DEST.lower_alarm_limit = 0.;
	DBL_DEST.upper_warning_limit = DBL_DEST.lower_warning_limit = 0.;
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
#define LNG_DEST pSChan->grBuf.glngval
	LNG_DEST.status = LNG_DEST.severity = NO_ALARM;
	(void)strcpy(LNG_DEST.units, " ");
	LNG_DEST.upper_disp_limit = LNG_DEST.lower_disp_limit = 0;
	LNG_DEST.upper_alarm_limit = LNG_DEST.lower_alarm_limit = 0;
	LNG_DEST.upper_warning_limit = LNG_DEST.lower_warning_limit = 0;
    }
    else if (pSChan->dbrType == DBR_TIME_STRING) {
#define STR_DEST pSChan->grBuf.gstrval
	STR_DEST.status = STR_DEST.severity = NO_ALARM;
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
#define ENM_DEST pSChan->grBuf.genmval
	ENM_DEST.status = ENM_DEST.severity = NO_ALARM;
	ENM_DEST.no_str = 0;
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
#define CHR_DEST pSChan->grBuf.gchrval
	CHR_DEST.status = CHR_DEST.severity = NO_ALARM;
	(void)strcpy(CHR_DEST.units, " ");
	CHR_DEST.upper_disp_limit = CHR_DEST.lower_disp_limit = 0;
	CHR_DEST.upper_alarm_limit = CHR_DEST.lower_alarm_limit = 0;
	CHR_DEST.upper_warning_limit = CHR_DEST.lower_warning_limit = 0;
    }
}
