/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	12-04-90
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
 *  .00 12-04-90 rac	initial version
 *  .01 06-18-91 rac	installed in SCCS
 *  .02 06-19-91 rac	replace <fields.h> with <alarm.h>
 *  .03 08-14-91 rac	jjj
 *  .04 02-27-92 rac	do ts rounding here instead of sydSubr.c
 *  .05 08-18-92 rac	add SYD_FC_STOP function code
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	sydSubr.c - acquire synchronous samples from AR `sample set' files
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

long sydSSFFunc();
long sydSSFFuncGetGR();
long sydSSFFuncReadData();
long sydSSFFuncSeekSample();
long sydSSFFuncSeekStamp();

/*+/macro*********************************************************************
* NAME	posXxx - text file search and positioning routines
*
* DESCRIPTION
*	These routines allow setting the position for text files.  The
*	file may be positioned at its beginning or its end.  It is
*	supported to search, either forward or backward, for a string,
*	leaving the file positioned at the beginning of the string.
*
*	long	pos;
*	FILE	*pStream;
*
*	pos = posBOF(pStream);
*	pos = posEOF(pStream);
*	pos = posKeyFwd("key", pStream);
*	pos = posKeyRev("key", pStream, pos);
*	pos = posPos(pStream, pos);
*
* RETURNS
*	position, or
*	<0 if an error occurs
*
* BUGS
* o	the use of fseek and ftell doesn't match the Standard C specification
*	and may be non-portable.  They have been verified to function
*	properly under SunOS 4.1 and VxWorks 4.0.2.
*
*-*/
static long posBOF();
static long posEOF();
static long posKeyFwd();
static long posKeyRev();
static long posPos();

static long
posBOF(pStream)
FILE	*pStream;
{
    if (fseek(pStream, 0L, 0) < 0) {
	perror("posBOF: fseek error");
	return -1;
    }
    return ftell(pStream);
}

static long
posEOF(pStream)
FILE	*pStream;
{
    if (fseek(pStream, 0L, 2) < 0) {
	perror("posEOF: fseek error");
	return -1;
    }
    return ftell(pStream);
}

static long
posKeyFwd(key, pStream)
char	*key;
FILE	*pStream;
{
    int		i, lastChar;
    int		c;
    long	pos;

    lastChar = strlen(key) - 1;
    i = 0;

    if ((pos = ftell(pStream)) < 0) {
	if (!feof(pStream))
	    perror("posKeyFwd: ftell error");
	return -1;
    }

    while (1) {
	if ((c = fgetc(pStream)) == EOF)
	    return -1;
	if ((char)c == key[i]) {
	    if (i == lastChar)
		break;
	    i++;
	}
	else {
	    if (i == 0) {
		if ((pos = ftell(pStream)) < 0) {
		    if (!feof(pStream))
			perror("posKeyFwd: ftell error");
		    return -1;
		}
	    }
	    else if (i == 1) {
		pos++;
		if ((char)c != key[0])
		    i = 0;
	    }
	    else {
		pos++;
		if (fseek(pStream, pos, 0) != 0) {
		    if (!feof(pStream))
			perror("posKeyFwd: fseek error");
		    return -1;
		}
		i = 0;
	    }
	}
    }
    if (fseek(pStream, pos, 0) != 0) {
	if (!feof(pStream))
	    perror("posKeyFwd: fseek error");
	return -1;
    }
    return pos;
}

static long
posKeyRev(key, pStream, lastPos)
char	*key;
FILE	*pStream;
long	lastPos;	/* I position to start search if reverse; -1
				says to start at EOF */
{
    int		i, lastChar;
    int		c;
    long	pos;

    lastChar = i = strlen(key) - 1;

    if (lastPos >= 0) {
	if (fseek(pStream, lastPos, 0) != 0) {
	    if (!feof(pStream))
		perror("posKeyRev: fseek error");
	    return -1;
	}
    }
    else {
	if (fseek(pStream, 0L, 2) != 0) {
	    if (!feof(pStream))
		perror("posKeyRev: fseek error");
	    return -1;
	}
    }
    while (1) {
	if (ftell(pStream) <= 0)
	    return -1;
	if (fseek(pStream, -1L, 1) != 0) {
	    if (!feof(pStream))
		perror("posKeyRev: fseek error");
	    return -1;
	}
	if ((c = fgetc(pStream)) == EOF)
	    return -1;
	if (fseek(pStream, -1L, 1) != 0) {
	    if (!feof(pStream))
		perror("posKeyRev: fseek error");
	    return -1;
	}
	if ((char)c == key[i]) {
	    if (i == 0)
		break;
	    i--;
	}
	else
	    i = lastChar;
    }
    if ((pos = ftell(pStream)) < 0)
	perror("posKeyRev: ftell error");
    return pos;
}

static long
posPos(pStream, pos)
FILE	*pStream;
long	pos;
{
    if (fseek(pStream, pos, 0) < 0) {
	perror("posPos: fseek error");
	return -1;
    }
    return ftell(pStream);
}

long
sydOpenSSF(ppSspec, filePath)
SYD_SPEC **ppSspec;	/* O pointer to synchronous set spec pointer */
char	*filePath;	/* I path name for `sample set' archive file */
{
    long	stat;

    assert(ppSspec != NULL);

    if ((*ppSspec = (SYD_SPEC *)GenMalloc(sizeof(SYD_SPEC))) == NULL)
	return S_syd_noMem;
    (*ppSspec)->pFunc = sydSSFFunc;
    (*ppSspec)->type = SYD_TY_SSF;
    if ((stat = sydSSFFunc(*ppSspec, NULL,SYD_FC_INIT,filePath)) != S_syd_OK){
	GenFree((char *)*ppSspec);
	*ppSspec = NULL;
	return stat;
    }
    (*ppSspec)->nInBufs = 2;
    return sydOpen(ppSspec);
}
/*+/subr**********************************************************************
* NAME	sydSSFFunc - handle "sample set" data file interactions
*
* DESCRIPTION
*
* sydSSFFunc(pSspec, NULL, SYD_FC_INIT, filePath)  open "sample set" file
* sydSSFFunc(pSspec, pSChan, SYD_FC_OPEN, NULL)  chanName already in pSChan
* sydSSFFunc(pSspec, pSChan, SYD_FC_READ, NULL)
* sydSSFFunc(pSspec, pSChan, SYD_FC_POSITION, &stamp)
* sydSSFFunc(pSspec, pSChan, SYD_FC_CLOSE, NULL)
* sydSSFFunc(pSspec, NULL, SYD_FC_FILEINFO, outStream)
* sydSSFFunc(pSspec, NULL, SYD_FC_WRAPUP, NULL)	close "sample set" file
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
* o	needs a "get graphics information" function and function code
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydSSFFunc(pSspec, pStruct, funcCode, pArg)
SYD_SPEC *pSspec;	/* IO pointer to synchronous set spec */
void	*pStruct;	/* IO pointer to data struct used by funcCode */
enum sydFuncCode funcCode;/* I function code */
void	*pArg;		/* I pointer to arg, as required by funcCode */
{
    SYD_CHAN	*pSChan;	/* pointer to syncSet channel descriptor */
    FILE	*ssFile;	/* file pointer for sample set data file */
    long	retStat=S_syd_OK;
    long	stat;
    int		i;
    long	pos;
    char	record[120];
    char	recDelim;
    char	*pRecord;
    char	*pField;
    char	delim;
    char	chanName[db_name_dim+2];
    chtype	type;
    int		elCount;

    pSChan = (SYD_CHAN *)pStruct;
    if (pSspec != NULL)
	ssFile = (FILE *)pSspec->pHandle;

    if (funcCode ==					SYD_FC_INIT) {
	if ((ssFile = fopen((char *)pArg, "r")) == NULL) {
	    (void)printf("couldn't open %s\n", (char *)pArg);
	    perror(NULL);
	    retStat = S_syd_ERROR;
	}
	pSspec->pHandle = (void *)ssFile;
    }
    else if (funcCode ==				SYD_FC_OPEN) {
	(void)posBOF(ssFile);
	if ((pos = posKeyFwd("%endData%", ssFile)) < 0) {
	    (void)printf("couldn't find %%endData%%\n");
	    (void)posBOF(ssFile);
	    return S_syd_chanNotFound;
	}
	strcpy(chanName, "\n");
	strcat(chanName, pSChan->name);
	strcat(chanName, " ");
	if ((pos = posKeyRev(chanName, ssFile, pos)) < 0)
		return S_syd_chanNotFound;
	assert(sizeof(record) == 120);
	fgetc(ssFile);		/* skip over the \n */
	if (fgets(record, 120, ssFile) == NULL)
	    assertAlways(0);
	pRecord = record;
	recDelim = record[strlen(record)-1];
	if (nextChanNameField(&pRecord, &pField, &delim) <= 1)
	    assertAlways(0);
	if (strcmp(pField, pSChan->name) != 0)
	    assertAlways(0);
	pSChan->pHandle = NULL;
	if (nextAlphField(&pRecord, &pField, &delim) <= 1)
	    assertAlways(0);
	dbf_text_to_type(pField, type);
	assert(type >= 0);
	pSChan->dbfType = type;
	pSChan->dbrType = dbf_type_to_DBR_TIME(type);
	pSChan->dbrGrType = dbf_type_to_DBR_GR(type);
	stat = sscanf(pRecord, "%*d %*d %d", &elCount);
	assert(stat == 1);
	pSChan->elCount = elCount;
	pSChan->sync = SYD_SY_NONF;	/* force channel to be sync */
	sydSSFFuncGetGR(pSspec, pSChan);
	(void)posBOF(ssFile);
    }
    else if (funcCode ==				SYD_FC_READ) {
	TS_STAMP timeStamp;
	enum sydBStatus bufStat;
	int	bufNum, oldBufNum;

/*-----------------------------------------------------------------------------
*    find the next line with SAMPLE: and get the time stamp.  When done:
*    o  bufStat will be one of:
*		SYD_B_RESTART	this sample starts a new "run" of data
*		SYD_B_FULL	this is an ordinary sample
*		SYD_B_EOF	end of file
*    o  the file will be positioned following the %endHeader% line
*    o  if time stamp rounding has been requested, it will be done
*----------------------------------------------------------------------------*/
	bufStat = sydSSFFuncSeekSample(ssFile, &timeStamp);
	if (pSspec->roundNsec > 0)
	    sydTsRound(&timeStamp, pSspec->roundNsec);

/*-----------------------------------------------------------------------------
*    now, read the actual data.  This is done by processing all the data
*    lines in the file, storing those which match a channel in the synchronous
*    set.  (This is somewhat contrary to the "advertising" for this function
*    code, which implies that only a single channel is read.)  When done:
*    o  all channels in the set will have an appropriate buffer status
*    o  all channels will have the same time stamp
*    o  all channels with data will have the data in the buffer
*    o  all channels will have .lastInBuf updated
*----------------------------------------------------------------------------*/
	pSChan = pSspec->pChanHead;
	oldBufNum = pSChan->lastInBuf;
	bufNum = NEXT_INBUF(pSChan, oldBufNum);
	while (pSChan != NULL) {
	    if (bufStat != SYD_B_EOF)
		pSChan->inStatus[bufNum] = SYD_B_MISSING;
	    else
		pSChan->inStatus[bufNum] = bufStat;
	    pSChan->pInBuf[bufNum]->tfltval.stamp = timeStamp;
	    pSChan->lastInBuf = bufNum;
	    if (pSChan->firstInBuf < 0)
	        pSChan->firstInBuf = bufNum;
	    pSChan = pSChan->pNext;
	}
	if (bufStat == SYD_B_EOF)
	    return S_syd_EOF;
	while (1) {
	    stat = fscanf(ssFile, "%s", chanName);
	    assert(stat == 1);
	    if ((pSChan = sydChanFind(pSspec, chanName)) != NULL) {
		(void)fscanf(ssFile, "%*s");	/* skip DBF_xxx */
		stat = sydSSFFuncReadData(pSChan, ssFile, 
						bufStat, bufNum, oldBufNum);
	    }
	    else if (strcmp(chanName, "%endData%") == 0)
		break;
	    else {
		while ((delim = fgetc(ssFile)) != EOF && delim != '\n')
		    ;	/* keep skipping until end of line */
	    }
	}
    }
    else if (funcCode ==				SYD_FC_STOP) {
    }
    else if (funcCode ==				SYD_FC_POSITION) {
	TS_STAMP *pStamp;
	TS_STAMP stamp, earlyStamp;
	long	earlyPos;

	pStamp = (TS_STAMP *)pArg;
	if (pStamp == NULL) {
	    (void)posBOF(ssFile);
	    return retStat;
	}
/*-----------------------------------------------------------------------------
*    find 2 adjacent samples to make it easy to determine which way to
*    search.  There are several cases:
*    1. the file is at least 2 away from EOF and both stamps can be found
*	forward
*    2.	the file is 1 away from EOF--1 stamp backward and 1 forward
*    3.	the file contains no stamps
*    4.	the file is at EOF and both stamps must be found backward
*    5. the file only contains one stamp
*
*    Cases 2 and 4 are treated by rewinding the file to avoid any backward
*    searching.  This transforms those cases into one of the other cases.
*
*    When this section is done, `earlyStamp' and `stamp' will contain two
*    adjacent stamps.  If they are equal, then case 4 exists.  The file
*    will be positioned following `stamp'.
*----------------------------------------------------------------------------*/
	earlyPos = sydSSFFuncSeekStamp(ssFile, &earlyStamp, 0, 0L);
	pos = sydSSFFuncSeekStamp(ssFile, &stamp, 0, 0L);
	if (earlyPos < 0 || pos < 0) {
	    (void)posBOF(ssFile);
	    earlyPos = sydSSFFuncSeekStamp(ssFile, &earlyStamp, 0, 0L);
	    if ((pos = sydSSFFuncSeekStamp(ssFile, &stamp, 0, 0L)) < 0) {
		stamp = earlyStamp;
		pos = earlyPos;
	    }
	}
	if (earlyPos < 0)
	    return S_syd_ERROR;
	if (TsCmpStampsLT(pStamp, &earlyStamp)) {
	    (void)posBOF(ssFile);
	    earlyPos = sydSSFFuncSeekStamp(ssFile, &earlyStamp, 0, 0L);
	    if ((pos = sydSSFFuncSeekStamp(ssFile, &stamp, 0, 0L)) < 0) {
		stamp = earlyStamp;
		pos = earlyPos;
	    }
	}
	if (earlyPos < 0)
	    return S_syd_ERROR;
/*-----------------------------------------------------------------------------
*    now, find the record with the proper time stamp, going forward
*----------------------------------------------------------------------------*/
	while (1) {
	    if (TsCmpStampsLE(pStamp, &earlyStamp))
		break;
	    else if (TsCmpStampsLE(pStamp, &stamp) &&
	    				TsCmpStampsGE(pStamp, &earlyStamp)) {
		if (TsCmpStampsEQ(pStamp, &earlyStamp)) {
		    stamp = earlyStamp;
		    pos = earlyPos;
		}
		break;
	    }
	    else if (TsCmpStampsEQ(&earlyStamp, &stamp))
		break;
	    else if (TsCmpStampsGT(pStamp, &stamp)) {
		earlyStamp = stamp;
		earlyPos = pos;
		pos = sydSSFFuncSeekStamp(ssFile, &stamp, 0, earlyPos);
		if (pos < 0) {
		    stamp = earlyStamp;
		    pos = earlyPos;
		}
	    }
	}
/*-----------------------------------------------------------------------------
*    finally, position the file at the proper record
*----------------------------------------------------------------------------*/
	if (TsCmpStampsGT(pStamp, &stamp))
	    return S_syd_EOF;
	if (TsCmpStampsLE(pStamp, &earlyStamp))
	    (void)posPos(ssFile, earlyPos);
	else
	    (void)posPos(ssFile, pos);
    }
    else if (funcCode ==				SYD_FC_CLOSE) {
	;	/* no action to close a channel in sample set files */
    }
    else if (funcCode ==				SYD_FC_FILEINFO) {
	TS_STAMP    oldestStamp;
	char        oldestStampText[28];
	TS_STAMP    newestStamp;
	char        newestStampText[28];
	int		j;

	if (posBOF(ssFile) < 0) {
	    (void)fprintf((FILE *)pArg, "error positioning file\n");
	    return S_syd_ERROR;
	}
	if (sydSSFFuncSeekStamp(ssFile, &oldestStamp, 0, 0L) < 0) {
	    (void)fprintf((FILE *)pArg, "error finding oldest time stamp\n");
	    return S_syd_ERROR;
	}
	if (sydSSFFuncSeekStamp(ssFile, &newestStamp, 1, -1L) < 0) {
	    (void)fprintf((FILE *)pArg, "error finding newest time stamp\n");
	    return S_syd_ERROR;
	}
	(void)fprintf((FILE *)pArg, "data from %s through %s\n",
	       tsStampToText(&oldestStamp, TS_TEXT_MMDDYY, oldestStampText),
               tsStampToText(&newestStamp, TS_TEXT_MMDDYY, newestStampText) );
	if (posBOF(ssFile) < 0) {
	    (void)fprintf((FILE *)pArg, "error positioning file\n");
	    return S_syd_ERROR;
	}
	if (posKeyFwd("%endHeader%", ssFile) < 0) {
	    (void)fprintf((FILE *)pArg,
				"error locating %%endHeader%% in file\n");
	    return S_syd_ERROR;
	}
	(void)fgets(record, 120, ssFile);	/* skip over endHeader */
	while (1) {
	    if (fgets(record, 120, ssFile) == NULL) {
		(void)fprintf((FILE *)pArg, "error reading channel name\n");
		return S_syd_ERROR;
	    }
	    pRecord = record;
	    recDelim = record[strlen(record)-1];
	    if (nextChanNameField(&pRecord, &pField, &delim) <= 1) {
		(void)fprintf((FILE *)pArg, "error reading channel type\n");
		return S_syd_ERROR;
	    }
	    if (strcmp(pField, "%endData%") == 0)
		break;
	    (void)strcpy(chanName, pField);
	    if (nextAlphField(&pRecord, &pField, &delim) <= 1) {
		(void)fprintf((FILE *)pArg, "error reading channel type\n");
		return S_syd_ERROR;
	    }
	    if (sscanf(pRecord, "%*d %*d %d", &elCount) != 1) {
		(void)fprintf((FILE *)pArg, "error reading element count\n");
		return S_syd_ERROR;
	    }
	    j = strlen(chanName);
	    if (j > 4) {
		if (strncmp(&chanName[j-4], ".VAL", 4) == 0)
		    chanName[j-4] = '\0';
	    }
	
	    (void)fprintf((FILE *)pArg, "%s %d %s\n",
						chanName, elCount, pField);
	    if (recDelim != '\n') {
		while ((delim = fgetc(ssFile)) != EOF && delim != '\n')
		    ;	/* keep skipping until end of line */
	    }
	}
	if (posBOF(ssFile) < 0) {
	    (void)fprintf((FILE *)pArg, "error positioning file\n");
	    return S_syd_ERROR;
	}
    }
    else if (funcCode ==				SYD_FC_WRAPUP) {
	if (ssFile != NULL)
	    (void)close(ssFile);
	pSspec->pHandle = NULL;
    }

    return retStat;
}

