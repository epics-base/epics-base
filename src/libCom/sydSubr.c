/*	@(#)sydSubr.c	1.18 2/23/93
 *	Author:	Roger A. Cole
 *	Date:	12-04-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1990-93, the Regents of the University of California,
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
 *  .00 12-04-90 rac	initial version
 *  .01 06-18-91 rac	installed in SCCS
 *  .02 06-19-91 rac	replace <fields.h> with <alarm.h>
 *  .03 08-15-91 rac	update documentation; add args to sydChanOpen;
 *			enhance for use by ordinary users
 *  .04 09-11-91 rac	for CA, report "noData" if ts==0; add more
 *			documentation
 *  .05 09-22-91 rac	add sydInputFetch; add trigger routines;
 *			handle asynchronous ca_search
 *  .06 10-21-91 rac	set the reference time stamp for the first
 *			sample which is stored; better handle long
 *			lines in printing; fold channel names when
 *			printing; put EGU in channel structure;
 *			put EGU in export file; support time stamp
 *			rounding; args changed for: sydSampleSetPrint;
 *			sydSamplePrint; sydInputStoreInSet;
 *  .07 11-02-91 rac	add sydSampleWriteSSF, sydSampleSetWriteSSF
 *  .08 12-08-91 rac	fix alignment for printing of channel names
 *  .09 01-20-92 rac	add a code for VALID_ALARM and handle
 *			invalid values properly
 *  .10 02-04-92 rac	allow multiple chanOpen for same name
 *  .11 02-18-92 rac	finally handle array channels on print and export;
 *			add an export column for text time stamp
 *  .12 02-26-92 rac	move timestamp rounding to the individual read
 *			routines, using a new routine here; add sydSetAttr
 *			and default deadband
 *  .13 03-23-92 rac	set default monitor to use ADEL deadband
 *  .14 03-31-92 rac	install a kludge to cope with 0 timestamps from EPICS;
 *			add attributes for calling a user CA monitor routine;
 *			add a "set-wide" connect flag and a "set-wide" flag
 *			for "waiting for graphics info"; generalize routines
 *			for copying buffers; use exponential format for
 *			exporting and printing tiny numbers; add support
 *			for mean and standard deviation; add capability to
 *			restrict range of interest to a subset of the samples;
 *			put a blank line in export file following snapshot;
 *			for .RVAL channels, set prec=0, EGU="counts"
 *  .15 08-25-92 rac	fix bugs in exporting for restricted time range;
 *			fix sydSamplePrint to put a separator in front of
 *			the no data message; add better handling for SSF
 *			files with missing data; added a routine to prepare
 *			a channel for a new retrieval; discontinue use of
 *			special malloc routines
 *  .16 09-30-92 rac	discard samples with earlier time stamps than the most
 *			recent sample;
 *  .17 01-05-93 rac	add some documentation regarding the handling of
 *			samples with SYD_B_MISSING status; fix a bug for
 *			the handling of SYD_B_MISSING samples; add an
 *			option for exporting only means; fix a bug in
 *			sydInputReset where last buffer didn't get discarded;
 *  .18	03-22-93	changed VALID_ALARM to INVALID_ALARM - thanks
 *  .19 04-19-94 mrk	removed static from sydSamplePrintStats
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert checking
 *      -DDEBUG         compile various debug code
 */




#if 0	/* allow comments within the module heading */
/* /*+/mod***********************************************************************
/* * TITLE	sydSubr.c - synchronous data routines
/* *
/* * DESCRIPTION
/* *	These routines support:
/* *	o   defining a set of channels for which synchronous samples are
/* *	    to be acquired
/* *	o   checking to see if a synchronous sample is available
/* *	o   acquiring the next synchronous sample or a specified number
/* *	    of synchronous samples
/* *	o   storing the most recently acquired synchronous sample at the
/* *	    end of the set of synchronous samples
/* *	o   printing a particular synchronous sample or all samples in
/* *	    a synchronous sample set
/* *
/* *	Sample acquisition can be from one of:
/* *	o   archiver `sample set' file
/* *	o   archiver `by channel' file
/* *	o   Channel Access
/* *
/* *	sydPlot.c contains routines which support plotting a synchronous
/* *	sample set.
/* *
/* * QUICK REFERENCE
/* *
/* * #include <genDefs.h>		/* some general use definitions */
/* * #include <db_access.h>	/* definitions for database related items */
/* * #include <sydDefs.h>		/* structures for synchronous data routines */
/* *
/* * SYD_SPEC *pSspec;	/* pointer to synchronous set spec */
/* * SYD_CHAN *pSChan;	/* pointer to synchronous channel descriptor */
/* *
/* *     long  sydChanClose(        pSspec,  pSChan			)
/* * SYD_CHAN *sydChanFind(         pSspec,  chanName			)
/* *     long  sydChanOpen(         pSspec, >ppSChan, chanName,  sync, pArg, trig)
/* *				         sync = SYD_SY_{NONF,FILLED}
/* *     long  sydChanPrep(         pSspec,  pSChan			)
/* *     long  sydClose(            pSspec					)
/* *     void  sydCopyGr(          >pDest,   pSrc,  srcDbrType             )
/* *     void  sydCopyVal(         >pDest,   pSrc,  count,  srcDbrType     )
/* *     long  sydFileInfo(         pSspec,  out                           )
/* *     long  sydInputFetch(       pSspec					)
/* *     long  sydInputGet(         pSspec, >pMoreFlag			)
/* *     void  sydInputReset(       pSspec					)
/* *     void  sydInputResetKeepNewest(pSspec				)
/* *     void  sydInputResetSampled(pSspec					)
/* *     void  sydInputStoreInSet(  pSspec,  ignorePartial			)
/* *     long  sydInputSync(        pSspec         			)
/* *     long  sydMonitorStart(     pSspec         			)
/* *     long  sydMonitorStop(      pSspec         			)
/* *     long  sydOpenCA(          >ppSspec, NULL				)
/* *     long  sydOpenCF(          >ppSspec, filePath			)
/* *     long  sydOpenSSF(         >ppSspec, filePath			)
/* *     long  sydPosition(         pSspec,  pStamp			)
/* *     long  sydSampleExport(     pSspec,  out,  fmtFlag,  hdrFlag,  sampNum)
/* *     long  sydSampleExportStats(pSspec,  out,  snapNum			)
/* *     long  sydSamplePrint(      pSspec,  out,  fmtFlag,  hdrFlag,
/* *						  nCol, colWidth, sampNum)
/* *     long  sydSamplePrintStats( pSspec,  out,  fmtFlag,  hdrFlag,
/* *						  nCol, colWidth, snapNum)
/* *     long  sydSampleWriteSSF(   pSspec,  pFile, progDesc, sampDesc, sampNum)
/* *
/* *     long  sydSampleSetAlloc(   pSspec,  reqCount			)
/* *     long  sydSampleSetExport(  pSspec,  out,  fmtFlag			)
/* *     long  sydSampleSetFree(    pSspec					)
/* *     long  sydSampleSetGet(     pSspec					)
/* *     long  sydSampleSetPrint(   pSspec,  out,  fmtFlag, nCol, colWidth	)
/* *     long  sydSampleSetRestrict(pSspec,  pTsBegin, pTsEnd		)
/* *     long  sydSampleSetStats(   pSspec					)
/* *     long  sydSampleSetWriteSSF(pSspec,  pFile, progDesc, sampDesc     )
/* *     long  sydSetAttr(          pSspec,  attr, value, pArg		)
/* *                   SYD_ATTR_DEADBAND, 0, {"ADEL" or "MDEL"}
/* *                   SYD_ATTR_MON_FN, 0, function
/* *                   SYD_ATTR_USE_STATS, 1, NULL
/* *                   SYD_ATTR_USE_MEANS, 1, NULL
/* *
/* *     long  sydTest(             pSspec					)
/* *     long  sydTestAddFromText(  pSspec, text				)
/* *     long  sydTestClose(        pSspec					)
/* *      int  sydValAsDbl(         pSChan,  sampNum, pDbl			)
/* *
/* * BUGS
/* * o	error detection and handling isn't "leakproof"
/* * o	for retrieving from sample set files, if all the channels in the
/* *	set are missing for two snapshots in a row, EOF is reported
/* *
/* * DESCRIPTION, continued
/* *
/* * o  special terms
/* *
/* *	synchronous sample--is a set of values, one for each channel,
/* *	    with the same time stamp for each
/* *
/* *	synchronous sample set--is a set of synchronous samples, in order
/* *	    by time stamp.  This frequently will be referred to as a
/* *	    synchronous set.
/* *
/* *	synchronous channel descriptor--is the information about one
/* *	    of the channels for which synchronous data is to be acquired
/* *
/* *	synchronous set specification--is the set of information describing
/* *	    how synchronous samples are to be acquired, and which channels
/* *	    are to be used
/* *
/* * o  synchronous set specification
/* *
/* *	A program can have one or more synchronous set specifications at the
/* *	same time.  Each is created with an "open" and destroyed with a
/* *	"close".  The "open" returns a pointer which is used in all
/* *	subsequent operations with the "sync set spec".
/* *
/* *	There are several routines available for opening a sync set spec; the
/* *	one which is used determines where samples will be obtained.  Most
/* *	other sydXxx routines are independent of which source is being used.
/* *	See sydOpen for more details.
/* *
/* * o  synchronous channel descriptor
/* *
/* *	After a sync set spec has been opened, one or more channels will
/* *	be added to it with calls to sydChanOpen.  When a channel is opened,
/* *	it must be specified whether the channel will be treated as "filled"
/* *	or as "synchronous".  This distinction comes into play while samples
/* *	are being acquired (see below).
/* *
/* *	The value for a channel might be a scalar, as for a thermocouple,
/* *	or an array, as for a digitized waveform.  Most of the discussion
/* *	which follows makes no distinction between the two kinds of values.
/* *
/* *	When a channel is no longer wanted as part of a sync set spec, the
/* *	sydChanClose call can be used to remove it.  (The sydClose call
/* *	automatically closes all channels in a sync set spec.)
/* *
/* * o  acquiring synchronous samples
/* *
/* *	Each channel is treated as having a stream of time-stamped values.
/* *	When sydInputGet is called, the earliest time stamp for all the
/* *	channels is found--this is the time stamp for the sample.  Each
/* *	channel which has a value with that time stamp is placed in the
/* *	sample.  If no samples have yet been recieved for a channel, then
/* *	the channel is flagged as "missing" in the sample.  The action taken
/* *	when there is no value for a channel depends on how the channel was
/* *	opened.
/* *
/* *	If the sydChanOpen call specified SYD_SY_FILLED, then when the channel
/* *	has no value at the chosen time stamp the most recent prior value is
/* *	placed into the sample.  This implements the assumption that the
/* *	channel's value has remained constant.
/* *
/* *	Specifying SYD_SY_NONF in the sydChanOpen call inhibits "filling in"
/* *	a value in the sample when a channel has no value at the chosen
/* *	time stamp.  In this case, the channel will be flagged as "missing"
/* *	in the sample.
/* *
/* *	If data acquisition is from Channel Access, then some additional
/* *	details come into play.  The result is to compensate for possible
/* *	network delays in transmitting data, which means that the calling
/* *	program may occasionally receive a "no data now" status.  In that
/* *	case, the calling program is expected to try again later to see
/* *	if additional data have been received.
/* *
/* * o  accessing data for a sample
/* *
/* * o  accessing data for a sample set
/* *
/* * EXAMPLE
/* *
/* * #include <genDefs.h>
/* * #include <sydDefs.h>
/* * #include <cadef.h>
/* * #include <db_access.h>
/* *
/* * main()
/* * {
/* *    SYD_SPEC	*pSspec;	/* pointer to sync set spec */
/* *    SYD_CHAN	*pSchanBase;	/* pointer to sync chan desc for POWER:BASE */
/* *    SYD_CHAN	*pSchanLag;	/* pointer to sync chan desc for POWER:LAG30 */
/* *    long	stat;		/* status return */
/* *    int	i;
/* *    int	moreFlag;	/* 1 if more samples waiting */
/* *    float	sumBase, sumLag;/* sums for the two channels */
/* *    char	timeText[28];	/* text for time stamp */
/* *    int	chanStat;	/* input status for channel */
/* *
/* * /*---------------------------------------------------------------------------
/* * *	open the synchronous sample set specification and add the channels
/* * *	to it
/* * *--------------------------------------------------------------------------*/
/* *    stat = sydOpenCA(&pSspec, NULL);
/* *    if (stat != S_syd_OK) {
/* *	printf("couldn't open sync set spec\n");
/* *	exit(1);
/* *    }
/* *    stat = sydChanOpen(pSspec, &pSchanBase, "rai_2000", SYD_SY_FILLED,NULL,0);
/* *    if (stat != S_syd_OK) {
/* *	printf("couldn't open POWER:BASE\n");
/* *	exit(1);
/* *    }
/* *    stat = sydChanOpen(pSspec, &pSchanLag, "rao_2000", SYD_SY_FILLED,NULL,0);
/* *    if (stat != S_syd_OK) {
/* *	printf("couldn't open POWER:LAG30\n");
/* *	exit(1);
/* *    }
/* * /*---------------------------------------------------------------------------
/* * *	now get 100 synchronous samples and accumulate a running sum for
/* * *	each channel.  Since this example program is using Channel Access,
/* * *	it loops on sydInputGet until a status of S_syd_noDataNow is
/* * *	received; when retrieving from an archive file, such a loop wouldn't
/* * *	be used.
/* * *--------------------------------------------------------------------------*/
/* *    sumBase = sumLag = 0.;
/* *    i = 0;
/* *    while (i < 100) {
/* *	ca_pend_event(.1);		/* allow Channel Access to get values */
/* *	stat = sydInputGet(pSspec, &moreFlag);	/* see if any were obtained */
/* *	while (stat == S_syd_OK || stat == S_syd_partial) {
/* *	    i++;
/* *	    tsStampToText(&SydInputTs(pSspec), TS_TEXT_MMDDYY, timeText);
/* *	    printf("sample at %s more:%d--", timeText, moreFlag);
/* *	    chanStat = SydInputStatus(pSchanBase);
/* *	    if (chanStat != SYD_B_EOF && chanStat != SYD_B_MISSING) {
/* *		sumBase += SydInputValAsFloat(pSchanBase);
/* *		printf("%s= %f ", SydChanName(pSchanBase),
/* *					SydInputValAsFloat(pSchanBase));
/* *		SydInputMarkAsSampled(pSchanBase);
/* *	    }
/* *	    chanStat = SydInputStatus(pSchanLag);
/* *	    if (chanStat != SYD_B_EOF && chanStat != SYD_B_MISSING) {
/* *		sumLag += SydInputValAsFloat(pSchanLag);
/* *		printf("%s= %f ", SydChanName(pSchanLag),
/* *					SydInputValAsFloat(pSchanLag));
/* *		SydInputMarkAsSampled(pSchanLag);
/* *	    }
/* *	    printf("\n");
/* *	    stat = sydInputGet(pSspec, &moreFlag);
/* *	}
/* *    }
/* *    printf("sumBase= %f   sumLag= %f\n", sumBase, sumLag);
/* * }
/* *-***************************************************************************/
/****/
#endif	/* allow comments within the module heading */

#include <genDefs.h>
#define SYD_PRIVATE
#define SYD_PRIVATE_DATA
#include <sydDefs.h>
#include <alarm.h>
#ifndef INC_tsDefs_h
#   include <tsDefs.h>
#endif
#ifndef INCLcadefh
#   include <cadef.h>
#endif

#ifdef vxWorks
#   include <vxWorks.h>
#   include <ioLib.h>		/* for O_RDWR and O_RDONLY definitions */
#   include <stdioLib.h>
#   include <strLib.h>
#else
#   include <stdio.h>
#   include <sys/file.h>	/* for O_RDWR and O_RDONLY definitions */
#   include <string.h>
#   include <strings.h>
#   include <math.h>
#endif

#define PRE_FL	1
#define POST_FL	2
#define SHOW_AR 4
#define USE_QUO 8
#define ENF_WID 16
static void sydSamplePrint1();
static void sydSamplePrintVal();
static void sydChanFreeArrays();
static void sydInputGetIn();
long sydSampleSetAlloc();
long sydSampleSetFree();


