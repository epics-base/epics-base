/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	12-04-90
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
 * .00	12-04-90	rac	initial version
 * .01	06-18-91	rac	installed in SCCS
 * .02  06-19-91	rac	replace <fields.h> with <alarm.h>
 * .03	08-15-91	rac	update documentation; add args to sydChanOpen;
 *				enhance for use by ordinary users
 * .04	09-11-91	rac	for CA, report "noData" if ts==0; add more
 *				documentation
 * .05	09-22-91	rac	add sydInputFetch; add trigger routines;
 *				handle asynchronous ca_search
 * .06	10-21-91	rac	set the reference time stamp for the first
 *				sample which is stored; better handle long
 *				lines in printing; fold channel names when
 *				printing; put EGU in channel structure;
 *				put EGU in export file; support time stamp
 *				rounding;
 *				args changed for: sydSampleSetPrint;
 *				sydSamplePrint; sydInputStoreInSet;
 * .07	11-02-91	rac	add sydSampleWriteSSF, sydSampleSetWriteSSF
 * .08	12-08-91	rac	fix alignment for printing of channel names
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
#if 0	/* allow comments within the module heading */
/*+/mod***********************************************************************
* TITLE	sydSubr.c - synchronous data routines
*
* DESCRIPTION
*	These routines support:
*	o   defining a set of channels for which synchronous samples are
*	    to be acquired
*	o   checking to see if a synchronous sample is available
*	o   acquiring the next synchronous sample or a specified number
*	    of synchronous samples
*	o   storing the most recently acquired synchronous sample at the
*	    end of the set of synchronous samples
*	o   printing a particular synchronous sample or all samples in
*	    a synchronous sample set
*
*	Sample acquisition can be from one of:
*	o   archiver `sample set' file
*	o   archiver `by channel' file
*	o   Channel Access
*
*	sydPlot.c contains routines which support plotting a synchronous
*	sample set.
*
* QUICK REFERENCE
*
* #include <genDefs.h>		/* some general use definitions */
* #include <db_access.h>	/* definitions for database related items */
* #include <sydDefs.h>		/* structures for synchronous data routines */
*
* SYD_SPEC *pSspec;	/* pointer to synchronous set spec */
* SYD_CHAN *pSChan;	/* pointer to synchronous channel descriptor */
*
*     long  sydChanClose(        pSspec,  pSChan			)
* SYD_CHAN *sydChanFind(         pSspec,  chanName			)
*     long  sydChanOpen(         pSspec, >ppSChan, chanName,  sync, pArg, trig)
*				         sync = SYD_SY_{NONF,FILLED}
*     long  sydClose(            pSspec					)
*     long  sydFileInfo(         pSspec,  out                           )
*     long  sydInputFetch(       pSspec					)
*     long  sydInputGet(         pSspec, >pMoreFlag			)
*     void  sydInputReset(       pSspec					)
*     void  sydInputResetKeepNewest(pSspec				)
*     void  sydInputResetSampled(pSspec					)
*     void  sydInputStoreInSet(  pSspec,  ignorePartial			)
*     long  sydInputSync(        pSspec         			)
*     long  sydOpenCA(          >ppSspec, NULL				)
*     long  sydOpenCF(          >ppSspec, filePath			)
*     long  sydOpenPFO(         >ppSspec, NULL				)
*     long  sydOpenSSF(         >ppSspec, filePath			)
*     long  sydPosition(         pSspec,  pStamp			)
*     long  sydSampleExport(     pSspec,  out,  fmtFlag,  hdrFlag,  sampNum)
*     long  sydSamplePrint(      pSspec,  out,  fmtFlag,  hdrFlag,
*						  nCol, colWidth, sampNum)
*     long  sydSampleWriteSSF(   pSspec,  pFile, progDesc, sampDesc, sampNum)
*
*     long  sydSampleSetAlloc(   pSspec,  reqCount			)
*     long  sydSampleSetExport(  pSspec,  out,  fmtFlag			)
*     long  sydSampleSetFree(    pSspec					)
*     long  sydSampleSetGet(     pSspec					)
*     long  sydSampleSetPrint(   pSspec,  out,  fmtFlag, nCol, colWidth	)
*     long  sydSampleSetWriteSSF(pSspec,  pFile, progDesc, sampDesc     )
*
*     long  sydTriggerAddFromText(pSspec,  text				)
*     long  sydTriggerClose(     pSspec					)
*
* BUGS
* o	error detection and handling isn't "leakproof"
*
* DESCRIPTION, continued
*
* o  special terms
*
*	synchronous sample--is a set of values, one for each channel,
*	    with the same time stamp for each
*
*	synchronous sample set--is a set of synchronous samples, in order
*	    by time stamp.  This frequently will be referred to as a
*	    synchronous set.
*
*	synchronous channel descriptor--is the information about one
*	    of the channels for which synchronous data is to be acquired
*
*	synchronous set specification--is the set of information describing
*	    how synchronous samples are to be acquired, and which channels
*	    are to be used
*
* o  synchronous set specification
*
*	A program can have one or more synchronous set specifications at the
*	same time.  Each is created with an "open" and destroyed with a
*	"close".  The "open" returns a pointer which is used in all
*	subsequent operations with the "sync set spec".
*
*	There are several routines available for opening a sync set spec; the
*	one which is used determines where samples will be obtained.  Most
*	other sydXxx routines are independent of which source is being used.
*	See sydOpen for more details.
*
* o  synchronous channel descriptor
*
*	After a sync set spec has been opened, one or more channels will
*	be added to it with calls to sydChanOpen.  When a channel is opened,
*	it must be specified whether the channel will be treated as "filled"
*	or as "synchronous".  This distinction comes into play while samples
*	are being acquired (see below).
*
*	The value for a channel might be a scalar, as for a thermocouple,
*	or an array, as for a digitized waveform.  Most of the discussion
*	which follows makes no distinction between the two kinds of values.
*
*	When a channel is no longer wanted as part of a sync set spec, the
*	sydChanClose call can be used to remove it.  (The sydClose call
*	automatically closes all channels in a sync set spec.)
*
* o  acquiring synchronous samples
*
*	Each channel is treated as having a stream of time-stamped values.
*	When sydInputGet is called, the earliest time stamp for all the
*	channels is found--this is the time stamp for the sample.  Each
*	channel which has a value with that time stamp is placed in the
*	sample.  If no samples have yet been recieved for a channel, then
*	the channel is flagged as "missing" in the sample.  The action taken
*	when there is no value for a channel depends on how the channel was
*	opened.
*
*	If the sydChanOpen call specified SYD_SY_FILLED, then when the channel
*	has no value at the chosen time stamp the most recent prior value is
*	placed into the sample.  This implements the assumption that the
*	channel's value has remained constant.
*
*	Specifying SYD_SY_NONF in the sydChanOpen call inhibits "filling in"
*	a value in the sample when a channel has no value at the chosen
*	time stamp.  In this case, the channel will be flagged as "missing"
*	in the sample.
*
*	If data acquisition is from Channel Access, then some additional
*	details come into play.  The result is to compensate for possible
*	network delays in transmitting data, which means that the calling
*	program may occasionally receive a "no data now" status.  In that
*	case, the calling program is expected to try again later to see
*	if additional data have been received.
*
* o  accessing data for a sample
*
* o  accessing data for a sample set
*
* EXAMPLE
*
* #include <genDefs.h>
* #include <sydDefs.h>
* #include <cadef.h>
* #include <db_access.h>
*
* main()
* {
*    SYD_SPEC	*pSspec;	/* pointer to sync set spec */
*    SYD_CHAN	*pSchanBase;	/* pointer to sync chan desc for POWER:BASE */
*    SYD_CHAN	*pSchanLag;	/* pointer to sync chan desc for POWER:LAG30 */
*    long	stat;		/* status return */
*    int	i;
*    int	moreFlag;	/* 1 if more samples waiting */
*    float	sumBase, sumLag;/* sums for the two channels */
*    char	timeText[28];	/* text for time stamp */
*    int	chanStat;	/* input status for channel */
*
* /*---------------------------------------------------------------------------
* *	open the synchronous sample set specification and add the channels
* *	to it
* *--------------------------------------------------------------------------*/
*    stat = sydOpenCA(&pSspec, NULL);
*    if (stat != S_syd_OK) {
*	printf("couldn't open sync set spec\n");
*	exit(1);
*    }
*    stat = sydChanOpen(pSspec, &pSchanBase, "rai_2000", SYD_SY_FILLED,NULL,0);
*    if (stat != S_syd_OK) {
*	printf("couldn't open POWER:BASE\n");
*	exit(1);
*    }
*    stat = sydChanOpen(pSspec, &pSchanLag, "rao_2000", SYD_SY_FILLED,NULL,0);
*    if (stat != S_syd_OK) {
*	printf("couldn't open POWER:LAG30\n");
*	exit(1);
*    }
* /*---------------------------------------------------------------------------
* *	now get 100 synchronous samples and accumulate a running sum for
* *	each channel.  Since this example program is using Channel Access,
* *	it loops on sydInputGet until a status of S_syd_noDataNow is
* *	received; when retrieving from an archive file, such a loop wouldn't
* *	be used.
* *--------------------------------------------------------------------------*/
*    sumBase = sumLag = 0.;
*    i = 0;
*    while (i < 100) {
*	ca_pend_event(.1);		/* allow Channel Access to get values */
*	stat = sydInputGet(pSspec, &moreFlag);	/* see if any were obtained */
*	while (stat == S_syd_OK || stat == S_syd_partial) {
*	    i++;
*	    tsStampToText(&SydInputTs(pSspec), TS_TEXT_MMDDYY, timeText);
*	    printf("sample at %s more:%d--", timeText, moreFlag);
*	    chanStat = SydInputStatus(pSchanBase);
*	    if (chanStat != SYD_B_EOF && chanStat != SYD_B_MISSING) {
*		sumBase += SydInputValAsFloat(pSchanBase);
*		printf("%s= %f ", SydChanName(pSchanBase),
*					SydInputValAsFloat(pSchanBase));
*		SydInputMarkAsSampled(pSchanBase);
*	    }
*	    chanStat = SydInputStatus(pSchanLag);
*	    if (chanStat != SYD_B_EOF && chanStat != SYD_B_MISSING) {
*		sumLag += SydInputValAsFloat(pSchanLag);
*		printf("%s= %f ", SydChanName(pSchanLag),
*					SydInputValAsFloat(pSchanLag));
*		SydInputMarkAsSampled(pSchanLag);
*	    }
*	    printf("\n");
*	    stat = sydInputGet(pSspec, &moreFlag);
*	}
*    }
*    printf("sumBase= %f   sumLag= %f\n", sumBase, sumLag);
* }
*-***************************************************************************/
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
#endif

