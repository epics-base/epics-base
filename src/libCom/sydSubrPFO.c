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
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	sydSubrPFO.c - acquire synchronous samples from `gsd_...' routines
*
* DESCRIPTION
*	
*-***************************************************************************/
#include <genDefs.h>
#define SYD_PRIVATE
#include <sydDefs.h>
#include <alarm.h>
#ifndef INC_tsDefs_h
#   include <tsDefs.h>
#endif
#ifndef INCLcadefh
#   include <cadef.h>
#endif
#include <gsd_sync_defs.h>
#define PFO_CHAN_DEF
#include <pfoPlotDefs.h>

#ifdef vxWorks
#   include <vxWorks.h>
#   include <ioLib.h>		/* for O_RDWR and O_RDONLY definitions */
#   include <stdioLib.h>
#else
#   include <stdio.h>
#   include <sys/file.h>	/* for O_RDWR and O_RDONLY definitions */
#endif

long sydPFOFunc();
long sydPFOFuncGetGR();

long
sydOpenPFO(ppSspec, pHandle)
SYD_SPEC **ppSspec;	/* O pointer to synchronous set spec pointer */
void	*pHandle;	/* I pointer to handle for "source" */
{
    long	stat;

    assert(ppSspec != NULL);

    if ((*ppSspec = (SYD_SPEC *)GenMalloc(sizeof(SYD_SPEC))) == NULL)
	return S_syd_noMem;
    (*ppSspec)->pFunc = sydPFOFunc;
    (*ppSspec)->type = SYD_TY_PFO;
    if ((stat = sydPFOFunc(*ppSspec, NULL, SYD_FC_INIT, pHandle)) != S_syd_OK){
	GenFree((char *)*ppSspec);
	*ppSspec = NULL;
	return stat;
    }
    (*ppSspec)->nInBufs = 2;
    return sydOpen(ppSspec, pHandle);
}
/*+/subr**********************************************************************
* NAME	sydPFOFunc - handle gsd_xxx sync data interactions
*
* DESCRIPTION
*
* sydPFOFunc(pSspec, NULL, SYD_FC_INIT, NULL)
* sydPFOFunc(pSspec, pSChan, SYD_FC_OPEN, pPfoChan)
* sydPFOFunc(pSspec, pSChan, SYD_FC_READ, NULL)
* sydPFOFunc(pSspec, pSChan, SYD_FC_POSITION, NULL)
* sydPFOFunc(pSspec, pSChan, SYD_FC_CLOSE, NULL)
* sydPFOFunc(pSspec, NULL, SYD_FC_FILEINFO, NULL)
* sydPFOFunc(pSspec, NULL, SYD_FC_WRAPUP, NULL)
*
* RETURNS
*	S_syd_OK, or
*	other code indicating error
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
sydPFOFunc(pSspec, pStruct, funcCode, pArg)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
void	*pStruct;	/* IO pointer to data struct used by funcCode */
enum sydFuncCode funcCode;/* I function code */
void	*pArg;		/* I pointer to arg, as required by funcCode */
{
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    PFO_CHAN	*pPChan;	/* pointer to pfo channel struct */
    struct gsd_sync_data *pSyn;	/* pointer to pfo sync data struct */
    long	retStat=S_syd_OK;
    int		bufNum, nBytes;

    pSChan = (SYD_CHAN *)pStruct;

    if (funcCode ==					SYD_FC_INIT) {
	pSspec->pHandle = NULL;
    }
    else if (funcCode ==				SYD_FC_OPEN) {
	pPChan = (PFO_CHAN *)pArg;
	pSChan->pHandle = (void *)pPChan->pGsd;
	pSChan->dbrType = pPChan->pGsd->time_type;
	assert(dbr_type_is_TIME(pSChan->dbrType));
	pSChan->elCount = pPChan->pGsd->count;
	sydPFOFuncGetGR(pSspec, pSChan, pPChan);
    }
    else if (funcCode ==				SYD_FC_READ) {
	pSyn = (struct gsd_sync_data *)pSChan->pHandle;
	if (pSyn->svalid != 0) {
	    bufNum = NEXT_INBUF(pSChan, pSChan->lastInBuf);
	    nBytes = dbr_size_n(pSChan->dbrType, pSChan->elCount);
	    bcopy((char *)pSyn->pSdata, (char *)pSChan->pInBuf[bufNum], nBytes);
	    pSChan->inStatus[bufNum] = SYD_B_FULL;
	    pSChan->lastInBuf = bufNum;
	    if (pSChan->firstInBuf < 0)
		pSChan->firstInBuf = bufNum;
	}
    }
    else if (funcCode ==				SYD_FC_POSITION) {
	;	/* no action */
    }
    else if (funcCode ==				SYD_FC_CLOSE) {
	;	/* no action */
    }
    else if (funcCode ==				SYD_FC_FILEINFO) {
	;	/* no action */
    }
    else if (funcCode ==				SYD_FC_WRAPUP) {
	pSspec->pHandle = NULL;
    }

    return retStat;
}

static long
sydPFOFuncGetGR(pSspec, pSChan, pPChan)
SYD_SPEC *pSspec;	/* pointer to syncSet specification */
SYD_CHAN *pSChan;	/* pointer to syncSet channel descriptor */
PFO_CHAN *pPChan;	/* pointer to pfo channel struct */
{
    int		i;

    if (pSChan->dbrType == DBR_TIME_FLOAT) {
	assert(pPChan->dbrGrType == DBR_GR_FLOAT ||
				pPChan->dbrGrType == DBR_CTRL_FLOAT);
	pSChan->grBuf.gfltval = pPChan->pDbrGr->gfltval;
	pSChan->dbrGrType = DBR_GR_FLOAT;
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
	assert(pPChan->dbrGrType == DBR_GR_SHORT ||
				pPChan->dbrGrType == DBR_CTRL_SHORT);
	pSChan->grBuf.gshrtval = pPChan->pDbrGr->gshrtval;
	pSChan->dbrGrType = DBR_GR_SHORT;
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
	assert(pPChan->dbrGrType == DBR_GR_DOUBLE ||
				pPChan->dbrGrType == DBR_CTRL_DOUBLE);
	pSChan->grBuf.gdblval = pPChan->pDbrGr->gdblval;
	pSChan->dbrGrType = DBR_GR_DOUBLE;
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
	assert(pPChan->dbrGrType == DBR_GR_LONG ||
				pPChan->dbrGrType == DBR_CTRL_LONG);
	pSChan->grBuf.glngval = pPChan->pDbrGr->glngval;
	pSChan->dbrGrType = DBR_GR_LONG;
    }
    else if (pSChan->dbrType == DBR_TIME_STRING) {
	assert(pPChan->dbrGrType == DBR_GR_STRING ||
				pPChan->dbrGrType == DBR_CTRL_STRING);
	pSChan->grBuf.gstrval = pPChan->pDbrGr->gstrval;
	pSChan->dbrGrType = DBR_GR_STRING;
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
	assert(pPChan->dbrGrType == DBR_GR_ENUM ||
				pPChan->dbrGrType == DBR_CTRL_ENUM);
	pSChan->grBuf.genmval = pPChan->pDbrGr->genmval;
	pSChan->dbrGrType = DBR_GR_ENUM;
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
	assert(pPChan->dbrGrType == DBR_GR_CHAR ||
				pPChan->dbrGrType == DBR_CTRL_CHAR);
	pSChan->grBuf.gchrval = pPChan->pDbrGr->gchrval;
	pSChan->dbrGrType = DBR_GR_FLOAT;
    }
    return S_syd_OK;
}
