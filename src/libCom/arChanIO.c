/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	03-09-90
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
 *  .01 03-09-90 rac	initial version
 *  .02 07-31-91 rac	installed in SCCS
 *  .03 09-19-91 rac	add a new assert check; fix bug in writing
 *			index blocks
 *  .04 09-14-92 rac	remove use of special malloc and free routines
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code
 */
/*+/mod***********************************************************************
* TITLE	arChanIO.c - AR channel data file routines
*
* DESCRIPTION
*	`Private' routines for the implementation of AR channel data files.
*	These routines are intended for use only by arAccessLib.c routines.
*	
* SEE ALSO
*	arAccessLib.h and arAccessLib.c for some code which needs to be
*			directly accessible to users of the AR access
*			library.
*-***************************************************************************/

#define ARACC_TEXT_GLBLSOURCE
#include <arAccessLib.h>
#ifndef INC_arCS_h
#   include <arCS.h>
#endif
#ifndef INC_genDefs_h
#   include <genDefs.h>
#endif
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
#else
#   include <stdio.h>
#   include <sys/file.h>	/* for O_RDWR and O_RDONLY definitions */
#endif


/*----------------------------------------------------------------------------
* AR channel file routines
*
*  long  arCFAsserts(							)
*  long  arCF_DFillRbufAndGetc(  pArChanDesc				)
*  long  arCF_DFlushAndFree(     pArChanDesc				)
*  long  arCF_DFlushWbufAndPutc( c,           pArChanDesc		)
*  long  arCF_DNextBlock(        pArChanDesc				)
*  long  arCF_DRead(             pArChanDesc, pDBuf,   blockNum		)
*
*  long  arCF_GRGetBlock(        pArChanDesc				)
*  long  arCF_GRReadBlock(       pArCfDesc,   pArChanHdr		)
*
*  long  arCF_IFlushAndFree(     pArChanDesc				)
*  long  arCF_INextDatInfo(      pArChanDesc				)
*  long  arCF_IRead(             pArChanDesc, pIBuf,   blockNum		)
*
*  long  arCF_MIAddBlock(        pArCfDesc				)
*  long  arCF_MIAddChan(         pArCfDesc,   name,   >ppMIBuf, >pIndex	)
*  long  arCF_MIDelChan(         pArCfDesc,   pMIBuf,  index		)
*  long  arCF_MIFindChan(        pArCfDesc,   name,   >ppMIBuf, >pIndex	)
*---------------------------------------------------------------------------*/

/*+/subhead-------------------------------------------------------------------
* NAME	arCF_malloc/arCF_free
*
* malloc and free routines for arCF routines to use for the various kinds
*	of data structures.  Should the need arise, these routines will
*	be enhanced to maintain lists of free data structures of the various
*	kinds, to eliminate malloc()/free() overhead and memory fragmentation.
*
*    The following macros are used for calling malloc() and free() for
*    some common data structures in arChanIO.  They have two versions,
*    depending on whether DEBUG is defined.  Both versions perform the
*    described function.  The DEBUG version provides the capability for
*    `bounds checking' on allocated buffers, and also for filling buffers
*    with a known pattern for use in `un-initialized buffer' bug detection.
*
*       AR_MI_BUF *arCF_mallocMI_BUF()      void arCF_freeMI_BUF(pMIBuf
*    AR_INDEX_BUF *arCF_mallocINDEX_BUF()   void arCF_freeINDEX_BUF(pIndexBuf)
*     AR_DATA_BUF *arCF_mallocDATA_BUF()    void arCF_freeDATA_BUF(pDataBuf)
*       AR_GR_BUF *arCF_mallocGR_BUF()      void arCF_freeGR_BUF(pDataBuf)
*
*---------------------------------------------------------------------------*/
static int	glArMallocDebug=0;		/* 1 says print info */
static unsigned long glArNMalloc=0;		/* # of successful malloc's */
static unsigned long glArNMallocFailed=0;	/* # of failed mallocs */
static unsigned long glArNbytesMalloc=0;	/* # of bytes ever malloc'ed */
static unsigned long glArNetNbytesMalloc=0;	/* # bytes malloc, not free */
static unsigned long glArNFree=0;		/* # of free's */
static unsigned long glArNbytesFree=0;		/* # of bytes ever free'ed */

void
arFree(pMem, size, text)
void	*pMem;
int	size;
char	*text;
{
    free((char *)pMem);
    glArNFree++;
    glArNbytesFree += size;
    glArNetNbytesMalloc -= size;
    if (glArMallocDebug) {
	(void)printf("free %s=0x%x sz=%d\n", text, pMem, size);
	arMallocStats();
    }
}

void *
arMalloc(size, text)
int	size;
char	*text;
{
    void	*pMem;
    pMem = (void *)malloc(size);
    if (pMem != NULL) {
	glArNMalloc++;
	glArNbytesMalloc += size;
	glArNetNbytesMalloc += size;
    }
    else
	glArNMallocFailed++;

    if (glArMallocDebug) {
	(void)printf("malloc %s=0x%x sz=%d\n", text, pMem, size);
	arMallocStats();
    }
    return pMem;
}
void
arMallocDebug(n)
int	n;
{
    glArMallocDebug = n;
}
void
arMallocStats()
{
    (void)printf("Nmal,Nfree,net= %d %d %d\n",
		glArNMalloc, glArNFree, glArNetNbytesMalloc);
}

AR_MI_BUF *
arCF_mallocMI_BUF()
{
#ifdef DEBUG
    return (AR_MI_BUF *)arMalloc(sizeof(AR_MI_BUF), "AR_MI_BUF");
#else
    return (AR_MI_BUF *)malloc(sizeof(AR_MI_BUF));
#endif
}
void
arCF_freeMI_BUF(ptr)
AR_MI_BUF *ptr;
{
#ifdef DEBUG
    arFree(ptr, sizeof(AR_MI_BUF), "AR_MI_BUF");
#else
    free(ptr);
#endif
}

AR_INDEX_BUF *
arCF_mallocINDEX_BUF()
{
#ifdef DEBUG
    return (AR_INDEX_BUF *)arMalloc(sizeof(AR_INDEX_BUF), "AR_INDEX_BUF");
#else
    return (AR_INDEX_BUF *)malloc(sizeof(AR_INDEX_BUF));
#endif
}
void
arCF_freeINDEX_BUF(ptr)
AR_INDEX_BUF *ptr;
{
#ifdef DEBUG
    arFree(ptr, sizeof(AR_INDEX_BUF), "AR_INDEX_BUF");
#else
    free(ptr);
#endif
}

AR_DATA_BUF *
arCF_mallocDATA_BUF()
{
#ifdef DEBUG
    return (AR_DATA_BUF *)arMalloc(sizeof(AR_DATA_BUF), "AR_DATA_BUF");
#else
    return (AR_DATA_BUF *)malloc(sizeof(AR_DATA_BUF));
#endif
}
void
arCF_freeDATA_BUF(ptr)
AR_DATA_BUF *ptr;
{
#ifdef DEBUG
    arFree(ptr, sizeof(AR_DATA_BUF), "AR_DATA_BUF");
#else
    free(ptr);
#endif
}

AR_GR_BUF *
arCF_mallocGR_BUF()
{
#ifdef DEBUG
    return (AR_GR_BUF *)arMalloc(sizeof(AR_GR_BUF), "AR_GR_BUF");
#else
    return (AR_GR_BUF *)malloc(sizeof(AR_GR_BUF));
#endif
}
void
arCF_freeGR_BUF(ptr)
AR_GR_BUF *ptr;
{
#ifdef DEBUG
    arFree(ptr, sizeof(AR_GR_BUF), "AR_GR_BUF");
#else
    free(ptr);
#endif
}