long
sydSSFFuncReadData(pSChan, ssFile, bufStat, bufNum, oldBufNum)
SYD_CHAN *pSChan;	/* pointer to syncSet channel descriptor */
FILE	*ssFile;
int	bufStat, bufNum, oldBufNum;
{
    long	stat;
    int		alStat, alSev, elCount;
    int		i;

/*-----------------------------------------------------------------------------
*    first, get and store the alarm status and severity .
*    (This is done using the "float" form of the buffer, which is
*    possible because for these items all types of buffers look the same.)
*----------------------------------------------------------------------------*/
    stat = fscanf(ssFile, "%d%d%d", &alSev, &alStat, &elCount);
    assert(stat == 3);
    pSChan->pInBuf[bufNum]->tfltval.status = alStat;
    pSChan->pInBuf[bufNum]->tfltval.severity = alSev;
/*-----------------------------------------------------------------------------
*    now get the actual data
*----------------------------------------------------------------------------*/
    if (pSChan->dbrType == DBR_TIME_FLOAT) {
	float	*pFl;
	pFl = &pSChan->pInBuf[bufNum]->tfltval.value;
	for (i=0; i<elCount; i++) {
	    stat = fscanf(ssFile, "%f", pFl++);
	    assert(stat == 1);
	}
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
	short	*pSh;
	pSh = &pSChan->pInBuf[bufNum]->tshrtval.value;
	for (i=0; i<elCount; i++) {
	    stat = fscanf(ssFile, "%hd", pSh++);
	    assert(stat == 1);
	}
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
	double	*pDbl;
	pDbl = &pSChan->pInBuf[bufNum]->tdblval.value;
	for (i=0; i<elCount; i++) {
	    stat = fscanf(ssFile, "%lf", pDbl++);
	    assert(stat == 1);
	}
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
	long	*pL;
	pL = &pSChan->pInBuf[bufNum]->tlngval.value;
	for (i=0; i<elCount; i++) {
	    stat = fscanf(ssFile, "%ld", pL++);
	    assert(stat == 1);
	}
    }
    else if (pSChan->dbrType == DBR_TIME_STRING) {
	char	*pC;
	int	nChar;
	pC = pSChan->pInBuf[bufNum]->tstrval.value;
	for (i=0; i<elCount; i++) {
	    stat = fscanf(ssFile, "%d", &nChar);
	    assert(stat == 1);
	    assert(nChar > 0 && nChar < db_strval_dim);
	    if (fgets(pC, nChar, ssFile) == NULL)
		assertAlways(0);
	    pC += db_strval_dim;
	}
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
	unsigned char	*pC;
	pC = &pSChan->pInBuf[bufNum]->tchrval.value;
	for (i=0; i<elCount; i++) {
	    stat = fscanf(ssFile, "%c", pC++);
	    assert(stat == 1);
	}
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
	short	*pSh;
	char	state[80];
	int	iState;
	pSh = &pSChan->pInBuf[bufNum]->tenmval.value;
	for (i=0; i<elCount; i++) {
	    stat = fscanf(ssFile, "%s", state);
	    assert(stat == 1);
	    assert(strlen(state) < db_state_text_dim);
	    iState = 0;
	    while (1) {
		if (iState >= pSChan->grBuf.genmval.no_str) {
		    pSChan->grBuf.genmval.no_str++;
		    (void)strcpy(pSChan->grBuf.genmval.strs[iState], state);
		    break;
		}
		else if (strcmp(pSChan->grBuf.genmval.strs[iState],state) == 0)
		    break;
		iState++;
		assert(iState < db_state_dim);
	    }
	    *pSh++ = iState;
	}
    }
/*-----------------------------------------------------------------------------
*    set the buffer status.  If the previous record had a status of MISSING,
*    then the buffer status will be SYD_B_RESTART; otherwise, the caller's
*    status will be used.
*----------------------------------------------------------------------------*/
    if (oldBufNum >= 0 && pSChan->inStatus[oldBufNum] == SYD_B_MISSING)
	pSChan->inStatus[bufNum] = SYD_B_RESTART;
    else
	pSChan->inStatus[bufNum] = bufStat;

    return S_syd_OK;
}

