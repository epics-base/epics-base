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
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	sydSubr.c - routines for building and using "synchronous data specs"
*
* DESCRIPTION
*	These routines provide for acquiring and handling sets of
*	synchronous samples of data.
*
*	The functions provided are:
*	o   create and/or alter a synchronous data specification via
*	    subroutine calls
*	o   read a synchronous data specification from a file (not yet
*	    implemented)
*	o   get next n synchronous samples
*	o   print in tabular form the samples in a synchronous sample set
*	o   plot the samples in a synchronous sample set
*	o   make a hard copy of either the tabular printout or the plot
*	o   write a file containing the samples in a synchronous sample set,
*	    with various options on the file format
*
*	These routines can obtain synchronous sample sets from any of several
*	sources, including archiver "by channel" data files, archiver
*	"sample set" data files, and Channel Access.  (Only a single
*	source can be used at once for a particular sync data spec, but
*	several sync data spec's can exist simultaneously.)
*
* QUICK REFERENCE
*
* #include <genDefs.h>		some general use definitions
* #include <db_access.h>	definitions for database related items
* #include <sydDefs.h>	structures for synchronous data routines
*
* SYD_SPEC *pSspec;	pointer to synchronous set spec
* SYD_CHAN *pSChan;	pointer to synchronous channel descriptor
*
*     long  sydChanClose(        pSspec,  pSChan			)
* SYD_CHAN *sydChanFind(         pSspec,  chanName			)
*     long  sydChanOpen(         pSspec,  chanName,  sync, pArg		)
*				         sync = SYD_SY_{NONF,FILLED}
*     long  sydClose(            pSspec					)
*     long  sydFileInfo(         pSspec,  out                           )
*     long  sydOpenCA(          >ppSspec, NULL				)
*     long  sydOpenCF(          >ppSspec, pHandle			)
*     long  sydOpenPFO(         >ppSspec, pHandle			)
*     long  sydOpenSSF(         >ppSspec, pHandle			)
*     long  sydReadSync(         pSspec         			)
*     void  sydResetBuf(         pSspec					)
*     void  sydResetBufKeepNewest(pSspec				)
*     void  sydResetBufSampled(  pSspec					)
*     long  sydSampleGet(        pSspec					)
*     long  sydSamplePrint(      pSspec,  out,  fmtFlag,  hdrFlag,  sampNum)
*     void  sydSampleStore(      pSspec,  sampleNum			)
*
*     long  sydSampleSetAlloc(   pSspec,  reqCount			)
*     long  sydSampleSetFree(    pSspec					)
*     long  sydSampleSetGet(     pSspec					)
*     long  sydSampleSetPrint(   pSspec,  out,  fmtFlag			)
*
* BUGS
* o	error detection and handling is in VERY ratty shape
* o	sydOpenCF should allow specifying a file name
*	
*-***************************************************************************/
#include <genDefs.h>
#define SYD_PRIVATE
#include <sydDefs.h>
#include <fields.h>
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

/*+/subr**********************************************************************
* NAME	sydChanClose - delete a channel from a synchronous data spec
*
* DESCRIPTION
*	This routine deletes a channel from a synchronous data spec.
*
* RETURNS
*	S_syd_OK
*
* SEE ALSO
*	sydOpen(), sydChanOpen(), sydChanFind()
*	sydGetNextSample()
*
*-*/
long
sydChanClose(pSspec, pSChan)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
SYD_CHAN *pSChan;	/* IO pointer to synchronous channel descriptor */
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
* NAME	sydChanFind - find a channel in a synchronous data spec
*
* DESCRIPTION
*	This routine finds a channel in a synchronous data spec.
*
* RETURNS
*	SYD_CHAN * for channel, if found, or
*	NULL
*
*-*/
SYD_CHAN *
sydChanFind(pSspec, chanName)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
char	*chanName;	/* I channel name to find in synchronous data spec */
{
    SYD_CHAN	*pSChan;	/* pointer to channel descriptor */

    assert(pSspec != NULL);
    assert(chanName != NULL);

    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
        if (strcmp(pSChan->name, chanName) == 0)
	    break;
	pSChan = pSChan->pNext;
    }

    return pSChan;
}