void sydChanFreeArrays();
void sydInputGetIn();
void sydSamplePrint1();
long sydSampleSetAlloc();
long sydSampleSetFree();


#if SYD_TEST
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

/*-----------------------------------------------------------------------------
*	open the synchronous sample set specification and add the channels
*	to it
*----------------------------------------------------------------------------*/
    stat = sydOpenCA(&pSspec, NULL);
    if (stat != S_syd_OK) {
	printf("couldn't open sync set spec\n");
	exit(1);
    }
    stat = sydChanOpen(pSspec, &pSchanBase, "rai_2000", SYD_SY_FILLED,NULL,0);
    if (stat != S_syd_OK) {
	printf("couldn't open POWER:BASE\n");
	exit(1);
    }
    stat = sydChanOpen(pSspec, &pSchanLag, "rao_2000", SYD_SY_FILLED,NULL,0);
    if (stat != S_syd_OK) {
	printf("couldn't open POWER:LAG30\n");
	exit(1);
    }
/*-----------------------------------------------------------------------------
*	now get 100 synchronous samples and accumulate a running sum for
*	each channel.  Since this example program is using Channel Access,
*	it loops on sydInputGet until a status of S_syd_noDataNow is
*	received; when retrieving from an archive file, such a loop wouldn't
*	be used.
*----------------------------------------------------------------------------*/
    sumBase = sumLag = 0.;
    i = 0;
    while (i < 100) {
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
}
#endif