/*+/subr**********************************************************************
* NAME	arCFAsserts - check some assumptions used in arChanIO routines
*
* DESCRIPTION
*	This routine checks a number of hardware and software items
*	which influence the proper operation of the arChanIO routines and
*	compatibility between various execution platforms.  Aside from
*	the simple motivation of being able to run, these checks are
*	important in avoiding the clobbering of a file full of data.
*
* RETURNS
*	OK
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
arCFAsserts()
{
    static ULONG in[]={0,0,0}, out[]={0,0,0}, i;
    char	between[4];
    TS_STAMP	time_s;

    assertAlways(4 == sizeof(time_s.secPastEpoch));
					/* important for file layout; also
					needed to make structures properly
					aligned */
    assertAlways(4 == sizeof(time_s.nsec));/* important for file layout; also
					needed to make structures properly
					aligned */
    assertAlways(1 == sizeof(char));
    assertAlways(2 == sizeof(short));
    assertAlways(4 == sizeof(long));
    assertAlways(4 == sizeof(float));
    assertAlways(8 == sizeof(double));
    assertAlways(AR_UNITS_DIM == 8);	/* important for file layout */
    assertAlways(AR_UNITS_DIM % 8 == 0);/* important for file layout; also
					needed to make structures properly
					aligned */
    assertAlways(db_state_dim == 16);	/* important for file layout; also
					needed to make structures properly
					aligned */
    assertAlways(db_state_text_dim == 26);/* important for file layout; also
					needed to make structures properly
					aligned */
    assertAlways(dbr_size[DBF_ENUM] == 2);/* important for file layout; also
					needed for proper functioning of
					code which copies DBF_ENUM values */
    assertAlways(AR_UNITS_DIM % 8 == 0);/* important for file layout; also
					needed to make structures properly
					aligned */
    assertAlways(AR_GR_BUF_PAD >= 1);	/* block has to hold state strings */
    assertAlways(ERROR < 0);		/* must not be a valid block number */

/*----------------------------------------------------------------------------
*    make sure that changing long to character stream and back actually works
*---------------------------------------------------------------------------*/
    in[1] = 0x87654321L;
    for (i=0; i<4; i++) {
	between[i] = *(((char *)&in[1])+i);
    }
    for (i=0; i<4; i++) {
	*(((char *)&out[1])+i) = between[i];
    }
    assertAlways(in[1] == out[1]);

    return OK;
}

/*+/subr**********************************************************************
* NAME	arCF_DFillRbufAndGetc - get next data buffer and get a character
*
* DESCRIPTION
*	Gets the next data buffer for reading, puts the time stamp
*	information from datInfo into the chanDesc, and returns the first
*	character for reading.
*
*	The time stamp information in the chanDesc is initialized from
*	the datInfo structure in the index block, if the datInfo
*	actually contains a time stamp.  (Remember that blocks which
*	don't have the beginning of an item have zero time stamp in
*	their datInfo.)
*
* RETURNS
*	character, or
*	ERROR
*
*-*/
long
arCF_DFillRbufAndGetc(pArChanDesc)
AR_CHAN_DESC *pArChanDesc;	/* pointer to channel descriptor */
{
    if (arCF_DNextBlock(pArChanDesc) != OK)
	return ERROR;
    if (--pArChanDesc->remainCount >= 0) {
	if (ArCDDatInfo(pArChanDesc).stamp.secPastEpoch != 0)
	    pArChanDesc->timeStamp = ArCDDatInfo(pArChanDesc).stamp;
	return ((int)(*(++pArChanDesc->pData))) & 0xff;
    }
    else
	assertAlways(0);
/* NOTREACHED */
}

/*+/subr**********************************************************************
* NAME	arCF_DFlushAndFree - flush data block and free buffer
*
* DESCRIPTION
*	If the data block has been modified, the `.lastByte' item in the
*	block is updated and the block is written.
*
*	The block is free'd and the buffer pointer in the chanDesc is
*	set to NULL.
*
* RETURNS
*	OK, or
*	ERROR
*
* NOTES
* 1.	This routine depends on the channel descriptor having .remainCount
*	and .pData be relevant to the buffer being flushed.
*
* BUGS
* o	doesn't provide a mechanism for multiple buffering
*
*-*/
long
arCF_DFlushAndFree(pArChanDesc)
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status to caller */

    if (ArCFModifyTestAndReset(pArChanDesc->pArCfDesc, pArChanDesc->pDataBuf)){
	assert((pArChanDesc->flags & AR_CDESC_WRITE) != 0);
	if (pArChanDesc->pData != NULL) {
	    pArChanDesc->pDataBuf->bfInfo.lastByte = pArChanDesc->pData -
			    (char *)&pArChanDesc->pDataBuf->bfInfo;
	    assert(pArChanDesc->pDataBuf->bfInfo.lastByte > 0);
	    assert(pArChanDesc->pDataBuf->bfInfo.lastByte < 2000);
	}
	stat = bfWrite(pArChanDesc->pArCfDesc->pBfDesc,
			&pArChanDesc->pDataBuf->bfInfo,
			pArChanDesc->pDataBuf->blkNum);
	if (stat != OK) {
	    (void)fprintf(stderr,
		    	"arCF_DFlushAndFree: can't write data block\n");
	    retStat = ERROR;
	}
    }
    arCF_freeDATA_BUF(pArChanDesc->pDataBuf);
    pArChanDesc->pDataBuf = NULL;

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCF_DFlushWbufAndPutc - flush data buffer and write a character
*
* DESCRIPTION
*	Write the current data block to the file, get a new data block for
*	writing, and put the character into the new block.  Manipulations
*	for index and master index blocks are performed as needed.
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	text
*
* EXAMPLE
*	for (i=0; i<dataCount; i++) {
*	    if (--pArChanDesc->remainCount >= 0)
*		*(char *)(++(pArChanDesc->pData)) = data[i];
*	    else
*		stat = arCF_DFlushWbufAndPutc(data[i], pArChanDesc);
*	}
*
*-*/
long
arCF_DFlushWbufAndPutc(c, pArChanDesc)
char		c;		/* I character to write */
AR_CHAN_DESC	*pArChanDesc;	/* IO pointer to channel descriptor */
{

    assert(pArChanDesc != NULL);

    if (arCF_DNextBlock(pArChanDesc) != OK)
	return ERROR;
    if (--pArChanDesc->remainCount >= 0)
	*(++pArChanDesc->pData) = c;
    else
	assertAlways(0);

    return OK;
}