static long
sydSSFFuncGetGR(pSspec, pSChan)
SYD_SPEC *pSspec;	/* pointer to syncSet specification */
SYD_CHAN *pSChan;	/* pointer to syncSet channel descriptor */
{
    int		i;

    if (pSChan->dbrType == DBR_TIME_FLOAT) {
#define FLT_DEST pSChan->grBuf.gfltval
	FLT_DEST.status = NO_ALARM;
	FLT_DEST.severity = NO_ALARM;
#if 1
	FLT_DEST.precision = 3;
	(void)strcpy(FLT_DEST.units, " ");
	FLT_DEST.upper_disp_limit = 0.;
	FLT_DEST.lower_disp_limit = 0.;
	FLT_DEST.upper_alarm_limit = 0.;
	FLT_DEST.lower_alarm_limit = 0.;
	FLT_DEST.upper_warning_limit = 0.;
	FLT_DEST.lower_warning_limit = 0.;
#else
#define FLT_SRC ArCDChanHdr(pChanDesc).graphics.floatGr
	(void)strcpy(FLT_DEST.units, FLT_SRC.units);
	FLT_DEST.upper_disp_limit = FLT_SRC.upDispLim;
	FLT_DEST.lower_disp_limit = FLT_SRC.lowDispLim;
	FLT_DEST.upper_alarm_limit = FLT_SRC.upAlmLim;
	FLT_DEST.lower_alarm_limit = FLT_SRC.lowAlmLim;
	FLT_DEST.upper_warning_limit = FLT_SRC.upWarnLim;
	FLT_DEST.lower_warning_limit = FLT_SRC.lowWarnLim;
#endif
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
#define SHRT_DEST pSChan->grBuf.gshrtval
	SHRT_DEST.status = NO_ALARM;
	SHRT_DEST.severity = NO_ALARM;
#if 1
	(void)strcpy(SHRT_DEST.units, " ");
	SHRT_DEST.upper_disp_limit = 0;
	SHRT_DEST.lower_disp_limit = 0;
	SHRT_DEST.upper_alarm_limit = 0;
	SHRT_DEST.lower_alarm_limit = 0;
	SHRT_DEST.upper_warning_limit = 0;
	SHRT_DEST.lower_warning_limit = 0;
#else
#define SHRT_SRC ArCDChanHdr(pChanDesc).graphics.shortGr
	(void)strcpy(SHRT_DEST.units, SHRT_SRC.units);
	SHRT_DEST.upper_disp_limit = SHRT_SRC.upDispLim;
	SHRT_DEST.lower_disp_limit = SHRT_SRC.lowDispLim;
	SHRT_DEST.upper_alarm_limit = SHRT_SRC.upAlmLim;
	SHRT_DEST.lower_alarm_limit = SHRT_SRC.lowAlmLim;
	SHRT_DEST.upper_warning_limit = SHRT_SRC.upWarnLim;
	SHRT_DEST.lower_warning_limit = SHRT_SRC.lowWarnLim;
#endif
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
#define DBL_DEST pSChan->grBuf.gdblval
	DBL_DEST.status = NO_ALARM;
	DBL_DEST.severity = NO_ALARM;
#if 1
	DBL_DEST.precision = 3;
	(void)strcpy(DBL_DEST.units, " ");
	DBL_DEST.upper_disp_limit = 0.;
	DBL_DEST.lower_disp_limit = 0.;
	DBL_DEST.upper_alarm_limit = 0.;
	DBL_DEST.lower_alarm_limit = 0.;
	DBL_DEST.upper_warning_limit = 0.;
	DBL_DEST.lower_warning_limit = 0.;
#else
#define DBL_SRC ArCDChanHdr(pChanDesc).graphics.doubleGr
	(void)strcpy(DBL_DEST.units, DBL_SRC.units);
	DBL_DEST.upper_disp_limit = DBL_SRC.upDispLim;
	DBL_DEST.lower_disp_limit = DBL_SRC.lowDispLim;
	DBL_DEST.upper_alarm_limit = DBL_SRC.upAlmLim;
	DBL_DEST.lower_alarm_limit = DBL_SRC.lowAlmLim;
	DBL_DEST.upper_warning_limit = DBL_SRC.upWarnLim;
	DBL_DEST.lower_warning_limit = DBL_SRC.lowWarnLim;
#endif
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
#define LNG_DEST pSChan->grBuf.glngval
	LNG_DEST.status = NO_ALARM;
	LNG_DEST.severity = NO_ALARM;
#if 1
	(void)strcpy(LNG_DEST.units, " ");
	LNG_DEST.upper_disp_limit = 0;
	LNG_DEST.lower_disp_limit = 0;
	LNG_DEST.upper_alarm_limit = 0;
	LNG_DEST.lower_alarm_limit = 0;
	LNG_DEST.upper_warning_limit = 0;
	LNG_DEST.lower_warning_limit = 0;
#else
#define LNG_SRC ArCDChanHdr(pChanDesc).graphics.longGr
	(void)strcpy(LNG_DEST.units, LNG_SRC.units);
	LNG_DEST.upper_disp_limit = LNG_SRC.upDispLim;
	LNG_DEST.lower_disp_limit = LNG_SRC.lowDispLim;
	LNG_DEST.upper_alarm_limit = LNG_SRC.upAlmLim;
	LNG_DEST.lower_alarm_limit = LNG_SRC.lowAlmLim;
	LNG_DEST.upper_warning_limit = LNG_SRC.upWarnLim;
	LNG_DEST.lower_warning_limit = LNG_SRC.lowWarnLim;
#endif
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
#if 1
	ENM_DEST.no_str = 0;
#else
#define ENM_SRC ArCDChanHdr(pChanDesc).graphics.enumGr
	ENM_DEST.no_str = ArCFChanNStates(pChanDesc);
	for (i=0; i<ENM_DEST.no_str; i++)
	    (void)strcpy(ENM_DEST.strs[i], ArCFChanStates(pChanDesc)[i]);
#endif
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
#define CHR_DEST pSChan->grBuf.gchrval
	CHR_DEST.status = NO_ALARM;
	CHR_DEST.severity = NO_ALARM;
	(void)strcpy(CHR_DEST.units, " ");
#if 1
	CHR_DEST.upper_disp_limit = 0;
	CHR_DEST.lower_disp_limit = 0;
	CHR_DEST.upper_alarm_limit = 0;
	CHR_DEST.lower_alarm_limit = 0;
	CHR_DEST.upper_warning_limit = 0;
	CHR_DEST.lower_warning_limit = 0;
#else
#define CHR_SRC ArCDChanHdr(pChanDesc).graphics.charGr
	(void)strcpy(CHR_DEST.units, CHR_SRC.units);
	CHR_DEST.upper_disp_limit = CHR_SRC.upDispLim;
	CHR_DEST.lower_disp_limit = CHR_SRC.lowDispLim;
	CHR_DEST.upper_alarm_limit = CHR_SRC.upAlmLim;
	CHR_DEST.lower_alarm_limit = CHR_SRC.lowAlmLim;
	CHR_DEST.upper_warning_limit = CHR_SRC.upWarnLim;
	CHR_DEST.lower_warning_limit = CHR_SRC.lowWarnLim;
#endif
    }
    return S_syd_OK;
}