/*+/internal******************************************************************
* NAME	sydCvtDblToTxt - format a double for printing
*
* DESCRIPTION
*	Formats a double for printing.  This routine is dedicated to
*	getting as large a range of values as possible into a particular
*	field width.
*
*	This routine doesn't attempt to handle extremely small values.
*	It assumes that the field is large enough to handle the smallest
*	significant value to be encountered.
*
* RETURNS
*	void
*
* BUGS
* o	extremely small values aren't handled well
* o	this is the same as pprCvtDblToTxt; it ought to be in a general-
*	purpose library somewhere
*
* NOTES
* 1.	If the value can't be represented at all in the field, the sign
*	followed by *'s appears.
* 2.	In extreme cases, only the magnitude of the value will appear, as
*	En or Enn.  For negative values, a - will precede the E.
* 3.	When appropriate, the value is rounded to the nearest integer
*	for formatting.
*
*-*/
static void
sydCvtDblToTxt(text, width, value, sigDig)
char	*text;		/* O text representation of value */
int	width;		/* I max width of text string (not counting '\0') */
double	value;		/* I value to print */
int	sigDig;		/* I max # of dec places to print */
{
    double	valAbs;		/* absolute value of caller's value */
    int		wholeNdig;	/* number of digits in "whole" part of value */
    double	logVal;		/* log10 of value */
    int		decPlaces;	/* number of decimal places to print */
    int		expWidth;	/* width needed for exponent field */
    int		excess;		/* number of low order digits which
				    won't fit into the field */

    if (value == 0.) {
	(void)strcpy(text, "0");
	return;
    }

/*-----------------------------------------------------------------------------
*    find out how many columns are required to represent the integer part
*    of the value.  A - is counted as a column;  the . isn't.
*----------------------------------------------------------------------------*/
    valAbs = value>0 ? value : -value;
    logVal = log10(valAbs);
    wholeNdig = 1 + (int)logVal;
    if (wholeNdig < 0)
	wholeNdig = 1;
    if (value < 0.)
	wholeNdig++;
    if (wholeNdig < width-1) {
/*-----------------------------------------------------------------------------
*    the integer part fits well within the field.  Find out how many
*    decimal places can be printed (honoring caller's sigDig limit).
*----------------------------------------------------------------------------*/
	decPlaces = width - wholeNdig - 1;
	if (sigDig < decPlaces)
	    decPlaces = sigDig;
	if (sigDig > 0)
	    (void)sprintf(text, "%.*f", decPlaces, value);
	else
	    (void)sprintf(text, "%d", nint(value));
    }
    else if (wholeNdig == width || wholeNdig == width-1) {
/*-----------------------------------------------------------------------------
*    The integer part just fits within the field.  Print the value as an
*    integer, without printing the superfluous decimal point.
*----------------------------------------------------------------------------*/
	(void)sprintf(text, "%d", nint(value));
    }
    else {
/*-----------------------------------------------------------------------------
*    The integer part is too large to fit within the caller's field.  Print
*    with an abbreviated E notation.
*----------------------------------------------------------------------------*/
	expWidth = 2;				/* assume that En will work */
	excess = wholeNdig - (width - 2);
	if (excess > 999) {
	    expWidth = 5;			/* no! it must be Ennnn */
	    excess += 3;
	}
	else if (excess > 99) {
	    expWidth = 4;			/* no! it must be Ennn */
	    excess += 2;
	}
	else if (excess > 9) {
	    expWidth = 3;			/* no! it must be Enn */
	    excess += 1;
	}
/*-----------------------------------------------------------------------------
*	Four progressively worse cases, with all or part of exponent fitting
*	into field, but not enough room for any of the value
*		Ennn		positive value; exponent fits
*		-Ennn		negative value; exponent fits
*		+****		positive value; exponent too big
*		-****		negative value; exponent too big
*----------------------------------------------------------------------------*/
	if (value >= 0. && expWidth == width)
	    (void)sprintf(text, "E%d", nint(logVal));
	else if (value < 0. && expWidth == width-1)
	    (void)sprintf(text, "-E%d", nint(logVal));
	else if (value > 0. && expWidth > width)
	    (void)sprintf(text, "%.*s", width, "+*******");
	else if (value < 0. && expWidth > width-1)
	    (void)sprintf(text, "%.*s", width, "-*******");
	else {
/*-----------------------------------------------------------------------------
*	The value can fit, in exponential notation
*----------------------------------------------------------------------------*/
	    (void)sprintf(text, "%dE%d",
			nint(value/exp10((double)excess)), excess);
	}
    }
}

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
	    GenFree((char *)pSChan->pInBuf[i]);
    }
    sydChanFreeArrays(pSChan);

    pSspec->chanCount--;
    DoubleListRemove(pSChan, pSspec->pChanHead, pSspec->pChanTail);
    if (pSChan->trigChan)
	sydTriggerClose(pSspec);
    GenFree((char *)pSChan);

    return S_syd_OK;
}
static void
sydChanFreeArrays(pSChan)
SYD_CHAN *pSChan;
{
    if (pSChan->pData != NULL) {
	GenFree((char *)pSChan->pData);
	pSChan->pData = NULL;
    }
    if (pSChan->pDataAlStat != NULL) {
	GenFree((char *)pSChan->pDataAlStat);
	pSChan->pDataAlStat = NULL;
    }
    if (pSChan->pDataAlSev != NULL) {
	GenFree((char *)pSChan->pDataAlSev);
	pSChan->pDataAlSev = NULL;
    }
    if (pSChan->pDataCodeL != NULL) {
	GenFree((char *)pSChan->pDataCodeL);
	pSChan->pDataCodeL = NULL;
    }
    if (pSChan->pDataCodeR != NULL) {
	GenFree((char *)pSChan->pDataCodeR);
	pSChan->pDataCodeR = NULL;
    }
    if (pSChan->pFlags != NULL) {
	GenFree((char *)pSChan->pFlags);
	pSChan->pFlags = NULL;
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
* RETURNS
*	S_syd_OK, or
*	S_syd_noMem		no memory for adding channel to set
*	S_syd_chanNotConn	channel not connected to Channel Access (this
*				isn't treated as an error condition--the
*				channel is open and added to sync set spec)
*	S_syd_chanNotFound	channel not found
*	S_syd_ERROR		if channel already exists in sync set spec
*
* BUGS
* o	always places channel at end of list
* o	data are always acquired using the native type of the channel,
*	rather than caller-specified type
*
* SEE ALSO
*	sydOpen, sydChanClose
*	sydInputGet
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
* 3.	For retrievals form Channel Access, the usual return status will
*	be S_syd_chanNotConn.  To force the connection attempt to completion,
*	the caller can call ca_pend_event following the call to this routine.
*	If SydChanDbfType(pSChan) is TYPENOTCONN following the ca_pend_event,
*	then the caller can assume the channel isn't presently connectable.
*
* EXAMPLE
*
*-*/
long
sydChanOpen(pSspec, ppSChan, chanName, sync, pArg, trig)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
SYD_CHAN **ppSChan;	/* O pointer to sync channel pointer; or NULL */
char	*chanName;	/* I channel name (.VAL assumed if field omitted) */
enum sydChanSync sync;	/* I sync/non-sync; SYD_SY_NONF or SYD_SY_FILLED */
void	*pArg;		/* I pointer to struct needed to init, or NULL */
int	trig;		/* I 0,1 if this is data,trigger channel */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    enum sydChanStep step;	/* step type: SYD_ST_STEP or SYD_ST_CONTIN */
    int		i;

    assert(pSspec != NULL);
    assert(strlen(chanName) > 0);
    assert(strlen(chanName) < db_name_dim);

    if (ppSChan != NULL)
	*ppSChan = NULL;

    if ((pSChan = sydChanFind(pSspec, chanName)) != NULL) {
	if (trig == 0) {
	    if (pSChan->dataChan == 1)
		return S_syd_ERROR;
	}
	else {
	    if (pSChan->trigChan == 0) {
		pSChan->trigChan = 1;
		if (ppSChan != NULL)
		    *ppSChan = pSChan;
	    }
	    else
		return S_syd_ERROR;
	}
    }

/*-----------------------------------------------------------------------------
*    allocate and initialize an empty synchronous channel structure
*----------------------------------------------------------------------------*/
    if ((pSChan = (SYD_CHAN *)GenMalloc(sizeof(SYD_CHAN))) != NULL) {
	DoubleListAppend(pSChan, pSspec->pChanHead, pSspec->pChanTail);
	pSspec->chanCount++;
	pSChan->pSspec = pSspec;
	pSChan->pHandle = NULL;
	pSChan->nInBufs = pSspec->nInBufs;
	for (i=0; i<pSChan->nInBufs; i++) {
	    pSChan->inStatus[i] = SYD_B_EMPTY;
	    pSChan->pInBuf[i] = NULL;
	}
	pSChan->firstInBuf = -1;
	pSChan->lastInBuf = -1;
	pSChan->sampInBuf = -1;
	pSChan->minDataVal = 0.;
	pSChan->maxDataVal = 0.;
	strcpy(pSChan->name, chanName);
	strcpy(pSChan->label, chanName);
	strcat(pSChan->label, " not conn");
	strcpy(pSChan->EGU, "none");
	pSChan->sync = sync;
	pSChan->pData = NULL;
	pSChan->pDataAlStat = NULL;
	pSChan->pDataAlSev = NULL;
	pSChan->pDataCodeL = NULL;
	pSChan->pDataCodeR = NULL;
	pSChan->pFlags = NULL;
	pSChan->conn = 0;
	pSChan->discon = 1;
	pSChan->dataChan = trig?0:1;
	pSChan->trigChan = trig;
    }
    else
	return S_syd_noMem;
/*-----------------------------------------------------------------------------
*    "open" the "source" for the data for the channel.  This will provide
*    information about how large the buffers need to be.  If retrieval is
*    from Channel Access, the buffer allocation, etc., will be deferred if
*    the connection isn't complete.
*----------------------------------------------------------------------------*/
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

    if (ppSChan != NULL)
	*ppSChan = pSChan;

    return stat;
}

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
	pSChan->pInBuf[i] = (union db_access_val *)GenMalloc(
			dbr_size_n(pSChan->dbrType, pSChan->elCount));
	if (pSChan->pInBuf[i] == NULL) {
	    (void)sydChanClose(pSspec, pSChan);
	    retStat = S_syd_noMem;
	}
    }
    return retStat;
}