#if SYD_SUBR_TEST
main()
{
    SYD_SPEC	*pSspec;	/* pointer to sync set spec */
    SYD_CHAN	*pSchanBase;	/* pointer to sync chan desc for POWER:BASE */
    SYD_CHAN	*pSchanLag;	/* pointer to sync chan desc for POWER:LAG30 */
    long	stat;		/* status return */
    int		i;
    int		moreFlag;	/* 1 if more samples waiting */
    float	sumBase, sumLag;/* sums for the two channels */
    char	timeText[28];	/* text for time stamp */
    int		chanStat;	/* input status for channel */
    int		beg, end;
    TS_STAMP	begTs, endTs;
    char	begText[28], endText[28];

/*-----------------------------------------------------------------------------
*	open the synchronous sample set specification and add the channels
*	to it
*----------------------------------------------------------------------------*/
    stat = sydOpenCA(&pSspec, NULL);
    if (stat != S_syd_OK) {
	printf("couldn't open sync set spec\n");
	exit(1);
    }
    stat = sydChanOpen(pSspec, &pSchanBase, "gva_2000", SYD_SY_FILLED,NULL,0);
    if (stat != S_syd_OK && stat != S_syd_chanNotConn) {
	printf("couldn't open gva_2000\n");
	exit(1);
    }
    stat = sydChanOpen(pSspec, &pSchanLag, "gva_2001", SYD_SY_FILLED,NULL,0);
    if (stat != S_syd_OK && stat != S_syd_chanNotConn) {
	printf("couldn't open gva_2001\n");
	exit(1);
    }
#if 0
/*-----------------------------------------------------------------------------
*	now get 50 synchronous samples and accumulate a running sum for
*	each channel.  Since this example program is using Channel Access,
*	it loops on sydInputGet until a status of S_syd_noDataNow is
*	received; when retrieving from an archive file, such a loop wouldn't
*	be used.
*----------------------------------------------------------------------------*/
    sumBase = sumLag = 0.;
    i = 0;
    while (i < 20) {
	ca_pend_event(.1);		/* allow Channel Access to get values */
	stat = sydInputGet(pSspec, &moreFlag);	/* see if any were obtained */
	while (stat == S_syd_OK || stat == S_syd_partial) {
	    i++;
	    tsStampToText(&SydInputTs(pSspec), TS_TEXT_MMDDYY, timeText);
	    printf("sample at %s more:%d--", timeText, moreFlag);
	    chanStat = SydInputStatus(pSchanBase);
	    if (chanStat != SYD_B_EOF && chanStat != SYD_B_MISSING) {
		sumBase += SydInputValAsFloat(pSchanBase);
		printf("%s= %f ", SydChanName(pSchanBase),
					SydInputValAsFloat(pSchanBase));
		SydInputMarkAsSampled(pSchanBase);
	    }
	    chanStat = SydInputStatus(pSchanLag);
	    if (chanStat != SYD_B_EOF && chanStat != SYD_B_MISSING) {
		sumLag += SydInputValAsFloat(pSchanLag);
		printf("%s= %f ", SydChanName(pSchanLag),
					SydInputValAsFloat(pSchanLag));
		SydInputMarkAsSampled(pSchanLag);
	    }
	    printf("\n");
	    stat = sydInputGet(pSspec, &moreFlag);
	}
    }
    printf("sumBase= %f   sumLag= %f\n", sumBase, sumLag);
#endif
    printf("checking connections\n");
    for (i=0; i<20; i++) {
	if (pSchanBase->discon || pSchanLag->discon)
	    ca_pend_event(.1);
	else
	    break;
    }
    if (pSchanBase->discon || pSchanLag->discon) {
	printf("at least one channel isn't connected\n");
	exit(1);
    }
    printf("getting samples to store--storing all\n");
    stat = sydSampleSetAlloc(pSspec, 50);
    assert(stat == S_syd_OK);
    for (i=0; i<20; ) {
	ca_pend_event(.1);
	stat = sydInputGet(pSspec, &moreFlag);	/* see if any were obtained */
	while (stat == S_syd_OK || stat == S_syd_partial) {
	    sydInputStoreInSet(pSspec, 0);
	    printf(".");
	    fflush(stdout);
	    i++;
	    stat = sydInputGet(pSspec, &moreFlag);
	}
    }
    printf("\n");
    stat = sydSampleSetPrint(pSspec, stdout, 0, 5, 10);
    assert(stat == S_syd_OK);
    beg = 5;
    end = 15;
    begTs = pSspec->pTimeStamp[beg];
    begTs.nsec = 0;
    endTs = pSspec->pTimeStamp[end];
    endTs.nsec = 999000000;
    tsStampToText(&begTs, TS_TEXT_MMDDYY, begText);
    tsStampToText(&endTs, TS_TEXT_MMDDYY, endText);
    printf("range: %s to %s\n", begText, endText);
    stat = sydSampleSetRestrict(pSspec, &begTs, &endTs);
    assert(stat == S_syd_OK);
    stat = sydSampleSetPrint(pSspec, stdout, 0, 5, 10);
    assert(stat == S_syd_OK);

    printf("getting samples to store--storing if gva_200 > 4.5\n");
    stat = sydSampleSetAlloc(pSspec, 50);
    assert(stat == S_syd_OK);
    stat = sydTestAddFromText(pSspec, "if gva_2000 > 4.5");
    assert(stat == S_syd_OK);
    for (i=0; i<20; ) {
	ca_pend_event(.1);
	stat = sydInputGet(pSspec, &moreFlag);	/* see if any were obtained */
	while (stat == S_syd_OK || stat == S_syd_partial) {
	    sydInputStoreInSet(pSspec, 0);
	    printf(".");
	    fflush(stdout);
	    i++;
	    stat = sydInputGet(pSspec, &moreFlag);
	}
    }
    printf("\n");
    stat = sydSampleSetPrint(pSspec, stdout, 0, 5, 10);
    assert(stat == S_syd_OK);
}
#endif

/*+/subr**********************************************************************
* NAME	sydChanClose - delete a channel from a synchronous set spec
*
* DESCRIPTION
*	This routine deletes a channel from a synchronous set spec.  The
*	other channels in the set aren't affected, and their data is
*	preserved.
*
* RETURNS
*	S_syd_OK
*
* SEE ALSO
*	sydOpen, sydChanOpen, sydChanFind
*	sydInputGet
*
*-*/
long
sydChanClose(pSspec, pSChan)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
SYD_CHAN *pSChan;	/* I pointer to synchronous channel descriptor */
{
    int		i;

    assert(pSspec != NULL);
    assert(pSChan != NULL);

    if (pSspec->pFunc != NULL)
	(void)(pSspec->pFunc)(pSspec, pSChan, SYD_FC_CLOSE, NULL);
    for (i=0; i<pSChan->nInBufs; i++) {
	if (pSChan->pInBuf[i] != NULL)
	    free((char *)pSChan->pInBuf[i]);
    }
    sydChanFreeArrays(pSChan);

    pSspec->chanCount--;
    DoubleListRemove(pSChan, pSspec->pChanHead, pSspec->pChanTail);
    if (pSChan->testChan) {
	free((char *)pSspec->pAccept);
	pSspec->pAccept = NULL;
    }
    free((char *)pSChan);

    return S_syd_OK;
}
/*+/subr**********************************************************************
* NAME	sydChanFreeArrays
*
*-*/
static void
sydChanFreeArrays(pSChan)
SYD_CHAN *pSChan;
{
    if (pSChan->pData != NULL) {
	free((char *)pSChan->pData);
	pSChan->pData = NULL;
    }
    if (pSChan->pDataAlStat != NULL) {
	free((char *)pSChan->pDataAlStat);
	pSChan->pDataAlStat = NULL;
    }
    if (pSChan->pDataAlSev != NULL) {
	free((char *)pSChan->pDataAlSev);
	pSChan->pDataAlSev = NULL;
    }
    if (pSChan->pDataCodeL != NULL) {
	free((char *)pSChan->pDataCodeL);
	pSChan->pDataCodeL = NULL;
    }
    if (pSChan->pDataCodeR != NULL) {
	free((char *)pSChan->pDataCodeR);
	pSChan->pDataCodeR = NULL;
    }
    if (pSChan->pFlags != NULL) {
	free((char *)pSChan->pFlags);
	pSChan->pFlags = NULL;
    }
    if (pSChan->pStats != NULL) {
	free((char *)pSChan->pStats);
	pSChan->pStats = NULL;
    }
}

/*+/subr**********************************************************************
* NAME	sydChanFind - find a channel in a synchronous set spec
*
* DESCRIPTION
*	This routine finds a channel in a synchronous set spec.  This can
*	be useful in detecting duplications in channel name.
*
* RETURNS
*	SYD_CHAN * for channel, if found, or
*	NULL
*
*-*/
SYD_CHAN *
sydChanFind(pSspec, chanName)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
char	*chanName;	/* I channel name to find in synchronous set spec */
{
    SYD_CHAN	*pSChan;	/* pointer to channel descriptor */

    assert(pSspec != NULL);
    assert(chanName != NULL);

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
        if (strcmp(pSChan->name, chanName) == 0)
	    break;
    }

    return pSChan;
}

/*+/subr**********************************************************************
* NAME	sydChanOpen - add a channel to a synchronous set spec
*
* DESCRIPTION
*	This routine opens a channel and adds it to the end of a synchronous
*	set spec.
*
*	Several data channels with the same name can be opened.  (If
*	retrieving from Channel Access, each will have its own monitor
*	to the IOC.  If the caller has his own monitor handler, monitoring
*	doesn't actually begin until sydMonitorStart is called.)
*
*	A channel can also be specified as a `test' channel, to be used
*	by sydTest to check a value-related condition.  Only one test
*	channel can be specified for a sync set spec.
*
* RETURNS
*	S_syd_OK, or
*	S_syd_noMem		no memory for adding channel to set
*	S_syd_chanNotConn	channel not connected to Channel Access (this
*				isn't treated as an error condition--the
*				channel is open and added to sync set spec)
*	S_syd_chanNotFound	channel not found
*	S_syd_ERROR		test channel specified, but there is already
*				a test channel with that name
*
* BUGS
* o	always places channel at end of list
* o	data are always acquired using the native type of the channel,
*	rather than caller-specified type
* o	.RVAL channels are always treated as unsigned
*
* SEE ALSO
*	sydOpen, sydChanClose, sydSetAttr
*	sydInputGet, sydMonitorStart
*	sydTest
*
* MACROS
*	These macros extract fields from the synchronous channel structure
*	created by sydChanOpen.  By using these macros, the programmer
*	can avoid the need to look inside the sample structures.
*
*	Each macro takes as an argument a pointer to a synchronous
*	channel structure, as returned by sydChanOpen or sydChanFind.
*
*	SydChanArrayCount(pSChan)	obtain the array count (1 for scalar)
*	SydChanDbfType(pSChan)		obtain the channel type.  This will
*		be one of the DBF_XXX types.
*	SydChanDbrType(pSChan)		obtain the value type.  This will
*		be one of the DBR_XXX types.
*	SydChanName(pSChan)		obtain the channel's name.
*	SydChanEGU(pSChan)		obtain the channel's engineering
*		units text (if channel doesn't have EGU, this will be "none")
*
*	Additional macros are described under sydInputGet.
*
* NOTES
* 1.	For channels in AR sample set data files, SYD_SY_NONF is forced,
*	without comment.
* 2.	DBF_ENUM channels are flagged as SYD_ST_STEP; all other channels
*	are flagged as SYD_ST_SMOOTH.
* 3.	For retrievals from Channel Access, the usual return status will
*	be S_syd_chanNotConn.  To force the connection attempt to completion,
*	the caller can call ca_pend_event following the call to this routine.
*	If SydChanDbfType(pSChan) is TYPENOTCONN following the ca_pend_event,
*	then the caller can assume the channel isn't presently connectable.
* 4.	For retrievals from Channel Access, the deadband which will be
*	used is determined by the SYD_ATTR_DEADBAND attribute.  By default,
*	the ADEL deadband is used.
* 5.	For retrievals from Channel Access with a caller-specified monitor
*	handler, the caller must call sydMonitorStart once for the sync set
*	spec in order to get a monitor started.
* 6.	For .RVAL channels, PREC is set to 0 and EGU is set to "counts".
*	Special processing is done for printing, plotting, exporting, and
*	statistics computing--the value is treated as unsigned.
*
* EXAMPLE
*
*-*/
long
sydChanOpen(pSspec, ppSChan, chanName, sync, pArg, test)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
SYD_CHAN **ppSChan;	/* O pointer to sync channel pointer; or NULL */
char	*chanName;	/* I channel name (.VAL assumed if field omitted) */
enum sydChanSync sync;	/* I sync/non-sync; SYD_SY_NONF or SYD_SY_FILLED */
void	*pArg;		/* I pointer to struct needed to init, or NULL */
int	test;		/* I 0,1 if this is data,test channel */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    enum sydChanStep step;	/* step type: SYD_ST_STEP or SYD_ST_CONTIN */
    int		i;
    int		new=1;

    assert(pSspec != NULL);
    assert(strlen(chanName) > 0);
    assert(strlen(chanName) < db_name_dim);

    if (ppSChan != NULL)
	*ppSChan = NULL;

/*-----------------------------------------------------------------------------
*    allocate and initialize an empty synchronous channel structure.  If this
*    channel has already been opened as a data or test channel, then the
*    previously used structure is used.
*----------------------------------------------------------------------------*/
    if ((pSChan = sydChanFind(pSspec, chanName)) != NULL) {
	if (pSChan->testChan) {
	    if (test)
		return S_syd_ERROR;
	    if (pSChan->dataChan == 0)
		new = 0;
	}
	else
	    new = 0;
    }
    if (new) {
	pSChan = (SYD_CHAN *)malloc(sizeof(SYD_CHAN));
	if (pSChan == NULL)
	    return S_syd_noMem;
	DoubleListAppend(pSChan, pSspec->pChanHead, pSspec->pChanTail);
	pSspec->chanCount++;
	pSChan->pSspec = pSspec;
	pSChan->pHandle = NULL;
	pSChan->evid = NULL;
	pSChan->nInBufs = pSspec->nInBufs;
	for (i=0; i<pSChan->nInBufs; i++) {
	    pSChan->inStatus[i] = SYD_B_EMPTY;
	    pSChan->pInBuf[i] = NULL;
	}
	pSChan->firstInBuf = pSChan->lastInBuf = pSChan->sampInBuf = -1;
	pSChan->minDataVal = pSChan->maxDataVal = 0.;
	pSChan->minMaxNeedInit = 1;
	pSChan->restrictMinDataVal = pSChan->restrictMaxDataVal = 0.;
	strcpy(pSChan->name, chanName);
	strcpy(pSChan->label, chanName);
	strcat(pSChan->label, " not conn");
	strcpy(pSChan->EGU, "none");
	pSChan->sync = sync;
	pSChan->isRVAL = 0;
	pSChan->pData = NULL;
	pSChan->pDataAlStat = pSChan->pDataAlSev = NULL;
	pSChan->pDataCodeL = pSChan->pDataCodeR = NULL;
	pSChan->pFlags = NULL;
	pSChan->conn = 0;
	pSChan->discon = 1;
	pSChan->dataChan = test?0:1;
	pSChan->pArg = NULL;
	pSChan->pStats = NULL;
    }
    if (pArg != NULL && pSChan->pArg == NULL)
	pSChan->pArg = pArg;
    pSChan->testChan = test;

/*-----------------------------------------------------------------------------
*    "open" the "source" for the data for the channel.  This will provide
*    information about how large the buffers need to be.  If retrieval is
*    from Channel Access, the buffer allocation, etc., will be deferred if
*    the connection isn't complete.
*----------------------------------------------------------------------------*/
    if (new) {
	stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_OPEN, pArg);
	if (stat == S_syd_OK) {
	    stat = sydChanOpen1(pSspec, pSChan);
	    if (stat == S_syd_OK)
		stat = sydChanOpenGR(pSChan);
	}
	else if (stat == S_syd_chanNotConn && pSspec->type == SYD_TY_CA) {
	    if (ppSChan != NULL)
		*ppSChan = pSChan;
	}

	if (stat != S_syd_OK && stat != S_syd_chanNotConn) {
	    (void)sydChanClose(pSspec, pSChan);
	    return stat;
	}
	retStat = stat;
    }

    if (ppSChan != NULL)
	*ppSChan = pSChan;

    return retStat;
}

/*+/internal******************************************************************
* NAME	sydChanOpen1 - open completion, after connect is complete
*
*-*/
long
sydChanOpen1(pSspec, pSChan)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
SYD_CHAN *pSChan;	/* I sync channel pointer */
{
    int		i;
    long	retStat=S_syd_OK;

    assert(pSspec != NULL);
    assert(pSChan != NULL);

    pSChan->conn = 1;
    pSChan->discon = 0;

/*-----------------------------------------------------------------------------
*    allocate the required number of input buffers
*    set either SMOOTH or STEP for the channel
*----------------------------------------------------------------------------*/
    for (i=0; i<pSChan->nInBufs; i++) {
	pSChan->pInBuf[i] = (union db_access_val *)malloc(
			dbr_size_n(pSChan->dbrType, pSChan->elCount));
	if (pSChan->pInBuf[i] == NULL) {
	    (void)sydChanClose(pSspec, pSChan);
	    retStat = S_syd_noMem;
	}
    }
    return retStat;
}

