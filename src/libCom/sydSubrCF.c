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
* TITLE	sydSubrCF.c - acquire synchronous samples from `by channel' AR file
*
* DESCRIPTION
*	
*-***************************************************************************/
#include <genDefs.h>
#define SYD_PRIVATE
#include <sydDefs.h>
#include <alarm.h>
#ifndef INC_arAccessLib_h
#   include <arAccessLib.h>
#endif
#ifndef INC_tsDefs_h
#   include <tsDefs.h>
#endif
#ifndef INC_arCS_h
#   include <arCS.h>
#endif
#ifndef INCLcadefh
#   include <cadef.h>
#endif

#ifdef vxWorks
#   include <vxWorks.h>
#   include <ioLib.h>		/* for O_RDWR and O_RDONLY definitions */
#   include <stdioLib.h>
#else
#   include <stdio.h>
#   include <sys/file.h>	/* for O_RDWR and O_RDONLY definitions */
#endif

long sydCFFunc();
long sydCFFuncGetGR();

long
sydOpenCF(ppSspec, pHandle)
SYD_SPEC **ppSspec;	/* O pointer to synchronous set spec pointer */
void	*pHandle;	/* I pointer to handle for "source" */
{
    long	stat;

    assert(ppSspec != NULL);

    if ((*ppSspec = (SYD_SPEC *)GenMalloc(sizeof(SYD_SPEC))) == NULL)
	return S_syd_noMem;
    (*ppSspec)->pFunc = sydCFFunc;
    (*ppSspec)->type = SYD_TY_CF;
    if ((stat = sydCFFunc(*ppSspec, NULL, SYD_FC_INIT, pHandle)) != S_syd_OK){
	GenFree((char *)*ppSspec);
	*ppSspec = NULL;
	return stat;
    }
    (*ppSspec)->nInBufs = 2;
    return sydOpen(ppSspec, pHandle);
}
/*+/subr**********************************************************************
* NAME	sydCFFunc - handle "by channel" data file interactions
*
* DESCRIPTION
*
* sydCFFunc(pSspec, NULL, SYD_FC_INIT, NULL)	open "by channel" file
* sydCFFunc(pSspec, pSChan, SYD_FC_OPEN, NULL)  chanName already in pSChan
* sydCFFunc(pSspec, pSChan, SYD_FC_READ, NULL)
* sydCFFunc(pSspec, NULL, SYD_FC_READ_SYNC, NULL)  sync "by channel" file
* sydCFFunc(pSspec, pSChan, SYD_FC_POSITION, &stamp)
* sydCFFunc(pSspec, pSChan, SYD_FC_CLOSE, NULL)
* sydCFFunc(pSspec, NULL, SYD_FC_FILEINFO, outFile)
* sydCFFunc(pSspec, NULL, SYD_FC_WRAPUP, NULL)	close "by channel" file
*
* RETURNS
*	S_syd_OK, or
*	S_syd_EOF, or
*	other code indicating error
*
* BUGS
* o	doesn't detect or report EOF
* o	doesn't do anything (or even detect) overwriting un-sampled
*	buffers (for SYD_FC_READ)
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydCFFunc(pSspec, pStruct, funcCode, pArg)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
void	*pStruct;	/* IO pointer to data struct used by funcCode */
enum sydFuncCode funcCode;/* I function code */
void	*pArg;		/* I pointer to arg, as required by funcCode */
{
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    AR_CHAN_DESC *pChanDesc;	/* pointer to chanDesc for channel */
    AR_CF_DESC	*pCfDesc;	/* pointer to cfDesc for channel file */
    long	retStat=S_syd_OK;
    long	stat, inStatus;
    int		i;

    pSChan = (SYD_CHAN *)pStruct;
    if (pSChan != NULL)
	pChanDesc = (AR_CHAN_DESC *)pSChan->pHandle;
    if (pSspec != NULL)
	pCfDesc = (AR_CF_DESC *)pSspec->pHandle;

    if (funcCode ==					SYD_FC_INIT) {
	pCfDesc = arCFOpen("arChan/arChanData.bin", O_RDONLY);
	if (pCfDesc == NULL) {
	    (void)printf("couldn't open arChan/arChanData.bin\n");
	    retStat = S_syd_ERROR;
	}
	pSspec->pHandle = (void *)pCfDesc;
    }
    else if (funcCode ==				SYD_FC_OPEN) {
	pChanDesc = arCFChanOpen(pCfDesc, pSChan->name, O_RDONLY);
	if (pChanDesc == NULL)
	    retStat = S_syd_chanNotFound;
	else {
	    pSChan->pHandle = (void *)pChanDesc;
	    pSChan->dbrType =
			    dbf_type_to_DBR_TIME(ArCFChanFieldType(pChanDesc));
	    pSChan->elCount = ArCFChanElementCount(pChanDesc);
	    sydCFFuncGetGR(pSChan, pChanDesc);
	}
    }
    else if (funcCode ==				SYD_FC_READ) {
	i = NEXT_INBUF(pSChan, pSChan->lastInBuf);
	pSChan->inStatus[i] = inStatus = SYD_B_EMPTY;
	while (pSChan->inStatus[i] == SYD_B_EMPTY) {
	    stat = arCFChanRead(pChanDesc,
		pSChan->dbrType, pSChan->pInBuf[i], pSChan->elCount);
	    if (stat == OK) {
		if (inStatus == SYD_B_EMPTY)
		    pSChan->inStatus[i] = SYD_B_FULL;
		else
		    pSChan->inStatus[i] = SYD_B_RESTART;
	    }
	    else if ((pChanDesc->flags & AR_CDESC_EOF) != 0)
		pSChan->inStatus[i] = SYD_B_EOF;
	    else if ((pChanDesc->flags & AR_CDESC_BEGIN) != 0)
		inStatus = SYD_B_RESTART;
	}
	pSChan->lastInBuf = i;
	if (pSChan->firstInBuf < 0)
	    pSChan->firstInBuf = i;
    }
    else if (funcCode ==				SYD_FC_READ_SYNC) {
	/* synchronize file buf in memory with data on disk */
	stat = arCFSyncRead(pCfDesc);
	if (stat != OK)
	    retStat = S_syd_ERROR;
    }
    else if (funcCode ==				SYD_FC_POSITION) {
	stat = arCFChanPosition(pChanDesc, (TS_STAMP *)pArg);
	if (stat != OK) {
	    if ((pChanDesc->flags & AR_CDESC_EOF) == 0) {
		(void)printf("error for %s\n", pSChan->name);
		assertAlways(0);
	    }
	}
    }
    else if (funcCode ==				SYD_FC_CLOSE) {
	if (pSChan->pHandle != NULL)
	    retStat = arCFChanClose((AR_CHAN_DESC **)&pSChan->pHandle);
    }
    else if (funcCode ==				SYD_FC_FILEINFO) {
	TS_STAMP	oldestStamp, newestStamp;
	char		oldestStampText[28], newestStampText[28];
	char		name[db_name_dim];
	int		j;

	assert(pCfDesc != NULL);
	if (ArB0MIHead(pCfDesc) != 0) {
	    AR_MI_BUF *pMICurrent;
	    AR_CHAN_HDR *pChanHdr;

	    assert(ArB0MIHead(pCfDesc) > 0);
	    assert(ArB0MITail(pCfDesc) > 0);
	    assert(ArB0MINblk(pCfDesc) > 0);
	    assert(pCfDesc->pMIHead != NULL);
	    assert(pCfDesc->pMITail != NULL);

	    pMICurrent = pCfDesc->pMIHead;
	    while (pMICurrent != NULL) {
		int i;

		for (i=0; i<AR_CF_NHDR; i++) {
		    pChanHdr = &pMICurrent->chanHdr[i];

		    if (pChanHdr->name[0] != '\0') {
			strcpy(name, pChanHdr->name);
			j = strlen(name);
			if (j > 4) {
			    if (strncmp(&name[j-4], ".VAL", 4) == 0)
				name[j-4] = '\0';
			}
			oldestStamp.secPastEpoch = pChanHdr->oldestSecPastEpoch;
			oldestStamp.nsec = 0;
			newestStamp.secPastEpoch = pChanHdr->newestSecPastEpoch;
			newestStamp.nsec = 0;
			(void)fprintf((FILE *)pArg, "%s %d %s %s  %s\n",
                                   name, pChanHdr->elCount,
                                   dbf_type_to_text(pChanHdr->chanType),
                                   tsStampToText(&oldestStamp, TS_TEXT_MMDDYY,
                                                        oldestStampText),
                                   tsStampToText(&newestStamp, TS_TEXT_MMDDYY,
                                                        newestStampText) );
		    }
		}    
		pMICurrent = pMICurrent->pNextMI;
	    }
	}
	else
	    (void)fprintf((FILE *)pArg, "no channels in file\n");
    }
    else if (funcCode ==				SYD_FC_WRAPUP) {
	if (pCfDesc != NULL) {
	    if ((stat = arCFClose(&pCfDesc)) != OK) {
		(void)printf("trouble closing arChan/arChanData.bin\n");
		retStat = S_syd_ERROR;
	    }
	}
	pSspec->pHandle = NULL;
    }

    return retStat;
}

long
sydCFFuncGetGR(pSChan, pChanDesc)
SYD_CHAN *pSChan;	/* pointer to syncSet channel descriptor */
AR_CHAN_DESC *pChanDesc;	/* pointer to chanDesc for channel */
{
    int		i;

    if (pSChan->dbrType == DBR_TIME_FLOAT) {
#define FLT_SRC ArCDChanHdr(pChanDesc).graphics.floatGr
#define FLT_DEST pSChan->grBuf.gfltval
	FLT_DEST.status = NO_ALARM;
	FLT_DEST.severity = NO_ALARM;
	FLT_DEST.precision = FLT_SRC.precision;
	(void)strcpy(FLT_DEST.units, FLT_SRC.units);
	FLT_DEST.upper_disp_limit = FLT_SRC.upDispLim;
	FLT_DEST.lower_disp_limit = FLT_SRC.lowDispLim;
	FLT_DEST.upper_alarm_limit = FLT_SRC.upAlmLim;
	FLT_DEST.lower_alarm_limit = FLT_SRC.lowAlmLim;
	FLT_DEST.upper_warning_limit = FLT_SRC.upWarnLim;
	FLT_DEST.lower_warning_limit = FLT_SRC.lowWarnLim;
	pSChan->dbrGrType = DBR_GR_FLOAT;
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
#define SHRT_SRC ArCDChanHdr(pChanDesc).graphics.shortGr
#define SHRT_DEST pSChan->grBuf.gshrtval
	SHRT_DEST.status = NO_ALARM;
	SHRT_DEST.severity = NO_ALARM;
	(void)strcpy(SHRT_DEST.units, SHRT_SRC.units);
	SHRT_DEST.upper_disp_limit = SHRT_SRC.upDispLim;
	SHRT_DEST.lower_disp_limit = SHRT_SRC.lowDispLim;
	SHRT_DEST.upper_alarm_limit = SHRT_SRC.upAlmLim;
	SHRT_DEST.lower_alarm_limit = SHRT_SRC.lowAlmLim;
	SHRT_DEST.upper_warning_limit = SHRT_SRC.upWarnLim;
	SHRT_DEST.lower_warning_limit = SHRT_SRC.lowWarnLim;
	pSChan->dbrGrType = DBR_GR_SHORT;
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
#define DBL_SRC ArCDChanHdr(pChanDesc).graphics.doubleGr
#define DBL_DEST pSChan->grBuf.gdblval
	DBL_DEST.status = NO_ALARM;
	DBL_DEST.severity = NO_ALARM;
	DBL_DEST.precision = DBL_SRC.precision;
	(void)strcpy(DBL_DEST.units, DBL_SRC.units);
	DBL_DEST.upper_disp_limit = DBL_SRC.upDispLim;
	DBL_DEST.lower_disp_limit = DBL_SRC.lowDispLim;
	DBL_DEST.upper_alarm_limit = DBL_SRC.upAlmLim;
	DBL_DEST.lower_alarm_limit = DBL_SRC.lowAlmLim;
	DBL_DEST.upper_warning_limit = DBL_SRC.upWarnLim;
	DBL_DEST.lower_warning_limit = DBL_SRC.lowWarnLim;
	pSChan->dbrGrType = DBR_GR_DOUBLE;
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
#define LNG_SRC ArCDChanHdr(pChanDesc).graphics.longGr
#define LNG_DEST pSChan->grBuf.glngval
	LNG_DEST.status = NO_ALARM;
	LNG_DEST.severity = NO_ALARM;
	(void)strcpy(LNG_DEST.units, LNG_SRC.units);
	LNG_DEST.upper_disp_limit = LNG_SRC.upDispLim;
	LNG_DEST.lower_disp_limit = LNG_SRC.lowDispLim;
	LNG_DEST.upper_alarm_limit = LNG_SRC.upAlmLim;
	LNG_DEST.lower_alarm_limit = LNG_SRC.lowAlmLim;
	LNG_DEST.upper_warning_limit = LNG_SRC.upWarnLim;
	LNG_DEST.lower_warning_limit = LNG_SRC.lowWarnLim;
	pSChan->dbrGrType = DBR_GR_LONG;
    }
    else if (pSChan->dbrType == DBR_TIME_STRING) {
#define STR_DEST pSChan->grBuf.gstrval
	STR_DEST.status = NO_ALARM;
	STR_DEST.severity = NO_ALARM;
	pSChan->dbrGrType = DBR_GR_STRING;
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
#define ENM_SRC ArCDChanHdr(pChanDesc).graphics.enumGr
#define ENM_DEST pSChan->grBuf.genmval
	ENM_DEST.status = NO_ALARM;
	ENM_DEST.severity = NO_ALARM;
	ENM_DEST.no_str = ArCFChanNStates(pChanDesc);
	for (i=0; i<ENM_DEST.no_str; i++)
	    (void)strcpy(ENM_DEST.strs[i], ArCFChanStates(pChanDesc)[i]);
	pSChan->dbrGrType = DBR_GR_ENUM;
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
#define CHR_SRC ArCDChanHdr(pChanDesc).graphics.charGr
#define CHR_DEST pSChan->grBuf.gchrval
	CHR_DEST.status = NO_ALARM;
	CHR_DEST.severity = NO_ALARM;
	(void)strcpy(CHR_DEST.units, CHR_SRC.units);
	CHR_DEST.upper_disp_limit = CHR_SRC.upDispLim;
	CHR_DEST.lower_disp_limit = CHR_SRC.lowDispLim;
	CHR_DEST.upper_alarm_limit = CHR_SRC.upAlmLim;
	CHR_DEST.lower_alarm_limit = CHR_SRC.lowAlmLim;
	CHR_DEST.upper_warning_limit = CHR_SRC.upWarnLim;
	CHR_DEST.lower_warning_limit = CHR_SRC.lowWarnLim;
	pSChan->dbrGrType = DBR_GR_CHAR;
    }
    return S_syd_OK;
}