long
sydSSFFuncSeekStamp(ssFile, pStamp, revFlag, pos)
FILE	*ssFile;
TS_STAMP *pStamp;
int	revFlag;	/* I 1 says seek in reverse direction */
long	pos;		/* I position to start search if reverse; -1
				says to start at EOF */
{
    long	bufStat;
    char	record[120], recDelim;
    char	*pRecord, *pField, delim;
    long	stat;

    if (revFlag)
	pos = posKeyRev("SAMPLE: at", ssFile, pos);
    else 
	pos = posKeyFwd("SAMPLE: at", ssFile);
    if (pos < 0)
	return pos;
    assert(sizeof(record) == 120);
    if (fgets(record, 120, ssFile) == NULL)
	return -1;

    pRecord = record;
    recDelim = record[strlen(record)-1];
    if (nextNonSpaceField(&pRecord, &pField, &delim) <= 1)
	assertAlways(0);
    if (strcmp(pField, "SAMPLE:") != 0)
	assertAlways(0);
    if (nextNonSpaceField(&pRecord, &pField, &delim) <= 1)
	assertAlways(0);
    if (strcmp(pField, "at") != 0)
	assertAlways(0);
    stat = tsTextToStamp(pStamp, &pRecord);
    assert(stat == S_ts_OK);
    if (revFlag) {
	if (posPos(ssFile, pos) < 0)
	    assertAlways(0);
    }
    return pos;
}