/*+/internal******************************************************************
* NAME	sydChanOpenGR - handle graphics info at open time, connect done
*
* DESCRIPTION
*	Sets prec, EGU, and label.  If the channel is a .RVAL channel,
*	prec is set to 0 and EGU is set to `counts'.
*
*	`isRVAL' is set for .RVAL channels.
*	`step' is set to SYD_ST_SMOOTH, or _STEP for ENUM channels
*
*-*/
long
sydChanOpenGR(pSChan)
SYD_CHAN *pSChan;	/* I sync channel pointer */
{
    char	*pC;

    if (pSChan->dbrType != DBR_TIME_ENUM)
	pSChan->step = SYD_ST_SMOOTH;
    else
	pSChan->step = SYD_ST_STEP;
    if ((pC = strrchr(pSChan->name, '.')) != NULL &&
					strcmp(pC, ".RVAL") == 0) {
	pSChan->precision = 0;
	strcpy(pSChan->EGU, "counts");
	pSChan->isRVAL = 1;
    }
    else if (pSChan->dbrType == DBR_TIME_FLOAT) {
        pSChan->precision = pSChan->grBuf.gfltval.precision;
	strcpy(pSChan->EGU, pSChan->grBuf.gfltval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
        pSChan->precision = 0;
	strcpy(pSChan->EGU, pSChan->grBuf.gshrtval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
        pSChan->precision = pSChan->grBuf.gdblval.precision;
	strcpy(pSChan->EGU, pSChan->grBuf.gdblval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
        pSChan->precision = 0;
	strcpy(pSChan->EGU, pSChan->grBuf.glngval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
        pSChan->precision = 0;
	strcpy(pSChan->EGU, pSChan->grBuf.gchrval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
        pSChan->precision = -1;
	strcpy(pSChan->EGU, " ");
    }    
    else {
        pSChan->precision = 0;
	strcpy(pSChan->EGU, " ");
    }
    sprintf(pSChan->label, "%s  %s", pSChan->name, pSChan->EGU);
    if (pSChan->dbrType != DBR_TIME_ENUM && pSChan->isRVAL == 0) {
	if (pSChan->precision > 10 || pSChan->precision <= 0)
	    pSChan->precision = 3;
    }

    if (*pSChan->EGU == ' ' || strlen(pSChan->EGU) == 0)
	strcpy(pSChan->EGU, "none");
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydChanPrep - prepare a channel for a new retrieval
*
* DESCRIPTION
*	This routine initializes a channel structure in preparation for
*	a new retrieval.
*
* RETURNS
*	S_syd_OK
*
* SEE ALSO
*	sydSampleSetGet, sydInputGet, sydInputStoreInSet
*
*-*/
long
sydChanPrep(pSspec, pSChan)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
SYD_CHAN *pSChan;	/* I pointer to synchronous channel descriptor */
{
    int		i;

    assert(pSspec != NULL);
    assert(pSChan != NULL);

    pSChan->minDataVal = pSChan->maxDataVal = 0.;
    pSChan->minMaxNeedInit = 1;
    pSChan->restrictMinDataVal = pSChan->restrictMaxDataVal = 0.;
}

/*+/subr**********************************************************************
* NAME	sydClose - close a synchronous set spec
*
* DESCRIPTION
*	This routine closes a synchronous set spec, closing channels, etc.,
*	wrapping up the retrieval "source", and then free()'s the data
*	structures associated with the set.
*
* RETURNS
*	S_syd_OK, or
*	other status codes if an error occurs
*
* BUGS
* o	the `wrapup' call should do the free, since the free must
*	match the malloc--if this routine is compiled with DEBUG and
*	the routine which did the malloc (e.g., sydSubrCA) is compiled
*	without it, then the free here will complain.
*
* SEE ALSO
*	sydOpen
*
* EXAMPLE
*
*-*/
long
sydClose(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;

    assert(pSspec != NULL);

    while (pSspec->pChanHead != NULL) {
	if (sydChanClose(pSspec, pSspec->pChanHead) != S_syd_OK)
	    (void)printf("sydClose: error closing channel\n");
    }

    sydSampleSetFree(pSspec);

    stat = (pSspec->pFunc)(pSspec, NULL, SYD_FC_WRAPUP, NULL);
    if (stat != S_syd_OK)
	retStat = stat;

    free((char *)pSspec);
    return retStat;
}

/*+/subr**********************************************************************
* NAME	sydCopyGr
*
* DESCRIPTION
*	Copies the graphics information from the Channel Access buffer into
*	the caller's buffer.  The CA buffer must be a DBR_GR type; the
*	caller's buffer is assumed to be the same type.
*
*-*/
void
sydCopyGr(pDest, pSrc, srcDbrType)
void	*pDest;		/* O pointer to buffer to receive graphics info */
void	*pSrc;		/* I pointer to buffer holding graphics info */
chtype	srcDbrType;	/* I DBR_xxx type of the source buffer */
{
#define U_DB union db_access_val *
    if (srcDbrType == DBR_GR_FLOAT) {
	((U_DB)pDest)->gfltval = ((U_DB)pSrc)->gfltval;
	((U_DB)pDest)->gfltval.units[7] = '\0';
    }
    else if (srcDbrType == DBR_GR_SHORT) {
	((U_DB)pDest)->gshrtval = ((U_DB)pSrc)->gshrtval;
	((U_DB)pDest)->gshrtval.units[7] = '\0';
    }
    else if (srcDbrType == DBR_GR_DOUBLE) {
	((U_DB)pDest)->gdblval = ((U_DB)pSrc)->gdblval;
	((U_DB)pDest)->gdblval.units[7] = '\0';
    }
    else if (srcDbrType == DBR_GR_LONG) {
	((U_DB)pDest)->glngval = ((U_DB)pSrc)->glngval;
	((U_DB)pDest)->glngval.units[7] = '\0';
    }
    else if (srcDbrType == DBR_GR_STRING)
	((U_DB)pDest)->gstrval = ((U_DB)pSrc)->gstrval;
    else if (srcDbrType == DBR_GR_ENUM)
	((U_DB)pDest)->genmval = ((U_DB)pSrc)->genmval;
    else if (srcDbrType == DBR_GR_CHAR) {
	((U_DB)pDest)->gchrval = ((U_DB)pSrc)->gchrval;
	((U_DB)pDest)->gchrval.units[7] = '\0';
    }
    else
	assertAlways(0);
}

/*+/subr**********************************************************************
* NAME	sydCopyVal - copy channel value
*
* DESCRIPTION
*	Copies the value from the Channel Access buffer into the caller's
*	buffer.  The CA buffer can be any valid DBR_xxx type; the caller's
*	buffer is assumed to be the corresponding DBF_xxx type--i.e., with
*	room only for the value, and not the status, time stamp, etc.
*
*-*/
void
sydCopyVal(pDest, pSrc, count, srcDbrType)
void	*pDest;		/* O pointer to buffer to receive value */
void	*pSrc;		/* I pointer to buffer holding value */
int	count;		/* I array element count */
chtype	srcDbrType;	/* I DBR_xxx type of the source buffer */
{
    int		i;

    if (dbr_type_is_FLOAT(srcDbrType)) {
	float	*pD=(float *)pDest;
	float	*pS=(float *)dbr_value_ptr(pSrc, srcDbrType);
	for (i=0; i<count; i++)
	    *pD++ = *pS++;
    }
    else if (dbr_type_is_SHORT(srcDbrType)) {
	short	*pD=(short *)pDest;
	short	*pS=(short *)dbr_value_ptr(pSrc, srcDbrType);
	for (i=0; i<count; i++)
	    *pD++ = *pS++;
    }
    else if (dbr_type_is_DOUBLE(srcDbrType)) {
	double	*pD=(double *)pDest;
	double	*pS=(double *)dbr_value_ptr(pSrc, srcDbrType);
	for (i=0; i<count; i++)
	    *pD++ = *pS++;
    }
    else if (dbr_type_is_LONG(srcDbrType)) {
	long	*pD=(long *)pDest;
	long	*pS=(long *)dbr_value_ptr(pSrc, srcDbrType);
	for (i=0; i<count; i++)
	    *pD++ = *pS++;
    }
    else if (dbr_type_is_STRING(srcDbrType)) {
	char	*pD=(char *)pDest;
	char	*pS=(char *)dbr_value_ptr(pSrc, srcDbrType);
	for (i=0; i<count; i++) {
	    strcpy(pD, pS);
	    pD += db_strval_dim;
	    pS += db_strval_dim;
	}
    }
    else if (dbr_type_is_ENUM(srcDbrType)) {
	short	*pD=(short *)pDest;
	short	*pS=(short *)dbr_value_ptr(pSrc, srcDbrType);
	for (i=0; i<count; i++)
	    *pD++ = *pS++;
    }
    else if (dbr_type_is_CHAR(srcDbrType)) {
	unsigned char	*pD=(unsigned char *)pDest;
	unsigned char	*pS=(unsigned char *)dbr_value_ptr(pSrc, srcDbrType);
	for (i=0; i<count; i++)
	    *pD++ = *pS++;
    }
}

/*+/subr**********************************************************************
* NAME	sydFileInfo - print information about the data file
*
* DESCRIPTION
*	This routine prints some information about the source of data for
*	the synchronous sample set.  Typically, this will be information
*	about the sample range and channel names in a data file.
*
* RETURNS
*	S_syd_OK, or
*	S_syd_ERROR
*
* BUGS
* o	text
*
*-*/
long
sydFileInfo(pSspec, outStream)
SYD_SPEC *pSspec;	/* O pointer to synchronous set spec */
FILE	*outStream;	/* I file pointer to receive information */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;

    assert(pSspec != NULL);

    stat = (pSspec->pFunc)(pSspec, NULL, SYD_FC_FILEINFO, outStream);
    if (stat != S_syd_OK)
	retStat = S_syd_ERROR;
    return retStat;
}

/*+/subr**********************************************************************
* NAME	sydInputGet - get the next synchronous sample
*
* DESCRIPTION
*	This routine acquires the next sample for the channels in the
*	synchronous set spec.  For retrievals from Channel Access when
*	the caller is using his own monitor handler, calling this
*	routine does nothing and isn't necessary.
*
*	The desired time stamp for the `next sample' is determined by
*	searching forward from the time of the previous sample.  Among the
*	channels in the synchronous set spec, the one which changes soonest
*	is the one whose time stamp is used for the next sample.  Values are
*	handled on either a "non-filled" or a "filled" basis.
*
*	For "filled" channels, it is assumed that if a change isn't
*	detected in a channel's value, then the present value (i.e., the
*	value at the desired time stamp) must be the same as it was when
*	it last changed.  If there is no data at the desired time stamp for
*	one or more of the channels, then the return status will be
*	S_syd_partial.  There is no data for a "filled" channel
*	if the desired time stamp is prior to the first value for the
*	channel or if the desired time stamp is after the last value in
*	the file for the channel.
*
*	For "non-filled" channels, it is assumed that if there is no value
*	for the desired time stamp then the channel has missing data at
*	that time; in this case the return status will be S_syd_partial.
*
*	When retrieving from Channel Access, the situation is slightly more
*	complicated.  Since there may be an unpredictable delay in receiving
*	a monitored value for a channel, values are buffered prior to
*	assembling a synchronous sample.  If one or more channels don't
*	have a value for the time stamp, it may be because appropriate data
*	haven't arrived yet.  If this is the case, and if no other channels
*	are about to overflow their buffers, then a return status of
*	S_syd_noDataNow is generated.  When S_syd_noDataNow is returned, the
*	caller should try again later.  Because of the nature of acquiring
*	data using Channel Access monitors, it is recommended that this
*	routine be called repeatedly until a status of S_syd_noDataNow is
*	returned, and then check periodically until more data are available.
*	The caller must periodically call ca_pend_event in order to allow
*	Channel Access to give data to the sydXxx routines.
*
*	There are two main ways of dealing with samples obtained from this
*	routine--use the other sydXxx routines to manipulate it or access
*	it directly for manipulation by user code.  The first case is
*	the simplest, and would probably use sydSampleSetGet rather than
*	this routine.  The second alternative is described in more detail
*	below.
*
*	The status of the sample for a channel is obtained with the
*	SydInputStatus macro; several cases may be encountered:
*	o   status is SYD_B_FULL, SYD_B_RESTART, or SYD_B_SAMPLED; the
*	    buffer contains valid sample data.  Once the caller has dealt
*	    with the sample, THE CALLER MUST call SydInputMarkAsSampled!!!
*	o   status is SYD_B_MISSING or SYD_B_EOF; no data is available
*	o   statuses of SYD_B_SNAP_BEGIN, SYD_B_SNAP_SINGLE, and
*	    SYD_B_SNAP_END can also occur; these can be treated as synonyms
*	    of SYD_B_FULL.
*
* RETURNS
*	S_syd_OK		all channels are present in sample
*	S_syd_partial		some channels are missing from sample;
*				such channels will have .sampInBuf of -1
*				or else buffer status will be SYD_B_EOF
*				or SYD_B_MISSING
*	S_syd_noDataNow		for Channel Access retrievals, a sample
*				isn't available now; try later
*	S_syd_EOF		end of file--no more samples available
*
* BUGS
* o	should get and report actual number of bytes from the read
* o	for AR sample set files, if all channels are missing for 2
*	successive samples, EOF is reported, even though file isn't at end
*
* SEE ALSO
*	sydChanOpen, sydSamplePrint, sydInputStoreInSet, sydSampleSetGet
*
* MACROS
*	These macros extract fields from input buffers produced by sydInputGet.
*	By using these macros, the programmer can avoid the need to look
*	inside the sample structures.
*
*	Each macro takes as an argument a pointer to a synchronous sample
*	channel structure, as returned by sydChanOpen or sydChanFind.
*
*	The SydInputValAsXxx macros must not be called if the value
*	returned by SydInputStatus is SYD_B_MISSING or SYD_B_EOF.
*
*	SydInputStatus(pSChan)		obtain the input sample status for a
*		channel.  This will be one of the SYD_B_XXX values.
*	SydInputTs(pSspec)		obtain time stamp of the input
*		sample.  (Note that this uses the sample spec, not the
*		channel pointer.)
*	SydInputMarkAsSampled(pSChan)	set the input sample status to
*		SYD_B_SAMPLED
*
*	Access the input value for a channel, following sydInputGet (these
*	macros don't do any conversion--the one that is used must match
*	the type of the input value)
*		SydInputValAsFloat(pSChan)	returns a float
*		SydInputValAsDouble(pSChan)	returns a double
*		SydInputValAsShort(pSChan)	returns a short
*		SydInputValAsLong(pSChan)	returns a long
*		SydInputValAsChar(pSChan)	returns a char
*		SydInputValAsString(pSChan)	returns a string (i.e., char *)
*		SydInputValAsVoidPtr(pSChan)	returns a void * pointer to
*			value (useful for array channels)
*
*	Additional macros are described under sydChanOpen.
*
* NOTES
* 1.	The `more flag' is for use only with Channel Access.  It serves as
*	a hint that another call to sydInputGet will be successful.  This
*	is useful for programs which want to throw away all but the most
*	recent sample if they fall behind.  The technique with such
*	programs is to repeatedly call sydInputGet until the return status
*	is S_syd_noDataNow, and process only those cases in which the
*	`more flag' is returned as zero.  (SydInputMarkAsSampled must
*	be called for the skipped samples, as well as for those which
*	are processed.)  It may be interesting to note that a zero value
*	doesn't guarantee that the next call will result in a status of
*	S_syd_noDataNow.
* 2.	A sample for a channel can have a valid time stamp and still have
*	a status of SYD_B_MISSING.  The main way this happens is when
*	reading a `sample set' file.
*
* EXAMPLE
*	Get the next sample, then print the values and statuses for the
*	channels.  If Channel Access has queued up several values from the
*	IOC, then several samples will probably be processed.  This example
*	ignores the differences between SYD_B_FULL, SYD_B_RESTART, and
*	SYD_B_SAMPLED.
*
*	SYD_CHAN *pSChan1, *pSChan2, ...;	pointers to channel structures
*	int	status;
*	float	value;
*	char	timeText[28];
*	int	moreFlag;
*
*	ca_pend_event(.1);	
*	stat = sydInputGet(pSspec, &moreFlag);
*	while (stat == S_syd_OK || stat == S_syd_partial) {
*	    tsStampToText(&SydInputTs(pSspec), TS_TEXT_MMDDYY, timeText);
*	    printf("sample time stamp is %s\n", timeText);
*	    if (stat == S_syd_partial)
*		printf("one or more channels don't have data\n");
*	    status = SydInputStatus(pSChan1);
*	    if (status == SYD_B_MISSING || status == SYD_B_EOF)
*		printf("%s has missing data\n", pSChan1->name);
*	    else {
*		value = SydInputValAsFloat(pSChan1);
*		printf("%s value is: %f\n", value);
*		SydInputMarkAsSampled(pSChan1);
*	    }
*
*	    stat = sydInputGet(pSspec, &moreFlag);	try for another sample
*	}
*
*-*/
long 
sydInputGet(pSspec, pMoreFlag)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
int	*pMoreFlag;	/* O pointer to flag or NULL; a return value of 1
			    indicates more data is available; 0 indicates
			    uncertainty about whether more is available */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    int		readIndx;	/* select buffer to use for read */
    SYD_CHAN	*pSChan;	/* pointer to channel in Sspec */
    double	refDiff;	/* difference from reference time  */
    TS_STAMP	nextTs;		/* time stamp for next sample */
    int		i, i1, i2, j, now=0, later=0;
    int		moreFlag=1;	/* assume there's more; reset if not */
    enum sydBStatus inStatus_i, inStatus_i1, inStatus_i2;
    int		full_i, full_i1, full_i2;

    assert(pSspec != NULL);
    assert(pSspec->pChanHead != NULL);

    nextTs.secPastEpoch = 0;
    nextTs.nsec = 0;
    pSspec->eofCount = 0;

#define BUFiTS pSChan->pInBuf[i]->tstrval.stamp
#define BUFi1TS pSChan->pInBuf[i1]->tstrval.stamp

/*-----------------------------------------------------------------------------
*    try to get at least two "available" buffers of input data
*----------------------------------------------------------------------------*/
    sydInputGetIn(pSspec);

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
/*-----------------------------------------------------------------------------
*    with two buffers of data, determine if this channel can contribute to
*    the time stamp for the sample.  It can potentially contribute if:
*    o  .firstInBuf is SYD_B_FULL or SYD_B_RESTART or SYD_B_MISSING, or
*    o  .firstInBUf is SYD_B_SAMPLED and the next buffer is SYD_B_FULL
*
*    For SYD_TY_CA, some buffer checks are performed.  If a channel is
*    about to lose data (because buffer is nearly full), then the `now'
*    flag is set to force generating a partial sample.  If a channel has
*    no data, then the `later' flag is set to gently request returning a
*    status of S_syd_noDataNow; this is also done if the channel has only
*    one value and its status is SYD_B_SAMPLED.
*
*    the time stamp for the sample is the smallest time stamp of all
*    the channels which can contribute.  If no SYD_B_FULL or SYD_B_RESTART or
*    SYD_B_MISSING buffers are available, then .nextTs will be 0  .
*----------------------------------------------------------------------------*/
	i = pSChan->firstInBuf;
	inStatus_i = pSChan->inStatus[i];
	full_i = inStatus_i == SYD_B_FULL || inStatus_i == SYD_B_RESTART ||
	    inStatus_i == SYD_B_SNAP_BEGIN ||
	    inStatus_i == SYD_B_SNAP_SINGLE || inStatus_i == SYD_B_SNAP_END;
	i1 = NEXT_INBUF(pSChan, i);
	inStatus_i1 = pSChan->inStatus[i1];
	full_i1 = inStatus_i1 == SYD_B_FULL || inStatus_i1 == SYD_B_RESTART ||
	    inStatus_i1 == SYD_B_SNAP_BEGIN ||
	    inStatus_i1 == SYD_B_SNAP_SINGLE || inStatus_i1 == SYD_B_SNAP_END;
	if (i < 0)
	    later = 1;
	else if (full_i) {
	    if (pSspec->type == SYD_TY_CA) {
		j = pSChan->lastInBuf;
		if (j < pSChan->firstInBuf)
		    j += pSChan->nInBufs;
		if (pSChan->nInBufs - (j - pSChan->firstInBuf +1) < 3)
		    now = 1;	/* 3 or fewer empty slots left */
	    }
	    if (nextTs.secPastEpoch == 0 || TsCmpStampsLT(&BUFiTS, &nextTs))
		nextTs = BUFiTS;
	}
	else if (inStatus_i == SYD_B_MISSING) {
	    if (TsCmpStampsLT(&BUFiTS, &nextTs))
		nextTs = BUFiTS;
	}
	else if (full_i1) {
	    if (pSspec->type == SYD_TY_CA) {
		j = pSChan->lastInBuf;
		if (j < pSChan->firstInBuf)
		    j += pSChan->nInBufs;
		if (pSChan->nInBufs - (j - pSChan->firstInBuf +1) < 3)
		    now = 1;	/* 3 or fewer empty slots left */
	    }
	    if (nextTs.secPastEpoch == 0 || TsCmpStampsLT(&BUFi1TS, &nextTs))
		nextTs = BUFi1TS;
	}
	else if (inStatus_i1 == SYD_B_MISSING) {
	    if (TsCmpStampsLT(&BUFi1TS, &nextTs))
		nextTs = BUFi1TS;
	}
	else if (i == pSChan->lastInBuf && inStatus_i == SYD_B_SAMPLED) {
	    later = 1;
	}
	else
	    later = 1;
    }

/*----------------------------------------------------------------------------
*    now that we have as much info as possible to play with, figure out how
*    to put together a sample to return to the caller.  If no data at all
*    are available, just return an EOF notification (or, for SYD_TY_CA, give
*    a noDataNow notification).
*----------------------------------------------------------------------------*/
    if (pSspec->type == SYD_TY_CA && now == 0 && later == 1) {
	return S_syd_noDataNow;
    }
    if (nextTs.secPastEpoch == 0)
	if (pSspec->type != SYD_TY_CA)
	    return S_syd_EOF;
	else
	    return S_syd_noDataNow;

    if (pSspec->refTs.secPastEpoch == 0 || pSspec->sampleCount == 0) {
/*----------------------------------------------------------------------------
*	It is necessary to initialize the reference time stamp in the Sspec
*	descriptor.  The reference time stamp starts on a one second boundary.
*----------------------------------------------------------------------------*/
	pSspec->refTs.secPastEpoch = nextTs.secPastEpoch;
	pSspec->refTs.nsec = 0;
	pSspec->restrictRefTs = pSspec->refTs;
    }

/*----------------------------------------------------------------------------
*    A channel may be included in the sample if either its oldest or its
*    second oldest buffer has a valid time stamp.  A buffer will have a
*    valid time stamp if its status is SYD_B_FULL, SYD_B_RESTART, or
*    SYD_B_MISSING.
*
*    The oldest buffer is included in the sample a) if its time stamp is
*    <= to the retrieval time stamp; or b) if its time stamp is less
*    than the retrieval time stamp and the second oldest buffer isn't
*    eligible to include in the sample.  If the oldest buffer IS included,
*    the sample status will be `partial' if the buffer status is either
*    SYD_B_MISSING or SYD_B_EOF.  If a previously sampled value is used for
*    the current sample, then .reused is set to 1  .
*
*    If the oldest buffer isn't included in the sample, then the second
*    oldest buffer will be included if its time stamp is <= to the retrieval
*    time stamp.  If the second oldest buffer is included in the sample,
*    then the oldest buffer is flagged as empty.
*
*    For Channel Access retrievals, the moreFlag will be reset if:
*    o   a channel doesn't contribute to the sample; or
*    o   a channel doesn't have one more SYD_B_FULL buffer
*----------------------------------------------------------------------------*/

    pSspec->priorTs = pSspec->sampleTs;
    pSspec->sampleTs = nextTs;
    pSspec->partial = 0;
    TsDiffAsDouble(&pSspec->sampleSec, &pSspec->sampleTs, &pSspec->refTs);
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	i = pSChan->firstInBuf;
	inStatus_i = pSChan->inStatus[i];
	full_i = inStatus_i == SYD_B_FULL || inStatus_i == SYD_B_RESTART ||
	    inStatus_i == SYD_B_SNAP_BEGIN ||
	    inStatus_i == SYD_B_SNAP_SINGLE || inStatus_i == SYD_B_SNAP_END;
	i1 = NEXT_INBUF(pSChan, i);
	inStatus_i1 = pSChan->inStatus[i1];
	full_i1 = inStatus_i1 == SYD_B_FULL || inStatus_i1 == SYD_B_RESTART ||
	    inStatus_i1 == SYD_B_SNAP_BEGIN ||
	    inStatus_i1 == SYD_B_SNAP_SINGLE || inStatus_i1 == SYD_B_SNAP_END;
	if (i < 0) {
	    pSChan->sampInBuf = -1;
	    moreFlag = 0;
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}
	else if (full_i && TsCmpStampsLE(&BUFiTS, &nextTs)) {
	    pSChan->sampInBuf = i;
	    if (inStatus_i1 != SYD_B_FULL)
		moreFlag = 0;
	    pSChan->reused = 0;
	}
	else if (pSChan->inStatus[i] == SYD_B_SAMPLED ||
			pSChan->inStatus[i] == SYD_B_MISSING) {
	    if ((full_i1 || inStatus_i1 == SYD_B_MISSING) &&
					TsCmpStampsLE(&BUFi1TS, &nextTs)) {
		pSChan->inStatus[i] = SYD_B_EMPTY;
		pSChan->firstInBuf = i1;
		pSChan->reused = 0;
		pSChan->sampInBuf = i1;
		i2 = NEXT_INBUF(pSChan, i1);
		inStatus_i2 = pSChan->inStatus[i2];
		full_i2 = inStatus_i2 == SYD_B_FULL ||
				inStatus_i2 == SYD_B_RESTART ||
		    		inStatus_i2 == SYD_B_SNAP_BEGIN ||
		    		inStatus_i2 == SYD_B_SNAP_SINGLE ||
				inStatus_i2 == SYD_B_SNAP_END;
		if (!full_i2)
		    moreFlag = 0;
	    }
	    else {
		if (pSChan->inStatus[i] == SYD_B_SAMPLED)
		    pSChan->reused = 1;
		pSChan->sampInBuf = i;
		if (!full_i1)
		    moreFlag = 0;
	    }
	}
	else if (inStatus_i == SYD_B_EOF) {
	    pSChan->sampInBuf = i;
	    moreFlag = 0;
	    pSspec->eofCount++;
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}
	else {
	    pSChan->sampInBuf = -1;
	    moreFlag = 0;
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}
	if (pSChan->sampInBuf >= 0 &&
			pSChan->inStatus[pSChan->sampInBuf] == SYD_B_MISSING) {
	    moreFlag = 0;
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}
    }
    if (pMoreFlag != NULL)
	*pMoreFlag = moreFlag;

    return retStat;
}

/*+/internal******************************************************************
* NAME	sydInputGetIn - get new input data, possibly discarding old
*
* DESCRIPTION
*	Checks input buffers, discarding old buffers, if possible.  Then,
*	except for SYD_TY_CA, an attempt is made to fill 2 input buffers.
*	Finally, the time stamps are checked for the first 2 buffers--if
*	either is 0, it is set to 1.
*
* RETURNS
*	NULL
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
sydInputGetIn(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
{
    long	stat;           /* status return from calls */
    SYD_CHAN	*pSChan;	/* pointer to channel in Sspec */
    int		i, i1, discard;
    enum sydBStatus inStatus_i1;
    int		full_i1;

    assert(pSspec != NULL);
    assert(pSspec->pChanHead != NULL);

/*----------------------------------------------------------------------------
*    try to get both buffers for the channel to be SYD_B_FULL, performing
*    reads as necessary.  When processing is complete, buffer status will be
*    either SYD_B_FULL, SYD_B_SAMPLED, SYD_B_MISSING, SYD_B_EMPTY,
*    or SYD_B_EOF.
*----------------------------------------------------------------------------*/
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
/*-----------------------------------------------------------------------------
*    see if the oldest input can be thrown away.  This is possible if:
*    o  buffer status is SYD_B_SAMPLED and one of the following is also true:
*	-  channel is SYD_SY_NONF, or
*	-  channel is SYD_SY_FILLED, and either:
*	   .  the next buffer's status is SYD_B_FULL (or RESTART), and the
*	      next buffer's time stamp is less than or equal to the previous
*	      sample time stamp; or,
*	   .  the next buffer's status is SYD_B_MISSING, and the next
*	      buffer's time stamp is less than or equal to the previous
*	      sample time stamp; or,
*    o  buffer status is SYD_B_MISSING and:
*	-  the next buffer's status is SYD_B_FULL (or RESTART), and the next
*	   buffer's time stamp is less than or equal to the previous
*	   sample time stamp
*
*----------------------------------------------------------------------------*/
	if ((i = pSChan->firstInBuf) >= 0) {
	    discard = 0;
	    i1 = NEXT_INBUF(pSChan, i);
	    inStatus_i1 = pSChan->inStatus[i1];
	    full_i1 = inStatus_i1==SYD_B_FULL || inStatus_i1==SYD_B_RESTART ||
		inStatus_i1==SYD_B_SAMPLED || inStatus_i1==SYD_B_SNAP_BEGIN ||
		inStatus_i1==SYD_B_SNAP_SINGLE || inStatus_i1==SYD_B_SNAP_END;
	    if (pSChan->inStatus[i] == SYD_B_SAMPLED) {
		if (pSChan->sync == SYD_SY_NONF)
		    discard = 1;
		else if (inStatus_i1 == SYD_B_MISSING || full_i1) {
		    if (TsCmpStampsLE(&BUFi1TS, &pSspec->sampleTs))
			discard = 1;
		}
	    }
	    else if (pSChan->inStatus[i] == SYD_B_MISSING) {
		if (pSChan->sync == SYD_SY_NONF &&
			TsCmpStampsLE(&BUFiTS, &pSspec->sampleTs)) {
		    discard = 1;
		}
		else if (full_i1) {
		    if (TsCmpStampsLE(&BUFi1TS, &pSspec->sampleTs))
			discard = 1;
		}
		else if (inStatus_i1 == SYD_B_MISSING)
		    discard = 1;
	    }
	    if (discard) {
		pSChan->inStatus[i] = SYD_B_EMPTY;
		if (i == pSChan->lastInBuf) {
		    pSChan->lastInBuf = -1;
		    pSChan->firstInBuf = -1;
		}
		else
		    pSChan->firstInBuf = NEXT_INBUF(pSChan, i);
	    }
	}
    }
/*-----------------------------------------------------------------------------
*    now try to get 2 buffers of input data (except for CA), then check for
*    zero time stamps.  If a zero time stamp is found, it is set to 1
*    second past the epoch.  (This is done because a lot of the code depends
*    on non-zero time stamp as a way of detecting valid data.)
*----------------------------------------------------------------------------*/
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSspec->type != SYD_TY_CA) {
	    if (pSChan->firstInBuf < 0)
		stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_READ, NULL);
	    if (pSChan->firstInBuf == pSChan->lastInBuf)
		stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_READ, NULL);