/*+/subr**********************************************************************
* NAME	sydChanOpen - add a channel to a synchronous data spec
*
* DESCRIPTION
*	This routine adds a channel to a synchronous data spec.
*
* RETURNS
*	S_syd_OK, or
*	S_syd_noMem		no memory for adding channel to set
*	S_syd_chanNotFound	channel not found in file
*	S_syd_ERROR		if channel already exists in sync data spec
*
* BUGS
* o	always places channel at end of list
*
* SEE ALSO
*	sydOpen(), sydChanClose()
*	sydGetNextSample()
*
* NOTES
* 1.	For channels in AR sample set data files, SYD_SY_NONF if forced,
*	without comment.
* 2.	DBF_ENUM channels are flagged as SYD_ST_STEP; all other channels
*	are flagged as SYD_ST_SMOOTH.
*
* EXAMPLE
*
*-*/
long
sydChanOpen(pSspec, chanName, sync, pArg)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
char	*chanName;	/* I channel name (.VAL assumed if field omitted) */
enum sydChanSync sync;	/* I sync/non-sync; SYD_SY_NONF or SYD_SY_FILLED */
void	*pArg;		/* I pointer to struct needed to init, or NULL */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    enum sydChanStep step;	/* step type: SYD_ST_STEP or SYD_ST_CONTIN */
    int		i;

    assert(pSspec != NULL);
    assert(strlen(chanName) > 0);
    assert(strlen(chanName) < db_name_dim);

    if (sydChanFind(pSspec, chanName) != NULL)
	return S_syd_ERROR;

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
	pSChan->sync = sync;
	pSChan->pData = NULL;
	pSChan->pDataAlStat = NULL;
	pSChan->pDataAlSev = NULL;
	pSChan->pDataCodeL = NULL;
	pSChan->pDataCodeR = NULL;
	pSChan->pFlags = NULL;
    }
    else
	return S_syd_noMem;
/*-----------------------------------------------------------------------------
*    "open" the "source" for the data for the channel.  This will provide
*    information about how large the buffers need to be.
*----------------------------------------------------------------------------*/
    stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_OPEN, pArg);
    if (stat != S_syd_OK) {
	(void)sydChanClose(pSspec, pSChan);
	return stat;
    }
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
    if (retStat != S_syd_OK)
	return retStat;
    if (pSChan->dbrType != DBR_TIME_ENUM)
	pSChan->step = SYD_ST_SMOOTH;
    else
	pSChan->step = SYD_ST_STEP;
    if (pSChan->dbrType == DBR_TIME_FLOAT) {
        pSChan->precision = pSChan->grBuf.gfltval.precision;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.gfltval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
        pSChan->precision = 0;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.gshrtval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
        pSChan->precision = pSChan->grBuf.gdblval.precision;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.gdblval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
        pSChan->precision = 0;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.glngval.units);
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
        pSChan->precision = 0;
        sprintf(pSChan->label, "%s  %s",
                                pSChan->name, pSChan->grBuf.gchrval.units);
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

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydClose - close a synchronous data spec
*
* DESCRIPTION
*	This routine closes a synchronous data spec, closing channels, etc.,
*	wrapping up the retrieval "source", and then free()'s the data
*	structures associated with the set.
*
* RETURNS
*	S_syd_OK, or
*	other status codes if an error occurs
*
* BUGS
* o	text
*
* SEE ALSO
*	sydOpen()
*
* EXAMPLE
*
*-*/
long
sydClose(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
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

/*+/macro*********************************************************************
* NAME	sydOpen - create and initialize an empty synchronous data spec
*
* DESCRIPTION
*	This routine builds an empty synchronous data spec structure.  In
*	order to use the SYD_SPEC, channels for synchronous samples must
*	be specified.  Retrieval will be for synchronous samples of data,
*	in which all channels have the same time stamp; retrieval begins
*	with the oldest data available and continues to the most recent.
*	Several versions of this routine are available, corresponding
*	to the source of the data:
*
*	sydOpenCA(&pSspec, NULL)	init for Channel Access
*	sydOpenCF(&pSspec, NULL)	init for "by channel" file
*	sydOpenPFO(&pSspec, fileName)	init for OPI sync data
*	sydOpenSSF(&pSspec, fileName)	init for "sample set" file
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
*	sydChanOpen(), sydAddTrigger(), sydAddTimeRange()
*	sydGetNextSample()
*
* EXAMPLE
*
*-*/
long
sydOpen(ppSspec, pHandle)
SYD_SPEC **ppSspec;	/* O pointer to synchronous set spec pointer */
void	*pHandle;	/* I pointer to handle for "source" */
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
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydReadSync - synchronize read buffers with disk
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
sydReadSync(pSspec)
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
* NAME	sydResetBuf - reset synchronous sample buffers for channels
*
* DESCRIPTION
*	This routine flags as EMPTY the input buffers for the channels in a
*	synchronous data spec.  Two routines are available:
*
*	sydResetBuf() flags as empty all buffers for all channels
*			This routine is appropriate to use after a
*			positioning operation, since such an operation
*			invalidates both FULL and SAMPLED buffers.
*
*	sydResetBufKeepNewest() flags as empty all buffers except the
*				newest one, which is flagged as SAMPLED
*			This routine is primarily intended for use with
*			Channel Access.  Keeping the newest buffer and
*			flagging it as SAMPLED allows: a) using it if
*			no new value comes in; or b) throwing it away
*			if a new value does come in.
*	sydResetBufSampled() flags as empty only those buffers which
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
sydResetBuf(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
{
    SYD_CHAN	*pSChan;
    int		i;

    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
	for (i=0; i<pSChan->nInBufs; i++)
	    pSChan->inStatus[i] = SYD_B_EMPTY;
	pSChan->firstInBuf = -1;
	pSChan->lastInBuf = -1;
	pSChan = pSChan->pNext;
    }
    pSspec->sampleTs.secPastEpoch = 0;
}

