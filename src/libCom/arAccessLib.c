/*	@(#)arAccessLib.c	1.6 11/12/92
 *	Author:	Roger A. Cole
 *	Date:	03-09-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1990-92, the Regents of the University of California,
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
 *  .03 09-18-91 rac	change arg for arCFChanRead to int--short
 *			causes problems on Sun4; other minor fixes
 *  .04 04-04-92 rac	add arCFChanWrite_args; handle flags for begin
 *			and end of snapshot
 *  .05 09-14-92 rac	remove use of special malloc and free routines
 *  .06 09-30-92 rac	ignore time running backward
 *
 * make options
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code
 */
/*+/mod***********************************************************************
* TITLE	arAccessLib.c - AR access library for AR data set files
*
* DESCRIPTION
*	
* QUICK REFERENCE
* AR_CHAN_DESC *pCFDesc;	pointer to 'AR channel file' descriptor
*
* #include <genDefs.h>		some general use definitions
* #include <tsDefs.h>		definitions for time stamps
* #include <cadef.h>		definitions for Channel Access
* #include <db_access.h>	definitions for database related items
* #include <arAccessLib.h>	structures for AR access library routines
* #include <sys/file.h>		obtain mode definitions for SunOS
*
* AR_CHAN_DESC *pChanDesc;	pointer to channel descriptor in chan file
*   AR_CF_DESC *pCfDesc;	pointer to channel file descriptor
*       chtype  type;		one of the DBR_xxx types
*     TS_STAMP *pStamp;		pointer to a time stamp structure
*          int  mode;		mode to open file or channel--O_RDONLY
*				or O_RDWR
*
*         long  arCFChanClose(        >ppChanDesc			)
*         long  arCFChanDel(          >ppChanDesc			)
*         long  arCFChanFind(          pCfDesc,     chanName        	)
* AR_CHAN_DESC *arCFChanOpen(          pCfDesc,     chanName,   mode	)
*         long  arCFChanPosition(      pChanDesc,   pStamp		)
*         long  arCFChanPurge(         pChanDesc,   pStamp		)
*         long  arCFChanRead(          pChanDesc,   type,  >pDbrBuf,  count)
*         long  arCFChanWrite(         pChanDesc,   pEvHandArg		)
*         long  arCFChanWrite_args(    pChanDesc,   stamp, dbfType,
*                                              count, pData, alStat, alSevr)
*         long  arCFChanWriteByte(     pArChanDesc, value		)
*         long  arCFChanWriteGR(       pChanDesc,   pEvHandArg		)
*         long  arCFClose(            >ppCfDesc				)
*   AR_CF_DESC *arCFCreate(            name,        maxBytes		)
*   AR_CF_DESC *arCFOpen(              name,        mode		)
*         long  arCFSyncRead(          pCfDesc				)
*         long  arCFSyncWrite(         pCfDesc				)
*
*          int  ArCFChanElementCount(  pChanDesc			)
*       chtype  ArCFChanFieldType(     pChanDesc			)
*         char *ArCFChanName(          pChanDesc			)
*          int  ArCFChanNStates(       pChanDesc			)
*          int  ArCFChanPrec(          pChanDesc			)
*         char *ArCFChanStates(        pChanDesc			)
*         char *ArCFChanUnits(         pChanDesc			)
*
*-***************************************************************************/

#include <arAccessLib.h>
#ifndef INC_genDefs_h
#   include <genDefs.h>
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
#ifndef INCLdb_accessh
#   include <db_access.h>
#endif

#include <stdio.h>
#include <sys/file.h>	/* for O_RDWR and O_RDONLY definitions */
#include <strings.h>

/*+/internal------------------------------------------------------------------
* NAME  arCF_malloc/arCF_free
*
* malloc and free routines for arCF routines to use for the various kinds
*       of data structures.  Should the need arise, these routines will
*       be enhanced to maintain lists of free data structures of the various
*       kinds, to eliminate malloc()/free() overhead and memory fragmentation.
*
*    The following routines are used for calling malloc() and free() for
*    some common data structures in arAccessLib.  They have two versions,
*    depending on whether DEBUG is defined.  Both versions perform the
*    described function.  The DEBUG version provides the capability for
*    `bounds checking' on allocated buffers, and also for filling buffers
*    with a known pattern for use in `un-initialized buffer' bug detection.
*
*      AR_CF_DESC *arCF_mallocCF_DESC()     void  arCF_freeCF_DESC(pCFDesc)
*    AR_CHAN_DESC *arCF_mallocCHAN_DESC()   void  arCF_freeCHAN_DESC(pChanDesc)
*---------------------------------------------------------------------------*/


/* Forward declarations */
long arCFChanReadTs();
long arCFReadMIBuffers();

AR_CHAN_DESC *
arCF_mallocCHAN_DESC()
{
#ifdef DEBUG
    return (AR_CHAN_DESC *)arMalloc(sizeof(AR_CHAN_DESC), "AR_CHAN_DESC");
#else
    return (AR_CHAN_DESC *)malloc(sizeof(AR_CHAN_DESC));
#endif
}
void
arCF_freeCHAN_DESC(ptr)
AR_CHAN_DESC *ptr;
{
#ifdef DEBUG
    arFree(ptr, sizeof(AR_CHAN_DESC), "AR_CHAN_DESC");
#else
    free(ptr);
#endif
}

AR_CF_DESC *
arCF_mallocCF_DESC()
{
#ifdef DEBUG
    return (AR_CF_DESC *)arMalloc(sizeof(AR_CF_DESC), "AR_CF_DESC");
#else
    return (AR_CF_DESC *)malloc(sizeof(AR_CF_DESC));
#endif
}
void
arCF_freeCF_DESC(ptr)
AR_CF_DESC *ptr;
{
#ifdef DEBUG
    arFree(ptr, sizeof(AR_CF_DESC), "AR_CF_DESC");
#else
    free(ptr);
#endif
}

/*+/macro*********************************************************************
* NAME	ArCFChanXxx - convenience macros for channel files
*
* DESCRIPTION
*	These macros provide easy access to information about a channel in
*	a channel file.  The macros are defined in arAccessLib.h
*
*          int  ArCFChanElementCount( pArChanDesc	)
*       chtype  ArCFChanFieldType(    pArChanDesc	)
*         char *ArCFChanName(         pArChanDesc	)
*
*	The following marcos for obtaining graphics information bypasses
*	the need to do a read with DBR_GR_xxx, and thus simplifies
*	programming.  However, programs which must run with both
*	Channel Access and the AR access library should probably use a
*	read to obtain the information.
*
*          int  ArCFChanNStates(      pArChanDesc	)
*          int  ArCFChanPrec(         pArChanDesc	)
*         char *ArCFChanStates(       pArChanDesc	)
*         char *ArCFChanUnits(        pArChanDesc	)
*
* RETURNS
*	as indicated
*
* NOTES
* 1.	These macros assume, but do not check, that the channel descriptor
*	is open.
* 2.	The macros for obtaining graphics information assume, but do not
*	check, that the macro being used is appropriate for the native
*	type of the channel.
*
*-*/


/*+/subr**********************************************************************
* NAME	arCFChanClose - close a channel of an AR channel data file
*
* DESCRIPTION
*	Close a channel descriptor.  If any I/O is outstanding for the
*	descriptor, that I/O is forced to completion; this may include
*	writing data and index blocks to disk and updating oldestStamp and
*	newestStamp in the chanHdr.  Memory buffers for data and index blocks
*	are free()'d.
*
*	The count of channel descriptors open to the channel is decremented.
*
*	The channel descriptor is removed from the channel descriptor list
*	in the channel file descriptor.  The channel descriptor is free()'d
*	and NULL is stored in caller's pointer to prevent further use
*
* RETURNS
*	OK
*
* BUGS
* o	doesn't flush the MI buffer
* o	primitive error handling
*
* SEE ALSO
*	arCFChanOpen(), arCFChanDel()
*
*-*/
long
arCFChanClose(ppArChanDesc)
AR_CHAN_DESC **ppArChanDesc;	/* IO ptr to ptr to channel descriptor;
					will be NULL at completion */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status to caller */
    AR_CHAN_DESC *pArChanDesc;	/* pointer to channel descriptor */
    AR_CHAN_HDR *pChanHdr;	/* pointer to channel header */

    assert(*ppArChanDesc != NULL);
    pArChanDesc = *ppArChanDesc;
    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pArCfDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);
    pChanHdr = &ArCDChanHdr(pArChanDesc);
    assert(pChanHdr->count > 0);

/*----------------------------------------------------------------------------
*    flush data and index buffers and free() them.
*    if the MI for this channel has been altered, write it
*---------------------------------------------------------------------------*/
    if (pArChanDesc->pDataBuf != NULL) {
	stat = arCF_DFlushAndFree(pArChanDesc);
	assert(stat == OK);
    }
    if (pArChanDesc->pIndexBuf != NULL) {
	stat = arCF_IFlushAndFree(pArChanDesc);
	assert(stat == OK);
    }
    if (pArChanDesc->pMIBuf != NULL) {
	stat = arCF_MIFlush(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);
	assert(stat == OK);
    }
/*----------------------------------------------------------------------------
*    remove this channel descriptor from the list of channel descriptors.
*---------------------------------------------------------------------------*/
    if (pArChanDesc->pNextDesc != NULL)
	pArChanDesc->pNextDesc->pPrevDesc = pArChanDesc->pPrevDesc;
    else
	pArChanDesc->pArCfDesc->pChanDescTail = pArChanDesc->pPrevDesc;
    if (pArChanDesc->pPrevDesc != NULL)
	pArChanDesc->pPrevDesc->pNextDesc = pArChanDesc->pNextDesc;
    else
	pArChanDesc->pArCfDesc->pChanDescHead = pArChanDesc->pNextDesc;
/*----------------------------------------------------------------------------
*    decrement count of descriptors open to this chanHdr
*    if this descriptor had the chanHdr open in O_RDWR mode, reset the
*	flag in the chanHdr
*---------------------------------------------------------------------------*/
    pChanHdr->count -= 1;
    if ((pArChanDesc->flags & AR_CDESC_WRITE) != 0) {
	assert((pChanHdr->flags & AR_CHDR_WRITE) != 0);
	pChanHdr->flags ^= AR_CHDR_WRITE;
    }

/*----------------------------------------------------------------------------
*    free the descriptor and put NULL in the caller's pointer, to prevent
*    further use of the descriptor
*---------------------------------------------------------------------------*/
    arCF_freeCHAN_DESC(pArChanDesc);
    *ppArChanDesc = NULL;

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCFChanDel - delete a channel of an AR channel data file
*
* DESCRIPTION
*	Deletes all data and index buffers for the channel, deletes the
*	chanHdr for the channel, and closes the channel descriptor.  If the
*	delete is successful, then the channel descriptor has been changed
*	to NULL, preventing further use; memory associated with data buffers,
*	index buffers, and the channel descriptor has been free()'d.  Disk
*	blocks which are deleted from the channel are added to the channel
*	file's free block list; the chanHdr is added to the free chanHdr list.
*
*	The channel descriptor must be open O_RDWR, and there must be no
*	other channel descriptors open to the channel.
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
arCFChanDel(ppArChanDesc)
AR_CHAN_DESC **ppArChanDesc;	/* IO ptr to ptr to channel descriptor */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status to caller */
    AR_CHAN_DESC *pArChanDesc;	/* pointer to channel descriptor */
    AR_CHAN_HDR *pChanHdr;	/* pointer to channel header */

    assert(*ppArChanDesc != NULL);
    pArChanDesc = *ppArChanDesc;
    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pArCfDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);
    pChanHdr = &ArCDChanHdr(pArChanDesc);
    assert(pChanHdr->count > 0);

    if ((pArChanDesc->flags & AR_CDESC_WRITE) == 0) {
	(void)fprintf(stderr, "arCFChanDel: %s not open O_RDWR\n",
			ArCDChanHdr(pArChanDesc).name);
	retStat = ERROR;
    }
    if (pChanHdr->count > 1) {
	(void)fprintf(stderr, "arCFChanDel: others have %s open\n",
			ArCDChanHdr(pArChanDesc).name);
	retStat = ERROR;
    }

/*----------------------------------------------------------------------------
*    free all the data and index buffers for this channel
*---------------------------------------------------------------------------*/
    if (retStat == OK) {
	if ((stat = arCFChanPurge(pArChanDesc, (TS_STAMP *)NULL)) != OK) {
	    (void)fprintf(stderr,
"arCFChanDel: error deleting data and/or index buffers for %s\n",
		ArCDChanHdr(pArChanDesc).name);
	    retStat = ERROR;
	}
    }

/*----------------------------------------------------------------------------
*    remove this channel descriptor from the list of channel descriptors.
*---------------------------------------------------------------------------*/
    if (retStat == OK) {
	if (pArChanDesc->pNextDesc != NULL)
	    pArChanDesc->pNextDesc->pPrevDesc = pArChanDesc->pPrevDesc;
	else
	    pArChanDesc->pArCfDesc->pChanDescTail = pArChanDesc->pPrevDesc;
	if (pArChanDesc->pPrevDesc != NULL)
	    pArChanDesc->pPrevDesc->pNextDesc = pArChanDesc->pNextDesc;
	else
	    pArChanDesc->pArCfDesc->pChanDescHead = pArChanDesc->pNextDesc;

/*----------------------------------------------------------------------------
*    move the chanHdr to the free chanHdr list and mark the MI block
*    as MODIFIED
*---------------------------------------------------------------------------*/
	stat = arCF_MIDelChan(pArChanDesc->pArCfDesc,
		pArChanDesc->pMIBuf, pArChanDesc->hdrNum);
	ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);