skipSecondRead:
	    ;
	}
	if ((i = pSChan->firstInBuf) >= 0) {
	    if (BUFiTS.secPastEpoch == 0)
		BUFiTS.secPastEpoch = 86401;
	    i = NEXT_INBUF(pSChan, i);
	    if (i >= 0 && BUFiTS.secPastEpoch == 0)
		BUFiTS.secPastEpoch = 86401;
	}
	if (pSspec->type == SYD_TY_SSF)
	    break;
    }
}

/*+/subr**********************************************************************
* NAME	sydInputReset - reset synchronous sample buffers for channels
*
* DESCRIPTION
*	This routine flags as EMPTY the input buffers for the channels in a
*	synchronous set spec.  Several routines are available:
*
*	sydInputReset   flags as empty all buffers for all channels
*			This routine is appropriate to use after a
*			positioning operation, since such an operation
*			invalidates both FULL and SAMPLED buffers.
*
*	sydInputResetKeepNewest   flags as empty all buffers except the
*				newest one, which is flagged as SAMPLED
*			This routine is primarily intended for use with
*			Channel Access.  Keeping the newest buffer and
*			flagging it as SAMPLED allows: a) using it if
*			no new value comes in; or b) throwing it away
*			if a new value does come in.
*
*	sydInputResetSampled   flags as empty only those buffers which
*				are flagged as SAMPLED
*			This routine is appropriate to use between
*			successive retrievals.  Since
*			there is no "gap" between retrievals, the FULL
*			buffers must be available to the next retrieval.
*			Making SAMPLED buffers available for the next
*			retrieval could result in the next retrieval
*			having the same retrieval time stamp as the
*			prior retrieval.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	text
*
* NOTES
* 1.	For retrievals from Channel Access, sydInputReset retains the
*	newest buffer for filled channels.
*
*-*/
void
sydInputReset(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
{
    SYD_CHAN	*pSChan;
    int		i;

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	i = pSChan->firstInBuf;
	while (i >= 0 && i != pSChan->lastInBuf) {
	    pSChan->inStatus[i] = SYD_B_EMPTY;
	    i = NEXT_INBUF(pSChan, i);
	    pSChan->firstInBuf = i;
	}
	if ((i=pSChan->firstInBuf) >= 0) {
	    if (pSspec->type == SYD_TY_CA && pSChan->sync == SYD_SY_FILLED)
		pSChan->inStatus[i] = SYD_B_FULL;
	    else {
		pSChan->inStatus[i] = SYD_B_EMPTY;
		pSChan->firstInBuf = pSChan->lastInBuf = -1;
	    }
	}
	pSChan->sampInBuf = -1;
    }
    pSspec->sampleTs.secPastEpoch = 0;
}

/*+/subr**********************************************************************
* NAME	sydInputResetKeepNewest
*
*-*/
void
sydInputResetKeepNewest(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    SYD_CHAN	*pSChan;
    int		i;

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	i = pSChan->firstInBuf;
	while (i >= 0 && i != pSChan->lastInBuf) {
	    pSChan->inStatus[i] = SYD_B_EMPTY;
	    i = NEXT_INBUF(pSChan, i);
	    pSChan->firstInBuf = i;
	}
	if ((i=pSChan->firstInBuf) > 0)
	    pSChan->inStatus[i] = SYD_B_SAMPLED;
	pSChan->sampInBuf = -1;
    }
    pSspec->sampleTs.secPastEpoch = 0;
}

/*+/subr**********************************************************************
* NAME	sydInputResetSampled
*
*-*/
void
sydInputResetSampled(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
{
    SYD_CHAN	*pSChan;
    int		i;

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	i = pSChan->firstInBuf;
	while (i >= 0) {
	    if (pSChan->inStatus[i] == SYD_B_SAMPLED) {
		pSChan->inStatus[i] = SYD_B_EMPTY;
		if (pSChan->lastInBuf == i) {
		    pSChan->lastInBuf = -1;
		    pSChan->firstInBuf = -1;
		    i = -1;
		}
		else if (pSChan->firstInBuf == i)
		    pSChan->firstInBuf = NEXT_INBUF(pSChan, i);
	    }
	    if (i == pSChan->lastInBuf)
		i = -1;				/* leave the loop */
	    else
		i = NEXT_INBUF(pSChan, i);
	}
	pSChan->sampInBuf = -1;
    }
    pSspec->sampleTs.secPastEpoch = 0;
}