void
sydResetBufKeepNewest(pSspec)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
{
    SYD_CHAN	*pSChan;
    int		i;

    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
	i = pSChan->firstInBuf;
	while (i >= 0 && i != pSChan->lastInBuf) {
	    pSChan->inStatus[i] = SYD_B_EMPTY;
	    i = NEXT_INBUF(pSChan, i);
	    pSChan->firstInBuf = i;
	}
	if ((i=pSChan->firstInBuf) > 0)
	    pSChan->inStatus[i] = SYD_B_SAMPLED;
	pSChan->sampInBuf = -1;
	pSChan = pSChan->pNext;
    }
    pSspec->sampleTs.secPastEpoch = 0;
}

void
sydResetBufSampled(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
{
    SYD_CHAN	*pSChan;
    int		i;

    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
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
	pSChan = pSChan->pNext;
    }
    pSspec->sampleTs.secPastEpoch = 0;
}

/*+/subr**********************************************************************
* NAME	sydSampleFilter
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	should use precision from graphic info for printing floats
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
sydSampleFilter(pSspec, out, option, samp)
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
	(void)fprintf(out, "\"%s\"\ntime", tsStampToText(
		    &pSspec->refTs, TS_TEXT_MMDDYY, stampText));
	pSChan = pSspec->pChanHead;
	while (pSChan != NULL) {
	    if (option == 2)
		(void)fprintf(out, "\tstat");
	    (void)fprintf(out, "\t%s", pSChan->name);
	    pSChan = pSChan->pNext;
	}
	(void)fprintf(out, "\n");
    }

/*-----------------------------------------------------------------------------
*    print the value for each channel for this sample.
*----------------------------------------------------------------------------*/
    (void)fprintf(out, "%.3f", pSspec->pDeltaSec[samp]);
    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
	if (option == 1)
	    sydSamplePrint1(pSChan, out, '\t', 0, 0, 0, samp);
	else if (option == 2)
	    sydSamplePrint1(pSChan, out, '\t', 1, 0, 0, samp);
	pSChan = pSChan->pNext;
    }
    (void)fprintf(out, "\n");
}