/*----------------------------------------------------------------------------
*    free the descriptor and put NULL in the caller's pointer, to prevent
*    further use of the descriptor
*---------------------------------------------------------------------------*/
	arCF_freeCHAN_DESC(pArChanDesc);
	*ppArChanDesc = NULL;
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCFChanFind - find a channel in an AR channel data file
*
* DESCRIPTION
*	Find a channel in a channel data file.  This routine is intended
*	to be used in cases where it is necessary to know prior to doing
*	an arCFChanOpen() whether the file contains a particular channel.
*
* RETURNS
*	OK if channel is found, or
*	ERROR
*
* BUGS
* o	text
*
* EXAMPLE
*
*-*/
long
arCFChanFind(pArCfDesc, chanName)
AR_CF_DESC *pArCfDesc;	/* I pointer to channel file descriptor */
char	*chanName;	/* I channel name */
{
    AR_MI_BUF	*pMIBuf;	/* ptr to MI block containing chanHdr */
    int		indx;		/* index in MI block of chanHdr */
    int		l;		/* length of chanName */
    char	name[AR_NAME_DIM]; /* buffer for appending .VAL */

    assert(pArCfDesc != NULL);
    l = strlen(chanName);

    if (index(chanName, '.') == NULL) {
	if (l <= 0 || l >= AR_NAME_DIM-5) {
	    (void)fprintf(stderr, "arCFChanFind: chanName illegal length\n");
	    return ERROR;
	}
	(void)strcpy(name, chanName);
	(void)strcat(name, ".VAL");
	chanName = name;
    }
    else {
	if (l <= 0 || l >= AR_NAME_DIM) {
	    (void)fprintf(stderr, "arCFChanFind: chanName illegal length\n");
	    return ERROR;
	}
    }
    return arCF_MIFindChan(pArCfDesc, chanName, &pMIBuf, &indx);
}

/*+/subr**********************************************************************
* NAME	arCFChanOpen - open a channel to an AR channel data file
*
* DESCRIPTION
*	Open a channel in a channel data file.  If the open is successful,
*	a channel descriptor is returned.  If the channel doesn't exist,
*	one is created if mode is O_RDWR; in this case, the .flags item in
*	the channel descriptor will have AR_CDESC_NEW set.
*
*	When the open is complete, the relevent index and data blocks will
*	be in memory.  If the mode is O_RDWR, the position will be at the
*	end of the channel's data, ready for writing; for O_RDONLY, at the
*	start of the channel's data, ready for reading.
*
*	If the channel descriptor isn't closed before the channel data file
*	is closed, it will be closed automatically, with updated blocks
*	written to the channel data file.
*
*	If O_RDWR is specified, the channel data file must have been opened
*	with O_RDWR and there must be no other channel descriptor open in
*	O_RDWR mode to the desired channel.
*
*	The count of channel descriptors open to the desired channel is
*	incremented.
*
* RETURNS
*	pointer to channel descriptor, or
*	NULL
*
* BUGS
* o	no exit handler (this will probably be covered by the exit handler
*	for the file, which isn't presently done)
* o	for reading, the possibility of opening to a position (date/time)
*	would potentially avoid wasting a disk read
*
* SEE ALSO
*	arCFChanClose(), arCFOpen(), arCFClose()
*
* EXAMPLE
*
*-*/
AR_CHAN_DESC *
arCFChanOpen(pArCfDesc, chanName, mode)
AR_CF_DESC *pArCfDesc;	/* IO pointer to channel file descriptor */
char	*chanName;	/* I channel name */
int	mode;		/* I mode for open--O_RDONLY or O_RDWR; if mode is
				O_RDWR and the channel doesn't exist, it
				is created */
{
    int		stat;		/* status return from calls */
    int		retStat=OK;	/* cumulative status in this routine */
    AR_MI_BUF	*pMIBuf;	/* ptr to MI block containing chanHdr */
    int		indx;		/* index in MI block of chanHdr */
    AR_CHAN_HDR *pChanHdr;	/* pointer to chanHdr for this channel */
    AR_CHAN_DESC *pChanDesc=NULL;/* pointer to channel descriptor */
    int		l;		/* length of chanName */
    char	name[AR_NAME_DIM]; /* buffer for appending .VAL */

    assert(pArCfDesc != NULL);
    assert(mode == O_RDONLY || mode == O_RDWR);

/*----------------------------------------------------------------------------
*    check chanName to see if it specifies a .field ; if it doesn't, copy
*    chanName to 'name' and append .VAL , then change chanName to point to
*    'name'
*---------------------------------------------------------------------------*/
    l = strlen(chanName);
    if (index(chanName, '.') == NULL) {
	if (l <= 0 || l >= AR_NAME_DIM-5) {
	    (void)fprintf(stderr, "arCFChanOpen: chanName illegal length\n");
	    retStat = ERROR;
	}
	else {
	    (void)strcpy(name, chanName);
	    (void)strcat(name, ".VAL");
	    chanName = name;
	}
    }
    else {
	if (l <= 0 || l >= AR_NAME_DIM) {
	    (void)fprintf(stderr, "arCFChanOpen: chanName illegal length\n");
	    retStat = ERROR;
	}
    }

/*----------------------------------------------------------------------------
*    make sure file is O_RDWR if ChanOpen request is O_RDWR
*---------------------------------------------------------------------------*/
    if ( (mode == O_RDWR) &&
		 ((pArCfDesc->pBfDesc->flags & BF_DESC_WRITE) == 0) ) {
        (void)fprintf(stderr,
		"arCFChanOpen: can't open %s O_RDWR--file not O_RDWR\n",
		chanName);
        retStat = ERROR;
    }

/*----------------------------------------------------------------------------
*    allocate a channel descriptor and clear the 'flags' item.  If the open
*    is with O_RDWR, set the AR_CDESC_WRITE bit in the 'flags' item.
*---------------------------------------------------------------------------*/
    if (retStat == OK) {
	if ((pChanDesc = arCF_mallocCHAN_DESC()) == NULL) {
	    (void)fprintf(stderr,
		"arCFChanOpen: can't malloc for chan desc for %s\n",
		chanName);
	    retStat = ERROR;
	}
	else {
	    pChanDesc->flags = 0;
	    if (mode == O_RDWR)
		pChanDesc->flags |= AR_CDESC_WRITE;
	}
    }

/*----------------------------------------------------------------------------
*    try to locate the chanHdr for the channel.  If no chanHdr is found,
*    create one, providing that mode is O_RDWR; if mode is O_RDONLY, then
*    not finding a chanHdr is an error.  If a new chanHdr is created, then
*    set AR_CDESC_NEW in the channel descriptor.
*---------------------------------------------------------------------------*/
    if (retStat == OK) {
	stat = arCF_MIFindChan(pArCfDesc, chanName, &pMIBuf, &indx);
	if (stat != OK) {
	    if ((pChanDesc->flags & AR_CDESC_WRITE) != 0) {
		stat = arCF_MIAddChan(pArCfDesc, chanName, &pMIBuf, &indx);
		if (stat == OK)
		    pChanDesc->flags |= AR_CDESC_NEW;
		else {
		    (void)fprintf(stderr,
			"arCFChanOpen: error adding chanHdr for %s\n",
			chanName);
		    retStat = ERROR;
		}
	    }
	    else {
		(void)fprintf(stderr, "arCFChanOpen: %s not found\n", chanName);
		retStat = ERROR;
	    }
	}
    }
    if (retStat == OK) {
/*----------------------------------------------------------------------------
*    compute the actual address of the chanHdr, for easier manipulation.  If
*    the chanHdr is already open in O_RDWR mode, and if this open is for
*    O_RDWR, an error exists.  If this is the only request for O_RDWR, flag
*    AR_CHDR_WRITE in the chanHdr to disallow other writers.  If all is well,
*    increment in the chanHdr the count of descriptors open to it.
*---------------------------------------------------------------------------*/
	assert(pMIBuf != NULL);
	pChanHdr = &pMIBuf->chanHdr[indx];
	if ((pChanDesc->flags & AR_CDESC_WRITE) != 0) {
	    if ((pChanHdr->flags & AR_CHDR_WRITE) != 0) {
		(void)fprintf(stderr,
			"arCFChanOpen: %s already open for writing\n",
			chanName);
		retStat = ERROR;
	    }
	    else {
		pChanHdr->flags |= AR_CHDR_WRITE;
		pChanHdr->count += 1;
	    }
	}
	else
	    pChanHdr->count += 1;

    }
    if (retStat == OK) {
/*----------------------------------------------------------------------------
*	the channel file descriptor has a list of open channel descriptors;
*	link this channel descriptor onto the end of that list (which is
*	a doubly linked list).
*---------------------------------------------------------------------------*/
	pChanDesc->pNextDesc = NULL;
	pChanDesc->pPrevDesc = pArCfDesc->pChanDescTail;
	if (pArCfDesc->pChanDescHead != NULL) {
	    assert(pArCfDesc->pChanDescTail != NULL);
	    pArCfDesc->pChanDescTail->pNextDesc = pChanDesc;
	}
	else {
	    assert(pArCfDesc->pChanDescTail == NULL);
	    pArCfDesc->pChanDescHead = pChanDesc;
	}
	pArCfDesc->pChanDescTail = pChanDesc;

/*----------------------------------------------------------------------------
*	set up the channel descriptor.  Part of this setup includes getting
*	an index buffer and a data buffer.  If the open is O_RDONLY, then
*	the position will be at the first index block and first data block;
*	for O_RDWR, the last index block and last data block will be used.
*---------------------------------------------------------------------------*/
	pChanDesc->pArCfDesc = pArCfDesc;
	pChanDesc->pMIBuf = pMIBuf;
	pChanDesc->hdrNum = indx;
	pChanDesc->pIndexBuf = NULL;
	pChanDesc->pDataBuf = NULL;
	pChanDesc->pData = NULL;
	pChanDesc->remainCount = -10;
	pChanDesc->datInfNum = -10;
	pChanDesc->timeStamp.secPastEpoch = 0;
	pChanDesc->timeStamp.nsec = 0;
	stat = arCF_DNextBlock(pChanDesc);
	if (stat == ERROR && (pChanDesc->flags & AR_CDESC_EOF) == 0)
	    assertAlways(0);
    }
    else {
	if (pChanDesc != NULL) {
	    arCF_freeCHAN_DESC(pChanDesc);
	    pChanDesc = NULL;
	}
    }

    return pChanDesc;
}

/*+/subr**********************************************************************
* NAME	arCFChanPosition - position channel decscriptor based on time stamp
*
* DESCRIPTION
*	Position a channel descriptor so it is ready to read the data with
*	the specified time stamp.  If no data with the specified time stamp
*	is available, the positioning is for the first data following.
*
*	End of file results in an error return, with the AR_CDESC_EOF flag
*	set in chanDesc.flags  .
*
* RETURNS
*	OK, or
*	ERROR
*
* SEE ALSO
*	arCFChanRead()
*
* EXAMPLE
*
*-*/
long
arCFChanPosition(pArChanDesc, pPosStamp)
AR_CHAN_DESC *pArChanDesc;/* IO pointer to channel descriptor */
TS_STAMP *pPosStamp;	/* I stamp for desired data; NULL for `rewind' */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status to caller */
    AR_CHAN_HDR *pChanHdr;	/* pointer to chanHdr */
    AR_INDEX_BUF *pIndexBuf;	/* pointer to buffer for index buffer */
    AR_DATA_BUF *pDataBuf;	/* pointer to buffer for data buffer */
    BF_BLKNUM indexBlock;	/* block # for index block */
    BF_BLKNUM nextIndexBlock;	/* block # for next index block */
    BF_BLKNUM dataBlock;	/* block # for data block */
    int		firstDatInf;	/* first datInfo in index block */
    int		curDatInf;	/* current datInfo in index block */
    int		lastDatInf;	/* last datInfo in index block */
    int		i;		/* temp for loops */
    chtype	type;

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pArCfDesc != NULL);
    pChanHdr = &ArCDChanHdr(pArChanDesc);
    assert(pChanHdr != NULL);

    if (pChanHdr->newestSecPastEpoch == 0) {
	pArChanDesc->flags |= AR_CDESC_EOF;
	return ERROR;
    }
    else if (pPosStamp == NULL ||
		    pPosStamp->secPastEpoch < pChanHdr->oldestSecPastEpoch) {
/*----------------------------------------------------------------------------
* rewind
*	either a rewind was requested, or else the requested stamp is prior
*	to the oldest data for the file
*----------------------------------------------------------------------------*/
	pArChanDesc->datInfNum = -10;
	pArChanDesc->remainCount = -10;
	stat = arCF_DNextBlock(pArChanDesc);
	if (stat == ERROR && (pArChanDesc->flags & AR_CDESC_EOF) == 0)
	    assertAlways(0);
	return stat;
    }