/*+/subr**********************************************************************
* NAME	sydInputStoreInSet - store a sample into the sample set
*
* DESCRIPTION
*	Stores a sample acquired by sydInputGet into the next available
*	sample set buffer.  Flags for the sample set buffer are set
*	appropriately.  The minDataVal and maxDataVal for the channel
*	are updated (unless the alarm severity indicates INVALID).
*
*	If a test condition has been established by sydTestAddFromText,
*	the condition is tested.  If the condition is false, then the
*	sample isn't stored.
*
*	If the `ignorePartial' flag is set, then some special processing
*	is done to avoid losing interesting status information for
*	synchronous channels.
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* NOTES
* 1.	If the retrieval source is Channel Access, and if the time stamp
*	for this sample is earlier than the previous sample, then this
*	sample is ignored.
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
void
sydInputStoreInSet(pSspec, ignorePartial)
SYD_SPEC *pSspec;
int	ignorePartial;	/* I 0,1 to store,ignore partial samples */
{
    int		sub;		/* subscript of sample */
    chtype	type;
    long	stat;
    SYD_CHAN	*pSChan;
    int		i, i1, el;
    int		useVal;		/* indicates if sample data is to be stored */
    short	alStat, alSev;
    int		ignore=0;
 
#define Si pSChan->inStatus[i]
#define Si1 pSChan->inStatus[i1]

    sub = pSspec->lastData;
    if (ignorePartial && pSspec->partial)
	ignore = 1;
    else if (!sydTest(pSspec))
	ignore = 1;
    else if (sub >= 0 && pSspec->type == SYD_TY_CA &&
		TsCmpStampsLE(&pSspec->sampleTs, &pSspec->pTimeStamp[sub])) {
	ignore = 1;
    }
    if (ignore) {
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	    i = pSChan->sampInBuf;
	    if (i != pSChan->lastInBuf) {
		i1 = i + 1;
		if (i1 >= pSChan->nInBufs)
		    i1 = 0;
		if (Si1==SYD_B_MISSING || Si1==SYD_B_EOF || Si1==SYD_B_EMPTY)
		    i1 = -1;
	    }
	    else
		i1 = -1;
	    if (i >= 0) {
		if (i1 >= 0) {
		    if (Si == SYD_B_RESTART)
			pSChan->inStatus[i1] = SYD_B_RESTART;
		    else if (Si == SYD_B_SNAP_BEGIN) {
			if (Si1 == SYD_B_SNAP_END)
			    pSChan->inStatus[i1] = SYD_B_SNAP_SINGLE;
			else if (Si1 == SYD_B_SNAP_SINGLE)
			    pSChan->inStatus[i1] = SYD_B_SNAP_BEGIN;
		    }
		}
		if (Si != SYD_B_MISSING)
		    pSChan->inStatus[i] = SYD_B_SAMPLED;
	    }
	}
	return;
    }
    if (++pSspec->lastData >= pSspec->dataDim)
	pSspec->lastData = 0;
    sub = pSspec->lastData;

    pSspec->pDeltaSec[sub] = pSspec->sampleSec;
    pSspec->pTimeStamp[sub] = pSspec->sampleTs;
    pSspec->pPartial[sub] = pSspec->partial;
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->pData == NULL || pSChan->dataChan == 0)
		;	/* no action if no malloc's or if not data channel */
	else if (pSChan->dataChan) {
	    bzero((char *)&pSChan->pFlags[sub], sizeof(*pSChan->pFlags));
	    pSChan->pDataCodeL[sub] = '#';
	    pSChan->pDataCodeR[sub] = ' ';
	    type = pSChan->dbrType;
	    if ((i = pSChan->sampInBuf) < 0) {
		pSChan->pFlags[sub].missing = 1;
		pSChan->pDataCodeL[sub] = 'M';
	    }
	    else if (pSChan->inStatus[i] == SYD_B_EOF) {
		pSChan->pFlags[sub].missing = 1;
		pSChan->pFlags[sub].eof = 1;
		pSChan->pDataCodeL[sub] = 'M';
		pSChan->pDataCodeR[sub] = 'E';
	    }
	    else if (pSChan->inStatus[i] == SYD_B_MISSING) {
		pSChan->pFlags[sub].missing = 1;
		pSChan->pDataCodeL[sub] = 'M';
		pSChan->pDataCodeR[sub] = 'N';
	    }
	    else {
/*-----------------------------------------------------------------------------
*	store the channel's alarm status and severity
*----------------------------------------------------------------------------*/
		pSChan->pDataAlStat[sub] = alStat =
					pSChan->pInBuf[i]->tstrval.status;
		pSChan->pDataAlSev[sub] = alSev =
					pSChan->pInBuf[i]->tstrval.severity;
/*-----------------------------------------------------------------------------
*	check out the channel's status, storing it into the status array.
*	If the status indicates a bad value, reset the useVal flag, so
*	that the value won't be used in computing the maximum and minimum
*	for the channel.
*----------------------------------------------------------------------------*/
		useVal = 1;
		if (alSev == INVALID_ALARM) {
		    useVal = 0;
		    pSChan->pDataCodeL[sub] = 'I';
		}
		else if (pSChan->reused) {
		    pSChan->pFlags[sub].filled = 1;
		    pSChan->pDataCodeL[sub] = 'F';
		}
		else if (pSChan->inStatus[i] == SYD_B_SNAP_BEGIN) {
		    pSChan->pFlags[sub].restart = 1;
		    pSChan->pFlags[sub].snapstart = 1;
		    pSChan->pDataCodeL[sub] = 'S';
		}
		else if (pSChan->inStatus[i] == SYD_B_SNAP_SINGLE) {
		    pSChan->pFlags[sub].restart = 1;
		    pSChan->pFlags[sub].snapstart = 1;
		    pSChan->pFlags[sub].snapend = 1;
		    pSChan->pDataCodeL[sub] = 'S';
		}
		else if (pSChan->inStatus[i] == SYD_B_SNAP_END) {
		    pSChan->pFlags[sub].ok = 1;
		    pSChan->pFlags[sub].snapend = 1;
		    pSChan->pDataCodeL[sub] = 'O';
		}
		else if (pSChan->inStatus[i] == SYD_B_RESTART) {
		    pSChan->pFlags[sub].restart = 1;
		    pSChan->pDataCodeL[sub] = 'R';
		}
		else {
		    pSChan->pFlags[sub].ok = 1;
		    pSChan->pDataCodeL[sub] = 'O';
		}
		if (alStat == NO_ALARM)
		    ;	/* no action */
		else if (alStat == READ_ALARM)
		    pSChan->pDataCodeR[sub] = 'R';
		else if (alStat == HIHI_ALARM)
		    pSChan->pDataCodeR[sub] = 'H';
		else if (alStat == HIGH_ALARM)
		    pSChan->pDataCodeR[sub] = 'h';
		else if (alStat == LOLO_ALARM)
		    pSChan->pDataCodeR[sub] = 'L';
		else if (alStat == LOW_ALARM)
		    pSChan->pDataCodeR[sub] = 'l';
		else if (alSev == MAJOR_ALARM)
		    pSChan->pDataCodeR[sub] = 'M';
		else if (alSev == MINOR_ALARM)
		    pSChan->pDataCodeR[sub] = 'm';
/*-----------------------------------------------------------------------------
*	now go ahead and store the sample into the set.  Also adjust the
*	maximum and minimum, as needed.
*----------------------------------------------------------------------------*/
		if (type == DBR_TIME_FLOAT) {
		    float	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tfltval.value;
		    pDest = ((float *)pSChan->pData) + sub * pSChan->elCount;
		    if (useVal && pSChan->minMaxNeedInit) {
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
			pSChan->minMaxNeedInit = 0;
		    }
		    for (el=0; el<(int) pSChan->elCount; el++) {
			if (useVal) {
			    if ((double)*pSrc > pSChan->maxDataVal)
				pSChan->maxDataVal = (double)*pSrc;
			    if ((double)*pSrc < pSChan->minDataVal)
				pSChan->minDataVal = (double)*pSrc;
			}
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_SHORT) {
		    short	*pSrc, *pDest;
		    double	dbl;
		    pSrc = &pSChan->pInBuf[i]->tshrtval.value;
		    pDest = ((short *)pSChan->pData) + sub * pSChan->elCount;
		    if (useVal && pSChan->minMaxNeedInit) {
			if (pSChan->isRVAL)
			    dbl = (double)(*(unsigned short *)pSrc);
			else
			    dbl = (double)(*pSrc);
			pSChan->maxDataVal = pSChan->minDataVal = dbl;
			pSChan->minMaxNeedInit = 0;
		    }
		    for (el=0; el<(int) pSChan->elCount; el++) {
			if (useVal) {
			    if (pSChan->isRVAL)
				dbl = (double)(*(unsigned short *)pSrc);
			    else
				dbl = (double)(*pSrc);
			    if (dbl > pSChan->maxDataVal)
				pSChan->maxDataVal = dbl;
			    if (dbl < pSChan->minDataVal)
				pSChan->minDataVal = dbl;
			}
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_DOUBLE) {
		    double	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tdblval.value;
		    pDest = ((double *)pSChan->pData) + sub * pSChan->elCount;
		    if (useVal && pSChan->minMaxNeedInit) {
			pSChan->maxDataVal = pSChan->minDataVal = *pSrc;
			pSChan->minMaxNeedInit = 0;
		    }
		    for (el=0; el<(int) pSChan->elCount; el++) {
			if (useVal) {
			    if (*pSrc > pSChan->maxDataVal)
				pSChan->maxDataVal = *pSrc;
			    if (*pSrc < pSChan->minDataVal)
				pSChan->minDataVal = *pSrc;
			}
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_LONG) {
		    long	*pSrc, *pDest;
		    double	dbl;
		    pSrc = &pSChan->pInBuf[i]->tlngval.value;
		    pDest = ((long *)pSChan->pData) + sub * pSChan->elCount;
		    if (useVal && pSChan->minMaxNeedInit) {
			if (pSChan->isRVAL)
			    dbl = (double)(*(unsigned long *)pSrc);
			else
			    dbl = (double)(*pSrc);
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
			pSChan->minMaxNeedInit = 0;
		    }
		    for (el=0; el<(int) pSChan->elCount; el++) {
			if (useVal) {
			    if (pSChan->isRVAL)
				dbl = (double)(*(unsigned long *)pSrc);
			    else
				dbl = (double)(*pSrc);
			    if (dbl > pSChan->maxDataVal)
				pSChan->maxDataVal = dbl;
			    if (dbl < pSChan->minDataVal)
				pSChan->minDataVal = dbl;
			}
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_STRING) {
		    char	*pSrc, *pDest;
		    pSrc = pSChan->pInBuf[i]->tstrval.value;
		    pDest = ((char *)pSChan->pData) +
					sub * db_strval_dim * pSChan->elCount;
		    for (el=0; el<(int) pSChan->elCount; el++) {
			strcpy(pDest, pSrc);
			pDest += db_strval_dim;
			pSrc += db_strval_dim;
		    }
		}
		else if (type == DBR_TIME_CHAR) {
		    unsigned char	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tchrval.value;
		    pDest = ((unsigned char *)pSChan->pData) +
						sub * pSChan->elCount;
		    if (useVal && pSChan->minMaxNeedInit) {
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
			pSChan->minMaxNeedInit = 0;
		    }
		    for (el=0; el<(int) pSChan->elCount; el++) {
			if (useVal) {
			    if ((double)*pSrc > pSChan->maxDataVal)
				pSChan->maxDataVal = (double)*pSrc;
			    if ((double)*pSrc < pSChan->minDataVal)
				pSChan->minDataVal = (double)*pSrc;
			}
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_ENUM) {
		    unsigned short	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tenmval.value;
		    pDest = ((unsigned short *)pSChan->pData) + sub * pSChan->elCount;
		    if (useVal && pSChan->minMaxNeedInit) {
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
			pSChan->minMaxNeedInit = 0;
		    }
		    for (el=0; el<(int) pSChan->elCount; el++) {
			if (useVal) {
			    if ((double)*pSrc > pSChan->maxDataVal)
				pSChan->maxDataVal = (double)*pSrc;
			    if ((double)*pSrc < pSChan->minDataVal)
				pSChan->minDataVal = (double)*pSrc;
			}
			*pDest++ = *pSrc++;
		    }
		}
		else
		    assertAlways(0);
	    }
	    if (i >= 0 && pSChan->inStatus[i] != SYD_B_MISSING)
		pSChan->inStatus[i] = SYD_B_SAMPLED;
	    pSChan->restrictMaxDataVal = pSChan->maxDataVal;
	    pSChan->restrictMinDataVal = pSChan->minDataVal;
	}
    }

    if (pSspec->firstData < 0 || pSspec->firstData == pSspec->lastData) {
	if (++pSspec->firstData >= pSspec->dataDim)
	    pSspec->firstData = 0;
	pSspec->restrictFirstData = pSspec->firstData;
    }
    if (pSspec->sampleCount < pSspec->dataDim)
	pSspec->sampleCount++;
    pSspec->restrictLastData = pSspec->lastData;
}

/*+/subr**********************************************************************
* NAME	sydInputSync - synchronize input buffers with disk for `by channel'
*
* DESCRIPTION
*	Get in sync with an archiver `by channel' data file, which may
*	have had information written to it since the sydOpen call or
*	the last sydInputSync call.
*
* RETURNS
*	S_syd_OK, or
*	other error code
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydInputSync(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    long	retStat=S_syd_OK;
    long	stat;

    assert(pSspec != NULL);

    if (pSspec->type != SYD_TY_CF)
	return S_syd_OK;
    stat = (pSspec->pFunc)(pSspec, NULL, SYD_FC_READ_SYNC, NULL);
    if (stat != S_syd_OK)
	retStat = stat;
    return retStat;
}

/*+/subr**********************************************************************
* NAME	sydMonitorStart - start monitoring channels for user monitoring
*
* DESCRIPTION
*	For each channel in the sync set spec, add a Channel Access monitor.
*	The monitor will use the deadband specified by the SYD_ATTR_DEADBAND
*	attribute (which defaults to .ADEL).
*
* RETURNS
*	S_syd_OK
*
* SEE ALSO
*	sydChanOpen, sydSetAttr, sydMonitorStop
*
*-*/
long
sydMonitorStart(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    SYD_CHAN	*pSChan;	/* pointer to channel in Sspec */

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext)
	stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_READ, NULL);
    return retStat;
}

/*+/subr**********************************************************************
* NAME	sydMonitorStop - stop monitoring channels for user monitoring
*
* DESCRIPTION
*	For each channel in the sync set spec, delete the Channel Access
*	monitor.  If the sync set spec isn't being monitored by a user
*	routine, this routine does nothing.
*
* RETURNS
*	S_syd_OK
*
* SEE ALSO
*	sydChanOpen, sydSetAttr, sydMonitorStart
*
*-*/
long
sydMonitorStop(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    SYD_CHAN	*pSChan;	/* pointer to channel in Sspec */

    if (pSspec->monFn != NULL) {
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext)
	    stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_STOP, NULL);
    }
    return retStat;
}

/*+/macro*********************************************************************
* NAME	sydOpen - create and initialize an empty synchronous set spec
*
* DESCRIPTION
*	This routine builds an empty synchronous set spec structure.  In
*	order to use the SYD_SPEC, channels for synchronous samples must
*	be specified.  Retrieval will be for synchronous samples of data,
*	in which all channels have the same time stamp; retrieval begins
*	with the oldest data available and continues to the most recent.
*	Several versions of this routine are available, corresponding
*	to the source of the data:
*
*	sydOpenCA(&pSspec, NULL)	init for Channel Access
*	sydOpenCF(&pSspec, filePath)	init for "by channel" file
*	sydOpenSSF(&pSspec, filePath)	init for "sample set" file
*
* RETURNS
*	S_syd_OK, or
*	S_syd_noMem	if memory isn't available to store set descriptor
*
* BUGS
* o	should set up, in the sync set spec, the number of input buffers,
*	rather than having that job be done in sydChanOpen.
*
* SEE ALSO
*	sydChanOpen, sydAddTest
*	sydInputGet, sydMonitorStart
*
* EXAMPLE
*
*-*/
long
sydOpen(ppSspec)
SYD_SPEC **ppSspec;	/* O pointer to synchronous set spec pointer */
{
    assert(ppSspec != NULL);
    assert(*ppSspec != NULL);

    (*ppSspec)->pChanHead = (*ppSspec)->pChanTail = NULL;
    (*ppSspec)->pAccept = NULL;
    (*ppSspec)->refTs.secPastEpoch = (*ppSspec)->refTs.nsec = 0;
    (*ppSspec)->priorTs = (*ppSspec)->refTs;
    (*ppSspec)->sampleTs = (*ppSspec)->refTs;
    (*ppSspec)->chanCount = 0;
    (*ppSspec)->disconCount = 0;
    (*ppSspec)->needGrCount = 0;
    (*ppSspec)->eofCount = 0;
    (*ppSspec)->dataDim = 0;
    (*ppSspec)->sampleCount = 0;
    (*ppSspec)->pDeltaSec = NULL;
    (*ppSspec)->pTimeStamp = NULL;
    (*ppSspec)->statDim = (*ppSspec)->statCount = 0;
    (*ppSspec)->pStatDeltaSec = NULL;
    (*ppSspec)->pStatTimeStamp = NULL;
    (*ppSspec)->pStatPopCount = NULL;
    (*ppSspec)->pPartial = NULL;
    (*ppSspec)->lastData = (*ppSspec)->firstData = -1;
    (*ppSspec)->restrictLastData = (*ppSspec)->restrictFirstData = -1;
    (*ppSspec)->restrictDeltaSecSubtract = 0.;
    (*ppSspec)->roundNsec = 0;
    (*ppSspec)->deadband = DBE_LOG;
    (*ppSspec)->useStats = (*ppSspec)->useMeans = 0;
    (*ppSspec)->monFn = NULL;
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydPosition - position the data file
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	doesn't report EOF if that condition occurs
* o	doesn't provide a way for the caller to find out the actual position
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long 
sydPosition(pSspec, pStamp)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
TS_STAMP *pStamp;	/* I stamp at which to position; NULL to rewind */
{
    SYD_CHAN	*pSChan;
    long	stat;

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_POSITION, pStamp);
	if (pSspec->type == SYD_TY_SSF)
	    break;
    }
    sydInputReset(pSspec);
    pSspec->sampleTs.secPastEpoch = pSspec->sampleTs.nsec = 0;
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydSampleExport
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	options should be #defined symbols
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydSampleExport(pSspec, out, option, samp)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*out;		/* IO stream pointer for output */
int	option;		/* I filtering option */
			/*   1 spreadsheet; delta time and values */
			/*   2 spreadsheet; delta time and values and status */
			/*   3 spreadsheet; delta time, count, and values */
			/*   4 spreadsheet; as for 3, with status */
int	samp;		/* I sample number in synchronous set */
{
    SYD_CHAN	*pSChan;	/* pointer to channel in synchronous set */
    char	stampText[28];
    int		i;

/*-----------------------------------------------------------------------------
* generate headings, depending on option:
* 1==>	"mm/dd/yy hh:mm:ss.msc"
*	date	time	name1	name2	...
*	stamp	seconds	egu	egu	...
*
* 2==>	"mm/dd/yy hh:mm:ss.msc"
*	date	time	stat	name1	stat	name2	...
*	stamp	seconds	stat	egu	stat	egu	...
*
* 3==>	"mm/dd/yy hh:mm:ss.msc"
*	date	time	nEl	name1	nEl	name2	...
*	stamp	seconds	nEl	egu	nEl	egu	...
*
* 4==>	"mm/dd/yy hh:mm:ss.msc"
*	date	time	stat	nEl	name1	stat	nEl	name2	...
*	stamp	seconds	stat	nEl	egu	stat	nEl	egu	...
*----------------------------------------------------------------------------*/
    if (samp == pSspec->restrictFirstData) {
	(void)fprintf(out, "\"%s\"\n\"date\"\t\"time\"", tsStampToText(
		    &pSspec->restrictRefTs, TS_TEXT_MMDDYY, stampText));
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan = pSChan->pNext) {
	    if (pSChan->dataChan) {
		if (option == 2 || option == 4)
		    (void)fprintf(out, "\t\"stat\"");
		if (option == 3 || option == 4)
		    (void)fprintf(out, "\t\"nEl\"");
		(void)fprintf(out, "\t\"%s\"", pSChan->name);
	    }
	}
	(void)fprintf(out, "\n\"stamp\"\t\"seconds\"");
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan = pSChan->pNext) {
	    if (pSChan->dataChan) {
		if (option == 2 || option == 4)
		    (void)fprintf(out, "\t\"stat\"");
		if (option == 3 || option == 4)
		    (void)fprintf(out, "\t\"nEl\"");
		(void)fprintf(out, "\t\"%s\"", pSChan->EGU);
	    }
	}
	(void)fprintf(out, "\n");
    }