/*+/subr**********************************************************************
* NAME	arCF_DNextBlock - get next data block
*
* DESCRIPTION
*	Gets the next data block, as dictated by the next datInfo in the
*	index block.  If the mode is O_RDWR, a new datInfo is created and
*	a new data block is acquired.  If the mode is O_RDONLY, then ERROR
*	is returned if there are no more datInfo's, with AR_CDESC_EOF set
*	in chanDesc.flags  .
*
* RETURNS
*	OK, or
*	ERROR
*
*-*/
long
arCF_DNextBlock(pArChanDesc)
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
{
    int		stat;		/* status from calls */

    AR_DATA_BUF *pDataBufNew;	/* pointer to new data buffer */
    BF_BLKNUM	blockNum;	/* block number of new data block */

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);

    if ((pArChanDesc->flags & AR_CDESC_WRITE) != 0) {
/*----------------------------------------------------------------------------
* O_RDWR -- write mode processing
*
*    first, get a buffer to hold the next data block;
*    then, get the next datInfo item.
*---------------------------------------------------------------------------*/
	if ((pDataBufNew = arCF_mallocDATA_BUF()) == NULL) {
	    if ((pArChanDesc->flags & AR_CDESC_SUP) == 0) {
		(void)fprintf(stderr,
			    "arCF_DNextBlock: can't malloc new data buf\n");
	    }
	    pArChanDesc->flags |= AR_CDESC_SUP;	/* suppress msg */
	    return ERROR;
	}

	if (arCF_INextDatInfo(pArChanDesc) != OK) {
	    if ((pArChanDesc->flags & AR_CDESC_SUP) == 0) {
		(void)fprintf(stderr,
			    "arCF_DNextBlock: error getting new datInfo\n");
	    }
	    pArChanDesc->flags |= AR_CDESC_SUP;	/* suppress msg */
	    arCF_freeDATA_BUF(pDataBufNew);
	    return ERROR;
	}

	blockNum = ArCDDatInfo(pArChanDesc).dataBlock;

	if (pArChanDesc->pDataBuf == NULL) {
/*----------------------------------------------------------------------------
*    Case 1:
*        this is the first call for the channel following the open.  Read the
*        last used data block for the channel.  If there wasn't a "last used
*        data block", acquire a new data block.
*---------------------------------------------------------------------------*/
	    if (blockNum > 0) {
		stat = bfRead(pArChanDesc->pArCfDesc->pBfDesc,
						&pDataBufNew->bfInfo, blockNum);
		if (stat != OK) {
		    (void)fprintf(stderr,
			    "arCF_DNextBlock: error reading old data block\n");
		    arCF_freeDATA_BUF(pDataBufNew);
		    return ERROR;
		}
		pDataBufNew->blkNum = blockNum;
		ArCFModifyInit(pDataBufNew);
		pDataBufNew->pBlock = &pDataBufNew->bfInfo;
	    }
	    else {
		blockNum = bfAcquire(pArChanDesc->pArCfDesc->pBfDesc,
							&pDataBufNew->bfInfo);
		if (blockNum == ERROR) {
		    if ((pArChanDesc->flags & AR_CDESC_SUP) == 0) {
			(void)fprintf(stderr,
			  "arCF_DNextBlock: error acquiring new data block\n");
		    }
		    pArChanDesc->flags |= AR_CDESC_SUP;	/* suppress msg */
		    arCF_freeDATA_BUF(pDataBufNew);
		    return ERROR;
		}
		pDataBufNew->blkNum = blockNum;
		ArCFModifyInit(pDataBufNew);
		pDataBufNew->pBlock = &pDataBufNew->bfInfo;
		ArCFModifySet(pArChanDesc->pArCfDesc, pDataBufNew);
		ArCDDatInfo(pArChanDesc).dataBlock = pDataBufNew->blkNum;
	    }
	}
	else {
/*----------------------------------------------------------------------------
*    Case 2:
*        this isn't the first call--instead, the caller wants the current
*        buffer flushed and a new one started.  Acquire a new block, link the
*        old one to it, write the old block, and free its buffer.
*---------------------------------------------------------------------------*/
	    blockNum = bfAcquire(pArChanDesc->pArCfDesc->pBfDesc,
							&pDataBufNew->bfInfo);
	    if (blockNum == ERROR) {
		if ((pArChanDesc->flags & AR_CDESC_SUP) == 0) {
		    (void)fprintf(stderr,
		      "arCF_DNextBlock: error acquiring new data block\n");
		}
		pArChanDesc->flags |= AR_CDESC_SUP;	/* suppress msg */
		arCF_freeDATA_BUF(pDataBufNew);
		return ERROR;
	    }
	    pDataBufNew->blkNum = blockNum;
	    ArCFModifyInit(pDataBufNew);
	    pDataBufNew->pBlock = &pDataBufNew->bfInfo;
	    ArCFModifySet(pArChanDesc->pArCfDesc, pDataBufNew);
	    ArCDDatInfo(pArChanDesc).dataBlock = pDataBufNew->blkNum;

	    pArChanDesc->pDataBuf->bfInfo.flink = blockNum;
	    if (pArChanDesc->pData != NULL) {
		pArChanDesc->pDataBuf->bfInfo.lastByte = pArChanDesc->pData -
				(char *)&pArChanDesc->pDataBuf->bfInfo;
		assert(pArChanDesc->pDataBuf->bfInfo.lastByte > 0);
		assert(pArChanDesc->pDataBuf->bfInfo.lastByte < 2000);
	    }
	    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pDataBuf);
	    if (arCF_DFlushAndFree(pArChanDesc) != OK) {
		(void)fprintf(stderr,
			    "arCF_DNextBlock: error writing old block\n");
		return ERROR;
	    }
	}
/*----------------------------------------------------------------------------
*    wrapup
*
*	set up the channel descriptor for writing to the new block
*---------------------------------------------------------------------------*/
	pArChanDesc->pDataBuf = pDataBufNew;
	pArChanDesc->remainCount =
			AR_CF_BLKSIZE - pDataBufNew->bfInfo.lastByte - 1;
	pArChanDesc->pData =
		(char *)&pDataBufNew->bfInfo + pDataBufNew->bfInfo.lastByte;
	return OK;
    }
    else {
/*----------------------------------------------------------------------------
* O_RDONLY
*
*    Case 1:
*	this is the first call for the channel following the open.  A buffer
*	must be obtained, then the first available data block for the
*	channel must be read.
*
*    Case 2:
*	this isn't the first call following open.  Simply read the next
*	available data block into the same buffer.
*
* NOTES:
* 1.	pDataBufNew is used here as the buffer pointer--except for the first
*	read for the channel, the `New' is inappropriate; peruse accordingly.
*---------------------------------------------------------------------------*/
	if ((pDataBufNew = pArChanDesc->pDataBuf) == NULL) {
	    if ((pDataBufNew = arCF_mallocDATA_BUF()) == NULL) {
		if ((pArChanDesc->flags & AR_CDESC_SUP) == 0) {
		    (void)fprintf(stderr,
			    "arCF_DNextBlock: can't malloc new data buf\n");
		}
		pArChanDesc->flags |= AR_CDESC_SUP;	/* suppress msg */
		return ERROR;
	    }
	}

	if (arCF_INextDatInfo(pArChanDesc) != OK)
	    return ERROR;

	blockNum = ArCDDatInfo(pArChanDesc).dataBlock;

	if (blockNum > 0) {
	    stat = arCF_DRead(pArChanDesc, pDataBufNew, blockNum);
	    if (stat != OK) {
		(void)fprintf(stderr,
			  "arCF_DNextBlock: error reading next data block\n");
		return ERROR;
	    }
	}
	else {
	    pArChanDesc->flags |= AR_CDESC_EOF;
	    return ERROR;		/* no data to read */
	}
	pArChanDesc->pDataBuf = pDataBufNew;
	return OK;
    }
}