/*+/subr**********************************************************************
* NAME	sydSampleGet - get the next synchronous sample
*
* DESCRIPTION
*	This routine acquires the next sample for the channels in the
*	synchronous data spec.  The desired time stamp for the `next sample'
*	is determined by searching forward from the time of the previous
*	sample.  Among the channels in the synchronous data spec, the one
*	which changes soonest is the one whose time stamp is used for the
*	next sample.  Channels are handled on either a "non-filled" or a
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
*	The sample buffer for each channel in the synchronous set is indicated
*	by the .sampInBuf item in the channel structure.  The status of the
*	sample buffer (from the input) is in the .inStatus[sampInBuf] item.
*	Several cases may be encountered:
*	o   inStatus is SYD_B_FULL, SYD_B_RESTART, or SYD_B_SAMPLED; the
*	    buffer contains valid sample data.  Once the caller has dealt
*	    with the sample, THE CALLER MUST SET THE SAMPLE BUFFER STATUS
*	    TO SYD_B_SAMPLED!!!
*	o   inStatus is SYD_B_MISSING or SYD_B_EOF; no data is available
*	o   .sampInBuf is < 0; no data is available
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
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long 
sydSampleGet(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    int		readIndx;	/* select buffer to use for read */
    SYD_CHAN	*pSChan;	/* pointer to channel in Sspec */
    double	refDiff;	/* difference from reference time  */
    TS_STAMP	nextTs;		/* time stamp for next sample */
    int		i, i1, j, now=0, later=0;

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
    sydSampleGetIn(pSspec);

    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
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
*    status of S_syd_noDataNow.
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
		if (j - pSChan->firstInBuf < 3)
		    now = 1;
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
		if (j - pSChan->firstInBuf < 3)
		    now = 1;
	    }
	    if (nextTs.secPastEpoch == 0 || TsCmpStampsLT(&BUFi1TS, &nextTs))
		nextTs = BUFi1TS;
	}
	else if (pSChan->inStatus[i1] == SYD_B_MISSING) {
	    if (TsCmpStampsLT(&BUFi1TS, &nextTs))
		nextTs = BUFi1TS;
	}
	else
	    later = 1;

	pSChan = pSChan->pNext;
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
	return S_syd_EOF;

    if (pSspec->refTs.secPastEpoch == 0) {
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
*----------------------------------------------------------------------------*/

    pSspec->priorTs = pSspec->sampleTs;
    pSspec->sampleTs = nextTs;
    pSspec->partial = 0;
    TsDiffAsDouble(&pSspec->sampleSec, &pSspec->sampleTs, &pSspec->refTs);
    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
	i = pSChan->firstInBuf;
	i1 = NEXT_INBUF(pSChan, i);
	if (i < 0) {
	    pSChan->sampInBuf = -1;
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}
	else if ((pSChan->inStatus[i] == SYD_B_FULL ||
		    pSChan->inStatus[i] == SYD_B_RESTART) &&
					TsCmpStampsLE(&BUFiTS, &nextTs)) {
	    pSChan->sampInBuf = i;
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
	    }
	    else {
		pSChan->reused = 1;
		pSChan->sampInBuf = i;
	    }
	}
	else if (pSChan->inStatus[i] == SYD_B_EOF) {
	    pSChan->sampInBuf = i;
	    pSspec->eofCount++;
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}
	else {
	    pSChan->sampInBuf = -1;
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}
	if (pSChan->sampInBuf >= 0 &&
			pSChan->inStatus[pSChan->sampInBuf] == SYD_B_MISSING) {
	    pSspec->partial = 1;
	    retStat = S_syd_partial;
	}

	pSChan = pSChan->pNext;
    }
    pSChan = pSspec->pChanHead;

    return retStat;
}

/*+/subr**********************************************************************
* NAME	sydSampleGetIn - get new input data, possibly discarding old
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
void 
sydSampleGetIn(pSspec)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
{
    long	retStat=S_syd_OK;/* return status to caller */
    long	stat;           /* status return from calls */
    SYD_CHAN	*pSChan;	/* pointer to channel in Sspec */
    int		i, i1, discard;

    assert(pSspec != NULL);
    assert(pSspec->pChanHead != NULL);

/*----------------------------------------------------------------------------
*    try to get both buffers for the channel to be SYD_B_FULL, performing
*    reads as necessary.  When processing is complete, buffer status will be
*    either SYD_B_FULL, SYD_B_SAMPLED, SYD_B_MISSING, SYD_B_EMPTY,
*    or SYD_B_EOF.
*----------------------------------------------------------------------------*/
    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
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
	pSChan = pSChan->pNext;
    }
    if (pSspec->type == SYD_TY_CA)
	return;
    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
/*-----------------------------------------------------------------------------
*    after possibly throwing away the oldest input data, try to get two
*    buffers of input data
*----------------------------------------------------------------------------*/
	if (pSChan->firstInBuf < 0) {
	    stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_READ, NULL);
	    if (pSspec->type == SYD_TY_PFO)
		goto skipSecondRead;
	}
	if (pSChan->firstInBuf == pSChan->lastInBuf)
	    stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_READ, NULL);