/*-----------------------------------------------------------------------------
*    print the value for each channel for this sample.
*----------------------------------------------------------------------------*/
    tsStampToText(&pSspec->pTimeStamp[samp], TS_TEXT_MMDDYY, stampText);
    (void)fprintf(out, "\"%s\"%c%.3f", stampText,'\t',
		pSspec->pDeltaSec[samp] - pSspec->restrictDeltaSecSubtract);
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->dataChan) {
	    if (option == 1)
		sydSamplePrint1(pSChan, out, '\t', USE_QUO, 0, samp);
	    else if (option == 2)
		sydSamplePrint1(pSChan, out, '\t', PRE_FL|USE_QUO, 0, samp);
	    else if (option == 3)
		sydSamplePrint1(pSChan, out, '\t', USE_QUO|SHOW_AR, 0, samp);
	    else if (option == 4)
		sydSamplePrint1(pSChan,out,'\t',PRE_FL|USE_QUO|SHOW_AR,0,samp);
	}
    }
    (void)fprintf(out, "\n");
    pSChan = pSspec->pChanHead;
    if (pSChan->pFlags[samp].snapend && !pSChan->pFlags[samp].snapstart)
	(void)fprintf(out, "\n");	/* blank line after snapshot */
}

/*+/subr**********************************************************************
* NAME	sydSampleExportStats
*
* DESCRIPTION
*	If the `useMeans' flag is set in conjunction with `useStats', then
*	only the means are printed.
*
* RETURNS
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydSampleExportStats(pSspec, out, snap)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*out;		/* IO stream pointer for output */
int	snap;		/* I snapshot number in synchronous set */
{
    SYD_CHAN	*pSChan;	/* pointer to channel in synchronous set */
    char	stampText[28];
    double	dblVal;

/*-----------------------------------------------------------------------------
* generate heading
*	"mm/dd/yy hh:mm:ss.msc"
*	date	time	sample	name1	egu	name2	egu	...
*	stamp	seconds	count	mean	stdDev	mean	stdDev	...
*
*	"mm/dd/yy hh:mm:ss.msc"
*	date	time	sample	name1	name2	...
*	stamp	seconds	count	egu	egu	...
*----------------------------------------------------------------------------*/
    if (snap == 0 && pSspec->useMeans == 0) {
	(void)fprintf(out, "\"%s\"\n\"date\"\t\"time\"\t\"sample\"",
		tsStampToText(&pSspec->restrictRefTs,TS_TEXT_MMDDYY,stampText));
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan = pSChan->pNext) {
	    if (pSChan->dataChan)
		(void)fprintf(out,"\t\"%s\"\t\"%s\"",pSChan->name,pSChan->EGU);
	}
	(void)fprintf(out, "\n\"stamp\"\t\"seconds\"\t\"count\"");
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan = pSChan->pNext) {
	    if (pSChan->dataChan)
		(void)fprintf(out, "\t\"mean\"\t\"stdDev\"");
	}
	(void)fprintf(out, "\n");
    }
    if (snap == 0 && pSspec->useMeans) {
	(void)fprintf(out, "\"%s\"\n\"date\"\t\"time\"\t\"sample\"",
		tsStampToText(&pSspec->restrictRefTs,TS_TEXT_MMDDYY,stampText));
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan = pSChan->pNext) {
	    if (pSChan->dataChan)
		(void)fprintf(out,"\t\"%s\"",pSChan->name);
	}
	(void)fprintf(out, "\n\"stamp\"\t\"seconds\"\t\"count\"");
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan = pSChan->pNext) {
	    if (pSChan->dataChan)
		(void)fprintf(out,"\t\"%s\"",pSChan->EGU);
	}
	(void)fprintf(out, "\n");
    }

/*-----------------------------------------------------------------------------
*    print the statistics for each channel for this snapshot.
*----------------------------------------------------------------------------*/
    tsStampToText(&pSspec->pStatTimeStamp[snap], TS_TEXT_MMDDYY, stampText);
    (void)fprintf(out, "\"%s\"\t%.3f", stampText,
	pSspec->pStatDeltaSec[snap] - pSspec->restrictDeltaSecSubtract);
    (void)fprintf(out, "\t%d", pSspec->pStatPopCount[snap]);
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->dataChan) {
	    dblVal = pSChan->pStats[snap].mean;
	    if (dblVal == 0.)
		(void)fprintf(out, "\t0");
	    else if (dblVal > -.1 && dblVal < .1)
		(void)fprintf(out, "\t%.*E", pSChan->precision, dblVal);
	    else
		(void)fprintf(out, "\t%.*f", pSChan->precision, dblVal);
	    if (pSspec->useMeans == 0) {
		dblVal = pSChan->pStats[snap].stdDev;
		if (dblVal == 0.)
		    (void)fprintf(out, "\t0");
		else if (dblVal > -.1 && dblVal < .1)
		    (void)fprintf(out, "\t%.*E", pSChan->precision, dblVal);
		else
		    (void)fprintf(out, "\t%.*f", pSChan->precision, dblVal);
	    }
	}
    }
    (void)fprintf(out, "\n");
}

/*+/internal******************************************************************
* NAME	widthPrint
*
*-*/
static void
widthPrint(out, pre, text, width, pass, pMore)
FILE	*out;
char	*pre, *text;
int	width, pass;
int	*pMore;
{
    int		L=strlen(text);
    int		sub=pass*width;

    if (pre[0] != '\0')
	(void)fprintf(out, "%s", pre);
    if (sub < L) {
	if (pass == 0 && L <= width)
	    (void)fprintf(out, "%*.*s", width, width, &text[sub]);
	else
	    (void)fprintf(out, "%-*.*s", width, width, &text[sub]);
    }
    else
	(void)fprintf(out, "%*s", width, " ");
    if (sub + width < L)
	*pMore = 1;
}

/*+/subr**********************************************************************
* NAME	sydSamplePrint
*
* DESCRIPTION
*	Print the values for all channels in a particular sample.  Various
*	aspects of formatting can be specified with arguments, including
*	the printing of column headings, number of columns, column width,
*	and the inclusion of printer control characters.
*
*	For array channels, only the first value of the array is printed
*	on the line with the time stamp--the full array is printed on
*	following lines.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydSamplePrint(pSspec, out, formatFlag, headerFlag, nCol, colWidth, samp)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*out;		/* I stream pointer for output */
int	formatFlag;	/* I ==1 causes page formatting for printing */
int	headerFlag;	/* I ==1 causes printing of column headings */
int	nCol;		/* I >0 specifies number of table columns,
				with folded lines to accomodate excess
				channels */
int	colWidth;	/* I >0 specifies column width, in characters */
int	samp;		/* I sample number in synchronous set */
{
    SYD_CHAN	*pSChan;	/* pointer to channel in synchronous set */
    char	stampText[28];
    int		i, tmpWid;
    int		more, pass, colNum;
    SYD_CHAN	*pNext;

    if (colWidth < 1)
	colWidth = 10;

/*-----------------------------------------------------------------------------
*    print a heading line with channel names; if this isn't the first page,
*    put a ^L prior to the heading.
*----------------------------------------------------------------------------*/
#define FMT_INDENT if (formatFlag) (void)fprintf(out, "    ")
#define SKIP_STAMP (void)fprintf(out, "  %21s", " ")

    if (headerFlag) {
	if (formatFlag) {
	    if (samp != pSspec->restrictFirstData)
		(void)fprintf(out, "\f");
	    (void)fprintf(out, "\n\n");		/* 2 line top margin */
	}
	pNext = pSspec->pChanHead;
	tmpWid = colWidth;
	if (tmpWid <= 2)
	    tmpWid = 1;
	while (1) {
	    pass = 0;
	    while (1) {
		more = 0;
		pSChan = pNext;
		FMT_INDENT; SKIP_STAMP;
		for (colNum=0; colNum<nCol; colNum++) {
		    if (pSChan->dataChan)
			widthPrint(out, " ", pSChan->name,tmpWid,pass,&more);
		    pSChan = pSChan->pNext;
		    if (pSChan == NULL)
			break;
		}
		(void)fprintf(out, "\n");
		if (more == 0)
		    break;
		pass++;
	    }
	    if (tmpWid > 2) {
		pass = 0;
		while (1) {
		    more = 0;
		    pSChan = pNext;
		    FMT_INDENT; SKIP_STAMP;
		    for (colNum=0; colNum<nCol; colNum++) {
			if (pSChan->dataChan)
			    widthPrint(out," ",pSChan->EGU,tmpWid,pass,&more);
			pSChan = pSChan->pNext;
			if (pSChan == NULL)
			    break;
		    }
		    (void)fprintf(out, "\n");
		    if (more == 0)
			break;
		    pass++;
		}
	    }
	    (void)fprintf(out, "\n");
	    if ((pNext = pSChan) == NULL)
		break;
	}
    }
/*-----------------------------------------------------------------------------
*    print the value for each channel for this sample.  Print the status
*    flags following values, but print only the first element for array
*    channels.
*----------------------------------------------------------------------------*/
    FMT_INDENT;
    if (pSspec->pPartial[samp])	(void)fprintf(out, "*");
    else			(void)fprintf(out, " ");
    (void)fprintf(out, " %s", tsStampToText(
		    &pSspec->pTimeStamp[samp], TS_TEXT_MMDDYY, stampText));
    colNum = 0;
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->dataChan) {
	    if (colNum >= nCol) {
		(void)fprintf(out, "\n");
		FMT_INDENT; SKIP_STAMP;
		colNum = 0;
	    }
	    sydSamplePrint1(pSChan, out, ' ', POST_FL|ENF_WID, colWidth, samp);
	    colNum++;
	}
    }
    (void)fprintf(out, "\n");
/*-----------------------------------------------------------------------------
*    if any of the channels is an array channel, print the full array following
*    the time-stamped line (which had only the first value)
*----------------------------------------------------------------------------*/
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->dataChan && pSChan->elCount > 1) {
	    (void)fprintf(out, "%s", pSChan->name);
	    for (i=0; i<(int) pSChan->elCount; i++) {
		if (i%nCol == 0)
		    (void)fprintf(out, "\n%5d", i);
		(void)fputc(' ', out);
		sydSamplePrintVal(out, pSChan, samp, i, ENF_WID, colWidth);
	    }
	    (void)fprintf(out, "\n");
	}
    }

    return S_syd_OK;
}

/*+/internal******************************************************************
* NAME	sydSamplePrint1 - print a value for a channel
*
* DESCRIPTION
*
* RETURNS
*	void
*
*-*/
static void
sydSamplePrint1(pSChan, out, sep, flags, colWidth, sampNum)
SYD_CHAN *pSChan;	/* I pointer to sync channel */
FILE	*out;		/* I file pointer for writing value */
char	sep;		/* I character to use a a prefix for each field,
				as a separator; usually ' ' or '\t' */
int	flags;		/* I flags, or'd together, or 0:
		PRE_FL     print status flag, prior to value
		POST_FL    print status flag, after value
		SHOW_AR    print all array values, preceded by
				array element count
		ENF_WID    enforce column width
		USE_QUO    put quotes around text items
			*/
int	colWidth;	/* I >0 specifies column width, in characters;
				== 2 requests only printing status code*/
int	sampNum;	/* I sample number in sync set */
{
    int		i;
    chtype	type;		/* type of value */
    char	text[100];
    char	*special;	/* != NULL says print "msg", not value */

    type = pSChan->dbrType;

    if (colWidth == 2) {
	if (pSChan->pData == NULL)
	    (void)fprintf(out, " %c", sep,'M');
	else
	    (void)fprintf(out, " %c", pSChan->pDataCodeL[sampNum]);
	return;
    }
    if (flags&PRE_FL || flags&POST_FL) {
	if (colWidth > 3)
	    colWidth -= 3;
    }

    if (flags&PRE_FL) {
	if (pSChan->pData == NULL) {
	    if (flags&USE_QUO)
		(void)fprintf(out, "%c\"%c%c\"", sep,'M','D');
	    else
		(void)fprintf(out, "%c%c%c", sep,'M','D');
	}
	else {
	    if (flags&USE_QUO) {
		(void)fprintf(out, "%c\"%c%c\"", sep,
		    pSChan->pDataCodeL[sampNum], pSChan->pDataCodeR[sampNum]);
	    }
	    else {
		(void)fprintf(out, "%c%c%c", sep,
		    pSChan->pDataCodeL[sampNum], pSChan->pDataCodeR[sampNum]);
	    }
	}
    }
    (void)fputc(sep, out);
    special = NULL;
    if (pSChan->pData == NULL)
	special = "no_data";
    else if (pSChan->pFlags[sampNum].eof)
	special = "EOF";
    else if (pSChan->pFlags[sampNum].missing)
	special = "no_data";
    else if (pSChan->pDataCodeL[sampNum] == 'I' &&
				flags&PRE_FL == 0 && flags&POST_FL == 0) {
	special = "invalid";
    }

    if (special == NULL) {
	if (flags&SHOW_AR) {
	    (void)fprintf(out, "%d", pSChan->elCount);
	    for (i=0; i<(int) pSChan->elCount; i++) {
		(void)fputc(sep, out);
		sydSamplePrintVal(out, pSChan, sampNum, i, flags, colWidth);
	    }
	}
	else
	    sydSamplePrintVal(out, pSChan, sampNum, 0, flags, colWidth);
    }
    else {
	if (flags&SHOW_AR)
	    (void)fprintf(out, "1%c", sep);
	if (flags&USE_QUO) {
	    if (flags&ENF_WID)
		(void)fprintf(out, "\"%*.*s\"", colWidth, colWidth, special);
	    else
		(void)fprintf(out, "\"%s\"", special);
	}
	else {
	    if (flags&ENF_WID)
		(void)fprintf(out, "%*.*s", colWidth, colWidth, special);
	    else
		(void)fprintf(out, "%s", special);
	}
    }

    if (flags&POST_FL) {
	if (pSChan->pData == NULL) {
	    if (flags&USE_QUO)
		(void)fprintf(out, "%c\"%c%c\"", sep,'M','D');
	    else
		(void)fprintf(out, "%c%c%c", sep,'M','D');
	}
	else {
	    if (flags&USE_QUO) {
		(void)fprintf(out, "%c\"%c%c\"", sep,
		    pSChan->pDataCodeL[sampNum], pSChan->pDataCodeR[sampNum]);
	    }
	    else {
		(void)fprintf(out, "%c%c%c", sep,
		    pSChan->pDataCodeL[sampNum], pSChan->pDataCodeR[sampNum]);
	    }
	}
    }
}

/*+/internal******************************************************************
* NAME	sydSamplePrintVal - print a single value
*
*-*/
static void
sydSamplePrintVal(out, pSChan, sampNum, sub, flags, colWidth)
FILE	*out;
SYD_CHAN *pSChan;
int	sampNum;
int	sub;		/* subscript for array channels */
int	flags;
int	colWidth;
{
    int		myType=0;		/* 0,1,2 for lng, dbl, str */
    chtype	type;		/* type of value */
    char	text[100];
    long	lngVal=0;
    double	dblVal=0.0;
    char	*strVal=NULL;

    type = pSChan->dbrType;

    if (type == DBR_TIME_STRING) {
	myType = 2;
	strVal = ((char *)pSChan->pData) +
	    sampNum * db_strval_dim * pSChan->elCount + sub * db_strval_dim;
    }
    else if (type == DBR_TIME_FLOAT) {
	float	*p;
	myType = 1;
	p = ((float *)pSChan->pData) + sampNum * pSChan->elCount + sub;
	dblVal = (double)(*p);
    }
    else if (type == DBR_TIME_SHORT) {
	short	*p;
	myType = 0;
	p = ((short *)pSChan->pData) + sampNum * pSChan->elCount + sub;
	if (pSChan->isRVAL)
	    lngVal = (long)(*(unsigned short *)p);
	else
	    lngVal = (long)(*p);
    }
    else if (type == DBR_TIME_DOUBLE) {
	double	*p;
	myType = 1;
	p = ((double *)pSChan->pData) + sampNum * pSChan->elCount + sub;
	dblVal = *p;
    }
    else if (type == DBR_TIME_LONG) {
	long	*p;
	myType = 0;
	p = ((long *)pSChan->pData) + sampNum * pSChan->elCount + sub;
	if (pSChan->isRVAL)
	    lngVal = *(unsigned long *)p;
	else
	    lngVal = *p;
    }
    else if (type == DBR_TIME_CHAR) {
	char	*p;
	myType = 0;
	p = ((char *)pSChan->pData) + sampNum * pSChan->elCount + sub;
	lngVal = (long)(*p);
    }
    else if (type == DBR_TIME_ENUM) {
	short	*p;
	myType = 0;
	p = ((short *)pSChan->pData) + sampNum * pSChan->elCount + sub;
	lngVal = (long)(*p);
	if (lngVal < pSChan->grBuf.genmval.no_str)
	    myType = 2, strVal = pSChan->grBuf.genmval.strs[lngVal];
    }

    if (flags&ENF_WID) {
	if (myType == 0)
	    strVal=text, cvtLngToTxt(text, colWidth, lngVal);
	else if (myType == 1)
	    strVal=text,cvtDblToTxt(text, colWidth, dblVal, pSChan->precision);
	(void)fprintf(out, "%*.*s", colWidth, colWidth, strVal);
    }
    else {
	if (myType == 0)
	    (void)fprintf(out, "%*d", colWidth, lngVal);
	else if (myType == 1) {
	    if (dblVal == 0.)
		(void)fprintf(out, "0");
	    else if (dblVal > -.1 && dblVal < .1)
		(void)fprintf(out, "%*.*E",colWidth,pSChan->precision,dblVal);
	    else
		(void)fprintf(out, "%*.*f",colWidth,pSChan->precision,dblVal);
	}
	else
	    (void)fprintf(out, "%*s", colWidth, strVal);
    }
}

/*+/internal******************************************************************
* NAME	sydSamplePrintStats
*
* DESCRIPTION
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long sydSamplePrintStats(pSspec, out, formatFlag, headerFlag, nCol, colWidth, snap)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*out;		/* I stream pointer for output */
int	formatFlag;	/* I ==1 causes page formatting for printing */
int	headerFlag;	/* I ==1 causes printing of column headings */
int	nCol;		/* I >0 specifies number of table columns,
				with folded lines to accomodate excess
				channels */