/*+/subr**********************************************************************
* NAME	arCF_DRead - read a data block into a data buffer
*
* DESCRIPTION
*
* RETURNS
*	OK, or
*	ERROR
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
arCF_DRead(pArChanDesc, pDataBuf, blockNum)
AR_CHAN_DESC	*pArChanDesc;	/* IO pointer to channel descriptor */
AR_DATA_BUF	*pDataBuf;	/* IO pointer to data buffer */
BF_BLKNUM	blockNum;	/* I block number to read */
{
    if (bfRead(pArChanDesc->pArCfDesc->pBfDesc, &pDataBuf->bfInfo, blockNum)
									!= OK)
	return ERROR;
    pDataBuf->blkNum = blockNum;
    ArCFModifyInit(pDataBuf);
    pDataBuf->pBlock = &pDataBuf->bfInfo;
/*----------------------------------------------------------------------------
*    wrapup
*
*	set up the channel descriptor for reading from the new block
*	case 1: no bytes in block; set remainCount = 0, return ERROR, EOF;
*	case 2: first read after open; use firstByte; this ignores overflow
*		bytes in the first block.
*	case 3: use BF_BLOCK_DATA, so as to read bytes which overflow from
*		one block to the next.
*---------------------------------------------------------------------------*/
    if (pDataBuf->bfInfo.lastByte < BF_BLOCK_DATA) {
	pArChanDesc->remainCount = 0;
	pArChanDesc->pData = (char *)&pDataBuf->bfInfo;
	pArChanDesc->flags |= AR_CDESC_EOF;
	return ERROR;
    }
    else {
	int i;
	if (pArChanDesc->remainCount < -9) {     /* 1st read after open */
	    if ((i = pDataBuf->bfInfo.firstByte) < BF_BLOCK_DATA)
		assertAlways(0); /* 1st block can't just have overflow */
	}
	else
	    i = BF_BLOCK_DATA;

	pArChanDesc->remainCount = pDataBuf->bfInfo.lastByte -i +1;
	pArChanDesc->pData = (char *)(&pDataBuf->bfInfo) +i -1;
    }

    return OK;
}