long
sydChanOpenGR(pSChan)
SYD_CHAN *pSChan;	/* I sync channel pointer */
{
    if (pSChan->dbrType != DBR_TIME_ENUM)
	pSChan->step = SYD_ST_SMOOTH;
    else
	pSChan->step = SYD_ST_STEP;
    if (pSChan->dbrType == DBR_TIME_FLOAT) {
        pSChan->precision = pSChan->grBuf.gfltval.precision;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.gfltval.units);
	strcpy(pSChan->EGU, pSChan->grBuf.gfltval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
        pSChan->precision = 0;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.gshrtval.units);
	strcpy(pSChan->EGU, pSChan->grBuf.gshrtval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
        pSChan->precision = pSChan->grBuf.gdblval.precision;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.gdblval.units);
	strcpy(pSChan->EGU, pSChan->grBuf.gdblval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
        pSChan->precision = 0;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.glngval.units);
	strcpy(pSChan->EGU, pSChan->grBuf.glngval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
        pSChan->precision = 0;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.gchrval.units);
	strcpy(pSChan->EGU, pSChan->grBuf.gchrval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
        pSChan->precision = -1;
        sprintf(pSChan->label, "%s", pSChan->name);
    }    
    else {
        sprintf(pSChan->label, "%s", pSChan->name);
        pSChan->precision = 0;
    }
    if (pSChan->dbrType != DBR_TIME_ENUM) {
	if (pSChan->precision > 10 || pSChan->precision < 0)
	    pSChan->precision = 3;
    }

    if (*pSChan->EGU == ' ' || strlen(pSChan->EGU) == 0)
	strcpy(pSChan->EGU, "none");
    return S_syd_OK;
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
* o	the `wrapup' call should do the GenFree, since the GenFree must
*	match the GenMalloc--if this routine is compiled with DEBUG and
*	the routine which did the GenMalloc (e.g., sydSubrCA) is compiled
*	without it, then the GenFree here will complain.
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

    GenFree((char *)pSspec);
    return retStat;
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
*	synchronous set spec.  The desired time stamp for the `next sample'
*	is determined by searching forward from the time of the previous
*	sample.  Among the channels in the synchronous set spec, the one
*	which changes soonest is the one whose time stamp is used for the
*	next sample.  Values are handled on either a "non-filled" or a
*	"filled" basis.
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
*	There are two main ways of dealing with samples obtaines from this
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
*	Access the input value for a channel, following sydInputGet
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
*	`more flag' is returned as zero.  It may be interesting to note
*	that a zero value doesn't guarantee that the next call will
*	result in a status of S_syd_noDataNow.
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
	i1 = NEXT_INBUF(pSChan, i);
	if (i < 0)
	    later = 1;
	else if (pSChan->inStatus[i] == SYD_B_FULL ||
				pSChan->inStatus[i] == SYD_B_RESTART) {
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
	else if (pSChan->inStatus[i] == SYD_B_MISSING) {
	    if (TsCmpStampsLT(&BUFiTS, &nextTs))
		nextTs = BUFiTS;
	}
	else if (pSChan->inStatus[i1] == SYD_B_FULL ||
				pSChan->inStatus[i1] == SYD_B_RESTART) {
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
	else if (pSChan->inStatus[i1] == SYD_B_MISSING) {
	    if (TsCmpStampsLT(&BUFi1TS, &nextTs))
		nextTs = BUFi1TS;
	}
	else if (i == pSChan->lastInBuf &&
				pSChan->inStatus[i] == SYD_B_SAMPLED) {
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
	i1 = NEXT_INBUF(pSChan, i);
	if (i < 0) {
	    pSChan->sampInBuf = -1;
	    moreFlag = 0;
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}
	else if ((pSChan->inStatus[i] == SYD_B_FULL ||
		    pSChan->inStatus[i] == SYD_B_RESTART) &&
					TsCmpStampsLE(&BUFiTS, &nextTs)) {
	    pSChan->sampInBuf = i;
	    if (pSChan->inStatus[i1] != SYD_B_FULL)
		moreFlag = 0;
	    pSChan->reused = 0;
	}
	else if (pSChan->inStatus[i] == SYD_B_SAMPLED) {
	    if ((pSChan->inStatus[i1] == SYD_B_FULL ||
		    pSChan->inStatus[i1] == SYD_B_MISSING ||
		    pSChan->inStatus[i1] == SYD_B_RESTART) &&
					TsCmpStampsLE(&BUFi1TS, &nextTs)) {
		pSChan->inStatus[i] = SYD_B_EMPTY;
		pSChan->firstInBuf = i1;
		pSChan->reused = 0;
		pSChan->sampInBuf = i1;
		i2 = NEXT_INBUF(pSChan, i1);
		if (pSChan->inStatus[i2] != SYD_B_FULL)
		    moreFlag = 0;
	    }
	    else {
		pSChan->reused = 1;
		pSChan->sampInBuf = i;
		if (pSChan->inStatus[i1] != SYD_B_FULL)
		    moreFlag = 0;
	    }
	}
	else if (pSChan->inStatus[i] == SYD_B_EOF) {
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
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    SYD_CHAN	*pSChan;	/* pointer to channel in Sspec */
    int		i, i1, discard;
    unsigned long roundTemp;

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
	    if (pSChan->inStatus[i] == SYD_B_SAMPLED) {
		if (pSChan->sync == SYD_SY_NONF)
		    discard = 1;
		else if (pSChan->inStatus[i1] == SYD_B_MISSING ||
			 pSChan->inStatus[i1] == SYD_B_FULL ||
			 pSChan->inStatus[i1] == SYD_B_RESTART) {
		    if (TsCmpStampsLE(&BUFi1TS, &pSspec->sampleTs))
			discard = 1;
		}
	    }
	    else if (pSChan->inStatus[i] == SYD_B_MISSING) {
		if (TsCmpStampsLE(&BUFi1TS, &pSspec->sampleTs))
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
    if (pSspec->type == SYD_TY_CA)
	return;
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
/*-----------------------------------------------------------------------------
*    after possibly throwing away the oldest input data, try to get two
*    buffers of input data; if time stamp rounding is to be done, do it now
*----------------------------------------------------------------------------*/
	if (pSChan->firstInBuf < 0) {
	    stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_READ, NULL);
	    if (pSspec->roundNsec > 0 && pSChan->firstInBuf >= 0) {
		i = pSChan->firstInBuf;
		roundTemp = BUFiTS.nsec;
		roundTemp = ( (roundTemp + pSspec->roundNsec/2) /
				pSspec->roundNsec ) * pSspec->roundNsec;
		if (roundTemp < 1000000000)
		    BUFiTS.nsec = roundTemp;
		else {
		    BUFiTS.nsec = roundTemp - 1000000000;
		    BUFiTS.secPastEpoch += 1;;
		}
	    }
	    if (pSspec->type == SYD_TY_PFO)
		goto skipSecondRead;
	}
	if (pSChan->firstInBuf == pSChan->lastInBuf) {
	    stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_READ, NULL);
	    if (pSspec->roundNsec > 0 &&
			pSChan->firstInBuf != pSChan->lastInBuf) {
		i = pSChan->lastInBuf;
		roundTemp = BUFiTS.nsec;
		roundTemp = ( (roundTemp + pSspec->roundNsec/2) /
				pSspec->roundNsec ) * pSspec->roundNsec;
		if (roundTemp < 1000000000)
		    BUFiTS.nsec = roundTemp;
		else {
		    BUFiTS.nsec = roundTemp - 1000000000;
		    BUFiTS.secPastEpoch += 1;;
		}
	    }
	}
skipSecondRead:
	;
    }
}

/*+/subr**********************************************************************
* NAME	sydInputReset - reset synchronous sample buffers for channels
*
* DESCRIPTION
*	This routine flags as EMPTY the input buffers for the channels in a
*	synchronous set spec.  Two routines are available:
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
*-*/
void
sydInputReset(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
{
    SYD_CHAN	*pSChan;
    int		i;

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	for (i=0; i<pSChan->nInBufs; i++)
	    pSChan->inStatus[i] = SYD_B_EMPTY;
	pSChan->firstInBuf = -1;
	pSChan->lastInBuf = -1;
    }
    pSspec->sampleTs.secPastEpoch = 0;
}

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
*	are updated.
*
* RETURNS
*	void
*
* BUGS
* o	text
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
    int		sub;		/* subscript to store sample */
    chtype	type;
    long	stat;
    SYD_CHAN	*pSChan;
    int		i, el;
    int		useVal;		/* indicates if sample data is to be stored */
    short	alStat, alSev;
    static struct sydChanFlags flags0={0,0,0,0,0,0,0};
 
    if (ignorePartial && pSspec->partial) {
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	    i = pSChan->sampInBuf;
	    if (i >= 0 && pSChan->inStatus[i] != SYD_B_MISSING)
		pSChan->inStatus[i] = SYD_B_SAMPLED;
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
	    pSChan->pFlags[sub] = flags0;
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
		if (pSChan->reused) {
		    pSChan->pFlags[sub].filled = 1;
		    pSChan->pDataCodeL[sub] = 'F';
		}
		else if (pSChan->inStatus[i] != SYD_B_RESTART) {
		    pSChan->pFlags[sub].ok = 1;
		    pSChan->pDataCodeL[sub] = 'O';
		}
		else {
		    pSChan->pFlags[sub].restart = 1;
		    pSChan->pDataCodeL[sub] = 'R';
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
		if (!useVal)
		    ;	/* no action */
		else if (type == DBR_TIME_FLOAT) {
		    float	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tfltval.value;
		    pDest = ((float *)pSChan->pData) + sub * pSChan->elCount;
		    if (pSspec->sampleCount == 0)
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
		    for (el=0; el<pSChan->elCount; el++) {
			if ((double)*pSrc > pSChan->maxDataVal)
			    pSChan->maxDataVal = (double)*pSrc;
			if ((double)*pSrc < pSChan->minDataVal)
			    pSChan->minDataVal = (double)*pSrc;
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_SHORT) {
		    short	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tshrtval.value;
		    pDest = ((short *)pSChan->pData) + sub * pSChan->elCount;
		    if (pSspec->sampleCount == 0) {
			pSChan->maxDataVal = (double)*pSrc;
			pSChan->minDataVal = (double)*pSrc;
		    }
		    for (el=0; el<pSChan->elCount; el++) {
			if ((double)*pSrc > pSChan->maxDataVal)
			    pSChan->maxDataVal = (double)*pSrc;
			if ((double)*pSrc < pSChan->minDataVal)
			    pSChan->minDataVal = (double)*pSrc;
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_DOUBLE) {
		    double	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tdblval.value;
		    pDest = ((double *)pSChan->pData) + sub * pSChan->elCount;
		    if (pSspec->sampleCount == 0)
			pSChan->maxDataVal = pSChan->minDataVal = *pSrc;
		    for (el=0; el<pSChan->elCount; el++) {
			if (*pSrc > pSChan->maxDataVal)
			    pSChan->maxDataVal = *pSrc;
			if (*pSrc < pSChan->minDataVal)
			    pSChan->minDataVal = *pSrc;
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_LONG) {
		    long	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tlngval.value;
		    pDest = ((long *)pSChan->pData) + sub * pSChan->elCount;
		    if (pSspec->sampleCount == 0)
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
		    for (el=0; el<pSChan->elCount; el++) {
			if ((double)*pSrc > pSChan->maxDataVal)
			    pSChan->maxDataVal = (double)*pSrc;
			if ((double)*pSrc < pSChan->minDataVal)
			    pSChan->minDataVal = (double)*pSrc;
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_STRING) {
		    char	*pSrc, *pDest;
		    pSrc = pSChan->pInBuf[i]->tstrval.value;
		    pDest = ((char *)pSChan->pData) +
					sub * db_strval_dim * pSChan->elCount;
		    if (pSspec->sampleCount == 0)
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
		    for (el=0; el<pSChan->elCount; el++) {
			if ((double)*pSrc > pSChan->maxDataVal)
			    pSChan->maxDataVal = (double)*pSrc;
			if ((double)*pSrc < pSChan->minDataVal)
			    pSChan->minDataVal = (double)*pSrc;
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
		    if (pSspec->sampleCount == 0)
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
		    for (el=0; el<pSChan->elCount; el++) {
			if ((double)*pSrc > pSChan->maxDataVal)
			    pSChan->maxDataVal = (double)*pSrc;
			if ((double)*pSrc < pSChan->minDataVal)
			    pSChan->minDataVal = (double)*pSrc;
			*pDest++ = *pSrc++;
		    }
		}
		else if (type == DBR_TIME_ENUM) {
		    short	*pSrc, *pDest;
		    pSrc = &pSChan->pInBuf[i]->tenmval.value;
		    pDest = ((short *)pSChan->pData) + sub * pSChan->elCount;
		    if (pSspec->sampleCount == 0)
			pSChan->maxDataVal = pSChan->minDataVal = (double)*pSrc;
		    for (el=0; el<pSChan->elCount; el++) {
			if ((double)*pSrc > pSChan->maxDataVal)
			    pSChan->maxDataVal = (double)*pSrc;
			if ((double)*pSrc < pSChan->minDataVal)
			    pSChan->minDataVal = (double)*pSrc;
			*pDest++ = *pSrc++;
		    }
		}
		else
		    assertAlways(0);
	    }
	    if (i >= 0 && pSChan->inStatus[i] != SYD_B_MISSING)
		pSChan->inStatus[i] = SYD_B_SAMPLED;
	}
    }

    if (pSspec->firstData < 0 || pSspec->firstData == pSspec->lastData) {
	if (++pSspec->firstData >= pSspec->dataDim)
	    pSspec->firstData = 0;
    }
    if (pSspec->sampleCount < pSspec->dataDim)
	pSspec->sampleCount++;
}

/*+/subr**********************************************************************
* NAME	sydInputSync - synchronize input buffers with disk for `by channel'
*
* DESCRIPTION
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
*	sydOpenPFO(&pSspec, NULL)	init for OPI sync data
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
*	sydChanOpen, sydAddTrigger, sydAddTimeRange
*	sydInputGet
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

    (*ppSspec)->pChanHead = NULL;
    (*ppSspec)->pChanTail = NULL;
    (*ppSspec)->pTrigger = NULL;
    (*ppSspec)->refTs.secPastEpoch = 0;
    (*ppSspec)->refTs.nsec = 0;
    (*ppSspec)->priorTs = (*ppSspec)->refTs;
    (*ppSspec)->sampleTs = (*ppSspec)->refTs;
    (*ppSspec)->chanCount = 0;
    (*ppSspec)->eofCount = 0;
    (*ppSspec)->dataDim = 0;
    (*ppSspec)->sampleCount = 0;
    (*ppSspec)->pDeltaSec = NULL;
    (*ppSspec)->pTimeStamp = NULL;
    (*ppSspec)->pPartial = NULL;
    (*ppSspec)->lastData = -1;
    (*ppSspec)->firstData = -1;
    (*ppSspec)->roundNsec = 0;
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
* o	doesn't handle array channels.  There needs to be an option
*	which puts elementCount prior to printing values
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
int	samp;		/* I sample number in synchronous set */
{
    SYD_CHAN	*pSChan;	/* pointer to channel in synchronous set */
    char	stampText[28];
    int		i;

/*-----------------------------------------------------------------------------
* generate headings, depending on option:
* 1==>	"mm/dd/yy hh:mm:ss.msc"
*	time	name1	name2	...
*
* 2==>	"mm/dd/yy hh:mm:ss.msc"
*	time	stat	name1	stat	name2	...
*----------------------------------------------------------------------------*/
    if (samp == pSspec->firstData) {
	(void)fprintf(out, "\"%s\"\n\"time\"", tsStampToText(
		    &pSspec->refTs, TS_TEXT_MMDDYY, stampText));
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan = pSChan->pNext) {
	    if (pSChan->dataChan) {
		if (option == 2)
		    (void)fprintf(out, "\t\"stat\"");
		(void)fprintf(out, "\t\"%s\"", pSChan->name);
	    }
	}
	(void)fprintf(out, "\n");
	(void)fprintf(out, "\"seconds\"");
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan = pSChan->pNext) {
	    if (pSChan->dataChan) {
		if (option == 2)
		    (void)fprintf(out, "\t\"stat\"");
		(void)fprintf(out, "\t\"%s\"", pSChan->EGU);
	    }
	}
	(void)fprintf(out, "\n");
    }