skipSecondRead:
	pSChan = pSChan->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydSamplePrint
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	should use precision from graphic info for printing floats
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydSamplePrint(pSspec, out, formatFlag, headerFlag, samp)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
FILE	*out;		/* IO stream pointer for output */
int	formatFlag;	/* I ==1 causes page formatting for printing */
int	headerFlag;	/* I ==1 causes printing of column headings */
int	samp;		/* I sample number in synchronous set */
{
    SYD_CHAN	*pSChan;	/* pointer to channel in synchronous set */
    char	stampText[28];
    int		i;

/*-----------------------------------------------------------------------------
*    print a heading line with channel names; if this isn't the first page,
*    put a ^L prior to the heading.
*----------------------------------------------------------------------------*/
    if (headerFlag) {
	if (formatFlag) {	/* indent if printing */
	    if (samp != pSspec->firstData)
		(void)fprintf(out, "\f");
	    (void)fprintf(out, "\n\n");
	    (void)fprintf(out, "    ");
	}
	(void)fprintf(out, "  %21s", " ");
	pSChan = pSspec->pChanHead;
	while (pSChan != NULL) {
	    (void)fprintf(out, " %14s", pSChan->name);
	    pSChan = pSChan->pNext;
	}
	(void)fprintf(out, "\n");
    }
/*-----------------------------------------------------------------------------
*    print the value for each channel for this sample.  Print the status
*    flags following values, but print only the first element for array
*    channels.
*----------------------------------------------------------------------------*/
    if (formatFlag)		/* indent if printing */
	(void)fprintf(out, "    ");
    if (pSspec->pPartial[samp])
	(void)fprintf(out, "*");
    else
	(void)fprintf(out, " ");
    (void)fprintf(out, " %s", tsStampToText(
		    &pSspec->pTimeStamp[samp], TS_TEXT_MMDDYY, stampText));
    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
	sydSamplePrint1(pSChan, out, ' ', 0, 1, 0, samp);
	pSChan = pSChan->pNext;
    }
    (void)fprintf(out, "\n");
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
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
void
sydSamplePrint1(pSChan, out, sep, preFlag, postFlag, showArray, sampNum)
SYD_CHAN *pSChan;	/* I pointer to coincidence channel */
FILE	*out;		/* I file pointer for writing value */
char	sep;		/* I character to use a a prefix for each field,
				as a separator; usually ' ' or '\t' */