int	colWidth;	/* I >0 specifies column width, in characters */
int	snap;		/* I snapshot number in synchronous set */
{
    SYD_CHAN	*pSChan;	/* pointer to channel in synchronous set */
    char	stampText[28];
    int		i;
    int		prec;
    char	text[100];
    int		more, pass, colNum;
    SYD_CHAN	*pNext;


    if (colWidth < 6)
	colWidth = 6;

/*-----------------------------------------------------------------------------
*    print a heading line with channel names; if this isn't the first page,
*    put a ^L prior to the heading.
*----------------------------------------------------------------------------*/
    if (headerFlag) {
	if (formatFlag) {
	    if (snap != 0)
		(void)fprintf(out, "\f");
	    (void)fprintf(out, "\n\n");		/* 2 line top margin */
	}
#undef SKIP_STAMP
#define SKIP_STAMP (void)fprintf(out, "  %21s %4s", " ", " ")
	pNext = pSspec->pChanHead;
	while (1) {
	    pass = 0;
	    while (1) {
		more = 0;
		pSChan = pNext;
		FMT_INDENT; SKIP_STAMP;
		for (colNum=0; colNum<nCol; colNum++) {
		    if (pSChan->dataChan)
			widthPrint(out, " ", pSChan->name,colWidth,pass,&more);
		    if (pSChan->dataChan && pSspec->useMeans == 0)
			widthPrint(out, " ", pSChan->EGU,colWidth,pass,&more);
		    pSChan = pSChan->pNext;
		    if (pSChan == NULL)
			break;
		}
		(void)fprintf(out, "\n");
		if (more == 0)
		    break;
		pass++;
	    }
	    if (pSspec->useMeans == 0)
		(void)fprintf(out, "\n");
	    if ((pNext = pSChan) == NULL)
		break;
	}
	pSChan = pSspec->pChanHead;
	FMT_INDENT; SKIP_STAMP;
	for (colNum=0; colNum<nCol; colNum++) {
	    if (pSChan->dataChan && pSspec->useMeans == 0) {
		(void)fprintf(out, " %*s %*s", colWidth, "mean",
				    colWidth, "stdDev");
	    }
	    else if (pSChan->dataChan && pSspec->useMeans)
		(void)fprintf(out, " %*s", colWidth, pSChan->EGU);
	    pSChan = pSChan->pNext;
	    if (pSChan == NULL)
		break;
	}
	(void)fprintf(out, "\n");
	if (pSspec->useMeans == 0) {
	    pSChan = pSspec->pChanHead;
	    FMT_INDENT; SKIP_STAMP;
	    for (colNum=0; colNum<nCol; colNum++) {
		if (pSChan->dataChan) {
		    fprintf(out, " %.*s", colWidth*2+1,
			"--------------------------------------------------");
		}
		pSChan = pSChan->pNext;
		if (pSChan == NULL)
		    break;
	    }
	}
	(void)fprintf(out, "\n");
    }
/*-----------------------------------------------------------------------------
*    print the stats for each channel for this snapshot.
*----------------------------------------------------------------------------*/
    FMT_INDENT;
    (void)fprintf(out, " ");
    (void)fprintf(out, " %s", tsStampToText(
		    &pSspec->pStatTimeStamp[snap], TS_TEXT_MMDDYY, stampText));
    (void)fprintf(out, " %4d", pSspec->pStatPopCount[snap]);
    colNum = 0;
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->dataChan) {
	    if (colNum >= nCol) {
		(void)fprintf(out, "\n");
		FMT_INDENT; SKIP_STAMP;
		colNum = 0;

	    }
	    prec = pSChan->precision;
	    cvtDblToTxt(text, colWidth, pSChan->pStats[snap].mean, prec);
	    fprintf(out, " %*.*s", colWidth, colWidth, text);
	    if (pSChan->dataChan && pSspec->useMeans == 0) {
		cvtDblToTxt(text, colWidth, pSChan->pStats[snap].stdDev, prec);
		fprintf(out, " %*.*s", colWidth, colWidth, text);
	    }
	    colNum++;
	}
    }
    (void)fprintf(out, "\n");

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydSampleWriteSSF
*
* DESCRIPTION
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	
*
* SEE ALSO
*
* EXAMPLE
*	sydSampleWriteSSF(pSspec, out, "PROGRAM: rfqTScan", "sample button",
*			sampNum);
*
*-*/
long
sydSampleWriteSSF(pSspec, pFile, reqDesc, sampDesc, samp)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*pFile;		/* I stream pointer for writing */
char	*reqDesc;	/* I description of "request", or NULL */
char	*sampDesc;	/* I description for "sample", or NULL */
int	samp;		/* I sample number in synchronous set */
{
    SYD_CHAN	*pSChan;	/* pointer to channel in synchronous set */
    char	stampText[28];

    if (reqDesc != NULL)
	(void)fprintf(pFile, "%s\n", reqDesc);
    (void)fprintf(pFile, "SAMPLE: at %s", tsStampToText(
		    &pSspec->pTimeStamp[samp], TS_TEXT_MMDDYY, stampText));
    if (sampDesc != NULL)
	(void)fprintf(pFile, "--%s", sampDesc);
    (void)fprintf(pFile, "\n%%endHeader%%\n");

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->dataChan) {
	    (void)fprintf(pFile, "%s %s", SydChanName(pSChan),
		dbf_type_to_text(SydChanDbfType(pSChan)));
	    (void)fprintf(pFile, " %d",
		pSChan->pDataAlSev != NULL ? pSChan->pDataAlSev[samp] : 0);
	    (void)fprintf(pFile, " %d",
		pSChan->pDataAlStat != NULL ? pSChan->pDataAlStat[samp] : 0);
	    sydSamplePrint1(pSChan, pFile, ' ', SHOW_AR, 0, samp);
	    (void)fprintf(pFile, "\n");
	}
    }
    (void)fprintf(pFile, "%%endData%%\n");
    (void)fflush(pFile);

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydSampleSetAlloc - acquire memory for holding a sample set
*
* DESCRIPTION
*	Conditionally allocates memory for holding a sample set.  If memory
*	has already been allocated to hold at least the requested number
*	of samples, then nothing is done.
*
*	This routine must be called prior to calling other sydSampleSet...
*	routines.  (But AFTER calling sydChanOpen for all channels of
*	interest!)
*
*	If memory has already been allocated, but it isn't sufficient to
*	hold the requested number of samples, then that memory is free'd
*	and larger memory is allocated.
*
*	The allocated memory is automatically free'd by sydClose.
*
* RETURNS
*	S_syd_OK, or
*	S_syd_noMem if sufficient memory is unavailable
*
* BUGS
* o	text
*
* SEE ALSO
*	sydClose
*
* EXAMPLE
*
*-*/
long
sydSampleSetAlloc(pSspec, reqCount)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
int	reqCount;	/* I number of samples in the set */
{
    SYD_CHAN	*pSChan;

    if (reqCount > pSspec->dataDim)
	sydSampleSetFree(pSspec);
    if (pSspec->dataDim <= 0) {
	pSspec->pDeltaSec = (double *)malloc(reqCount * sizeof(double));
	if (pSspec->pDeltaSec == NULL)
	    goto sampleMallocErr;
	pSspec->pTimeStamp = (TS_STAMP *)malloc(reqCount * sizeof(TS_STAMP));
	if (pSspec->pTimeStamp == NULL)
	    goto sampleMallocErr;
	pSspec->pPartial = (char *)malloc(reqCount * sizeof(char));
	if (pSspec->pPartial == NULL)
	    goto sampleMallocErr;
	pSspec->dataDim = reqCount;
    }

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->conn == 0)
	    ;			/* no action if never been connected */
	else if (pSChan->pData == NULL) {
	    pSChan->pData = (void *)malloc(reqCount *
			dbr_value_size[pSChan->dbrType] * pSChan->elCount);
	    if (pSChan->pData == NULL)
		goto sampleMallocErr;

	    pSChan->pDataAlStat = (void *)malloc(reqCount *
						sizeof(*pSChan->pDataAlStat));
	    if (pSChan->pDataAlStat == NULL)
		goto sampleMallocErr;
	    pSChan->pDataAlSev = (void *)malloc(reqCount *
						sizeof(*pSChan->pDataAlSev));
	    if (pSChan->pDataAlSev == NULL)
		goto sampleMallocErr;
	    pSChan->pDataCodeL = (void *)malloc(reqCount *
						sizeof(*pSChan->pDataCodeL));
	    if (pSChan->pDataCodeL == NULL)
		goto sampleMallocErr;
	    pSChan->pDataCodeR = (void *)malloc(reqCount *
						sizeof(*pSChan->pDataCodeR));
	    if (pSChan->pDataCodeR == NULL)
		goto sampleMallocErr;
	    pSChan->pFlags = (void *)malloc(reqCount *
						sizeof(*pSChan->pFlags));
	    if (pSChan->pFlags == NULL)
		goto sampleMallocErr;
	}
    }
    pSspec->reqCount = reqCount;
    pSspec->firstData = pSspec->lastData = -1;
    pSspec->sampleCount = 0;
    pSspec->refTs.secPastEpoch = pSspec->refTs.nsec = 0;
    pSspec->restrictFirstData = pSspec->restrictLastData = -1;
    pSspec->restrictRefTs = pSspec->refTs;
    pSspec->restrictDeltaSecSubtract = 0.;
    pSChan = pSspec->pChanHead;

    return S_syd_OK;

sampleMallocErr:
    (void)printf("couldn't get memory to store data\n");
    sydSampleSetFree(pSspec);
    return S_syd_noMem;
}

/*+/subr**********************************************************************
* NAME	sydSampleSetExport - export sample set data to output
*
* DESCRIPTION
*	Exports the sample set data with the desired filtering.
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
long 
sydSampleSetExport(pSspec, out, option)
FILE	*out;		/* I stream pointer for output */
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
int	option;		/* I filtering option */
{
    int		samp;		/* sample number in synchronous set */

    if (pSspec->useStats) {
	for (samp=0; samp<pSspec->statCount; samp++)
	    sydSampleExportStats(pSspec, out, samp);
	return S_syd_OK;
    }
    samp = pSspec->restrictFirstData;
    while (samp >= 0) {
	    sydSampleExport(pSspec, out, option, samp);
	if (samp == pSspec->restrictLastData)
	    samp = -1;
	else if (++samp >= pSspec->dataDim)
	    samp = 0;
    }
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydSampleSetFree - free the array storage for holding a sample set
*
* DESCRIPTION
*	Free's the sample set arrays for a synchronous sample spec.  The
*	underlying, channel-dependent items in the synchronous sample spec
*	aren't altered.
*
*	This routine is intended to be called when the number of synchronous
*	samples is increased--free the current sample set storage using this
*	routine and then allocate new storage using sydSampleSetAlloc.
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
long
sydSampleSetFree(pSspec)
SYD_SPEC *pSspec;
{
    SYD_CHAN	*pSChan;

    assert(pSspec != NULL);

    if (pSspec->pDeltaSec != NULL) {
	free((char *)pSspec->pDeltaSec);
	pSspec->pDeltaSec = NULL;
    }
    if (pSspec->pTimeStamp != NULL) {
	free((char *)pSspec->pTimeStamp);
	pSspec->pTimeStamp = NULL;
    }
    if (pSspec->pPartial != NULL) {
	free((char *)pSspec->pPartial);
	pSspec->pPartial = NULL;
    }
    if (pSspec->pStatTimeStamp != NULL) {
	free((char *)pSspec->pStatTimeStamp);
        pSspec->pStatTimeStamp = NULL;
    }
    if (pSspec->pStatDeltaSec != NULL) {
	free((char *)pSspec->pStatDeltaSec);
        pSspec->pStatDeltaSec = NULL;
    }
    if (pSspec->pStatPopCount != NULL) {
	free((char *)pSspec->pStatPopCount);
        pSspec->pStatPopCount = NULL;
    }
    pSspec->dataDim = 0;
    pSspec->sampleCount = 0;
    pSspec->firstData = pSspec->lastData = -1;
    pSspec->restrictFirstData = pSspec->restrictLastData = -1;
    pSspec->statDim = pSspec->statCount = 0;

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext)
	sydChanFreeArrays(pSChan);
    return S_syd_OK;
}


/*+/subr**********************************************************************
* NAME	sydSampleSetGet - get the samples for a sample set
*
* DESCRIPTION
*
* RETURNS
*	S_syd_OK, or
*	S_syd_EOF	if end of file is encountered, or
*	S_syd_chanNotConn if none of the channels is connected
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydSampleSetGet(pSspec)
SYD_SPEC *pSspec;
{
    long	stat=S_syd_OK;
    SYD_CHAN	*pSChan;
    int		n=0;

    assert(pSspec != NULL);

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->discon == 0)
	    n++;
	sydChanPrep(pSspec, pSChan);
    }

    pSspec->firstData = pSspec->lastData = -1;
    pSspec->restrictFirstData = pSspec->restrictLastData = -1;
    pSspec->restrictDeltaSecSubtract = 0.;
    pSspec->sampleCount = 0;

    if (n <= 0) {
	(void)printf("sydSampleSetGet: none of the channels is connected\n");
	return S_syd_chanNotConn;
    }

    while (stat != S_syd_EOF && pSspec->sampleCount < pSspec->reqCount) {
	if ((stat = sydInputGet(pSspec, NULL)) != S_syd_EOF)
	    sydInputStoreInSet(pSspec, 0);
    }
    return stat;
}

/*+/subr**********************************************************************
* NAME	sydSampleSetPrint - print the samples in a sample set
*
* DESCRIPTION
*
*	If the `useStats' flag is set in the sync set spec, then means
*	and standard deviations are printed instead of the actual data
*	values.  (The caller must already have called sydSampleSetStats.)
*
*	If the `useMeans' flag is set in conjunction with `useStats', then
*	only the means are printed.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long 
sydSampleSetPrint(pSspec, out, formatFlag, nCol, colWidth)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*out;		/* IO stream pointer for output */
int	formatFlag;	/* I ==1 causes page formatting for printing */
int	nCol;		/* I >0 causes that many table columns, folded lines */
int	colWidth;	/* I >0 specifies column width, in characters */
{
    int		samp;		/* sample number in synchronous set */
    int		lineCount=0;
    int		headerFlag=1;

    assert(pSspec != NULL);

    if (pSspec->useStats) {
	for (samp=0; samp<pSspec->statCount; samp++) {
	    if (lineCount == 0 && nCol <= 0)
		headerFlag = 1;
	    sydSamplePrintStats(pSspec, out,
			formatFlag, headerFlag, nCol, colWidth, samp);
	    headerFlag = 0;
	    if (++lineCount > 60)
		lineCount = 0;
	}
	return S_syd_OK;
    }
    samp = pSspec->restrictFirstData;
    while (samp >= 0) {
	if (lineCount == 0 && nCol <= 0)
	    headerFlag = 1;
	sydSamplePrint(pSspec,out,formatFlag,headerFlag,nCol,colWidth,samp);
	headerFlag = 0;
	if (++lineCount > 60)
	    lineCount = 0;
	if (samp == pSspec->restrictLastData)
	    samp = -1;
	else if (++samp >= pSspec->dataDim)
	    samp = 0;
    }
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydSampleSetRestrict - set for processing restricted range of time
*
* DESCRIPTION
*	Sets up for processing a restricted range of the samples in the
*	sample set, with the restriction based on time.  The range starts
*	with the first sample whose stamp is >= the desired begin stamp.
*	The range ends with the last sample whose stamp is <= the desired
*	end stamp.
*
*	If the begin stamp pointer is NULL, then the range starts with
*	the first sample.  The end stamp pointer is treated similarly.
*
* RETURNS
*	S_syd_OK, or
*	S_syd_ERROR if 1) begin stamp is after time range in set; or
*		2) end stamp is before time range in set; or 3) begin
*		stamp is equal to or after end stamp
*
*-*/
long
sydSampleSetRestrict(pSspec, pTsBegin, pTsEnd)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
TS_STAMP *pTsBegin;	/* I pointer to stamp to begin, or NULL */
TS_STAMP *pTsEnd;	/* I pointer to stamp to end, or NULL */
{
    SYD_CHAN	*pSChan;
    TS_STAMP	tsBegin, tsEnd;
    int		i, beg=-1, end=-1, first;
    int		statCount=0, sampCount;
    double	val;

    assert(pSspec != NULL);
    if (pSspec->sampleCount <= 1)
	return S_syd_OK;

    if (pTsBegin == NULL) tsBegin = pSspec->pTimeStamp[pSspec->firstData];
    else		  tsBegin = *pTsBegin;
    if (pTsEnd == NULL)   tsEnd = pSspec->pTimeStamp[pSspec->lastData];
    else		  tsEnd = *pTsEnd;
    if (TsCmpStampsGT(&tsBegin, &pSspec->pTimeStamp[pSspec->lastData]))
	return S_syd_ERROR;
    if (TsCmpStampsLT(&tsEnd, &pSspec->pTimeStamp[pSspec->firstData]))
	return S_syd_ERROR;
    if (TsCmpStampsLE(&tsEnd, &tsBegin))
	return S_syd_ERROR;

/*-----------------------------------------------------------------------------
*	find the appropriate spots to start and end
*----------------------------------------------------------------------------*/
    i = pSspec->firstData;
    while (beg < 0) {
	if (TsCmpStampsGE(&pSspec->pTimeStamp[i], &tsBegin)) {
	    beg = i;
	    break;
	}
	if (++i >= pSspec->dataDim)
	    i = 0;
    }
    assertAlways(beg >= 0);
    i = pSspec->lastData;
    while (end < 0) {
	if (TsCmpStampsLE(&pSspec->pTimeStamp[i], &tsEnd) || i == beg) {
	    end = i;
	    break;
	}
	if (--i < 0)
	    i = pSspec->dataDim - 1;
    }
    assertAlways(end >= 0);

/*-----------------------------------------------------------------------------
*	set up the `restrict' structure items
*----------------------------------------------------------------------------*/
    pSspec->restrictFirstData = beg;
    pSspec->restrictLastData = end;
    pSspec->restrictRefTs = pSspec->pTimeStamp[beg];
    pSspec->restrictRefTs.nsec = 0;
    TsDiffAsDouble(&pSspec->restrictDeltaSecSubtract,
		&pSspec->restrictRefTs, &pSspec->refTs);
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	i = beg;
	first = 1;
	while (1) {
	    if (sydValAsDbl(pSChan, i, &val)) {
		if (pSChan->restrictMinDataVal > val || first)
		    pSChan->restrictMinDataVal = val;
		if (pSChan->restrictMaxDataVal < val || first)
		    pSChan->restrictMaxDataVal = val;
		first = 0;
	    }
	    if (i == end)
		break;
	    if (++i >= pSspec->dataDim)
		i = 0;
		
	}
    }

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydSampleSetStats - compute statistics for sample set
*
*-*/
long
sydSampleSetStats(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    long	stat=S_syd_OK;
    SYD_CHAN	*pSChan;
    int		i, i1, n, beg, end;
    int		statCount=0, sampCount;
    assert(pSspec != NULL);

/*-----------------------------------------------------------------------------
*	first, find out how many snapshots there are and set up the
*	appropriate structures for statistics; assume that all channels
*	are the same.
*----------------------------------------------------------------------------*/
    pSChan = pSspec->pChanHead;
    i = pSspec->restrictFirstData;
    while (1) {
	i1 = i + 1 < pSspec->dataDim ? i + 1 : 0;
	if (pSChan->pFlags[i].snapend || pSChan->pFlags[i].eof ||
		i == pSspec->restrictLastData || pSChan->pFlags[i1].restart) {
	    statCount++;
	}
	if (i == pSspec->restrictLastData)	break;
	if (++i >= pSspec->dataDim)		i = 0;
    }
    if (statCount > pSspec->statDim) {
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	    if (!pSChan->dataChan)
		continue;
	    if (pSspec->statDim > 0) {
		free((char *)pSChan->pStats);
		free((char *)pSspec->pStatTimeStamp);
		free((char *)pSspec->pStatDeltaSec);
		free((char *)pSspec->pStatPopCount);
	    }
	    pSChan->pStats=(SYD_STATS *)malloc(statCount*sizeof(SYD_STATS));
	    assertAlways(pSChan->pStats != NULL);
	    pSspec->pStatDeltaSec =
			(double *)malloc(statCount*sizeof(double));
	    assertAlways(pSspec->pStatDeltaSec != NULL);
	    pSspec->pStatTimeStamp =
			(TS_STAMP *)malloc(statCount*sizeof(TS_STAMP));
	    assertAlways(pSspec->pStatTimeStamp != NULL);
	    pSspec->pStatPopCount =
			(long *)malloc(statCount*sizeof(long));
	    assertAlways(pSspec->pStatPopCount != NULL);
	}
	pSspec->statDim = statCount;
    }
    pSspec->statCount = statCount;