/*-----------------------------------------------------------------------------
*    print the value for each channel for this sample.
*----------------------------------------------------------------------------*/
    (void)fprintf(out, "%.3f", pSspec->pDeltaSec[samp]);
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (option == 1)
	    sydSamplePrint1(pSChan, out, '\t', 0, 0, 0, 1, samp);
	else if (option == 2)
	    sydSamplePrint1(pSChan, out, '\t', 1, 0, 0, 1, samp);
    }
    (void)fprintf(out, "\n");
}

/*+/subr**********************************************************************
* NAME	sydSamplePrint
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
long
sydSamplePrint(pSspec, out, formatFlag, headerFlag, nCol, colWidth, samp)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*out;		/* IO stream pointer for output */
int	formatFlag;	/* I ==1 causes page formatting for printing */
int	headerFlag;	/* I ==1 causes printing of column headings */
int	nCol;		/* I >0 causes that many table columns, folded lines */
int	colWidth;	/* I >0 specifies column width, in characters */
int	samp;		/* I sample number in synchronous set */
{
    SYD_CHAN	*pSChan;	/* pointer to channel in synchronous set */
    char	stampText[28];
    int		i;
    int		maxLen;		/* maximum length of items to go on line */
    int		colNum;		/* number of table column */
    int		nChan=0;

    if (colWidth < 1)
	colWidth = 15;

/*-----------------------------------------------------------------------------
*    print a heading line with channel names; if this isn't the first page,
*    put a ^L prior to the heading.
*----------------------------------------------------------------------------*/
    if (headerFlag) {
	if (formatFlag) {
	    if (samp != pSspec->firstData)
		(void)fprintf(out, "\f");
	    (void)fprintf(out, "\n\n");		/* 2 line top margin */
	    (void)fprintf(out, "    ");		/* indent 4 characters */
	}
	(void)fprintf(out, "  %21s", " ");	/* space for * and stamp */
	maxLen = colWidth - 1;
	for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	    nChan++;
	    if ((i = strlen(pSChan->name)) > maxLen)
		maxLen = i;
	}
	if (maxLen < colWidth) {
	    colNum = 0;
	    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
		(void)fprintf(out, "  %*s", colWidth-1, pSChan->name);
		if (nCol > 0 && ++colNum >= nCol) {
		    colNum = 0;
		    (void)fprintf(out, "\n");
		    if (formatFlag)
			(void)fprintf(out, "    ");	/* indent 4 char */
		    (void)fprintf(out, "  %21s", " ");	/* skip * and stamp */
		}
	    }
	    (void)fprintf(out, "\n");
	}
	else {
	    SYD_CHAN	*pNext=pSspec->pChanHead;
	    int		nBatches=1;
	    int		nPasses;
	    int		nameSub;
	    if (nCol > 0)
		nBatches = (nChan-1)/nCol + 1;
	    nPasses = (maxLen + colWidth - 2) / (colWidth - 1);
	    while (nBatches > 0) {
		nameSub = 0;
		for (i=0; i<nPasses; i++) {
		    pSChan = pNext;
		    for (colNum=0; colNum<nCol && pSChan != NULL; colNum++) {
			if (nameSub < strlen(pSChan->name)) {
			    (void)fprintf(out, " %-*.*s",
				colWidth-1, colWidth-1, &pSChan->name[nameSub]);
			}
			else
			    (void)fprintf(out, " %*s", colWidth-1, " ");
			if (colWidth > 2)
			    (void)fprintf(out, " ");
			pSChan = pSChan->pNext;
		    }
		    (void)fprintf(out, "\n");
		    if (formatFlag)
			(void)fprintf(out, "    ");	/* indent 4 char */
		    (void)fprintf(out, "  %21s", " ");	/* skip * and stamp */
		    nameSub += colWidth - 1;
		}
		(void)fprintf(out, "\n");
		pNext = pSChan;
		if (--nBatches > 0) {
		    if (formatFlag)
			(void)fprintf(out, "    ");	/* indent 4 char */
		    (void)fprintf(out, "  %21s", " ");	/* skip * and stamp */
		}
	    }
	}
    }