/*----------------------------------------------------------------------------
* search is required
*	It isn't known yet whether the search will be forward or reverse.
*	If the desired stamp is less than that of the first datInf,
*	then a backward search is required.  If the desired stamp
*	is greater than that of the first datInf, then a forward
*	search (possibly confined to this index block) is required.
*
*	A special case occurs when the present index block contains no
*	datInf's with time stamps.  In this case, a backward search is
*	done.  (This case arises because data blocks which contain only
*	data items don't have a time stamp in their datInf entry.)
*----------------------------------------------------------------------------*/
    pIndexBuf = pArChanDesc->pIndexBuf;
    lastDatInf = (pIndexBuf->bfInfo.lastByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
    while (lastDatInf > 0 &&
		    pIndexBuf->datInfo[lastDatInf].stamp.secPastEpoch == 0) {
	lastDatInf--;
    }
    firstDatInf = (pIndexBuf->bfInfo.firstByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
    while (firstDatInf < lastDatInf &&
		    pIndexBuf->datInfo[firstDatInf].stamp.secPastEpoch == 0) {
	firstDatInf++;
    }
    if (firstDatInf > lastDatInf ||
	    TsCmpStampsLT(pPosStamp, &pIndexBuf->datInfo[firstDatInf].stamp)) {
/*----------------------------------------------------------------------------
* reverse search
*	Since the file contains extremely little information to allow
*	`going backward', a reverse search is accomplished with a rewind
*	followed by a forward search.
*----------------------------------------------------------------------------*/
	pArChanDesc->datInfNum = -10;
	pArChanDesc->remainCount = -10;
	stat = arCF_INextDatInfo(pArChanDesc);
	if (stat == ERROR && (pArChanDesc->flags & AR_CDESC_EOF) == 0)
	    assertAlways(0);
	if (stat == ERROR)
	    return stat;
    }
    pIndexBuf = pArChanDesc->pIndexBuf;
    lastDatInf = (pIndexBuf->bfInfo.lastByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
    while (lastDatInf > 0 &&
		    pIndexBuf->datInfo[lastDatInf].stamp.secPastEpoch == 0) {
	lastDatInf--;
    }
/*----------------------------------------------------------------------------
* forward search
*----------------------------------------------------------------------------*/
    while (lastDatInf >= 0 &&
	    TsCmpStampsGT(pPosStamp, &pIndexBuf->datInfo[lastDatInf].stamp)) {
/*----------------------------------------------------------------------------
*	desired position may be in this index block, or it may be in a
*	following one.  In order to find out, we'll have to read the next
*	one and do an explicit check.
*----------------------------------------------------------------------------*/
	indexBlock = pIndexBuf->blkNum;
	if ((nextIndexBlock = pIndexBuf->bfInfo.flink) != 0) {
	    stat = arCF_IRead(pArChanDesc, pIndexBuf, nextIndexBlock);
	    if (stat == OK) {
		lastDatInf = (pIndexBuf->bfInfo.lastByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
		while (lastDatInf > 0 &&
		    pIndexBuf->datInfo[lastDatInf].stamp.secPastEpoch == 0) {
		    lastDatInf--;
		}
		firstDatInf = (pIndexBuf->bfInfo.firstByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
		while (firstDatInf < lastDatInf &&
		    pIndexBuf->datInfo[firstDatInf].stamp.secPastEpoch == 0) {
		    firstDatInf++;
		}
		if (firstDatInf > lastDatInf ||
		    TsCmpStampsLE(pPosStamp,
				    &pIndexBuf->datInfo[firstDatInf].stamp)) {
		    /* woops!!! time to back up! */
		    stat = arCF_IRead(pArChanDesc, pIndexBuf, indexBlock);
		    if (stat != OK)
			assertAlways(0);
		    else
			break;
		}
	    }
	    else {
		/* error reading next block; back up and let normal code
		handle the error */
		stat = arCF_IRead(pArChanDesc, pIndexBuf, indexBlock);
		if (stat != OK)
		    assertAlways(0);
		else
		    break;
	    }
	}
	else
	    break;
    }
/*----------------------------------------------------------------------------
*	desired position may be somewhere in this index block; this is the
*	index block where we'll start narrowing down the search.
*
*	First, the datInfo whose stamp is greater than or equal to the
*	desired stamp will be found.  Then reading will start with the
*	preceding datInfo until a stamp equal to or greater than the
*	desired stamp is found.
*----------------------------------------------------------------------------*/
    indexBlock = pIndexBuf->blkNum;
    lastDatInf = (pIndexBuf->bfInfo.lastByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
    while (lastDatInf > 0 &&
	pIndexBuf->datInfo[lastDatInf].stamp.secPastEpoch == 0) {
	lastDatInf--;
    }
    assert(lastDatInf >= 0);
    firstDatInf = (pIndexBuf->bfInfo.firstByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
    while (firstDatInf < lastDatInf &&
	pIndexBuf->datInfo[firstDatInf].stamp.secPastEpoch == 0) {
	firstDatInf++;
    }
    assert(firstDatInf >= 0);
    i = curDatInf = firstDatInf;
    while (i < lastDatInf) {
	if (pIndexBuf->datInfo[i+1].stamp.secPastEpoch == 0)
	    i++;
	else if (TsCmpStampsGE(&pIndexBuf->datInfo[i+1].stamp, pPosStamp))
	    break;
	else {
	    i++;
	    curDatInf = i;
	}
    }
    pArChanDesc->datInfNum = curDatInf;
    pArChanDesc->timeStamp= pIndexBuf->datInfo[curDatInf].stamp;
    pArChanDesc->remainCount = -10;
    stat = arCF_DRead(pArChanDesc, pArChanDesc->pDataBuf,
				    pIndexBuf->datInfo[curDatInf].dataBlock);
    if (stat != OK)
	assertAlways(0);
    while (1) {
	stat = arCFChanReadTs(pArChanDesc);
	if (stat != OK) {
	    if ((pArChanDesc->flags &
		(AR_CDESC_BEGIN|AR_CDESC_INACT|
		AR_CDESC_SNAP_BEGIN|AR_CDESC_SNAP_END)) == 0) {
		retStat = ERROR;
		break;
	    }
	}
	else if (TsCmpStampsGE(&pArChanDesc->timeStamp, pPosStamp))
	    break;
	else {
	    type = dbf_type_to_DBR(ArCFChanFieldType(pArChanDesc));
	    stat = arCFChanRead(pArChanDesc, type, (void *)NULL, 0);
	    if (stat != OK) {
		retStat = ERROR;
		break;
	    }
	}
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCFChanPurge - purge data for a channel of an AR channel data file
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	doesn't update oldestSecPastEpoch and newestSecPastEpoch
* o	doesn't initialize the read/write items in the chanDesc
* o	always does TOTAL purge, ignoring requested time stamp
* o	there are some critical sections unprotected
*	-  between malloc() and storing pointer in descriptor
*	-  between releasing blocks and updating pointers and links
* o	error handling is shakey, at best
*
* SEE ALSO
*	arCFChanDel()
*
* EXAMPLE
*
*-*/
long
arCFChanPurge(pArChanDesc, pKeepStamp)
AR_CHAN_DESC *pArChanDesc;/* IO pointer to channel descriptor */
TS_STAMP *pKeepStamp;	/* I stamp for oldest data to keep; NULL says
				delete all data */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status to caller */
    AR_CHAN_HDR *pChanHdr;	/* pointer to chanHdr */
    BF_DESC	*pBfDesc;	/* pointer to block file descriptor */
    AR_INDEX_BUF *pIndexBuf;	/* pointer to buffer for index buffer */
    AR_DATA_BUF *pDataBuf;	/* pointer to buffer for data buffer */

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pArCfDesc != NULL);
    pBfDesc = pArChanDesc->pArCfDesc->pBfDesc;
    assert(pBfDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);
    pChanHdr = &ArCDChanHdr(pArChanDesc);
    assert(pChanHdr->count > 0);

    if ((pArChanDesc->flags & AR_CDESC_WRITE) == 0) {
	(void)fprintf(stderr, "arCFChanPurge: %s not open O_RDWR\n",
			ArCDChanHdr(pArChanDesc).name);
	retStat = ERROR;
    }
    if (pChanHdr->count > 1) {
	(void)fprintf(stderr, "arCFChanPurge: others have %s open\n",
			ArCDChanHdr(pArChanDesc).name);
	retStat = ERROR;
    }

/*----------------------------------------------------------------------------
*    o  if a read block is in memory, free it
*    o  if a write block is in memory, ????
*    o  get buffers for index and data blocks, and put their pointers in
*	the channel descriptor
*    o  traverse the index buffer chain, and all the data buffers, freeing
*	the appropriate buffers
*---------------------------------------------------------------------------*/
    if (retStat == OK) {
	if ((pIndexBuf = arCF_mallocINDEX_BUF()) == NULL) {
	    (void)fprintf(stderr, "arCFChanPurge: can't malloc to purge %s\n",
			ArCDChanHdr(pArChanDesc).name);
	    retStat = ERROR;
	}
	else
	    pArChanDesc->pIndexBuf = pIndexBuf;

	if (retStat == OK) {
	    if ((pDataBuf = arCF_mallocDATA_BUF()) == NULL) {
		(void)fprintf(stderr,
			"arCFChanPurge: can't malloc to purge %s\n",
			ArCDChanHdr(pArChanDesc).name);
		retStat = ERROR;
	    }
	    else
		pArChanDesc->pDataBuf = pDataBuf;
	}
    }
    if (retStat == OK) {
	BF_BLKNUM indexBlock;		/* block # for index block */
	BF_BLKNUM nextIndexBlock;	/* block # for next index block */
	BF_BLKNUM dataBlock;		/* block # for data block */
	int	curDatInf;		/* current datInfo in index block */
	int	lastDatInf;		/* last datInfo in index block */

	indexBlock = pChanHdr->indexHead;
	while (retStat == OK && indexBlock != 0) {
	    stat = bfRead(pBfDesc, &pIndexBuf->bfInfo, indexBlock);
	    if (stat == OK) {
		nextIndexBlock = pIndexBuf->bfInfo.flink;
		pIndexBuf->blkNum = indexBlock;
		ArCFModifyInit(pIndexBuf);
		pIndexBuf->pBlock = &pIndexBuf->bfInfo;
	    }
	    else {
		(void)fprintf(stderr,
		    "arCFChanPurge: bfRead error, indxBlk for %s\n",
			ArCDChanHdr(pArChanDesc).name);
		retStat = ERROR;
	    }
	    if (stat == OK && pIndexBuf->bfInfo.lastByte > BF_BLOCK_DATA) {
/*----------------------------------------------------------------------------
*	march through all the datInfo entries in this index block.  Use
*	firstByte and lastByte to figure out how many datInfo's there are.
*
*	for each datInfo entry,
*	o  read the block
*	o  release the block to the free block list
*	o  adjust firstByte in the index block to be past the datInfo just
*	   processed (this keeps firstByte valid)
*
* BUGS
* o	this section of code should actually compare time stamps in datInfo
*	with the caller supplied time stamp.  If newestSecPastEpoch is newer
*	than the caller's time stamp, delete the whole data block; otherwise,
*	either the purge is complete, or only part of the data is to be
*	purged (which means keeping the datInfo entry, adjusting its
*	time stamp value, and adjusting firstByte in the data block).  In
*	addition, it must be taken into account that the stamps in some
*	datInfo are zero--for the data blocks which don't have a time item.
*---------------------------------------------------------------------------*/
		curDatInf = (pIndexBuf->bfInfo.firstByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);
		lastDatInf = (pIndexBuf->bfInfo.lastByte - BF_BLOCK_DATA) /
							sizeof(AR_DAT_INFO);

		while (curDatInf < lastDatInf && retStat == OK) {
		    dataBlock = pIndexBuf->datInfo[curDatInf].dataBlock;
		    stat = bfRead(pBfDesc, &pDataBuf->bfInfo, dataBlock);
		    if (stat == OK) {
			pDataBuf->blkNum = dataBlock;
			ArCFModifyInit(pDataBuf);
			pDataBuf->pBlock = &pDataBuf->bfInfo;
		    }
		    else {
			(void)fprintf(stderr,
			    "arCFChanPurge: bfRead error, dataBlk for %s\n",
				ArCDChanHdr(pArChanDesc).name);
			retStat = ERROR;
		    }
		    if (retStat == OK)
			stat = bfRelease(pBfDesc, &pDataBuf->bfInfo, dataBlock);
		    if (stat == OK) {
			/* adjust firstByte in index block to indicate that
			*  this blockInfo entry is no longer valid, just in
			*  case */
			pIndexBuf->bfInfo.firstByte += sizeof(AR_DAT_INFO);
		    }
		    else {
			(void)fprintf(stderr,
			    "arCFChanPurge: bfRelease error, dataBlk for %s\n",
				ArCDChanHdr(pArChanDesc).name);
			retStat = ERROR;
		    }
		    curDatInf++;
		}
	    }
	    if (retStat == OK) {
/*----------------------------------------------------------------------------
*	now release the index block back to the free block list.
*
*	set to process the next index block, using the forward link which
*	was saved earlier.  Also, set the pointer the the index list in the
*	chanHdr to point to the next index block.
*
* BUGS
* o	if purging by time stamp is being done, then not all datInfo
*	structures may have been deleted from the index block.  In that case,
*	it would be necessary to write the updated index block back to the
*	file and leave it as the head of the index block list.
*---------------------------------------------------------------------------*/
		stat = bfRelease(pBfDesc, &pIndexBuf->bfInfo, indexBlock);
		if (stat != OK) {
		    (void)fprintf(stderr,
			"arCFChanPurge: bfRelease error, indexBlk for %s\n",
			    ArCDChanHdr(pArChanDesc).name);
		    retStat = ERROR;
		}
		indexBlock = nextIndexBlock;
		pChanHdr->indexHead = indexBlock;
	    }
	}
    }
/*----------------------------------------------------------------------------
* final cleanup
*
* BUGS
* o	cleanup assumes total purge
*----------------------------------------------------------------------------*/
    arCF_freeDATA_BUF(pDataBuf);
    pArChanDesc->pDataBuf = NULL;
    arCF_freeINDEX_BUF(pIndexBuf);
    pArChanDesc->pIndexBuf = NULL;
    pArChanDesc->pData = NULL;
    pArChanDesc->remainCount = -10;
    pArChanDesc->datInfNum = -10;
    if (pChanHdr->indexHead == 0)
	pChanHdr->indexTail = 0;

    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);

    return retStat;
}

/*----------------------------------------------------------------------------
* macros for use by arCFChanRead() and friends
*
*    int ArCRFetch	fetch as an int the next character from the current
*			read buffer; if there isn't a next character
*			(or if there isn't a current read buffer),
*			get the next read buffer and fetch the
*			first available character from it.  On error
*			or at end of file, returns ERROR.
*---------------------------------------------------------------------------*/
#define ArCRFetch \
	(  (--pArChanDesc->remainCount >= 0) \
		    ? ((int)*(++pArChanDesc->pData))&0xff \
		    : arCF_DFillRbufAndGetc(pArChanDesc) )

/*+/subr**********************************************************************
* NAME	arCFChanRead - read next value from a channel
*
* DESCRIPTION
*	This routine reads the next value for a channel and fills in the
*	caller's db_access_val union with the value, time stamp, etc.
*	as appropriate.
*
*	An ERROR return indicates that a valid value wasn't put into the
*	caller's buffer.  The .flags item in the channel descriptor must be
*	examined to determine the reason:
*
*	    AR_CDESC_EOF indicates that the stream for the channel is at
*		end of file
*	    AR_CDESC_BEGIN indicates that a new run of data is beginning
*		for the channel
*	    AR_CDESC_SNAP_BEGIN indicates that a new run of data is beginning
*		for the channel, where the run is a synchronous snapshot
*	    AR_CDESC_SNAP_END indicates that the following value is the
*		last in a snapshot (may occur with AR_CDESC_SNAP_BEGIN)
*	    AR_CDESC_INACT indicates that the run of data is done, because
*		the archive request was inactivated (or disabled)
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	assumes that representation of numbers is the same on `retrieval
*	host' as on original `store host'
* o	the type of the read request must be the same `flavor' as that of
*	the channel--no conversions are provided from short to string, etc.
* o	need a way to return to caller number of elements actually read
*
* NOTES
* 1.	`count' need not match native count.  If the count is less than
*	the native count, excess values will be discarded.  If the requested
*	type is not a `plain' type (e.g., DBR_FLOAT), then information
*	will be stored in the caller's buffer even if count is 0 (assuming
*	the caller's buffer pointer isn't NULL).
*
*-*/
long
arCFChanRead(pArChanDesc, type, pAccessBuf, count)
register
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
chtype	type;			/* I buffer type; DBR_xxx */
register
union db_access_val *pAccessBuf;/* IO pointer to caller's buffer, or NULL */
int	count;			/* I array element count for caller's buffer */
{
    int		retStat=OK;	/* return status to caller */
    int		nBytes;		/* number of value bytes in file item value */
    int		countBytes;	/* number of bytes allowed by caller's buf */
    int		i;		/* temp for loops */
    int		byte0, byteN;	/* bytes from the entry */
    char	*pBuf;		/* temporary pointer for moving data */
    chtype	recType;	/* type of buffer */

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);
    assert(dbr_type_is_valid(type) || count == 0);
    assert(pAccessBuf != NULL || count == 0);
    assert(count >= 0);

/*----------------------------------------------------------------------------
* handle time stamp
*
*    (arCFChanReadTs() resets the various flags)
*
*    put the proper time stamp information into the access buffer if
*    the type is DBR_TIME_xxx.
*----------------------------------------------------------------------------*/
    if ((retStat = arCFChanReadTs(pArChanDesc)) == OK) {
	if (dbr_type_is_TIME(type))
	    pAccessBuf->tstrval.stamp = pArChanDesc->timeStamp;
    }

/*----------------------------------------------------------------------------
* handle alarm status/severity if this item has it.  If the item does
*    have it, process it and then exit this section with a make believe
*    item code of value without status.
*
*    This section assumes that all DBR_ types which have status have
*    the status and severity structure items in the same place as
*    struct dbr_sts_string .
*---------------------------------------------------------------------------*/
    if (retStat == OK) {
	byte0 = ArCRFetch;
	if (byte0 == AR_DAT_IC_VAL_STAT) {
	    byte0 = ArCRFetch;
	    byteN = ArCRFetch;
	    if (!dbr_type_is_plain(type)) {
		pAccessBuf->sstrval.status = byte0;
		pAccessBuf->sstrval.severity = byteN;
	    }
	    if (byteN == ERROR)
		retStat = ERROR;
	    else
		byte0 = AR_DAT_IC_VAL;
	}
    }

/*----------------------------------------------------------------------------
* if this is DBR_GR_xxx, get graphics info from chanHdr
*---------------------------------------------------------------------------*/
    if (retStat == OK && dbr_type_is_GR(type)) {
	switch (type) {
	    case DBR_GR_STRING:
		break;		/* no graphics for string */
	    case DBR_GR_SHORT:
#define ArCRShrtGr ArCDChanHdr(pArChanDesc).graphics.shortGr
		(void)strcpy(pAccessBuf->gshrtval.units, ArCRShrtGr.units);
		pAccessBuf->gshrtval.upper_disp_limit = ArCRShrtGr.upDispLim;
		pAccessBuf->gshrtval.lower_disp_limit = ArCRShrtGr.lowDispLim;
		pAccessBuf->gshrtval.upper_alarm_limit = ArCRShrtGr.upAlmLim;
		pAccessBuf->gshrtval.upper_warning_limit = ArCRShrtGr.upWarnLim;
		pAccessBuf->gshrtval.lower_warning_limit =
							ArCRShrtGr.lowWarnLim;
		pAccessBuf->gshrtval.lower_alarm_limit = ArCRShrtGr.lowAlmLim;
		break;
	    case DBR_GR_LONG:
#define ArCRLngGr ArCDChanHdr(pArChanDesc).graphics.longGr
		(void)strcpy(pAccessBuf->glngval.units, ArCRLngGr.units);
		pAccessBuf->glngval.upper_disp_limit = ArCRLngGr.upDispLim;
		pAccessBuf->glngval.lower_disp_limit = ArCRLngGr.lowDispLim;
		pAccessBuf->glngval.upper_alarm_limit = ArCRLngGr.upAlmLim;
		pAccessBuf->glngval.upper_warning_limit = ArCRLngGr.upWarnLim;
		pAccessBuf->glngval.lower_warning_limit =
							ArCRLngGr.lowWarnLim;
		pAccessBuf->glngval.lower_alarm_limit = ArCRLngGr.lowAlmLim;
		break;
	    case DBR_GR_CHAR:
#define ArCRChrGr ArCDChanHdr(pArChanDesc).graphics.shortGr
		(void)strcpy(pAccessBuf->gchrval.units, ArCRChrGr.units);
		pAccessBuf->gchrval.upper_disp_limit = ArCRChrGr.upDispLim;
		pAccessBuf->gchrval.lower_disp_limit = ArCRChrGr.lowDispLim;
		pAccessBuf->gchrval.upper_alarm_limit = ArCRChrGr.upAlmLim;
		pAccessBuf->gchrval.upper_warning_limit = ArCRChrGr.upWarnLim;
		pAccessBuf->gchrval.lower_warning_limit = ArCRChrGr.lowWarnLim;
		pAccessBuf->gchrval.lower_alarm_limit = ArCRChrGr.lowAlmLim;
		break;
	    case DBR_GR_FLOAT:
#define ArCRFltGr ArCDChanHdr(pArChanDesc).graphics.floatGr
		pAccessBuf->gfltval.precision = ArCRFltGr.precision;
		(void)strcpy(pAccessBuf->gfltval.units, ArCRFltGr.units);
		pAccessBuf->gfltval.upper_disp_limit = ArCRFltGr.upDispLim;
		pAccessBuf->gfltval.lower_disp_limit = ArCRFltGr.lowDispLim;
		pAccessBuf->gfltval.upper_alarm_limit = ArCRFltGr.upAlmLim;
		pAccessBuf->gfltval.upper_warning_limit = ArCRFltGr.upWarnLim;
		pAccessBuf->gfltval.lower_warning_limit = ArCRFltGr.lowWarnLim;
		pAccessBuf->gfltval.lower_alarm_limit = ArCRFltGr.lowAlmLim;
		break;
	    case DBR_GR_DOUBLE:
#define ArCRDblGr ArCDChanHdr(pArChanDesc).graphics.doubleGr
		pAccessBuf->gdblval.precision = ArCRDblGr.precision;
		(void)strcpy(pAccessBuf->gdblval.units, ArCRDblGr.units);
		pAccessBuf->gdblval.upper_disp_limit = ArCRDblGr.upDispLim;
		pAccessBuf->gdblval.lower_disp_limit = ArCRDblGr.lowDispLim;
		pAccessBuf->gdblval.upper_alarm_limit = ArCRDblGr.upAlmLim;
		pAccessBuf->gdblval.upper_warning_limit = ArCRDblGr.upWarnLim;
		pAccessBuf->gdblval.lower_warning_limit = ArCRDblGr.lowWarnLim;
		pAccessBuf->gdblval.lower_alarm_limit = ArCRDblGr.lowAlmLim;
		break;
	    case DBR_GR_ENUM:
#define ArCREnmGr ArCDChanHdr(pArChanDesc).graphics.enumGr.pGRBuf
		pAccessBuf->genmval.no_str = ArCREnmGr->numStrings;
		for (i=0; i<ArCREnmGr->numStrings; i++) {
		    (void)strcpy(pAccessBuf->genmval.strs[i],
							ArCREnmGr->states[i]);
		}
		break;
	    default:
		(void)fprintf(stderr, "arCFChanRead: unimplemented GR type\n");
		retStat = ERROR;
		break;
	}
    }

/*----------------------------------------------------------------------------
* now handle the value item.  Only the actual values are handled here, since
* status, time stamp, etc. have already been handled.
*---------------------------------------------------------------------------*/
    if (retStat != OK)
	;		/* no action */
    else if (byte0 != AR_DAT_IC_VAL) {
	(void)fprintf(stderr, "arCFChanRead: unexpected item code %d\n", byte0);
	retStat = ERROR;
    }
    else {
	assert(dbr_type_is_valid(type));
        recType = ArCDChanHdr(pArChanDesc).chanType;
	if (count <= 0) pBuf = NULL;
	else pBuf = ((char *)dbr_value_ptr(pAccessBuf, type)) - 1;
	/* set byte count one byte shy to allow checking status on the
	last one.  This is done as a time saving measure */
	nBytes = ArCDChanHdr(pArChanDesc).elCount*dbr_value_size[recType] - 1;
	countBytes = count * dbr_value_size[recType];
	for (i=0; i<nBytes; i++) {
	    if (i<countBytes)	*(++pBuf) = ArCRFetch;
	    else		ArCRFetch;
	}
	byteN = ArCRFetch;		/* last, for status */
	if (countBytes > nBytes)	*(++pBuf)  = byteN;
	if (byteN == ERROR)		retStat = ERROR;
    }

    return retStat;
}

/*+/internal******************************************************************
* NAME	arCFChanReadTs - read and process time stamp items
*
* DESCRIPTION
*	This routine reads successive items until a non-time stamp item
*	is found.  The chanDesc time stamp is updated appropriately.
*
*	End of file results in an error return, with the AR_CDESC_EOF flag
*	set in chanDesc.flags  .
*
*	Encountering one of the AR_DAT_IC_MON_xxx flags results in an error
*	return, with an appropriate AR_CDESC_xxx flag set in chanDesc.flags  .
*
* RETURNS
*	OK, or
*	ERROR
*
*-*/
long
arCFChanReadTs(pArChanDesc)
register
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
{
    int		retStat=OK;	/* return status to caller */
    TS_STAMP	timeDel;	/* delta time */
    int		byte0, byteN;	/* bytes from the entry */

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);

    /* turn off various flags */
    pArChanDesc->flags &= ~(AR_CDESC_EOF | AR_CDESC_BEGIN | AR_CDESC_INACT |
				AR_CDESC_SNAP_BEGIN | AR_CDESC_SNAP_END |
				AR_CDESC_DISCON | AR_CDESC_SUP);

/*----------------------------------------------------------------------------
* handle time stamp
*
*    if the first byte indicates a time stamp, process it,
*    updating the time information in the channel descriptor.
*
*    this section of code loops until a non-time item is found, since
*    several time related items can follow each other.  The chanDesc
*    is left so that the next read will start with the non-time item.
*
*    this section assumes that all DBR_ types which have time have
*    the the time structure items in the same place as
*    struct dbr_time_string .
*---------------------------------------------------------------------------*/
    while (retStat == OK) {
	timeDel.secPastEpoch = 0;
	timeDel.nsec = 0;
	if ( (byte0 = ArCRFetch) != ERROR) {
	    if ((byte0 & 0x80) == 0) {		/* delta time, milli-sec */
		if ( (byteN = ArCRFetch) != ERROR) {
		    if ((timeDel.nsec = ((byte0<<8) + byteN)) < 1000)
			timeDel.nsec *= 1000000;
		    else {
			timeDel.secPastEpoch = timeDel.nsec / 1000;
			timeDel.nsec = 1000000 * (timeDel.nsec % 1000);
		    }
		    TsAddStamps(&pArChanDesc->timeStamp,
					&pArChanDesc->timeStamp, &timeDel);
		}
		else
		    retStat = ERROR;
	    }
	    else if (byte0 == AR_DAT_IC_MON_BEGIN) {	/* begin monitor */
		pArChanDesc->flags |= AR_CDESC_BEGIN;
		retStat = ERROR;
	    }
	    else if (byte0 == AR_DAT_IC_SNAP_BEGIN) {	/* begin snapshot */
		pArChanDesc->flags |= AR_CDESC_SNAP_BEGIN;
		retStat = ERROR;
	    }
	    else if (byte0 == AR_DAT_IC_SNAP_SINGLE) {	/* 1 sample snapshot */
		pArChanDesc->flags |= AR_CDESC_SNAP_BEGIN | AR_CDESC_SNAP_END;
		retStat = ERROR;
	    }
	    else if (byte0 == AR_DAT_IC_SNAP_END) {	/* end snapshot */
		pArChanDesc->flags |= AR_CDESC_SNAP_END;
		retStat = ERROR;
	    }
	    else if (byte0 == AR_DAT_IC_MON_INACT) {	/* req inactivated */
		pArChanDesc->flags |= AR_CDESC_INACT;
		retStat = ERROR;
	    }
	    else if (byte0 == AR_DAT_IC_MON_DISCON) {	/* chan disconnected */
		pArChanDesc->flags |= AR_CDESC_DISCON;
		retStat = ERROR;
	    }
	    else if ((byte0 & 0xf0) != AR_DAT_IC_TIME_XXX) {
		pArChanDesc->remainCount++;	/* `un-read' the byte */
		pArChanDesc->pData--;
		goto timeDone;		/* not a time code; bail out */
	    }
	    else if (byte0 == AR_DAT_IC_TIME_STAMP) {
    		timeDel.secPastEpoch = ArCRFetch			 <<8;
    		timeDel.secPastEpoch = (timeDel.secPastEpoch + ArCRFetch)<<8;
    		timeDel.secPastEpoch = (timeDel.secPastEpoch + ArCRFetch)<<8;
		timeDel.secPastEpoch += (byteN = ArCRFetch);
		if (byteN != ERROR)
		    pArChanDesc->timeStamp = timeDel;
		else
		    retStat = ERROR;
	    }
	    else if (byte0 == AR_DAT_IC_TIME_USEC) { /* delta time, micro-sec */
    		timeDel.nsec = ArCRFetch		 <<8;
    		timeDel.nsec = (timeDel.nsec + ArCRFetch)<<8;
		if ((timeDel.nsec += (byteN = ArCRFetch)) < 1000000)
		    timeDel.nsec *= 1000;
		else {
		    timeDel.secPastEpoch = timeDel.nsec / 1000000;
		    timeDel.nsec = 1000 * (timeDel.nsec % 1000000);
		}
		if (byteN != ERROR) {
		    TsAddStamps(&pArChanDesc->timeStamp,
					&pArChanDesc->timeStamp, &timeDel);
		}
		else
		    retStat = ERROR;
	    }
	    else if (byte0 == AR_DAT_IC_TIME_NSEC) { /* delta time, nano-sec */
    		timeDel.nsec = ArCRFetch		 <<8;
    		timeDel.nsec = (timeDel.nsec + ArCRFetch)<<8;
    		timeDel.nsec = (timeDel.nsec + ArCRFetch)<<8;
		timeDel.nsec += (byteN = ArCRFetch);
		if (byteN != ERROR) {
		    TsAddStamps(&pArChanDesc->timeStamp,
					&pArChanDesc->timeStamp, &timeDel);
		}
		else
		    retStat = ERROR;
	    }
	    else
		assertAlways(0);	/* not one of the expected codes */

	}
	if (byte0 == ERROR)
	    retStat = ERROR;
    }
timeDone:		/* must FOLLOW the while loop */
    return retStat;
}

/*----------------------------------------------------------------------------
* macros for use by arCFChanWrite() and friends
*
*    int ArCWStore(c)		insert character into current write buffer; if
*				there isn't room (or if there isn't a current
*				buffer) flush current buffer and insert
*				character into new buffer
*
*    int ArCWFlushAndStore(c)	flush current write buffer (if any) and
*				insert character into new buffer
*---------------------------------------------------------------------------*/
#define ArCWFlushAndStore(c) ArCFFlushAndStore((c), pArChanDesc)
#define ArCWStore(c) ArCFStore((c), pArChanDesc)

/*+/subr**********************************************************************
* NAME	arCFChanWrite - write a channel access buffer to a channel
*
* DESCRIPTION
*	This routine accepts an event_handler_args structure for a channel and
*	writes time stamp, alarm status/severity, and value(s) to the
*	channel data file for the channel.  The buffer must be one of the
*	DBR_TIME_xxx types.
*
*	The header information for the channel must exist prior to calling
*	this routine.  In addition, the buffer type must agree with the
*	native type stored in the header (e.g., if buffer type is
*	DBR_TIME_SHORT, then the header must indicate DBF_SHORT).  If
*	any of these requirements are violated, then no write is done and
*	ERROR status is returned.
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	error handling needs to be validated
* o	no attention is paid to issues related to calling this routine from
*	Channel Access event handler.  In particular, there should be
*	queues and tasks for: receiving full buffers from this routine;
*	supplying empty buffers to this routine; and supplying free blocks
*	to this routine.  THIS ROUTINE SHOULD NEVER BLOCK!!!
*
* NOTES
* 1.	This routine uses the smallest delta time item consistent with the
*	significance present in the .nsec member of the time stamp.  E.g.,
*	if .nsec is a multiple of 1000000, then a milli-sec delta time will
*	be used.
*
*-*/
long
arCFChanWrite(pArChanDesc, pArg)
register
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
struct event_handler_args *pArg;/* I pointer to monitor structure */
{
#define CW_NSEC ((struct dbr_time_string *)pCaBuf)->stamp.nsec
#define CW_SPE ((struct dbr_time_string *)pCaBuf)->stamp.secPastEpoch
    register
    void	*pCaBuf;	/* pointer to buffer */

    int		stat;		/* status from calls */
    int		deltaCode;	/* AR_DAT_IC_TIME_xxx code for delta time */
    int		deltaCount;	/* count of bytes to represent delta time */
    TS_STAMP	diff;		/* difference between data stamp and chanDesc */
    ULONG	fraction;	/* fractional part of second, units vary */
    int		fracCode;	/* AR_DAT_IC_TIME_xxx code for delta time */
    int		fracCount;	/* count of bytes to represent delta time */
    register
    char	*ptr;		/* temp for pointer in loops */
    register
    char	*ptrEnd;	/* temp for pointer bound in loops */

/*----------------------------------------------------------------------------
*    get some stuff out of the structure supplied as input pArg
*---------------------------------------------------------------------------*/
    pCaBuf = pArg->dbr;

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);
    assert(pCaBuf != NULL);
    if (!dbr_type_is_TIME(pArg->type))
	goto error;
    if (pArg->count != ArCDChanHdr(pArChanDesc).elCount)
	goto error;	/* ignore changed count */

    pArChanDesc->flags &= ~AR_CDESC_SUP;	/* turn off suppress */
    
/*----------------------------------------------------------------------------
* time stamp handling
*
*    (This is done 'in-line' as an optimization strategy.  When done as a
*    separate routine, this used as much CPU time as all the rest of
*    arCFChanWrite() used for status and data for DBR_TIME_FLOAT.)
*
* NOTES
* 1.	A lot of the logic in this routine is in support of the rule that
*	when a "time item" is used, the item can't cross a block
*	boundary.
*---------------------------------------------------------------------------*/

    if (TsCmpStampsLE(&((struct dbr_time_string *)pCaBuf)->stamp,
						    &pArChanDesc->timeStamp)) {
        goto ignore;	/* ignore time running backward */
    }
    if (ArCDDatInfo(pArChanDesc).stamp.secPastEpoch == 0) {
/*----------------------------------------------------------------------------
* case 1
*    this is the first item which has its beginning in this block.  A
*    time stamp in the datInfo is needed; there must be enough room
*    in the block to avoid starting a new block--i.e., to hold the item
*    code for the data entry.
*
*    Since this is the first item which has its beginning in this block,
*    datInfo.stamp must be set.  The .firstByte item may also need to be set.
*
*    This may also be the first item for the channel.  If this is the case,
*    then .oldestSecPastEpoch in chanHdr needs to be set.
*---------------------------------------------------------------------------*/
	if (pArChanDesc->remainCount < 1) {
	    if (ArCWFlushAndStore(AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	else {
	    if (ArCWStore(      AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	if (pArChanDesc->pDataBuf->bfInfo.firstByte == 0) {
	    pArChanDesc->pDataBuf->bfInfo.firstByte = pArChanDesc->pData -
				    (char *)(&pArChanDesc->pDataBuf->bfInfo);
	}
	ArCDDatInfo(pArChanDesc).stamp =
				((struct dbr_time_string *)pCaBuf)->stamp;
	ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pIndexBuf);

	if (ArCDChanHdr(pArChanDesc).oldestSecPastEpoch == 0) {
	    ArCDChanHdr(pArChanDesc).oldestSecPastEpoch = CW_SPE;
	    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);
	}
    }
    else {
/*----------------------------------------------------------------------------
*    not first item beginning in this block.  Figure out first what's
*    going to be needed to handle the time stamp.  The questions are:
*    o  without loss of significance, what type of delta time item is
*	required?  For that type of delta time item, is the delta time
*	less than the maximum?  If so, is there enough space left in the
*	block to hold the delta time item and the first byte of a data item?
*    o  if a delta time item can't be used or if there isn't enough room
*	in the block, then a full time stamp is needed.
*---------------------------------------------------------------------------*/
	TsDiffAsStamp(&diff, &((struct dbr_time_string *)pCaBuf)->stamp,
						    &pArChanDesc->timeStamp);
	if (diff.nsec % 1000000 == 0 && diff.secPastEpoch < 30 &&
					pArChanDesc->remainCount >= 2+1) {
/*----------------------------------------------------------------------------
* case 2
*	the change can be represented as a delta milli-second (and, there is
*	room enough to store the item and the first byte of value)
*----------------------------------------------------------------------------*/
	    fraction = diff.secPastEpoch*1000 + diff.nsec/1000000;
	    (void)ArCWStore(    fraction>> 8        );
	    (void)ArCWStore(    fraction            );
	    if (ArCWStore(      AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	else if (diff.nsec % 1000 == 0 && diff.secPastEpoch < 15 &&
					pArChanDesc->remainCount >= 4+1) {
/*----------------------------------------------------------------------------
* case 3
*	the change can be represented as a delta micro-second (and, there is
*	room enough to store the item and the first byte of value)
*----------------------------------------------------------------------------*/
	    fraction = diff.secPastEpoch*1000000 + diff.nsec/1000;
	    (void)ArCWStore(    AR_DAT_IC_TIME_USEC );
	    (void)ArCWStore(    fraction>>16        );
	    (void)ArCWStore(    fraction>> 8        );
	    (void)ArCWStore(    fraction            );
	    if (ArCWStore(      AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	else if (diff.secPastEpoch < 4 && pArChanDesc->remainCount >= 5+1) {
/*----------------------------------------------------------------------------
* case 4
*	the change can be represented as a delta nano-second (and, there is
*	room enough to store the item and the first byte of value)
*----------------------------------------------------------------------------*/
	    fraction = diff.secPastEpoch*1000000000 + diff.nsec;
	    (void)ArCWStore(    AR_DAT_IC_TIME_NSEC );
	    (void)ArCWStore(    fraction>>24        );
	    (void)ArCWStore(    fraction>>16        );
	    (void)ArCWStore(    fraction>> 8        );
	    (void)ArCWStore(    fraction            );
	    if (ArCWStore(      AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	else {
/*----------------------------------------------------------------------------
* case 5
*	the change is either too large to be accomodated by a delta time
*	item or else there isn't enough room left in the block for a delta
*	time item followed by the first byte of a data item.  If there isn't
*	enough space left in the block for the required items, a new block
*	will be started.
*---------------------------------------------------------------------------*/
	    deltaCode = AR_DAT_IC_TIME_STAMP;	/* full time stamp needed */
	    deltaCount = 5+1;		/* 5 for stamp, 1 for value code */
	    if (CW_NSEC == 0) {
		fracCount = 2;
		fracCode = AR_DAT_IC_TIME_MSEC;	/* delta msec */
		/* don't add to deltaCount, since this item is optional */
		fraction = 0;
	    }
	    else if (CW_NSEC % 1000000 == 0) {
		fracCount = 2;
		fracCode = AR_DAT_IC_TIME_MSEC;	/* delta msec */
		deltaCount += fracCount;
		fraction = CW_NSEC / 1000000;
	    }
	    else if (CW_NSEC % 1000 == 0) {
		fracCount = 4;
		fracCode = AR_DAT_IC_TIME_USEC;	/* delta usec */
		deltaCount += fracCount;
		fraction = CW_NSEC / 1000;
	    }
	    else {
		fracCount = 5;
		fracCode = AR_DAT_IC_TIME_NSEC;	/* delta nsec */
		deltaCount += fracCount;
		fraction = CW_NSEC;
	    }

	    if (pArChanDesc->remainCount < deltaCount) {
		if (ArCWFlushAndStore(AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
		pArChanDesc->pDataBuf->bfInfo.firstByte = pArChanDesc->pData -
				    (char *)(&pArChanDesc->pDataBuf->bfInfo);
		ArCDDatInfo(pArChanDesc).stamp =
				((struct dbr_time_string *)pCaBuf)->stamp;
		ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pIndexBuf);
	    }
	    else {
		(void)ArCWStore(deltaCode    	       );
		(void)ArCWStore(CW_SPE   >>24          );
		(void)ArCWStore(CW_SPE   >>16          );
		(void)ArCWStore(CW_SPE   >> 8          );
		(void)ArCWStore(CW_SPE                 );
		if (fraction != 0) {
		    if (fracCode != AR_DAT_IC_TIME_MSEC) {
			(void)ArCWStore(fracCode	    );
			if (fracCode == AR_DAT_IC_TIME_NSEC)
			    (void)ArCWStore(fraction>>24    );
			(void)ArCWStore(fraction>>16        );
		    }
		    (void)ArCWStore(    fraction>> 8        );
		    (void)ArCWStore(    fraction            );
		}
		if (ArCWStore(          AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	    }    
	}
    }    
    pArChanDesc->timeStamp = ((struct dbr_time_string *)pCaBuf)->stamp;

/*----------------------------------------------------------------------------
*    now store the status and data.  The code above has already stored
*    an AR_DAT_IC_VAL_STAT item code in the file.
*----------------------------------------------------------------------------*/
    (void)ArCWStore(((struct dbr_time_string *)pCaBuf)->status);
    (void)ArCWStore(((struct dbr_time_string *)pCaBuf)->severity);
    if (pArg->type == DBR_TIME_STRING) {
#define ArString ((struct dbr_time_string *)pCaBuf)
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_STRING);
	ptr = (char *)(ArString->value);
	ptrEnd = ptr + pArg->count*db_strval_dim - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
#undef ArString
    }
    else if (pArg->type == DBR_TIME_SHORT) {
#define ArShort ((struct dbr_time_short *)pCaBuf)
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_SHORT);
	ptr = (char *)(&ArShort->value);
	ptrEnd = ptr + pArg->count * sizeof(short) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
#undef ArShort
    }
    else if (pArg->type == DBR_TIME_FLOAT) {
#define ArFloat ((struct dbr_time_float *)pCaBuf)
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_FLOAT);
	ptr = (char *)(&ArFloat->value);
	ptrEnd = ptr + pArg->count * sizeof(float) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
#undef ArFloat
    }
    else if (pArg->type == DBR_TIME_LONG) {
#define ArLong ((struct dbr_time_long *)pCaBuf)
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_LONG);
	ptr = (char *)(&ArLong->value);
	ptrEnd = ptr + pArg->count * sizeof(long) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
#undef ArLong
    }
    else if (pArg->type == DBR_TIME_DOUBLE) {
#define ArDouble ((struct dbr_time_double *)pCaBuf)
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_DOUBLE);
	ptr = (char *)(&ArDouble->value);
	ptrEnd = ptr + pArg->count * sizeof(double) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
#undef ArDouble
    }
    else if (pArg->type == DBR_TIME_ENUM) {
#define ArEnum ((struct dbr_time_enum *)pCaBuf)
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_ENUM);
	ptr = (char *)(&ArEnum->value);
	ptrEnd = ptr + pArg->count * sizeof(short) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
#undef ArEnum
    }
    else if (pArg->type == DBR_TIME_CHAR) {
#define ArChar ((struct dbr_time_char *)pCaBuf)
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_CHAR);
	ptr = (char *)(&ArChar->value);
	ptrEnd = ptr + pArg->count * sizeof(char) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
#undef ArChar
    }
    else
	assertAlways(0);

/*-----------------------------------------------------------------------------
* update bookkeeping information
*
*	.lastByte
*	.modified in data buffer
*	.newestStamp in MI buffer
*	.modified in MI buffer
*----------------------------------------------------------------------------*/
    if (pArChanDesc->pData != NULL) {
	pArChanDesc->pDataBuf->bfInfo.lastByte = pArChanDesc->pData -
			    (char *)&pArChanDesc->pDataBuf->bfInfo;
	assert(pArChanDesc->pDataBuf->bfInfo.lastByte > 0);
	assert(pArChanDesc->pDataBuf->bfInfo.lastByte < 2000);
    }
    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pDataBuf);
    ArCDChanHdr(pArChanDesc).newestSecPastEpoch =
					pArChanDesc->timeStamp.secPastEpoch;
    ArCDChanHdr(pArChanDesc).newestNsec = pArChanDesc->timeStamp.nsec;
    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);

    return OK;
error:
    return ERROR;
ignore:
    return OK;
#undef CW_NSEC
#undef CW_SPE
}

/*+/subr**********************************************************************
* NAME	arCFChanWrite_args
*
*-*/
long
arCFChanWrite_args(pArChanDesc, stamp, dbfType, count, pData, alStat, alSevr)
register
AR_CHAN_DESC *pArChanDesc;/* IO pointer to channel descriptor */
TS_STAMP stamp;		/* I time stamp for data */
chtype	dbfType;	/* I type of data, one of the DBF_... */
int	count;		/* I element count for data */
void	*pData;		/* I pointer to actual data */
int	alStat;		/* I alarm status for data */
int	alSevr;		/* I alarm severity for data */
{
#define CW_NSEC stamp.nsec
#define CW_SPE stamp.secPastEpoch

    int		stat;		/* status from calls */
    int		deltaCode;	/* AR_DAT_IC_TIME_xxx code for delta time */
    int		deltaCount;	/* count of bytes to represent delta time */
    TS_STAMP	diff;		/* difference between data stamp and chanDesc */
    ULONG	fraction;	/* fractional part of second, units vary */
    int		fracCode;	/* AR_DAT_IC_TIME_xxx code for delta time */
    int		fracCount;	/* count of bytes to represent delta time */
    register
    char	*ptr;		/* temp for pointer in loops */
    register
    char	*ptrEnd;	/* temp for pointer bound in loops */

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);

    if (count != ArCDChanHdr(pArChanDesc).elCount)
	goto error;	/* ignore changed count */

    pArChanDesc->flags &= ~AR_CDESC_SUP;	/* turn off suppress */
    
/*----------------------------------------------------------------------------
* time stamp handling
*
*    (This is done 'in-line' as an optimization strategy.  When done as a
*    separate routine, this used as much CPU time as all the rest of
*    arCFChanWrite() used for status and data for DBR_FLOAT.)
*
* NOTES
* 1.	A lot of the logic in this routine is in support of the rule that
*	when a "time item" is used, the item can't cross a block
*	boundary.
*---------------------------------------------------------------------------*/

    if (TsCmpStampsLE(&stamp, &pArChanDesc->timeStamp))
        goto ignore;	/* ignore time running backward */
    if (ArCDDatInfo(pArChanDesc).stamp.secPastEpoch == 0) {
/*----------------------------------------------------------------------------
* case 1
*    this is the first item which has its beginning in this block.  A
*    time stamp in the datInfo is needed; there must be enough room
*    in the block to avoid starting a new block--i.e., to hold the item
*    code for the data entry.
*
*    Since this is the first item which has its beginning in this block,
*    datInfo.stamp must be set.  The .firstByte item may also need to be set.
*
*    This may also be the first item for the channel.  If this is the case,
*    then .oldestSecPastEpoch in chanHdr needs to be set.
*---------------------------------------------------------------------------*/
	if (pArChanDesc->remainCount < 1) {
	    if (ArCWFlushAndStore(AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	else {
	    if (ArCWStore(      AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	if (pArChanDesc->pDataBuf->bfInfo.firstByte == 0) {
	    pArChanDesc->pDataBuf->bfInfo.firstByte = pArChanDesc->pData -
				    (char *)(&pArChanDesc->pDataBuf->bfInfo);
	}
	ArCDDatInfo(pArChanDesc).stamp = stamp;
	ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pIndexBuf);

	if (ArCDChanHdr(pArChanDesc).oldestSecPastEpoch == 0) {
	    ArCDChanHdr(pArChanDesc).oldestSecPastEpoch = CW_SPE;
	    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);
	}
    }
    else {
/*----------------------------------------------------------------------------
*    not first item beginning in this block.  Figure out first what's
*    going to be needed to handle the time stamp.  The questions are:
*    o  without loss of significance, what type of delta time item is
*	required?  For that type of delta time item, is the delta time
*	less than the maximum?  If so, is there enough space left in the
*	block to hold the delta time item and the first byte of a data item?
*    o  if a delta time item can't be used or if there isn't enough room
*	in the block, then a full time stamp is needed.
*---------------------------------------------------------------------------*/
	TsDiffAsStamp(&diff, &stamp, &pArChanDesc->timeStamp);
	if (diff.nsec % 1000000 == 0 && diff.secPastEpoch < 30 &&
					pArChanDesc->remainCount >= 2+1) {
/*----------------------------------------------------------------------------
* case 2
*	the change can be represented as a delta milli-second (and, there is
*	room enough to store the item and the first byte of value)
*----------------------------------------------------------------------------*/
	    fraction = diff.secPastEpoch*1000 + diff.nsec/1000000;
	    (void)ArCWStore(    fraction>> 8        );
	    (void)ArCWStore(    fraction            );
	    if (ArCWStore(      AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	else if (diff.nsec % 1000 == 0 && diff.secPastEpoch < 15 &&
					pArChanDesc->remainCount >= 4+1) {
/*----------------------------------------------------------------------------
* case 3
*	the change can be represented as a delta micro-second (and, there is
*	room enough to store the item and the first byte of value)
*----------------------------------------------------------------------------*/
	    fraction = diff.secPastEpoch*1000000 + diff.nsec/1000;
	    (void)ArCWStore(    AR_DAT_IC_TIME_USEC );
	    (void)ArCWStore(    fraction>>16        );
	    (void)ArCWStore(    fraction>> 8        );
	    (void)ArCWStore(    fraction            );
	    if (ArCWStore(      AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	else if (diff.secPastEpoch < 4 && pArChanDesc->remainCount >= 5+1) {
/*----------------------------------------------------------------------------
* case 4
*	the change can be represented as a delta nano-second (and, there is
*	room enough to store the item and the first byte of value)
*----------------------------------------------------------------------------*/
	    fraction = diff.secPastEpoch*1000000000 + diff.nsec;
	    (void)ArCWStore(    AR_DAT_IC_TIME_NSEC );
	    (void)ArCWStore(    fraction>>24        );
	    (void)ArCWStore(    fraction>>16        );
	    (void)ArCWStore(    fraction>> 8        );
	    (void)ArCWStore(    fraction            );
	    if (ArCWStore(      AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	}
	else {
/*----------------------------------------------------------------------------
* case 5
*	the change is either too large to be accomodated by a delta time
*	item or else there isn't enough room left in the block for a delta
*	time item followed by the first byte of a data item.  If there isn't
*	enough space left in the block for the required items, a new block
*	will be started.
*---------------------------------------------------------------------------*/
	    deltaCode = AR_DAT_IC_TIME_STAMP;	/* full time stamp needed */
	    deltaCount = 5+1;		/* 5 for stamp, 1 for value code */
	    if (CW_NSEC == 0) {
		fracCount = 2;
		fracCode = AR_DAT_IC_TIME_MSEC;	/* delta msec */
		/* don't add to deltaCount, since this item is optional */
		fraction = 0;
	    }
	    else if (CW_NSEC % 1000000 == 0) {
		fracCount = 2;
		fracCode = AR_DAT_IC_TIME_MSEC;	/* delta msec */
		deltaCount += fracCount;
		fraction = CW_NSEC / 1000000;
	    }
	    else if (CW_NSEC % 1000 == 0) {
		fracCount = 4;
		fracCode = AR_DAT_IC_TIME_USEC;	/* delta usec */
		deltaCount += fracCount;
		fraction = CW_NSEC / 1000;
	    }
	    else {
		fracCount = 5;
		fracCode = AR_DAT_IC_TIME_NSEC;	/* delta nsec */
		deltaCount += fracCount;
		fraction = CW_NSEC;
	    }

	    if (pArChanDesc->remainCount < deltaCount) {
		if (ArCWFlushAndStore(AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
		pArChanDesc->pDataBuf->bfInfo.firstByte = pArChanDesc->pData -
				    (char *)(&pArChanDesc->pDataBuf->bfInfo);
		ArCDDatInfo(pArChanDesc).stamp = stamp;
		ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pIndexBuf);
	    }
	    else {
		(void)ArCWStore(deltaCode    	       );
		(void)ArCWStore(CW_SPE   >>24          );
		(void)ArCWStore(CW_SPE   >>16          );
		(void)ArCWStore(CW_SPE   >> 8          );
		(void)ArCWStore(CW_SPE                 );
		if (fraction != 0) {
		    if (fracCode != AR_DAT_IC_TIME_MSEC) {
			(void)ArCWStore(fracCode	    );
			if (fracCode == AR_DAT_IC_TIME_NSEC)
			    (void)ArCWStore(fraction>>24    );
			(void)ArCWStore(fraction>>16        );
		    }
		    (void)ArCWStore(    fraction>> 8        );
		    (void)ArCWStore(    fraction            );
		}
		if (ArCWStore(          AR_DAT_IC_VAL_STAT  ) != OK)
								    goto error;
	    }    
	}
    }    
    pArChanDesc->timeStamp = stamp;

/*----------------------------------------------------------------------------
*    now store the status and data.  The code above has already stored
*    an AR_DAT_IC_VAL_STAT item code in the file.
*----------------------------------------------------------------------------*/
    (void)ArCWStore(alStat);
    (void)ArCWStore(alSevr);
    if (dbfType == DBF_STRING) {
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_STRING);
	ptr = (char *)(struct dbf_string *)pData;
	ptrEnd = ptr + count*db_strval_dim - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
    }
    else if (dbfType == DBF_SHORT) {
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_SHORT);
	ptr = (char *)(struct dbf_short *)pData;
	ptrEnd = ptr + count * sizeof(short) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
    }
    else if (dbfType == DBF_FLOAT) {
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_FLOAT);
	ptr = (char *)(struct dbf_float *)pData;
	ptrEnd = ptr + count * sizeof(float) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
    }
    else if (dbfType == DBF_LONG) {
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_LONG);
	ptr = (char *)(struct dbf_long *)pData;
	ptrEnd = ptr + count * sizeof(long) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
    }
    else if (dbfType == DBF_DOUBLE) {
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_DOUBLE);
	ptr = (char *)(struct dbf_double *)pData;
	ptrEnd = ptr + count * sizeof(double) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
    }
    else if (dbfType == DBF_ENUM) {
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_ENUM);
	ptr = (char *)(struct dbf_enum *)pData;
	ptrEnd = ptr + count * sizeof(short) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
    }
    else if (dbfType == DBF_CHAR) {
	assert(ArCDChanHdr(pArChanDesc).chanType == DBF_CHAR);
	ptr = (char *)(struct dbf_char *)pData;
	ptrEnd = ptr + count * sizeof(char) - 1;
	while (ptr < ptrEnd)
	    (void)ArCWStore(*ptr++);
	if (ArCWStore(*ptr++) != OK) goto error;
    }
    else
	assertAlways(0);

/*-----------------------------------------------------------------------------
* update bookkeeping information
*
*	.lastByte
*	.modified in data buffer
*	.newestStamp in MI buffer
*	.modified in MI buffer
*----------------------------------------------------------------------------*/
    if (pArChanDesc->pData != NULL) {
	pArChanDesc->pDataBuf->bfInfo.lastByte = pArChanDesc->pData -
			    (char *)&pArChanDesc->pDataBuf->bfInfo;
	assert(pArChanDesc->pDataBuf->bfInfo.lastByte > 0);
	assert(pArChanDesc->pDataBuf->bfInfo.lastByte < 2000);
    }
    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pDataBuf);
    ArCDChanHdr(pArChanDesc).newestSecPastEpoch =
					pArChanDesc->timeStamp.secPastEpoch;
    ArCDChanHdr(pArChanDesc).newestNsec = pArChanDesc->timeStamp.nsec;
    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);

    return OK;
error:
    return ERROR;
ignore:
    return OK;
#undef CW_NSEC
#undef CW_SPE
}

/*+/subr**********************************************************************
* NAME	arCFChanWriteByte - write byte into a channel's stream
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
*-*/
long
arCFChanWriteByte(pArChanDesc, value)
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
int	value;			/* I value to write to stream */
{
    int		stat;		/* status from calls */

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);

    pArChanDesc->flags &= ~AR_CDESC_SUP;	/* turn off suppress */

    if (pArChanDesc->remainCount < 1) {
	if (ArCWFlushAndStore(value  ) != OK)
								    goto error;
    }
    else {
	if (ArCWStore(        value  ) != OK)
								    goto error;
    }
    if (pArChanDesc->pDataBuf->bfInfo.firstByte == 0) {
	pArChanDesc->pDataBuf->bfInfo.firstByte = pArChanDesc->pData -
				    (char *)(&pArChanDesc->pDataBuf->bfInfo);
    }
    pArChanDesc->pDataBuf->bfInfo.lastByte = pArChanDesc->pData -
				    (char *)(&pArChanDesc->pDataBuf->bfInfo);
    assert(pArChanDesc->pDataBuf->bfInfo.lastByte > 0);
    assert(pArChanDesc->pDataBuf->bfInfo.lastByte < 2000);
    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pDataBuf);

    return OK;
error:
    return ERROR;
}

/*+/subr**********************************************************************
* NAME	arCFChanWriteGR - write a channel access DBR_GR buffer to a channel
*
* DESCRIPTION
*	This routine writes DBR_GR_xxx information to the chanHdr for a
*	channel.
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	text
*
* NOTES
* 1.	This routine does NOT write a value to the file.
*
*-*/
long
arCFChanWriteGR(pArChanDesc, pArg)
AR_CHAN_DESC *pArChanDesc;	/* IO pointer to channel descriptor */
struct event_handler_args *pArg;/* I pointer to monitor structure */
{
    void	*pCaBuf;	/* pointer to buffer */
    int		i;
    long	stat;

/*----------------------------------------------------------------------------
*    get some stuff out of the structure supplied as input pArg
*---------------------------------------------------------------------------*/
    pCaBuf = pArg->dbr;

    assert(pArChanDesc != NULL);
    assert(pArChanDesc->pMIBuf != NULL);
    assert(pCaBuf != NULL);
    assert(dbr_type_is_GR(pArg->type));

    if (pArg->type == DBR_GR_STRING) {
	ArCDChanHdr(pArChanDesc).chanType = DBF_STRING;
	ArCDChanHdr(pArChanDesc).elCount = pArg->count;
    }
    else if (pArg->type == DBR_GR_SHORT) {
#define ArCWShrtGr ArCDChanHdr(pArChanDesc).graphics.shortGr
#define ArCWgr_short ((struct dbr_gr_short *)pCaBuf)

	ArCDChanHdr(pArChanDesc).chanType = DBF_SHORT;
	ArCDChanHdr(pArChanDesc).elCount = pArg->count;
	(void)strcpy(ArCWShrtGr.units, ArCWgr_short->units);
	ArCWShrtGr.upDispLim  = ArCWgr_short->upper_disp_limit;
	ArCWShrtGr.lowDispLim = ArCWgr_short->lower_disp_limit;
	ArCWShrtGr.upAlmLim   = ArCWgr_short->upper_alarm_limit;
	ArCWShrtGr.upWarnLim  = ArCWgr_short->upper_warning_limit;
	ArCWShrtGr.lowWarnLim = ArCWgr_short->lower_warning_limit;
	ArCWShrtGr.lowAlmLim  = ArCWgr_short->lower_alarm_limit;
    }
    else if (pArg->type == DBR_GR_FLOAT) {
#define ArCWFltGr ArCDChanHdr(pArChanDesc).graphics.floatGr
#define ArCWgr_float ((struct dbr_gr_float *)pCaBuf)

	ArCDChanHdr(pArChanDesc).chanType = DBF_FLOAT;
	ArCDChanHdr(pArChanDesc).elCount = pArg->count;
	ArCWFltGr.precision  = ArCWgr_float->precision;
	(void)strcpy(ArCWFltGr.units, ArCWgr_float->units);
	ArCWFltGr.upDispLim  = ArCWgr_float->upper_disp_limit;
	ArCWFltGr.lowDispLim = ArCWgr_float->lower_disp_limit;
	ArCWFltGr.upAlmLim   = ArCWgr_float->upper_alarm_limit;
	ArCWFltGr.upWarnLim  = ArCWgr_float->upper_warning_limit;
	ArCWFltGr.lowWarnLim = ArCWgr_float->lower_warning_limit;
	ArCWFltGr.lowAlmLim  = ArCWgr_float->lower_alarm_limit;
    }
    else if (pArg->type == DBR_GR_LONG) {
#define ArCWLngGr ArCDChanHdr(pArChanDesc).graphics.longGr
#define ArCWgr_long ((struct dbr_gr_long *)pCaBuf)

	ArCDChanHdr(pArChanDesc).chanType = DBF_LONG;
	ArCDChanHdr(pArChanDesc).elCount = pArg->count;
	(void)strcpy(ArCWLngGr.units, ArCWgr_long->units);
	ArCWLngGr.upDispLim  = ArCWgr_long->upper_disp_limit;
	ArCWLngGr.lowDispLim = ArCWgr_long->lower_disp_limit;
	ArCWLngGr.upAlmLim   = ArCWgr_long->upper_alarm_limit;
	ArCWLngGr.upWarnLim  = ArCWgr_long->upper_warning_limit;
	ArCWLngGr.lowWarnLim = ArCWgr_long->lower_warning_limit;
	ArCWLngGr.lowAlmLim  = ArCWgr_long->lower_alarm_limit;
    }
    else if (pArg->type == DBR_GR_DOUBLE) {
#define ArCWDblGr ArCDChanHdr(pArChanDesc).graphics.doubleGr
#define ArCWgr_double ((struct dbr_gr_double *)pCaBuf)

	ArCDChanHdr(pArChanDesc).chanType = DBF_DOUBLE;
	ArCDChanHdr(pArChanDesc).elCount = pArg->count;
	ArCWDblGr.precision  = ArCWgr_double->precision;
	(void)strcpy(ArCWDblGr.units, ArCWgr_double->units);
	ArCWDblGr.upDispLim  = ArCWgr_double->upper_disp_limit;
	ArCWDblGr.lowDispLim = ArCWgr_double->lower_disp_limit;
	ArCWDblGr.upAlmLim   = ArCWgr_double->upper_alarm_limit;
	ArCWDblGr.upWarnLim  = ArCWgr_double->upper_warning_limit;
	ArCWDblGr.lowWarnLim = ArCWgr_double->lower_warning_limit;
	ArCWDblGr.lowAlmLim  = ArCWgr_double->lower_alarm_limit;
    }
    else if (pArg->type == DBR_GR_ENUM) {
/*----------------------------------------------------------------------------
*	Store the graphics info into the graphics buffer, set the modified
*	bit for the buffer, and also set lastByte for the disk block.
*---------------------------------------------------------------------------*/
#define ArCWEnmGr ArCDChanHdr(pArChanDesc).graphics.enumGr.pGRBuf
#define ArCWgr_enum ((struct dbr_gr_enum *)pCaBuf)

	if (ArCDChanHdr(pArChanDesc).chanType == DBF_ENUM) {
	    assert(ArCWEnmGr != NULL);		/* better have a buffer */
	    assert(ArCWEnmGr->blkNum != 0);	/* it must contain a block */
	}
	else {
	    ArCDChanHdr(pArChanDesc).chanType = DBF_ENUM;
	    if ((stat = arCF_GRGetBlock(pArChanDesc)) != OK)
		assertAlways(0);
	}

	ArCDChanHdr(pArChanDesc).elCount = pArg->count;
	ArCWEnmGr->numStrings = ArCWgr_enum->no_str;
	for (i=0; i<ArCWEnmGr->numStrings; i++)
	    (void)strcpy(ArCWEnmGr->states[i], ArCWgr_enum->strs[i]);
	ArCWEnmGr->bfInfo.lastByte = BF_BLOCK_DATA + 2 +
				ArCWEnmGr->numStrings * db_state_text_dim - 1;
	stat = bfWrite(pArChanDesc->pArCfDesc->pBfDesc,
					&ArCWEnmGr->bfInfo, ArCWEnmGr->blkNum);
	if (stat != OK) {
	    (void)fprintf(stderr, "arCFWriteGR: error writing GR block\n");
	    assertAlways(0);
	}
    }
    else if (pArg->type == DBR_GR_CHAR) {
#define ArCWChrGr ArCDChanHdr(pArChanDesc).graphics.charGr
#define ArCWgr_char ((struct dbr_gr_char *)pCaBuf)

	ArCDChanHdr(pArChanDesc).chanType = DBF_CHAR;
	ArCDChanHdr(pArChanDesc).elCount = pArg->count;
	(void)strcpy(ArCWChrGr.units, ArCWgr_char->units);
	ArCWChrGr.upDispLim  = ArCWgr_char->upper_disp_limit;
	ArCWChrGr.lowDispLim = ArCWgr_char->lower_disp_limit;
	ArCWChrGr.upAlmLim   = ArCWgr_char->upper_alarm_limit;
	ArCWChrGr.upWarnLim  = ArCWgr_char->upper_warning_limit;
	ArCWChrGr.lowWarnLim = ArCWgr_char->lower_warning_limit;
	ArCWChrGr.lowAlmLim  = ArCWgr_char->lower_alarm_limit;
    }
    else {
	assertAlways(0);
    }

    ArCFModifySet(pArChanDesc->pArCfDesc, pArChanDesc->pMIBuf);

    return OK;
}

/*+/subr**********************************************************************
* NAME	arCFClose - close an AR channel data file
*
* DESCRIPTION
*	Closes an AR channel data file.  If the file was open in O_RDWR
*	mode, then buffers are flushed and the lock file is deleted.
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	text
*
* SEE ALSO
*	arCFOpen(), arCFCreate()
*
* EXAMPLE
*
*-*/
long
arCFClose(ppArCfDesc)
AR_CF_DESC **ppArCfDesc;	/* IO ptr to ptr to channel file descriptor */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status */
    AR_CF_DESC *pCfDesc;	/* pointer to channel file descriptor */
    AR_CHAN_DESC *pChanDesc;	/* pointer to channel descriptor */

    assert(ppArCfDesc != NULL);
    assert(*ppArCfDesc != NULL);

    pCfDesc = *ppArCfDesc;

    if (pCfDesc->pChanDescHead != NULL) {
        assert(pCfDesc->pBfDesc != NULL);
	assert(pCfDesc->pChanDescTail != NULL);

	/* close all the channel descriptors */
	while ((pChanDesc = pCfDesc->pChanDescHead) != NULL) {
	    stat = arCFChanClose(&pChanDesc);
	}

    }

    assert(pCfDesc->pChanDescHead == NULL);	/* should be closed */
    assert(pCfDesc->pChanDescTail == NULL);

    if (pCfDesc->pMIHead != NULL) {
        assert(pCfDesc->pBfDesc != NULL);
	assert(pCfDesc->pMITail != NULL);

/*----------------------------------------------------------------------------
*	check all MI blocks for "MODIFIED" bit, and write those with it set;
*	ALL MI blocks will be free()'d.  When the entire MI block list has
*	been traversed, both MIHead and MITail in the channel file descriptor
*	will be null.
*
*	As part of the traversal, check each chanHdr for DBF_ENUM channels.
*	The GR block may need to be written, and the buffer will need to
*	be free()'d.
*---------------------------------------------------------------------------*/
	while (pCfDesc->pMIHead != NULL) {
	    AR_MI_BUF	*pMIBuf;	/* pointer to MI block */
	    AR_GR_BUF	*pGRBuf;	/* pointer to GR block */
	    int		i;

	    pMIBuf = pCfDesc->pMIHead;
	    for (i=0; i<AR_CF_NHDR; i++) {
		if (pMIBuf->chanHdr[i].name[0] == '\0')
		    ;		/* no action */
		else if (pMIBuf->chanHdr[i].chanType == DBF_ENUM &&
		  (pGRBuf=pMIBuf->chanHdr[i].graphics.enumGr.pGRBuf) != NULL) {
		    if (ArCFModifyTestAndReset(pCfDesc, pGRBuf)) {
			stat = bfWrite(pCfDesc->pBfDesc,
					&pGRBuf->bfInfo, pGRBuf->blkNum);
			if (stat != OK) {
			    (void)fprintf(stderr,
					"arCFClose: error writing GR block\n");
			    retStat = ERROR;
			}
		    }
		    arCF_freeGR_BUF(pGRBuf);
		    pMIBuf->chanHdr[i].graphics.enumGr.pGRBuf = NULL;
		}
	    }
	    pCfDesc->pMIHead = pMIBuf->pNextMI;
	    if ((stat = arCF_MIFlush(pCfDesc, pMIBuf)) != OK) {
		(void)fprintf(stderr, "arCFClose: error writing MI block\n");
		retStat = ERROR;
	    }
	    arCF_freeMI_BUF(pMIBuf);
	}
	pCfDesc->pMITail = NULL;
    }

    assert(pCfDesc->pMIHead == NULL);
    assert(pCfDesc->pMITail == NULL);

    if (pCfDesc->pBfDesc != NULL) {
	stat = bfClose(&(*ppArCfDesc)->pBfDesc);
	if (stat != OK) {
	    (void)fprintf(stderr, "arCFClose: error closing block file\n");
	    retStat = ERROR;
	}
    }

    arCF_freeCF_DESC(*ppArCfDesc);
    *ppArCfDesc = NULL;

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCFCreate - create an AR channel data file
*
* DESCRIPTION
*	Creates an empty 'block file' of type BF_TYPE_CHAN_DATA.  The
*	maximum size for the file will be set at the number of blocks
*	which will fit with the caller specified number of bytes.  The
*	initial size for the file is 1 block.
*
*	If the create is successful, then the file is open in O_RDWR mode
*	when control returns to the caller.  The channel file descriptor
*	and other structures have been set up as would be done by arCFOpen().
*	In addition, a lock file has been created to prevent multiple
*	writers.
*
*	The block size is forced to be AR_CF_BLKSIZE bytes.
*
* RETURNS
*	pointer to channel file descriptor, or
*	NULL if the file already exists or if an error occurs on create
*
* BUGS
* o	no exit handler is established (see arCFOpen() )
*
* SEE ALSO
*	arCFOpen(), arCFClose(), arCFChanOpen()
*
*-*/
AR_CF_DESC *
arCFCreate(name, maxBytes)
char	*name;		/* I file name */
int	maxBytes;	/* I maximum number of bytes for file */
{
    int		stat=OK;	/* status from calls */

    BF_BLKNUM	maxBlocks;	/* size limit */
    BF_DESC	*pDesc;		/* pointer to block file descriptor */
    AR_CF_DESC	*pArCfDesc=NULL;/* pointer to channel file descriptor */

    assert(strlen(name) < GEN_FNAME_DIM);
    assert(strlen(name) > 0);


/*----------------------------------------------------------------------------
* allocate memory for an AR channel file descriptor and initialize the
*    descriptor.  Then create the block file and put into the channel file
*    descriptor a pointer to the block file descriptor.  This hides from
*    most users the fact that there are actually two descriptors--they
*    just use the channel file descriptor.
*---------------------------------------------------------------------------*/
    if ((pArCfDesc = arCF_mallocCF_DESC()) == NULL) {
	(void)fprintf(stderr, "arCFCreate: can't malloc for arCfDesc\n");
	stat = ERROR;
    }
    else {
	pArCfDesc->pBfDesc = NULL;
	pArCfDesc->pMIHead = NULL;		/* no MI buffers yet */
	pArCfDesc->pMITail = NULL;
	pArCfDesc->pMIFree = NULL;
	pArCfDesc->MIFree_hdrNum = 0;
	pArCfDesc->pChanDescHead = NULL;	/* no channel desc exist yet */
	pArCfDesc->pChanDescTail = NULL;
	pArCfDesc->pWriteHead = NULL;
	pArCfDesc->pWriteTail = NULL;
	pArCfDesc->b0Modified = 0;
    }

    if (maxBytes < AR_CF_BLKSIZE) {
	(void)printf("nBytes must be at least %d\n", AR_CF_BLKSIZE);
	stat = ERROR;
    }
    if (stat == OK) {
	maxBlocks = maxBytes / AR_CF_BLKSIZE;

	pDesc = bfCreate(name, BF_TYPE_CHAN_DATA,
				1, AR_CF_BLKSIZE, BF_FLAG_GROW, maxBlocks);
	if (pDesc == NULL)
	    stat = ERROR;
	else
	    pArCfDesc->pBfDesc = pDesc;
    }
    if (stat == OK) {
/*----------------------------------------------------------------------------
* create has succeeded; initialize userInfo in block0
*---------------------------------------------------------------------------*/
	ArB0MIHead(pArCfDesc) = 0;
	ArB0MITail(pArCfDesc) = 0;
	ArB0MINblk(pArCfDesc) = 0;
	ArB0MIFree_blkNum(pArCfDesc) = 0;
	ArB0MIFree_hdrNum(pArCfDesc) = 0;
        ArB0FormatRev(pArCfDesc) = AR_CF_FORMAT_REV;
	if ((stat = bfWrite(pDesc, pDesc->pBlock0, 0)) != OK)
	    (void)printf("arCFCreate: error writing block0\n");
    }
    if (stat != OK) {
	if (pArCfDesc != NULL) {
/*----------------------------------------------------------------------------
* ERROR has occurred; clean up whatever structure has been built and close
*    the file.
*---------------------------------------------------------------------------*/
	    (void)arCFClose(&pArCfDesc);
	    assert(pArCfDesc == NULL);
	}
    }

    return pArCfDesc;
}

/*+/subr**********************************************************************
* NAME	arCFOpen - open an AR channel data file
*
* DESCRIPTION
*	Opens an existing AR channel data file (a 'block file' of type
*	BF_TYPE_CHAN_DATA) in the specified mode, O_RDONLY or O_RDWR.  If
*	the mode is O_RDWR, then a lock file is created to prevent multiple
*	writers of the file.
*
*	Upon a successful open, the master index blocks have been read into
*	memory (and the channel file descriptor has the appropriate pointers).
*
*	The block size in the file must be AR_CF_BLKSIZE.  The file
*	`format revision number' must match the `format revision number'
*	the code was compiled with.
*
* RETURNS
*	pointer to channel file descriptor, or
*	NULL
*
* BUGS
* o	lock file handling is somewhat `ratty', especially under VxWorks;
*	there is a slim chance that multiple writers could occur.
* o	no exit handler is established, which means that AR_CFClose()
*	isn't automatically called.  This means that buffers aren't flushed
*	and the lock file isn't deleted if the program aborts.
*
* SEE ALSO
*	arCFCreate(), arCFClose(), arCFChanOpen()
*
* EXAMPLE
*
*-*/
AR_CF_DESC *
arCFOpen(name, mode)
char	*name;		/* I file name */
int	mode;		/* I mode to use for open--O_RDONLY or O_RDWR */
{
    int		stat=OK;	/* status from calls */
    BF_DESC	*pDesc=NULL;	/* pointer to block file descriptor */
    AR_CF_DESC	*pArCfDesc=NULL;/* pointer to channel file descriptor */

    assert(strlen(name) < GEN_FNAME_DIM);
    assert(strlen(name) > 0);
    assert(mode == O_RDONLY || mode == O_RDWR);
    (void)arCSCheck();          /* check control system 'compatibility' */
    (void)arCFAsserts();        /* check control system 'compatibility' */

/*----------------------------------------------------------------------------
* allocate memory for an AR channel file descriptor and initialize the
*    descriptor.  Then open the block file and put into the channel file
*    descriptor a pointer to the block file descriptor.  This hides from
*    most users the fact that there are actually two descriptors--they
*    just use the channel file descriptor.
*---------------------------------------------------------------------------*/
    if ((pArCfDesc = arCF_mallocCF_DESC()) == NULL) {
	(void)fprintf(stderr, "arCFOpen: can't malloc for arCfDesc\n");
	stat = ERROR;
    }
    else {
	pArCfDesc->pBfDesc = NULL;
	pArCfDesc->pMIHead = NULL;		/* no MI buffers yet */
	pArCfDesc->pMITail = NULL;
	pArCfDesc->pMIFree = NULL;
	pArCfDesc->MIFree_hdrNum = 0;
	pArCfDesc->pChanDescHead = NULL;	/* no channel desc exist yet */
	pArCfDesc->pChanDescTail = NULL;
	pArCfDesc->pWriteHead = NULL;
	pArCfDesc->pWriteTail = NULL;
	pArCfDesc->b0Modified = 0;
    }
    if (stat == OK) {
	if ((pDesc = bfOpen(name, mode, BF_TYPE_CHAN_DATA, 0)) == NULL)
	    stat = ERROR;
	else
	    pArCfDesc->pBfDesc = pDesc;
    }
    if (stat == OK) {
        if (BfB0BlockSize(pDesc) != AR_CF_BLKSIZE) {
	    (void)fprintf(stderr, "arCfOpen: block size mismatch\n");
	    stat = ERROR;
	}
    }
    if (stat == OK) {
        if (ArB0FormatRev(pArCfDesc)/100 != AR_CF_FORMAT_REV/100) {
	    (void)fprintf(stderr,
		"arCfOpen: file format mismatch--file %d, code %d\n",
		ArB0FormatRev(pArCfDesc), AR_CF_FORMAT_REV);
	    stat = ERROR;
	}
    }
    if (stat == OK) {
/*----------------------------------------------------------------------------
* so far, the open is going OK.  Do any additional processing needed.
*---------------------------------------------------------------------------*/
	stat = arCFReadMIBuffers(pArCfDesc);
    }

    if (stat != OK) {
	if (pArCfDesc != NULL) {
/*----------------------------------------------------------------------------
* ERROR has occurred; clean up whatever structure has been built and close
*    the file.
*---------------------------------------------------------------------------*/
	    (void)arCFClose(&pArCfDesc);
	    assert(pArCfDesc == NULL);
	}
    }

    return pArCfDesc;
}

/*+/subr**********************************************************************
* NAME	arCFReadMIBuffers - read (or re-read) the MI blocks
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	doesn't have any protection against changes in linked lists while
*	this routine is running
* o	doesn't handle deleting channels or purging data (i.e., deleting
*	.datInfo items or entire index blocks)
* o	comments are out of date--they don't describe re-read of MI blocks
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
arCFReadMIBuffers(pArCfDesc)
AR_CF_DESC *pArCfDesc;	/* I ptr to channel file descriptor */
{
    int		retStat=OK;	/* return status to caller */
    int		stat;           /* status return from calls */
    BF_BLKNUM	blockNum;	/* block number */
    AR_MI_BUF	*pMIBuf;	/* ptr to buffer for block */
    int		i;
    AR_MI_BUF	MIBuf;		/* temporary buffer for re-read of MI block */

    if (ArB0MIHead(pArCfDesc) == 0)
	return OK;
/*----------------------------------------------------------------------------
*    the file has some Master Index blocks; read ALL OF THEM into memory.
*
*	For each MI block, get a buffer, link the buffer into the MI buffer
*	list, put the block number into the buffer, reset the buffer's flags,
*	and, finally, read the block into the buffer.  Once the block is in
*	the buffer, do some initialization for each chanHdr.  This
*	initialization includes reading the blocks for DBF_ENUM channels.
*
*	If block0 points to an MIFree, then check each MI block to see if it
*	is the one pointed to; when a match is found, initialize the MIFree
*	information in the channel file descriptor.
*---------------------------------------------------------------------------*/

    blockNum = ArB0MIHead(pArCfDesc);
    while (blockNum > 0) {
	pMIBuf = pArCfDesc->pMIHead;
	while (pMIBuf != NULL) {
	    if (pMIBuf->blkNum == blockNum)
		break;
	    pMIBuf = pMIBuf->pNextMI;
	}
	if (pMIBuf == NULL) {
	    if ((pMIBuf = arCF_mallocMI_BUF()) == NULL) {
		(void)fprintf(stderr, "arCFOpen: can't malloc for MI block\n");
		stat = ERROR;
		goto readMIError;
	    }
	    if (pArCfDesc->pMIHead == NULL)		/* first block? */
		pArCfDesc->pMIHead = pMIBuf;		/* yes; link to head */
	    else
		pArCfDesc->pMITail->pNextMI = pMIBuf;	/* no; link from prev */
	    pArCfDesc->pMITail = pMIBuf;		/* this is now tail */
	    pMIBuf->blkNum = blockNum;
	    pMIBuf->pNextMI = NULL;
	    ArCFModifyInit(pMIBuf);
	    pMIBuf->pBlock = &pMIBuf->bfInfo;

	    stat = bfRead(pArCfDesc->pBfDesc, &pMIBuf->bfInfo, blockNum);
	    if (stat != OK) {
		(void)fprintf(stderr, "arCFOpen: error reading MI block\n");
		stat = ERROR;
		goto readMIError;
	    }

	    for (i=0; i<AR_CF_NHDR; i++) {
		pMIBuf->chanHdr[i].count = 0;
		pMIBuf->chanHdr[i].flags = 0;
		if (pMIBuf->chanHdr[i].name[0] != '\0' &&
			    pMIBuf->chanHdr[i].chanType == DBF_ENUM) {
		    pMIBuf->chanHdr[i].graphics.enumGr.pGRBuf = NULL;
		    stat = arCF_GRReadBlock(pArCfDesc, &pMIBuf->chanHdr[i]);
		    if (stat != OK) {
			(void)fprintf(stderr,
			    "arCFOpen: error reading GR block for ENUM\n");
			stat = ERROR;
			goto readMIError;
		    }
		}
	    }
	}
	else {
	    stat = bfRead(pArCfDesc->pBfDesc, &MIBuf.bfInfo, blockNum);
	    if (stat != OK) {
		(void)fprintf(stderr, "arCFOpen: error reading MI block\n");
		stat = ERROR;
		goto readMIError;
	    }
	    pMIBuf->bfInfo.flink = MIBuf.bfInfo.flink;
	    for (i=0; i<AR_CF_NHDR; i++) {
		if (MIBuf.chanHdr[i].name[0] != '\0') {
		    assertAlways(strcmp(pMIBuf->chanHdr[i].name,
						MIBuf.chanHdr[i].name) == 0);
		    pMIBuf->chanHdr[i].indexTail = MIBuf.chanHdr[i].indexTail;
		    pMIBuf->chanHdr[i].newestSecPastEpoch =
					MIBuf.chanHdr[i].newestSecPastEpoch;
		    pMIBuf->chanHdr[i].newestNsec =
					MIBuf.chanHdr[i].newestNsec;
		}
		else {
		    pMIBuf->chanHdr[i].graphics.nextMIFree =
		    			MIBuf.chanHdr[i].graphics.nextMIFree;
		}
	    }
	    
	}
	if (ArB0MIFree_blkNum(pArCfDesc) == blockNum) {
	    pArCfDesc->pMIFree = pMIBuf;
	    pArCfDesc->MIFree_hdrNum = ArB0MIFree_hdrNum(pArCfDesc);
	}
	blockNum = pMIBuf->bfInfo.flink;	/* next MI block */
    }
    return OK;
readMIError:
    return ERROR;
}

/*+/subr**********************************************************************
* NAME	arCFSyncRead - synchronize read buffers in memory with disk
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
arCFSyncRead(pArCfDesc)
AR_CF_DESC *pArCfDesc;	/* I ptr to channel file descriptor */
{
    int		retStat=OK;	/* return status to caller */
    int		stat;           /* status return from calls */
    int		readOnly;	/* channel file is opened read-only */
    AR_CHAN_DESC *pChanDesc;
    BF_DESC	*pBfDesc;

    assert(pArCfDesc != NULL);
    assert(pArCfDesc->pBfDesc != NULL);
    pBfDesc = pArCfDesc->pBfDesc;

    readOnly = ((pBfDesc->flags & BF_DESC_WRITE) == 0);
    if (readOnly) {
	stat = bfRead(pBfDesc, pBfDesc->pBlock0, 0);
	assertAlways(stat == OK);
	stat = arCFReadMIBuffers(pArCfDesc);
	assertAlways(stat == OK);
    }
    pChanDesc = pArCfDesc->pChanDescHead;
    while (pChanDesc != NULL) {
	if ((pChanDesc->flags & AR_CDESC_WRITE) == 0) {
	    if (pChanDesc->pIndexBuf != NULL &&
				pChanDesc->pIndexBuf->bfInfo.flink == 0) {
		stat = bfRead(pBfDesc, &pChanDesc->pIndexBuf->bfInfo,
						pChanDesc->pIndexBuf->blkNum);
		assertAlways(stat == OK);
	    }
	    if (pChanDesc->pDataBuf != NULL &&
				pChanDesc->pDataBuf->bfInfo.flink == 0) {
		stat = bfRead(pBfDesc, &pChanDesc->pDataBuf->bfInfo,
						pChanDesc->pDataBuf->blkNum);
		assertAlways(stat == OK);
		pChanDesc->remainCount =
			pChanDesc->pData-(char *)&pChanDesc->pDataBuf->bfInfo -
			pChanDesc->pDataBuf->bfInfo.firstByte;
	    }
	}
	pChanDesc = pChanDesc->pNextDesc;
    }

    return retStat;
}

/*+/subr**********************************************************************
* NAME	arCFSyncWrite - synchronize disk with write buffers in memory
*
* DESCRIPTION
*
* RETURNS
*	OK
*
* BUGS
* o	doesn't lock for list and write operations
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
arCFSyncWrite(pArCfDesc)
AR_CF_DESC *pArCfDesc;	/* I ptr to channel file descriptor */
{
    int		retStat=OK;	/* return status to caller */
    int		stat;           /* status return from calls */
    AR_MI_BUF	*pBuf;		/* preamble is same for all types of buffer */
    int		modified=0;

    assert(pArCfDesc != NULL);

    while ((pBuf = pArCfDesc->pWriteHead) != NULL) {
	modified++;
	if (ArCFModifyTestAndReset(pArCfDesc, pBuf)) {
	    stat = bfWrite(pArCfDesc->pBfDesc, &pBuf->bfInfo, pBuf->blkNum);
	    assertAlways(stat == OK);
	}
    }

    if (modified) {
	pArCfDesc->b0Modified = 0;
	stat = bfWrite(pArCfDesc->pBfDesc, pArCfDesc->pBfDesc->pBlock0, 0);
	if (stat != OK)
	    retStat = ERROR;
    }

    return retStat;
}