int	preFlag;	/* I != 0 prints status flag prior to value */
int	postFlag;	/* I != 0 prints status flag following value */
int	showArray;	/* I != 0 to show all array elements, not just 1st */
int	sampNum;	/* I sample number in coincidence set */
{
    int		i;
    chtype	type;		/* type of value */

    type = pSChan->dbrType;

    if (preFlag) (void)fprintf(out, "%c%c%c", sep,
		pSChan->pDataCodeL[sampNum], pSChan->pDataCodeR[sampNum]);
    (void)fputc(sep, out);
    if (pSChan->pFlags[sampNum].eof)
	(void)fprintf(out, "%11s", "EOF");
    else if (pSChan->pFlags[sampNum].missing)
	(void)fprintf(out, "%11s", "no_data");
    else {
	if (type == DBR_TIME_STRING)
	    (void)fprintf(out, "%11s", ((char *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_FLOAT)
	    (void)fprintf(out, "%11.*f",
		pSChan->precision, ((float *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_SHORT)
	    (void)fprintf(out, "%11d", ((short *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_DOUBLE)
	    (void)fprintf(out, "%11.*f",
		pSChan->precision, ((double *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_LONG)
	    (void)fprintf(out, "%11d", ((long *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_CHAR)
	    (void)fprintf(out, "%11d", ((char *)pSChan->pData)[sampNum]);
	else if (type == DBR_TIME_ENUM) {
	    short	val;

	    val = ((short *)pSChan->pData)[sampNum];
	    if (val < pSChan->grBuf.genmval.no_str)
		(void)fprintf(out, "%11s", pSChan->grBuf.genmval.strs[val]);
	    else
		(void)fprintf(out, "%11d", val);
	}
    }

    if (postFlag) (void)fprintf(out, "%c%c%c", sep,
		pSChan->pDataCodeL[sampNum], pSChan->pDataCodeR[sampNum]);
}

sydSamplePrintArray(pSChan, sampNum)
SYD_CHAN *pSChan;	/* I pointer to coincidence channel */
int	sampNum;	/* I sample number in coincidence set */
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
	pprCvtDblToTxt(text, 6, value, pSChan->precision);
	(void)printf(" %6s", text);
	if ((i+1) % 10 == 0 || i+1 >= nEl)
	    (void)printf("\n");
	pSrc += nBytes;
    }
}

/*+/subr**********************************************************************
* NAME	sydSampleStore - store a sample into the sample set
*
* DESCRIPTION
*	Stores a sample acquired by sydSampleGet into the next available
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
sydSampleStore(pSspec)
SYD_SPEC *pSspec;
{
    int		sub;		/* subscript to store sample */
    chtype	type;
    long	stat;
    SYD_CHAN	*pSChan;
    int		i, el;
    int		useVal;		/* indicates if sample data is to be stored */
    short	alStat, alSev;
    static struct sydChanFlags flags0={0,0,0,0,0,0,0};
 
    if (++pSspec->lastData >= pSspec->dataDim)
	pSspec->lastData = 0;
    sub = pSspec->lastData;

    pSspec->pDeltaSec[sub] = pSspec->sampleSec;
    pSspec->pTimeStamp[sub] = pSspec->sampleTs;
    pSspec->pPartial[sub] = pSspec->partial;
    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
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
	    else if (alSev == MAJOR)
		pSChan->pDataCodeR[sub] = 'M';
	    else if (alSev == MINOR)
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
	pSChan = pSChan->pNext;
    }

    if (pSspec->firstData < 0 || pSspec->firstData == pSspec->lastData) {
	if (++pSspec->firstData >= pSspec->dataDim)
	    pSspec->firstData = 0;
    }
    if (pSspec->sampleCount < pSspec->dataDim)
	pSspec->sampleCount++;
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
*	routines.
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

	pSChan = pSspec->pChanHead;
	while (pSChan != NULL) {
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

	    pSChan = pSChan->pNext;
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
* NAME	sydSampleSetFilter - filter sample set data to output
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
sydSampleSetFilter(pSspec, out, option)
FILE	*out;		/* IO stream pointer for output */
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
int	option;		/* I filtering option */
{
    int		samp;		/* sample number in synchronous set */

    samp = pSspec->firstData;
    while (samp >= 0) {
	sydSampleFilter(pSspec, out, option, samp);
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

    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
	sydChanFreeArrays(pSChan);
	pSChan = pSChan->pNext;
    }
    return S_syd_OK;
}

/*/subr------------------------------------------------------------------------
* NAME sydSampleSetGet - get the samples for a sample set
*
* BUGS
* o	doesn't handle array channels
*----------------------------------------------------------------------------*/
long
sydSampleSetGet(pSspec)
SYD_SPEC *pSspec;
{
    long	stat;

    pSspec->firstData = -1;
    pSspec->lastData = -1;
    pSspec->sampleCount = 0;
    stat = S_syd_OK;
    while (stat != S_syd_EOF && pSspec->sampleCount < pSspec->reqCount) {
	if ((stat = sydSampleGet(pSspec)) != S_syd_EOF)
	    sydSampleStore(pSspec);
    }
    return stat;
}

/*+/subr**********************************************************************
* NAME	sydSampleSetPosition - position the data file
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
sydSampleSetPosition(pSspec, pStamp)
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
TS_STAMP *pStamp;	/* I stamp at which to position; NULL to rewind */
{
    SYD_CHAN	*pSChan;
    long	stat;

    pSChan = pSspec->pChanHead;
    while (pSChan != NULL) {
	stat = (pSspec->pFunc)(pSspec, pSChan, SYD_FC_POSITION, pStamp);
	if (pSspec->type == SYD_TY_SSF)
	    break;
	pSChan = pSChan->pNext;
    }
    sydResetBuf(pSspec);
    pSspec->sampleTs.secPastEpoch = pSspec->sampleTs.nsec = 0;
    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydSampleSetPrint - print the samples in a sample set
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
sydSampleSetPrint(pSspec, out, formatFlag)
FILE	*out;		/* IO stream pointer for output */
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
int	formatFlag;	/* I ==1 causes page formatting for printing */
{
    int		samp;		/* sample number in synchronous set */
    int		lineCount=0;
    int		headerFlag;

    samp = pSspec->firstData;
    while (samp >= 0) {
	if (lineCount == 0)
	    headerFlag = 1;
	sydSamplePrint(pSspec, out, formatFlag, headerFlag, samp);
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