/*-----------------------------------------------------------------------------
*    print the value for each channel for this sample.  Print the status
*    flags following values, but print only the first element for array
*    channels.
*----------------------------------------------------------------------------*/
    if (formatFlag)
	(void)fprintf(out, "    ");		/* indent 4 characters */
    if (pSspec->pPartial[samp])
	(void)fprintf(out, "*");
    else
	(void)fprintf(out, " ");
    (void)fprintf(out, " %s", tsStampToText(
		    &pSspec->pTimeStamp[samp], TS_TEXT_MMDDYY, stampText));
    colNum = 0;
    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->dataChan) {
	    if (colNum >= nCol) {
		(void)fprintf(out, "\n");
		if (formatFlag)
		    (void)fprintf(out, "    ");	/* indent 4 char */
		(void)fprintf(out, "  %21s", " ");	/* skip * and stamp */
		colNum = 0;
	    }
	    sydSamplePrint1(pSChan, out, ' ', 0, 1, 0, colWidth, samp);
	    colNum++;
	}
    }
    (void)fprintf(out, "\n");

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
* BUGS
* o	doesn't yet print entire array
* o	for spreadsheet, status flag, if written, should be in " "
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
static void
sydSamplePrint1(pSChan, out, sep, preFlag, postFlag, showArray, colWidth, sampNum)
SYD_CHAN *pSChan;	/* I pointer to sync channel */
FILE	*out;		/* I file pointer for writing value */
char	sep;		/* I character to use a a prefix for each field,
				as a separator; usually ' ' or '\t' */