long
sydSSFFuncSeekSample(ssFile, pStamp)
FILE	*ssFile;
TS_STAMP *pStamp;
{
    long	pos;
    long	bufStat;
    char	record[120], recDelim;
    char	*pRecord, *pField, delim;
    long	stat;

    if ((pos = posKeyFwd("SAMPLE: at", ssFile)) < 0)
	return SYD_B_EOF;
    assert(sizeof(record) == 120);
    if (fgets(record, 120, ssFile) == NULL)
	return SYD_B_EOF;

    pRecord = record;
    recDelim = record[strlen(record)-1];
    if (nextNonSpaceField(&pRecord, &pField, &delim) <= 1)
	assertAlways(0);
    if (strcmp(pField, "SAMPLE:") != 0)
	assertAlways(0);
    if (nextNonSpaceField(&pRecord, &pField, &delim) <= 1)
	assertAlways(0);
    if (strcmp(pField, "at") != 0)
	assertAlways(0);
    stat = tsTextToStamp(pStamp, &pRecord);
    assert(stat == S_ts_OK);
    while (*pRecord == '-')
	pRecord++;
    if (strncmp(pRecord, "initial", 7) == 0)
	bufStat = SYD_B_RESTART;
    else
	bufStat = SYD_B_FULL;
    if (recDelim != '\n') {
	while ((delim = fgetc(ssFile)) != EOF && delim != '\n')
	    ;	/* keep skipping until end of line */
    }
    if (posKeyFwd("%endHeader%", ssFile) < 0)
	return SYD_B_EOF;
    else if (fgets(record, 120, ssFile) == NULL)
	return SYD_B_EOF;
    return bufStat;
}