/*-----------------------------------------------------------------------------
*	then, do the statistics
*----------------------------------------------------------------------------*/
    n = sampCount = 0;
    i = beg = end = pSspec->restrictFirstData;
    pSChan = pSspec->pChanHead;
    while (1) {
	i1 = i + 1 < pSspec->dataDim ? i + 1 : 0;
	if (pSChan->pFlags[i].snapend || pSChan->pFlags[i].eof ||
		i == pSspec->restrictLastData || pSChan->pFlags[i1].restart) {
	    if (!pSChan->pFlags[i].eof) {
		end = i;
		sampCount++;
	    }
	    if (sampCount > 0)
		sydSampleSetStats_1(pSspec, n, beg, end, sampCount);
	    sampCount = 0;
	    beg = -1;
	    n++;
	}
	else {
	    end = i;
	    sampCount++;
	}
	if (i == pSspec->restrictLastData)	break;
	if (++i >= pSspec->dataDim)		i = 0;
	if (beg < 0)
	    beg = end = i;
    }

    return stat;
}

/*+/internal******************************************************************
* NAME	sydSampleSetStats_1 - do stats for one snapshot
*
*-*/
sydSampleSetStats_1(pSspec, statNum, sampBeg, sampEnd, sampCount)
SYD_SPEC *pSspec;
int	statNum, sampBeg, sampEnd, sampCount;
{
    SYD_CHAN	*pSChan;
    double	mean, stdDev, diff, val;
    int		i;

    pSspec->pStatDeltaSec[statNum] = pSspec->pDeltaSec[sampBeg];
    pSspec->pStatTimeStamp[statNum] = pSspec->pTimeStamp[sampBeg];
    pSspec->pStatPopCount[statNum] = sampCount;
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (!pSChan->dataChan)
	    continue;
	mean = stdDev = 0.;
	i = sampBeg;
	while (1) {
	    if (sydValAsDbl(pSChan, i, &val))
		mean += val;
	    if (i == sampEnd)			break;
	    if (++i >= pSspec->dataDim)		i = 0;
	}
	if (sampCount > 0)
	    mean /= sampCount;
	i = sampBeg;
	while (1) {
	    if (sydValAsDbl(pSChan, i, &val)) {
		diff = val - mean;
		stdDev += diff * diff;
	    }
	    if (i == sampEnd)			break;
	    if (++i >= pSspec->dataDim)		i = 0;
	}
	if (sampCount > 1) {
	    stdDev /= (double)(sampCount - 1);
	    stdDev = sqrt(stdDev);
	}
	else
	    stdDev = 0.;
        pSChan->pStats[statNum].mean = mean;
        pSChan->pStats[statNum].stdDev = stdDev;
    }
}

/*+/subr**********************************************************************
* NAME	sydSampleSetWriteSSF - write sample set data to `sample set' file
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
long 
sydSampleSetWriteSSF(pSspec, pFile, reqDesc, sampDesc)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*pFile;		/* I stream pointer for writing */
char	*reqDesc;	/* I description of "request", or NULL */
char	*sampDesc;	/* I description for "sample", or NULL */
{
    int		samp;		/* sample number in synchronous set */

    samp = pSspec->restrictFirstData;
    while (samp >= 0) {
	sydSampleWriteSSF(pSspec, pFile, reqDesc, sampDesc, samp);
	if (samp == pSspec->restrictLastData)
	    samp = -1;
	else if (++samp >= pSspec->dataDim)
	    samp = 0;
    }
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydSetAttr - set attributes for synchronous set spec
*
* DESCRIPTION
*	Set various attributes for a synchronous set specification.
*
*	Set the default deadband to use in monitoring channels.  This
*	attribute must be set before adding any channels to the sync set spec.
*	    sydSetAttr(pSspec, SYD_ATTR_DEADBAND, 0, text)
*		text can be "ADEL" or "MDEL"
*
*	Set a callback routine to be called each time a channel receives a
*	monitor from Channel Access (must be done prior to adding any
*	channels to the sync set spec):
*	    sydSetAttr(pSspec, SYD_ATTR_MON_FN, 0, callbackFn);
*
*	When this attribute is set, the normal sydSubr mechanism for handling
*	data is disabled, and the caller is entirely responsible for all
*	activities.  The callback function has the form:
*	    void callbackFn(arg)
*	    struct event_handler_args arg;
*	    {
*	    	arg.user is the pArg from the sydChanOpen call
*	    }
*
*	Set so that printing, plotting, and exporting use statistics (i.e.,
*	mean and standard deviation) rather than raw data:
*	    sydSetAttr(pSspec, SYD_ATTR_USE_STATS, 1, NULL);
*	(A value of 0 causes raw data to be used.)
*
*	Set so that printing, plotting, and exporting use sample means
*	rather than raw data:
*	    sydSetAttr(pSspec, SYD_ATTR_USE_MEANS, 1, NULL);
*	(A value of 0 causes raw data to be used.)
*
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydSetAttr(pSspec, attr, value, pArg)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
SYD_ATTR attr;		/* I attribute key; one of the SYD_ATTR_xxx */
int	value;		/* I value for numeric attributes */
void	*pArg;		/* I pointer to value for non-numeric attributes */
{
    if (attr == SYD_ATTR_DEADBAND) {
	assert(pSspec->pChanHead == NULL);
	if (strcmp((char *)pArg, "ADEL") == 0)
	    pSspec->deadband = DBE_LOG;
	else if (strcmp((char *)pArg, "MDEL") == 0)
	    pSspec->deadband = DBE_VALUE;
	else
	    assertAlways(0);
    }
    else if (attr == SYD_ATTR_MON_FN)
	pSspec->monFn = pArg;
    else if (attr == SYD_ATTR_USE_STATS) {
	pSspec->useStats = value;
	pSspec->useMeans = 0;
    }
    else if (attr == SYD_ATTR_USE_MEANS) {
	pSspec->useStats = value;
	pSspec->useMeans = value;
    }
    else
	assertAlways(0);
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydTest - test the condition for the sync set spec
*
* DESCRIPTION
*	For the sample most recently acquired by sydInputGet, tests the
*	condition established by sydTestAddFromText.
*
*	If no condition was established, this routine returns 1.
*
* RETURNS
*	1	if the condition is true for the present value of the channel
*	0	otherwise
*
*-*/
long
sydTest(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    SYD_CHAN	*pSChan;
    SYD_TEST	*pTest;
    int		i, change;
    chtype	type;
    double	dblVal=0.0;
    long	lngVal=0;
    char	*strVal=NULL;
    int		myType=0;		/* 0,1,2 for double, long, string */

    if ((pTest = pSspec->pAccept) == NULL)
	return 1;
    pSChan = pTest->pSChan;
    i = pSChan->sampInBuf;
    if (i < 0 || pSChan->inStatus[i] == SYD_B_EMPTY ||
		pSChan->inStatus[i] == SYD_B_EOF ||
		pSChan->inStatus[i] == SYD_B_MISSING)
	return 0;
    type = pSChan->dbrType;
#define TEST_IT(val, item,ty) val = pSChan->pInBuf[i]->item.value, myType = ty
    if (type == DBR_TIME_FLOAT)		TEST_IT(dblVal, tfltval, 0);
    else if (type == DBR_TIME_SHORT)	TEST_IT(lngVal, tshrtval, 1);
    else if (type == DBR_TIME_DOUBLE)	TEST_IT(dblVal, tdblval, 0);
    else if (type == DBR_TIME_LONG)	TEST_IT(lngVal, tlngval, 1);
    else if (type == DBR_TIME_STRING)
	strcpy(strVal, pSChan->pInBuf[i]->tstrval.value), myType = 2;
    else if (type == DBR_TIME_CHAR)	TEST_IT(lngVal, tchrval, 1);
    else if (type == DBR_TIME_ENUM) {
#if 0
	jjj
#endif
    }
    change = 0;
    if (myType == 0) {
	if (dblVal < pTest->dblVal)		change = -1;
	else if (dblVal > pTest->dblVal)	change = 1;
    }
    else if (myType == 1) {
	if (lngVal < pTest->lngVal)		change = -1;
	else if (lngVal > pTest->lngVal)	change = 1;
    }
#define COND_IS(ty) pTest->cond == SYD_TSTC_##ty
    if (change == 0) {
	if (COND_IS(EQ) || COND_IS(LE) || COND_IS(GE))
	    return 1;
    }
    else if (change == -1) {
	if (COND_IS(NE) || COND_IS(LE) || COND_IS(LT))
	    return 1;
    }
    else {
	if (COND_IS(NE) || COND_IS(GE) || COND_IS(GT))
	    return 1;
    }
    return 0;
}

/*+/subr**********************************************************************
* NAME	sydTestAddFromText - add a sample test from a text string
*
* DESCRIPTION
*	Scans a text string which specifies a test condition and
*	builds a test structure for the synchronous set spec
*
*	if chanName condition
*
*	This routine adds a synchronous channel structure to the synchronous
*	set spec, flagging the sync channel struct as a `test channel'.
*	If the sync set spec already has a sync channel struct for the
*	channel, then that sync channel struct will be flagged as both
*	a `data channel' and also a `test channel'.  Programs using
*	test channels which aren't also data channels must distinguish
*	between the two in their handling of samples.
*
* RETURNS
*	S_syd_OK, or
*	S_syd_noMem if malloc failed
*	S_syd_ERROR if an error is detected
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1.	The test structure must be closed by calling sydTestClose.
*	(This happens automatically when sydClose is called.)
*
* EXAMPLE
*
*-*/
long
sydTestAddFromText(pSspec, text)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
char	*text;		/* I pointer to string with test specification */
{
    char	*myText=NULL;
    char	*pText, *pFld, delim, *pMsg;
    SYD_TEST	*pTest=NULL;
    char	*name;
    enum sydChanSync sync=SYD_SY_NONF;
    int		i;
    long	stat;
    SYD_CHAN	*pSChan;

    assert(pSspec != NULL);
    assert(text != NULL);

    if (pSspec->pAccept != NULL) {
	(void)printf("sydTestAddFromText: sync set spec already has trigger\n");
	return S_syd_ERROR;
    }
    if ((myText = (char *)malloc(strlen(text))) == NULL) {
	(void)printf("sydTestAddFromText: couldn't malloc for text temp\n");
	return S_syd_noMem;
    }
    pText = myText;
    strcpy(myText, text);
    if ((pTest = (SYD_TEST *)malloc(sizeof(SYD_TEST))) == NULL) {
	(void)printf("sydTestAddFromText: couldn't malloc for test struct\n");
	free(myText);
	return S_syd_noMem;
    }
    pSspec->pAccept = pTest;
    pTest->type = pTest->cond = 0;
    pTest->pSChan = NULL;

    if (nextAlphField(&pText, &pFld, &delim) <= 1) {
	pMsg = "no trigger keyword found";
	goto trigScanErr;
    }
    if (strcmp(pFld,      "if") == 0)
	pTest->type = SYD_TST_IF;
    else {
	pMsg = "illegal test type";
	goto trigScanErr;
    }
    if (pTest->type == SYD_TST_IF) {
/*-----------------------------------------------------------------------------
*    if chanName condition
*----------------------------------------------------------------------------*/
	if (nextChanNameField(&pText, &name, &delim) <= 1) {
	    pMsg = "no channel name found";
	    goto trigScanErr;
	}
	if (strcmp(name, "F") == 0) {
	    sync = SYD_SY_FILLED;
	    if (nextChanNameField(&pText, &name, &delim) <= 1) {
		pMsg = "no channel name found";
		goto trigScanErr;
	    }
	}
	stat = sydChanOpen(pSspec, &pSChan, name, sync, NULL, 1);
	if (stat != S_syd_OK && stat != S_syd_chanNotConn) {
	    pMsg = "couldn't find channel";
	    goto trigScanErr;
	}
	pTest->pSChan = pSChan;
	if (nextNonSpaceField(&pText, &pFld, &delim) <= 1) {
	    pMsg = "condition expected";
	    goto trigScanErr;
	}
	for (i=1; i<SYD_TSTC_PAST; i++) {
	    if (strcmp(pFld, glSydTestText[i]) == 0)
		break;
	}
	if (i >= SYD_TSTC_PAST || i == SYD_TSTC_CHANGE) {
	    pMsg = "condition expected";
	    goto trigScanErr;
	}
	pTest->cond = i;
	if (nextNonSpaceField(&pText, &pFld, &delim) <= 1) {
	    pMsg = "comparison value expected";
	    goto trigScanErr;
	}
	if (strlen(pFld) >= db_strval_dim) {
	    pMsg = "comparison value too long";
	    goto trigScanErr;
	}
	strcpy(pTest->strVal, pFld);
	if (sscanf(pFld, "%lf", &pTest->dblVal) != 1)
	    pTest->dblVal = 0.;
	pTest->lngVal = pTest->dblVal;
    }

    free(myText);
    pSspec->pAccept = pTest;
    return S_syd_OK;

trigScanErr:
    (void)printf("sydTestAddFromText: %s\n%s\n", pMsg, myText);
    sydTestClose(pSspec);
    free(myText);
    return S_syd_ERROR;
}

/*+/subr**********************************************************************
* NAME	sydTestClose - add a sample test condition
*
* DESCRIPTION
*	Closes a test condition.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1.	The test structure must be closed by calling sydTestClose.
*	(This happens automatically when sydClose is called.)
*
* EXAMPLE
*
*-*/
long
sydTestClose(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    SYD_TEST	*pTest;
    assert(pSspec != NULL);

    if ((pTest = pSspec->pAccept) == NULL)
	return S_syd_OK;
    if (pTest->pSChan == NULL) {
	free((char *)pTest);
	pSspec->pAccept = NULL;
    }
    else if (pTest->pSChan->dataChan == 0)
	sydChanClose(pSspec, pTest->pSChan);
    else {
	pTest->pSChan->testChan = 0;
	free((char *)pTest);
	pSspec->pAccept = NULL;
    }

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydTsRound - round time stamp to nearest millisecond
*
*-*/
sydTsRound(pStamp, roundNsec)
TS_STAMP *pStamp;
int	roundNsec;
{
    unsigned long roundTemp;

    roundTemp = pStamp->nsec;
    roundTemp = ( (roundTemp + roundNsec/2) / roundNsec ) * roundNsec;
    if (roundTemp < 1000000000)
	pStamp->nsec = roundTemp;
    else {
	pStamp->nsec = roundTemp - 1000000000;
	pStamp->secPastEpoch++;
    }
}

/*+/subr**********************************************************************
* NAME	sydValAsDbl - fetch value from channel as a double
*
* RETURNS
*	1, or
*	0  if the value is missing or EOF, or if the type is invalid
*	
*-*/
int
sydValAsDbl(pSChan, sampNum, pDbl)
SYD_CHAN *pSChan;	/* I sync channel pointer */
int	sampNum;	/* I sample number */
double	*pDbl;		/* O value */
{
    if (pSChan->pFlags[sampNum].missing || pSChan->pFlags[sampNum].eof)
	return 0;
    if (pSChan->dbfType == DBF_FLOAT)
	*pDbl = (double)(((float *)pSChan->pData)[sampNum]);
    else if (pSChan->dbfType == DBF_DOUBLE)
	*pDbl = ((double *)pSChan->pData)[sampNum];
    else if (pSChan->dbfType == DBF_SHORT) {
	if (pSChan->isRVAL)
	    *pDbl = (double)(((unsigned short *)pSChan->pData)[sampNum]);
	else
	    *pDbl = (double)(((short *)pSChan->pData)[sampNum]);
    }
    else if (pSChan->dbfType == DBF_LONG) {
	if (pSChan->isRVAL)
	    *pDbl = (double)(((unsigned long *)pSChan->pData)[sampNum]);
	else
	    *pDbl = (double)(((long *)pSChan->pData)[sampNum]);
    }
    else if (pSChan->dbfType == DBF_CHAR)
	*pDbl = (double)(((char *)pSChan->pData)[sampNum]);
    else
	return 0;

    return 1;
}