int	preFlag;	/* I != 0 prints status flag prior to value */
int	postFlag;	/* I != 0 prints status flag following value */
int	showArray;	/* I != 0 to show all array elements, not just 1st */
int	colWidth;	/* I >0 specifies column width, in characters;
				== 2 requests only printing status code*/
int	sampNum;	/* I sample number in sync set */
{
    int		i;
    chtype	type;		/* type of value */

    type = pSChan->dbrType;

    if (colWidth == 2) {
	if (pSChan->pData == NULL)
	    (void)fprintf(out, " %c", sep,'M');
	else
	    (void)fprintf(out, " %c", pSChan->pDataCodeL[sampNum]);
	return;
    }
    if (preFlag || postFlag) {
	if (colWidth > 3)
	    colWidth -= 3;
    }

    if (preFlag) {
	if (pSChan->pData == NULL)
	    (void)fprintf(out, "%c%c%c", sep,'M','D');
	else {
	    (void)fprintf(out, "%c%c%c", sep,
		pSChan->pDataCodeL[sampNum], pSChan->pDataCodeR[sampNum]);
	}
    }
    (void)fputc(sep, out);
    if (pSChan->pData == NULL)
	(void)fprintf(out, "%*s", colWidth, "no_data");
    else if (pSChan->pFlags[sampNum].eof)
	(void)fprintf(out, "%*s", colWidth, "EOF");
    else if (pSChan->pFlags[sampNum].missing)
	(void)fprintf(out, "%*s", colWidth, "no_data");
    else {
	if (type == DBR_TIME_STRING)
	    (void)fprintf(out, "%*s", colWidth, ((char *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_FLOAT)
	    (void)fprintf(out, "%*.*f", colWidth,
			pSChan->precision, ((float *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_SHORT)
	    (void)fprintf(out, "%*d", colWidth,
			((short *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_DOUBLE)
	    (void)fprintf(out, "%*.*f", colWidth,
		pSChan->precision, ((double *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_LONG)
	    (void)fprintf(out, "%*d", colWidth, ((long *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_CHAR)
	    (void)fprintf(out, "%*d", colWidth, ((char *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_ENUM) {
	    short	val;

	    val = ((short *)pSChan->pData)[sampNum];
	    if (val < pSChan->grBuf.genmval.no_str)
		(void)fprintf(out, "\"%*s\"", colWidth,
				pSChan->grBuf.genmval.strs[val]);
	    else
		(void)fprintf(out, "%*d", colWidth, val);
	}
    }

    if (postFlag) {
	if (pSChan->pData == NULL)
	    (void)fprintf(out, "%c%c%c", sep,'M','D');
	else {
	    (void)fprintf(out, "%c%c%c", sep,
		pSChan->pDataCodeL[sampNum], pSChan->pDataCodeR[sampNum]);
	}
    }
}

/*+/internal******************************************************************
* NAME	sydSamplePrintArray
*
*-*/
static
sydSamplePrintArray(pSChan, sampNum)
SYD_CHAN *pSChan;	/* I pointer to sync channel */
int	sampNum;	/* I sample number in sync set */
{
    int		nEl, nBytes, i;
    char	*pSrc;
    double	value;
    char	text[7];

    nEl = pSChan->elCount;
    nBytes = dbr_value_size[pSChan->dbrType];
    pSrc = (char *)pSChan->pData + sampNum * nBytes * nEl;

    (void)printf("%s %d\n", pSChan->name, nEl);
    for (i=0; i<nEl; i++) {
	if (i % 10 == 0)
	    (void)printf("%05d ", i);
        if      (dbr_type_is_FLOAT(pSChan->dbrType))
            value = *(float *)pSrc;
        else if (dbr_type_is_SHORT(pSChan->dbrType))
            value = *(short *)pSrc;
        else if (dbr_type_is_DOUBLE(pSChan->dbrType))
            value = *(double *)pSrc;
        else if (dbr_type_is_LONG(pSChan->dbrType))
            value = *(long *)pSrc;
        else if (dbr_type_is_CHAR(pSChan->dbrType))
            value = *(unsigned char *)pSrc;
        else if (dbr_type_is_ENUM(pSChan->dbrType))
            value = *(short *)pSrc;
	sydCvtDblToTxt(text, 6, value, pSChan->precision);
	(void)printf(" %6s", text);
	if ((i+1) % 10 == 0 || i+1 >= nEl)
	    (void)printf("\n");
	pSrc += nBytes;
    }
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
	    (void)fprintf(pFile, "%s %s %d %d", SydChanName(pSChan),
		dbf_type_to_text(SydChanDbfType(pSChan)),
		pSChan->pDataAlSev[samp], pSChan->pDataAlStat[samp]);
	    sydSamplePrint1(pSChan, pFile, ' ', 0, 0, 1, 1, samp);
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
	pSspec->pDeltaSec = (double *)GenMalloc(reqCount * sizeof(double));
	if (pSspec->pDeltaSec == NULL)
	    goto sampleMallocErr;
	pSspec->pTimeStamp = (TS_STAMP *)GenMalloc(reqCount * sizeof(TS_STAMP));
	if (pSspec->pTimeStamp == NULL)
	    goto sampleMallocErr;
	pSspec->pPartial = (char *)GenMalloc(reqCount * sizeof(char));
	if (pSspec->pPartial == NULL)
	    goto sampleMallocErr;
	pSspec->dataDim = reqCount;
    }

    for (pSChan=pSspec->pChanHead; pSChan!=NULL; pSChan=pSChan->pNext) {
	if (pSChan->conn == 0)
	    ;			/* no action if never been connected */
	else if (pSChan->pData == NULL) {
	    pSChan->pData = (void *)GenMalloc(reqCount *
			dbr_value_size[pSChan->dbrType] * pSChan->elCount);
	    if (pSChan->pData == NULL)
		goto sampleMallocErr;

	    pSChan->pDataAlStat = (void *)GenMalloc(reqCount *
						sizeof(*pSChan->pDataAlStat));
	    if (pSChan->pDataAlStat == NULL)
		goto sampleMallocErr;
	    pSChan->pDataAlSev = (void *)GenMalloc(reqCount *
						sizeof(*pSChan->pDataAlSev));
	    if (pSChan->pDataAlSev == NULL)
		goto sampleMallocErr;
	    pSChan->pDataCodeL = (void *)GenMalloc(reqCount *
						sizeof(*pSChan->pDataCodeL));
	    if (pSChan->pDataCodeL == NULL)
		goto sampleMallocErr;
	    pSChan->pDataCodeR = (void *)GenMalloc(reqCount *
						sizeof(*pSChan->pDataCodeR));
	    if (pSChan->pDataCodeR == NULL)
		goto sampleMallocErr;
	    pSChan->pFlags = (void *)GenMalloc(reqCount *
						sizeof(*pSChan->pFlags));
	    if (pSChan->pFlags == NULL)
		goto sampleMallocErr;
	}
    }
    pSspec->reqCount = reqCount;
    pSspec->firstData = -1;
    pSspec->lastData = -1;
    pSspec->sampleCount = 0;
    pSspec->refTs.secPastEpoch = pSspec->refTs.nsec = 0;
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

    samp = pSspec->firstData;
    while (samp >= 0) {
	sydSampleExport(pSspec, out, option, samp);
	if (samp == pSspec->lastData)
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
	GenFree((char *)pSspec->pDeltaSec);
	pSspec->pDeltaSec = NULL;
    }
    if (pSspec->pTimeStamp != NULL) {
	GenFree((char *)pSspec->pTimeStamp);
	pSspec->pTimeStamp = NULL;
    }
    if (pSspec->pPartial != NULL) {
	GenFree((char *)pSspec->pPartial);
	pSspec->pPartial = NULL;
    }
    pSspec->dataDim = 0;
    pSspec->sampleCount = 0;
    pSspec->firstData = -1;
    pSspec->lastData = -1;

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
* o	doesn't handle array channels
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
    }

    pSspec->firstData = -1;
    pSspec->lastData = -1;
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

    samp = pSspec->firstData;
    while (samp >= 0) {
	if (lineCount == 0 && nCol <= 0)
	    headerFlag = 1;
	sydSamplePrint(pSspec, out, formatFlag, headerFlag, nCol, colWidth, samp);
	headerFlag = 0;
	if (++lineCount > 60)
	    lineCount = 0;
	if (samp == pSspec->lastData)
	    samp = -1;
	else if (++samp >= pSspec->dataDim)
	    samp = 0;
    }
    return S_syd_OK;
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

    samp = pSspec->firstData;
    while (samp >= 0) {
	sydSampleWriteSSF(pSspec, pFile, reqDesc, sampDesc, samp);
	if (samp == pSspec->lastData)
	    samp = -1;
	else if (++samp >= pSspec->dataDim)
	    samp = 0;
    }
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydTriggerClose - add a sample trigger condition from a text string
*
* DESCRIPTION
*	Scans a text string which specifies a sample trigger condition and
*	builds a trigger structure for the synchronous set spec
*
*	demand
*	event  chanName condition [duration]
*
* RETURNS
*	S_syd_OK, or
*	S_syd_ERROR if an error is detected
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1.	The trigger structure must be closed by calling sydTriggerClose.
*
* EXAMPLE
*
*-*/
long
sydTriggerClose(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    assert(pSspec != NULL);

    if (pSspec->pTrigger == NULL)
	return S_syd_OK;
    GenFree((char *)pSspec->pTrigger);
    pSspec->pTrigger = NULL;
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydTriggerAddFromText - add a sample trigger from a text string
*
* DESCRIPTION
*	Scans a text string which specifies a sample trigger condition and
*	builds a trigger structure for the synchronous set spec
*
*	always
*	delay hh:mm:ss
*	demand
*	event  chanName condition [duration]
*	never
*	periodic hh:mm:ss [duration]
*	while  chanName condition [duration]
*
*	This routine adds a synchronous channel structure to the synchronous
*	set spec, flagging the sync channel struct as a `trigger channel'.
*	If the sync set spec already has a sync channel struct for the
*	channel, then that sync channel struct will be flagged as both
*	a `data channel' and also a `trigger channel'.  Programs using
*	trigger channels which aren't also data channels must distinguish
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
* 1.	The trigger structure must be closed by calling sydTriggerClose.
*	(This happens automatically when sydClose is called.)
*
* EXAMPLE
*
*-*/
long
sydTriggerAddFromText(pSspec, text)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
char	*text;		/* I pointer to string with trigger specification */
{
    char	*myText=NULL;
    char	*pText, *pFld, delim, *pMsg;
    SYD_TRIG	*pTrig=NULL;
    int		allowDuration=0;

    assert(pSspec != NULL);
    assert(text != NULL);

    if (pSspec->pTrigger != NULL) {
	(void)printf("sydTriggerScan: sync set spec already has trigger\n");
	return S_syd_ERROR;
    }
    if ((myText = (char *)malloc(strlen(text))) == NULL) {
	(void)printf("sydTriggerScan: couldn't malloc for text temp\n");
	return S_syd_noMem;
    }
    pText = myText;
    if ((pTrig = (SYD_TRIG *)GenMalloc(sizeof(SYD_TRIG))) == NULL) {
	(void)printf("sydTriggerScan: couldn't malloc for trig struct\n");
	free(myText);
	return S_syd_noMem;
    }

    if (nextAlphField(&pText, &pFld, &delim) <= 1) {
	pMsg = "no trigger keyword found";
	goto trigScanErr;
    }
    if (strcmp(pFld,      "always") == 0)	pTrig->type = SYD_TR_ALWAYS;
    else if (strcmp(pFld, "delay") == 0) 	pTrig->type = SYD_TR_DELAY;
    else if (strcmp(pFld, "demand") == 0)	pTrig->type = SYD_TR_DEMAND;
    else if (strcmp(pFld, "event") == 0) 	pTrig->type = SYD_TR_EVENT;
    else if (strcmp(pFld, "never") == 0)	pTrig->type = SYD_TR_NEVER;
    else if (strcmp(pFld, "periodic") == 0) 	pTrig->type = SYD_TR_PERIODIC;
    else if (strcmp(pFld, "while") == 0) 	pTrig->type = SYD_TR_WHILE;
    else {
	pMsg = "illegal trigger type";
	goto trigScanErr;
    }
    if (pTrig->type == SYD_TR_EVENT || pTrig->type == SYD_TR_WHILE) {
/*-----------------------------------------------------------------------------
*    event chanName condition [duration]
*    while chanName condition [duration]
*----------------------------------------------------------------------------*/
	allowDuration=1;
	
    }
    else if (pTrig->type == SYD_TR_DELAY || pTrig->type == SYD_TR_PERIODIC) {
/*-----------------------------------------------------------------------------
*    delay hh:mm:ss
*    periodic hh:mm:ss [duration]
*----------------------------------------------------------------------------*/
	if (pTrig->type == SYD_TR_PERIODIC)
	    allowDuration=1;
    }

    free(myText);
    pSspec->pTrigger = pTrig;
    return S_syd_OK;

trigScanErr:
    (void)printf("sydTriggerScan: %s\n%s\n", pMsg, myText);
    sydTriggerClose(pTrig);
    free(myText);
    return S_syd_ERROR;
}