/*+/subr**********************************************************************
* NAME	arCF_GRGetBlock - get a block to hold ENUM graphics info
*
* DESCRIPTION
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	
*
*-*/
long
arCF_GRGetBlock(pArChanDesc)
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
{
    int		retStat=OK;	/* return status to caller */
    BF_BLKNUM	block;		/* number of acquired block */
    AR_GR_BUF	*pGRBuf;	/* pointer to buffer for block */

    assert(pArChanDesc != NULL);
    assert(ArCDChanHdr(pArChanDesc).chanType == DBF_ENUM);

/*----------------------------------------------------------------------------
*    allocate a buffer to hold new block and acquire a new block
*---------------------------------------------------------------------------*/
    if ((pGRBuf = arCF_mallocGR_BUF()) == NULL) {
	(void)fprintf(stderr,
			"arCF_GRAddBlock: can't malloc for new GR block\n");
	retStat = ERROR;
    }
    if (retStat == OK) {
	block = bfAcquire(pArChanDesc->pArCfDesc->pBfDesc, &pGRBuf->bfInfo);
	if (block == ERROR) {
	    (void)fprintf(stderr,
			"arCF_GRAddBlock: can't acquire block for grInfo\n");
	    retStat = ERROR;
	    arCF_freeGR_BUF(pGRBuf);
	}
    }
    if (retStat == OK) {
	pGRBuf->blkNum = block;
	ArCFModifyInit(pGRBuf);
	pGRBuf->pBlock = &pGRBuf->bfInfo;
/*----------------------------------------------------------------------------
*    initialize the block file information for the block
*---------------------------------------------------------------------------*/
	pGRBuf->bfInfo.flink = 0;
	pGRBuf->bfInfo.firstByte = BF_BLOCK_DATA;
	pGRBuf->bfInfo.lastByte = BF_BLOCK_DATA - 1;
	ArCFModifySet(pArChanDesc->pArCfDesc, pGRBuf);
/*-----------------------------------------------------------------------------
*    link the block and buffer into the chanHdr and set the modified
*    bit for the MI block
*----------------------------------------------------------------------------*/
	ArCDChanHdr(pArChanDesc).graphics.enumGr.block = block;
	ArCDChanHdr(pArChanDesc).graphics.enumGr.pGRBuf = pGRBuf;
	ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);
	pArChanDesc->pArCfDesc->b0Modified = 1;
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCF_GRReadBlock - get a buffer and read ENUM graphics info
*
* DESCRIPTION
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	
*
*-*/
long
arCF_GRReadBlock(pArCfDesc, pArChanHdr)
AR_CF_DESC *pArCfDesc;		/* I pointer to channel file descriptor */
AR_CHAN_HDR *pArChanHdr;	/* IO pointer to chanHdr */
{
    int		retStat=OK;	/* return status to caller */
    BF_BLKNUM	block;		/* number of acquired block */
    AR_GR_BUF	*pGRBuf;	/* pointer to buffer for block */

    assert(pArCfDesc != NULL);
    assert(pArChanHdr != NULL);
    assert(pArChanHdr->chanType == DBF_ENUM);

/*----------------------------------------------------------------------------
*    allocate a buffer to hold the block and read the block
*---------------------------------------------------------------------------*/
    if ((pGRBuf = arCF_mallocGR_BUF()) == NULL) {
	(void)fprintf(stderr, "arCF_GRReadBlock: can't malloc for GR block\n");
	retStat = ERROR;
    }
    if (retStat == OK) {
	block = bfRead(pArCfDesc->pBfDesc, &pGRBuf->bfInfo,
			    		pArChanHdr->graphics.enumGr.block);
	if (block == ERROR) {
	    (void)fprintf(stderr,
			"arCF_GRReadBlock: can't read block for grInfo\n");
	    retStat = ERROR;
	    arCF_freeGR_BUF(pGRBuf);
	}
    }
    if (retStat == OK) {
/*-----------------------------------------------------------------------------
*    link the block into the chanHdr.  (No need to set modified bit for
*    MI block, since this is only a run-time change, not for disk.)
*----------------------------------------------------------------------------*/
	pArChanHdr->graphics.enumGr.pGRBuf = pGRBuf;
	pGRBuf->blkNum = block;
	ArCFModifyInit(pGRBuf);
	pGRBuf->pBlock = &pGRBuf->bfInfo;
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCF_IFlushAndFree - flush index block and free buffer
*
* DESCRIPTION
*
* RETURNS
*	OK, or
*	ERROR
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
arCF_IFlushAndFree(pArChanDesc)
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status to caller */

    if (ArCFModifyTestAndReset(pArChanDesc->pArCfDesc, pArChanDesc->pIndexBuf)){
	assert((pArChanDesc->flags & AR_CDESC_WRITE) != 0);
	stat = bfWrite(pArChanDesc->pArCfDesc->pBfDesc,
				&pArChanDesc->pIndexBuf->bfInfo,
				pArChanDesc->pIndexBuf->blkNum);
	if (stat != OK) {
	    (void)fprintf(stderr,
		    	"arCF_IFlushAndFree: can't write index block\n");
	    retStat = ERROR;
	}
    }
    arCF_freeINDEX_BUF(pArChanDesc->pIndexBuf);
    pArChanDesc->pIndexBuf = NULL;

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCF_INextDatInfo - get next datInfo item in a channel's index
*
* DESCRIPTION
*	Obtains the next datInfo element for a channel; the chanDescr will
*	have the element's subscript.  'next' has several possible meanings,
*	depending on the present state of the channel descriptor.
*
*	If the channel descriptor is open O_RDWR, then the 'next' datInfo is
*	either the last used datInfo (if this is the first call following
*	opening the descriptor) or else a new datInfo at the tail end of the
*	channel's index.  When necessary, a new index block is obtained and,
*	if necessary, the previous index buffer is written and the buffer
*	is free()'d.
*
*	In O_RDONLY mode, the 'next' datInfo is either the first available
*	datInfo (if this is the first call following opening the descriptor)
*	or else the next datInfo in the index.  Initially, an index buffer is
*	obtained; the same buffer is used for successive index blocks.  When
*	no more datInfo are available for reading, ERROR is returned, with
*	AR_CDESC_EOF set in chanDesc.flags .
*
*	On a successful return, the following fields in the chanDesc have
*	been set up:
*	o  .pIndexBuf	points to the (possibly new) index buffer
*	o  .datInfNum	is the number of the datInf structure
*
*	The chanHdr is updated, if necessary.
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	a highly likely race exists between reading and writing: the chanHdr
*	has up-to-date information about 'full' write buffers, and the reader
*	has access to this information.  In an asynchronous situation, it
*	is unpredictable whether those write buffers will be available from
*	disk or from memory or (depending on the race) from neither.
*
* NOTES
* 1.	This routine is written so that it can be repeatedly called when
*	an error condition exists and it won't break.  This can be exploited
*	by having read or write not check status until after the last byte
*	has been transferred, which makes the transfer more efficient.  This
*	feature is implemented using the AR_CDESC_SUP flag in chanDesc  .
* 2.	The ArCDDatInfo() macro is used here.  Use of this macro must FOLLOW
*	storing a 'good' value for datInfNum in the channel descriptor.
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
arCF_INextDatInfo(pArChanDesc)
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
{
    int		stat;		/* status from calls */

    AR_INDEX_BUF *pIndexBuf;	/* pointer to index buffer */
    BF_BLKNUM	blockNum;	/* block number of new index block */
    short	datInfNum;	/* number of datInfo structure element */

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);
    assert(pArChanDesc->pArCfDesc != NULL);
    assert(pArChanDesc->pArCfDesc->pBfDesc != NULL);

    pIndexBuf = pArChanDesc->pIndexBuf;

    if ((pArChanDesc->flags & AR_CDESC_WRITE) != 0) {
/*----------------------------------------------------------------------------
* O_RDWR -- write mode processing.  There are 4 cases to handle.
*
*    increment datInfNum to point to next datInfo to be used.
*
*    if the new datInfNum is < 0  or is >= AR_CF_NDATINFO, then a new index
*    block (and thus index buffer) is needed.  The buffer is obtained here
*    and 'filled in' later by bfRead() or bfAcquire().
*---------------------------------------------------------------------------*/
	datInfNum = ++pArChanDesc->datInfNum;
	if (datInfNum < 0 || datInfNum >= AR_CF_NDATINFO) {
	    if ((pIndexBuf = arCF_mallocINDEX_BUF()) == NULL) {
		if ((pArChanDesc->flags & AR_CDESC_SUP) == 0) {
		    (void)fprintf(stderr,
			    "arCF_INextDatInfo: can't malloc index buffer\n");
		}
		pArChanDesc->flags |= AR_CDESC_SUP;	/* suppress msg */
		--pArChanDesc->datInfNum;
		return ERROR;
	    }
	}
	if (datInfNum >= 0 && datInfNum < AR_CF_NDATINFO)
	    goto rdwrCase1;
	else if (datInfNum >= AR_CF_NDATINFO)
	    goto rdwrCase2;
	else {
	    blockNum = ArCDChanHdr(pArChanDesc).indexTail;
	    if (blockNum > 0)
		goto rdwrCase3;
	    else
		goto rdwrCase4;
	}
rdwrCase1:
/*----------------------------------------------------------------------------
*    Case 1: the next datInfo in the index block is available for use.
*
*	o  adjust lastByte in the index block
*	o  set up chanDesc for the datInfo
*	o  set up new datInfo[datInfNum]
*---------------------------------------------------------------------------*/
	/* datInfNum in descriptor set up by ++ above */
	pIndexBuf->bfInfo.lastByte += sizeof(AR_DAT_INFO);
	assert(pIndexBuf->bfInfo.lastByte > 0);
	assert(pIndexBuf->bfInfo.lastByte < 2000);

	ArCDDatInfo(pArChanDesc).dataBlock = (BF_BLKNUM)0;
	ArCDDatInfo(pArChanDesc).stamp.secPastEpoch = 0;
	ArCDDatInfo(pArChanDesc).stamp.nsec = 0;

	ArCFModifySet(pArChanDesc->pArCfDesc, pIndexBuf);

	goto rdwrEnd;
rdwrCase3:
/*----------------------------------------------------------------------------
*    Case 3: this is the first call after the channel has been opened, and
*	the file already contains data for the channel.  The chanHdr points
*	to an existing index block (indexTail), and that index block already
*	has a datInfo structure set up pointing to a data block.
*
*	o  read the indexTail block into the index buffer
*	o  set up chanDesc for the indexBuf and datInfo
*	o  set up chanDesc for secPastEpoch from chanHdr "newest" values
*---------------------------------------------------------------------------*/
	    stat = arCF_IRead(pArChanDesc, pIndexBuf, blockNum);
	    if (stat != OK) {
		(void)fprintf(stderr,
			    "arCF_INextDatInfo: error reading index block\n");
		arCF_freeINDEX_BUF(pIndexBuf);
		--pArChanDesc->datInfNum;
		return ERROR;
	    }
	    datInfNum = (pIndexBuf->bfInfo.lastByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
	    /* firstByte and lastByte are OK */

	    pArChanDesc->pIndexBuf = pIndexBuf;
	    pArChanDesc->datInfNum = datInfNum;
	    pArChanDesc->timeStamp.secPastEpoch =
	    			ArCDChanHdr(pArChanDesc).newestSecPastEpoch;
	    pArChanDesc->timeStamp.nsec = ArCDChanHdr(pArChanDesc).newestNsec;

	    /* the information in chanHdr is OK */
	    /* the information in datInfo is OK */

	    goto rdwrEnd;
rdwrCase2:
rdwrCase4:
/*----------------------------------------------------------------------------
*    Case 2: the current datInfo uses the last slot in the index block.
*
*    Case 4: this is the first call after the channel has been opened, but
*	the file doesn't yet contain data for the channel.
*
*	A new index block is needed, with datInfo[0] to be the 'next' datInfo.
*	Much of the code is common for these two cases; for each action item,
*	the case(s) which use the item are indicated.
*
*	2,4  acquire a new index block and install it in the index buffer,
*	     marking the buffer as MODIFIED.
*	2,4  set up firstByte and lastByte in the index block
*	2    set the flink in the current index block to point to the new one
*	     and write the current index block, free()ing its buffer
*	4    set chanHdr.indexHead to point to the new index block
*	2,4  install the new block in the chanHdr (in the MI block); mark the
*	     MI buffer as MODIFIED.
*	2,4  set up chanDesc for the indexBuf and datInfo
*	4    set up chanDesc for secPastEpoch as "none"
*	2,4  set up datInfo[0]
*---------------------------------------------------------------------------*/
	blockNum = bfAcquire(pArChanDesc->pArCfDesc->pBfDesc,
						    &pIndexBuf->bfInfo);
	if (blockNum == ERROR) {
	    if ((pArChanDesc->flags & AR_CDESC_SUP) == 0) {
		(void)fprintf(stderr,
			    "arCF_INextDatInfo: can't acquire index block \n");
	    }
	    pArChanDesc->flags |= AR_CDESC_SUP;	/* suppress msg */
	    arCF_freeINDEX_BUF(pIndexBuf);
	    --pArChanDesc->datInfNum;
	    return ERROR;
	}
	datInfNum = 0;
	pIndexBuf->blkNum = blockNum;
	ArCFModifyInit(pIndexBuf);
	pIndexBuf->pBlock = &pIndexBuf->bfInfo;
	pIndexBuf->bfInfo.firstByte = BF_BLOCK_DATA;
	pIndexBuf->bfInfo.lastByte = BF_BLOCK_DATA + sizeof(AR_DAT_INFO) - 1;
	ArCFModifySet(pArChanDesc->pArCfDesc, pIndexBuf);

	if (ArCDChanHdr(pArChanDesc).indexHead == 0) {		/* case 4 */
	    ArCDChanHdr(pArChanDesc).indexHead = blockNum;
	    pArChanDesc->timeStamp.secPastEpoch = 0;
	    pArChanDesc->timeStamp.nsec = 0;
	}
	else {							/* case 2 */
	    assert(pArChanDesc->pIndexBuf->blkNum ==
					ArCDChanHdr(pArChanDesc).indexTail);
	    assert(pArChanDesc->pIndexBuf->bfInfo.flink == (BF_BLKNUM)0);
	    pArChanDesc->pIndexBuf->bfInfo.flink = (BF_BLKNUM)blockNum;
	    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pIndexBuf);
	    if (arCF_IFlushAndFree(pArChanDesc) != OK) {
		(void)fprintf(stderr,
		    "arCF_INextDatInfo: error writing previous index block\n");
		/* ignore this error */
	    }
	}
	ArCDChanHdr(pArChanDesc).indexTail = blockNum;
	ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);
	pArChanDesc->pArCfDesc->b0Modified = 1;

	pArChanDesc->pIndexBuf = pIndexBuf;
	pArChanDesc->datInfNum = datInfNum;

	ArCDDatInfo(pArChanDesc).dataBlock = (BF_BLKNUM)0;
	ArCDDatInfo(pArChanDesc).stamp.secPastEpoch = 0;
	ArCDDatInfo(pArChanDesc).stamp.nsec = 0;

rdwrEnd:
        ;
    }
    else {
	int	datInfLast;	/* # of last datInfo in block */
/*----------------------------------------------------------------------------
* O_RDONLY -- read mode processing.  There are 3 cases to handle.
*
*    increment datInfNum to point to next datInfo to be used.  If there is
*    no index buffer, then datInfNum will have been initialized elsewhere
*    to -10  .
*---------------------------------------------------------------------------*/
	datInfNum = ++pArChanDesc->datInfNum;
	if (pArChanDesc->pIndexBuf == NULL)
	    datInfLast = -1;
	else {
	    datInfLast = (pArChanDesc->pIndexBuf->bfInfo.lastByte -
	       pArChanDesc->pIndexBuf->bfInfo.firstByte) / sizeof(AR_DAT_INFO);
	}
	if (datInfNum >= 0 && datInfNum <= datInfLast) {
/*----------------------------------------------------------------------------
*    Case 1: the next datInfo in the index block is available for reading.
*
*	o  set up chanDesc for the indexBuf and datInfNum
*	o  set up chanDesc for time stamp from datInfo
*---------------------------------------------------------------------------*/
	    assert(pArChanDesc->pIndexBuf != NULL);

	    /* datInfNum in chanDesc set up by ++ above */
	    if (ArCDDatInfo(pArChanDesc).stamp.secPastEpoch != 0)
		pArChanDesc->timeStamp = ArCDDatInfo(pArChanDesc).stamp;
	}
	else {
/*----------------------------------------------------------------------------
*    Case 2: the current datInfo was in the last one in the index block.
*    Case 3: this is the first call after the channel was opened.
*
*	o  if there is no index buffer, allocate one
*	o  get the blockNum of the next index block, either from 'flink'
*	   (for case 2) or from indexHead (for case 3)
*	o  read the next index block
*	o  set up chanDesc for the indexBuf and datInfNum
*	o  set up chanDesc for time stamp from datInfo
*---------------------------------------------------------------------------*/
	    if (pArChanDesc->pIndexBuf == NULL) {
		if ((pArChanDesc->pIndexBuf = arCF_mallocINDEX_BUF()) ==
								       NULL) {
		    if ((pArChanDesc->flags & AR_CDESC_SUP) == 0) {
			(void)fprintf(stderr,
			    "arCF_INextDatInfo: can't malloc index buffer\n");
		    }
		    pArChanDesc->flags |= AR_CDESC_SUP;  /* suppress msg */
		    --pArChanDesc->datInfNum;
		    return ERROR;
		}
	    }
	    if (datInfNum < 0)
		blockNum = ArCDChanHdr(pArChanDesc).indexHead;
	    else
		blockNum = pArChanDesc->pIndexBuf->bfInfo.flink;
	    if (blockNum == 0) {
		pArChanDesc->flags |= AR_CDESC_EOF;
		--pArChanDesc->datInfNum;
		return ERROR;
	    }
	    stat = arCF_IRead(pArChanDesc, pArChanDesc->pIndexBuf, blockNum);
	    if (stat != OK) {
		(void)fprintf(stderr,
			    "arCF_INextDatInfo: error reading index block\n");
		--pArChanDesc->datInfNum;
		return ERROR;
	    }
	    if (pArChanDesc->pIndexBuf->bfInfo.firstByte < BF_BLOCK_DATA) {
		pArChanDesc->flags |= AR_CDESC_EOF;
		--pArChanDesc->datInfNum;
		return ERROR;
	    }
	    pArChanDesc->datInfNum =
		   (pArChanDesc->pIndexBuf->bfInfo.firstByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
	    assert(pArChanDesc->datInfNum >= 0);
	    assert(pArChanDesc->datInfNum < AR_CF_NDATINFO);

	    if (ArCDDatInfo(pArChanDesc).stamp.secPastEpoch != 0)
		pArChanDesc->timeStamp= ArCDDatInfo(pArChanDesc).stamp;
	}
    }

    return OK;
}

/*+/subr**********************************************************************
* NAME	arCF_IRead - read an index block into an index buffer
*
* DESCRIPTION
*
* RETURNS
*	OK, or
*	ERROR
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
arCF_IRead(pArChanDesc, pIndexBuf, blockNum)
AR_CHAN_DESC	*pArChanDesc;	/* IO pointer to channel descriptor */
AR_INDEX_BUF	*pIndexBuf;	/* IO pointer to index buffer */
BF_BLKNUM	blockNum;	/* I block number to read */
{
    if (bfRead(pArChanDesc->pArCfDesc->pBfDesc, &pIndexBuf->bfInfo, blockNum)
									!= OK)
	return ERROR;
    pIndexBuf->blkNum = blockNum;
    ArCFModifyInit(pIndexBuf);
    pIndexBuf->pBlock = &pIndexBuf->bfInfo;

    return OK;
}

/*+/subr**********************************************************************
* NAME	arCF_MIAddBlock - add a new block to the master index
*
* DESCRIPTION
*	Acquire a block (either from free block list or by expanding the
*	file) and add it to the master index.  The new block is formatted
*	with 'free channel headers', which are linked into the free chanHdr
*	list, at its head.
*
*	The MODIFIED flag is set for the new MI block.  If there was a 'tail'
*	MI block which has been linked to this one, its MODIFIED flag is
*	also set.
*
* RETURNS
*	OK, or
*	ERROR
*
*-*/
long
arCF_MIAddBlock(pArCfDesc)
AR_CF_DESC *pArCfDesc;		/* IO pointer to channel file descriptor */
{
    int		retStat=OK;	/* return status to caller */

    BF_BLKNUM	block;		/* number of acquired block */
    AR_MI_BUF	*pMIBuf;	/* pointer to buffer for block */
    int		i;		/* temp for loops */

    assert(pArCfDesc != NULL);
    assert(pArCfDesc->pBfDesc != NULL);

/*----------------------------------------------------------------------------
*    allocate a buffer to hold new block and acquire a new block
*---------------------------------------------------------------------------*/
    pMIBuf = arCF_mallocMI_BUF();
    if (pMIBuf == NULL) {
	(void)fprintf(stderr,
			"arCF_MIaddBlock: can't malloc for new MI block\n");
	retStat = ERROR;
    }
    if (retStat == OK) {
	block = bfAcquire(pArCfDesc->pBfDesc, &pMIBuf->bfInfo);
	if (block == ERROR) {
	    (void)fprintf(stderr,
			"arCF_MIaddBlock: can't acquire block for file\n");
	    retStat = ERROR;
	}
    }
    if (retStat == OK) {
/*----------------------------------------------------------------------------
*    set up miscellaneous fields in new block
*
*    install this block as MIHead, if it's the first MI block
*    if there was an old MITail, link it to this new block and mark it as
*	modified
*    install this block as MITail and mark it as modified
*    increment count of MI blocks (block0 only)
*
*    please pardon the confusion here, but there are two linked lists to
*    maintain--the list of blocks on disk and the list of buffers in memory.
*    The disk list is rooted in block0; the memory list is rooted in the
*    channel file descriptor.
*---------------------------------------------------------------------------*/
	pMIBuf->blkNum = block;
	ArCFModifyInit(pMIBuf);
	pMIBuf->pBlock = &pMIBuf->bfInfo;
	ArCFModifySet(pArCfDesc, pMIBuf);
	pArCfDesc->b0Modified = 1;
	pMIBuf->pNextMI = NULL;
	if (pArCfDesc->pMIHead == NULL) {
	    assert(ArB0MIHead(pArCfDesc) == 0);
	    assert(pArCfDesc->pMITail == NULL);
	    assert(ArB0MITail(pArCfDesc) == 0);
	    pArCfDesc->pMIHead = pMIBuf;
	    ArB0MIHead(pArCfDesc) = block;
	}
	else {
	    assert(pArCfDesc->pMITail != NULL);
	    assert(ArB0MITail(pArCfDesc) != 0);
	    pArCfDesc->pMITail->pNextMI = pMIBuf;
	    pArCfDesc->pMITail->bfInfo.flink = block;
	    ArCFModifySet(pArCfDesc, pArCfDesc->pMITail);
	    pArCfDesc->b0Modified = 1;
	}
	pArCfDesc->pMITail = pMIBuf;
	ArB0MITail(pArCfDesc) = block;
	ArB0MINblk(pArCfDesc) += 1;
/*----------------------------------------------------------------------------
*    'format' the new block into individual free channel headers and link
*    these into the memory (CfDesc) and disk (block0) structures.  (This
*    block is put at the head of the list of free chanHdr's.)  The name in
*    each free channel header has its first character set to '\0' .
*---------------------------------------------------------------------------*/
	for (i=0; i<AR_CF_NHDR; i++) {
	    pMIBuf->chanHdr[i].name[0] = '\0';
	    pMIBuf->chanHdr[i].graphics.nextMIFree.blkNum =
			i<AR_CF_NHDR-1 ? block : ArB0MIFree_blkNum(pArCfDesc);
	    pMIBuf->chanHdr[i].graphics.nextMIFree.hdrNum =
			i<AR_CF_NHDR-1 ? i+1 : ArB0MIFree_hdrNum(pArCfDesc);
	}
	ArB0MIFree_blkNum(pArCfDesc) = block;
	ArB0MIFree_hdrNum(pArCfDesc) = 0;
	pArCfDesc->pMIFree = pMIBuf;
	pArCfDesc->MIFree_hdrNum = 0;
/*----------------------------------------------------------------------------
*    initialize the block file information for the block; the file
*    flink in the previous block (if there was a previous block) was set above
*---------------------------------------------------------------------------*/
	pMIBuf->bfInfo.flink = 0;
	pMIBuf->bfInfo.firstByte = BF_BLOCK_DATA;
	pMIBuf->bfInfo.lastByte = BF_BLOCK_DATA - 1 +
					AR_CF_NHDR * sizeof(AR_CHAN_HDR);

    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCF_MIAddChan - add a channel to the master index
*
* DESCRIPTION
*	Gets the next free channel header from the free chanHdr list and
*	performs some initialization for the new chanHdr.  A new MI block
*	is acquired if there were no entries on the free chanHdr list.
*
*	The MODIFIED flag is set for the MI block.
*
*	This routine isn't intended as the primary user routine for adding
*	a channel to a channel file--use arCFChanOpen() instead.
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	if the forward link (to the next free chanHdr) in the acquired
*	chanHdr points to another block, then a search of the entire MI
*	list may be necessary to find the MI buffer pointer to store in
*	the channel file descriptor for pMIFree.
* o	doesn't add the channel to the hash table (which doesn't
*	presently exist)
*
* SEE ALSO
*	arCFChanOpen(), arCF_MIDelChan
*
*-*/
long
arCF_MIAddChan(pArCfDesc, name, ppMIBuf, pIndex)
AR_CF_DESC *pArCfDesc;	/* IO pointer to channel file descriptor */
char	*name;		/* I channel name */
AR_MI_BUF **ppMIBuf;	/* O ptr to ptr to MI block containing channel */
int	*pIndex;	/* O pointer to location to store index within
			MI block of chanHdr of added channel */
{
    int		retStat=OK;	/* return status to caller */
    AR_MI_BUF *pMIBuf;		/* ptr to MI block for new chanHdr */
    short	indx;		/* chanHdr index in MI block */
    AR_CHAN_HDR *pChanHdr;	/* pointer to actual chanHdr */

    assert(pArCfDesc != NULL);
    assert(pArCfDesc->pBfDesc != NULL);
    assert(name != NULL);
    assert(ppMIBuf != NULL);
    assert(pIndex != NULL);
    assert(strlen(name) > 0);
    assert(strlen(name) < AR_NAME_DIM);

/*----------------------------------------------------------------------------
*    if there aren't any entries on the free chanHdr list (pMIFree), then
*    add a new MI block to the file, putting its chanHdr's on the free list
*---------------------------------------------------------------------------*/
    if (pArCfDesc->pMIFree == NULL) {
	if (arCF_MIAddBlock(pArCfDesc) != OK)
	    retStat = ERROR;
    }
/*----------------------------------------------------------------------------
*    get a chanHdr, removing it from the free chanHdr list.  Then
*    initialize the chanHdr with some default information, including the
*    name provided by the caller.
*---------------------------------------------------------------------------*/
    if (retStat == OK) {
	BF_BLKNUM blkNum;	/* block # for next empty chanHdr */
	int	hdrNum;		/* index in block for next empty chanHdr */

/*----------------------------------------------------------------------------
*	grab the first entry from the free chanHdr list, getting its block
*	address and subscript, and also getting the actual address of the
*	chanHdr
*---------------------------------------------------------------------------*/
	pMIBuf = pArCfDesc->pMIFree;		/* 'address' of this */
	indx = pArCfDesc->MIFree_hdrNum;	/*    chanHdr */
	pChanHdr = &pMIBuf->chanHdr[indx];
	assert(pChanHdr->name[0] == '\0');	/* make sure it's empty */

/*----------------------------------------------------------------------------
*	set the head of the free chanHdr list to point to the next free
*	chanHdr.  Several special cases exist: no 'next'; 'next' is in the
*	same MI block as the chanHdr we just got; 'next' is in the next
*	MI block.  If none of the special cases are true, then the whole
*	MI chain is searched for the MI buffer with the specified block
*	number (remember, we don't have a direct way to translate between
*	'disk block' number and 'address of buffer in memory').
*
*	the 'free chanHdr list' has two parts to be set: the block number and
*	index in block0; and the buffer address and index in the channel file
*	descriptor.
*---------------------------------------------------------------------------*/
	blkNum = pChanHdr->graphics.nextMIFree.blkNum;
	hdrNum = pChanHdr->graphics.nextMIFree.hdrNum;
	if (blkNum == 0) {			/* no 'next' empty chanHdr */
	    pArCfDesc->pMIFree = NULL;
	    pArCfDesc->MIFree_hdrNum = 0;
	}
	else if (blkNum == pMIBuf->blkNum) {	/* in this block? */
	    pArCfDesc->pMIFree = pMIBuf;
	    pArCfDesc->MIFree_hdrNum = hdrNum;
	}
	else {			/* search for buffer with next empty chanHdr */
	    AR_MI_BUF *pMIFree;	/* temp during search */

	    if (pMIBuf->pNextMI != NULL) {	/* in next block? */
		if (blkNum == pMIBuf->pNextMI->blkNum) {
		    pMIFree = pMIBuf->pNextMI;
		    goto searchDone;
		}
	    }
	    pMIFree = pArCfDesc->pMIHead;
	    while (pMIFree != NULL) {
		if (pMIFree->blkNum == blkNum)
		    goto searchDone;
		pMIFree = pMIFree->pNextMI;
	    }
searchDone:
	    assert(pMIFree != NULL);		/* MUST find a match! */
	    assert(pMIFree->chanHdr[hdrNum].name[0] == '\0');
	    pArCfDesc->pMIFree = pMIFree;
	    pArCfDesc->MIFree_hdrNum = hdrNum;
	}
	ArB0MIFree_blkNum(pArCfDesc) = blkNum;
	ArB0MIFree_hdrNum(pArCfDesc) = hdrNum;

/*----------------------------------------------------------------------------
*	initialize the new chanHdr
*
*	mark the MI block as having been altered
*---------------------------------------------------------------------------*/
	(void)strcpy(pChanHdr->name, name);
	pChanHdr->chanType = TYPENOTCONN;
	pChanHdr->elCount = 0;
	pChanHdr->indexHead = 0;
	pChanHdr->indexTail = 0;
	pChanHdr->oldestSecPastEpoch = 0;
	pChanHdr->newestSecPastEpoch = 0;
	pChanHdr->newestNsec = 0;
	pChanHdr->flags = 0;
	pChanHdr->count = 0;

	ArCFModifySet(pArCfDesc, pMIBuf);
	pArCfDesc->b0Modified = 1;

	*ppMIBuf = pMIBuf;
	*pIndex = indx;
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCF_MIDelChan - delete a chanHdr from master index
*
* DESCRIPTION
*	Deletes a chanHdr from the master index, making it into a
*	'free chanHdr' and placing it at the head of the free chanHdr list.
*
*	The MODIFIED bit is set the the MI block.
*
* RETURNS
*	OK
*
* BUGS
* o	doesn't remove the channel from the hash table (which doesn't
*	presently exist)
* o	doesn't check for index blocks and data blocks--if either exists,
*	memory buffers and disk blocks are essentially 'lost'.
*
* SEE ALSO
*	arCF_MIAddChan()
*
*-*/
long
arCF_MIDelChan(pArCfDesc, pMIBuf, indx)
AR_CF_DESC *pArCfDesc;	/* IO pointer to channel file descriptor */
AR_MI_BUF *pMIBuf;	/* IO pointer to MI block containing channel */
int	indx;		/* I index within MI block of chanHdr for channel */
{
    assert(pArCfDesc != NULL);
    assert(pArCfDesc->pBfDesc != NULL);
    assert(indx >= 0);
    assert(indx < BfB0BlockSize(pArCfDesc->pBfDesc) / sizeof(AR_CHAN_HDR));

/*----------------------------------------------------------------------------
*    update disk information in disk block and block0
*---------------------------------------------------------------------------*/
    pMIBuf->chanHdr[indx].graphics.nextMIFree.blkNum =
						ArB0MIFree_blkNum(pArCfDesc);
    pMIBuf->chanHdr[indx].graphics.nextMIFree.hdrNum =
						ArB0MIFree_hdrNum(pArCfDesc);
    ArB0MIFree_blkNum(pArCfDesc) = pMIBuf->blkNum;
    ArB0MIFree_hdrNum(pArCfDesc) = indx;
/*----------------------------------------------------------------------------
*    update pointer and index in channel file descriptor
*---------------------------------------------------------------------------*/
    pArCfDesc->pMIFree = pMIBuf;
    pArCfDesc->MIFree_hdrNum = indx;

/*----------------------------------------------------------------------------
*    mark this chanHdr as 'free', and set the MODIFIED bit for the MI block
*---------------------------------------------------------------------------*/
    pMIBuf->chanHdr[indx].name[0] = '\0';
    ArCFModifySet(pArCfDesc, pMIBuf);
    pArCfDesc->b0Modified = 1;

    return OK;
}

/*+/subr**********************************************************************
* NAME	arCF_MIFindChan - search master index for a specified channel
*
* DESCRIPTION
*	Search for the specified channel name in the Master Index blocks
*	in memory.
*
* RETURNS
*	OK, or
*	ERROR if channel name not found
*
* BUGS
* o	uses a linear search through all MI blocks
* o	all MI blocks must be in memory
*
* EXAMPLE
*
*-*/
long
arCF_MIFindChan(pArCfDesc, name, ppMIBuf, pIndex)
AR_CF_DESC *pArCfDesc;	/* I pointer to channel file descriptor */
char	*name;		/* I channel name */
AR_MI_BUF **ppMIBuf;	/* O ptr to ptr to MI block containing channel;
				if NULL, no MI block pointer will be
				returned */
int	*pIndex;	/* O pointer to location to store index within
				MI block of chanHdr for channel; if NULL, no
				index will be returned */
{
    AR_MI_BUF	*pMIBuf;	/* pointer to MI block for search */
    int		i;

    assert(pArCfDesc != NULL);
    assert(pArCfDesc->pBfDesc != NULL);
    assert(name != NULL);
    assert(strlen(name) > 0);
    assert(strlen(name) < AR_NAME_DIM);

    pMIBuf = pArCfDesc->pMIHead;		/* head of MI list in memory */

    while (pMIBuf != NULL) {
	for (i=0; i<AR_CF_NHDR; i++) {
	    if (pMIBuf->chanHdr[i].name[0] != '\0') {
		if (strcmp(pMIBuf->chanHdr[i].name, name) == 0)
		    goto done;
	    }
	}
	pMIBuf = pMIBuf->pNextMI;	/* next MI block in memory */
    }

done:
    if (ppMIBuf != NULL)
	*ppMIBuf = pMIBuf;
    if (pIndex != NULL)
	*pIndex = i;

    return(pMIBuf==NULL ? ERROR : OK);
}

long
arCF_MIFlush(pCfDesc, pMIBuf)
AR_CF_DESC *pCfDesc;	/* I pointer to channel file descriptor */
AR_MI_BUF *pMIBuf;	/* I pointer to MI block */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status */

    if (ArCFModifyTestAndReset(pCfDesc, pMIBuf)) {
	stat = bfWrite(pCfDesc->pBfDesc, &pMIBuf->bfInfo, pMIBuf->blkNum);
	if (stat != OK)
	    retStat = ERROR;
    }
    if (pCfDesc->b0Modified) {
	pCfDesc->b0Modified = 0;
	stat = bfWrite(pCfDesc->pBfDesc, pCfDesc->pBfDesc->pBlock0, 0);
	if (stat != OK)
	    retStat = ERROR;
    }
    return retStat;
}
