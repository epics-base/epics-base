/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	03-05-90
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
 *  .01 03-05-90 rac	initial version
 *  .02 07-30-91 rac	installed in SCCS
 *  .03 09-14-92 rac	discontinue using special malloc routines
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code
 */
/*+/mod***********************************************************************
* TITLE	bfSubr.c - 'block file' subroutines
*
* DESCRIPTION
*    These subroutines implement a fixed block size file system based on
*    the usual byte-oriented file system available to C.  The operations
*    used by these routines are:
*    o  open() with O_RDONLY, O_RDWR, and O_CREAT as options.  The O_EXCL
*	option found in some operating systems isn't used.  Instead, these
*	routines create a "fileName.lock" file to enforce a single writer.
*    o  lseek() with L_SET as the option used.
*    o  read(), write(), and close()
*
*    On disk, the file system is a file containing fixed size blocks.  These
*    routines maintain a singly-threaded list of 'free' blocks which are
*    available for use.  User routines built on top of these routines
*    must maintain singly-threaded lists of 'in use' blocks.
*
*    These routines maintain some information in a file descriptor in memory.
*    This information isn't written to the disk until the file is closed.
*    Users of these routines must take particular care to ensure that
*    bfClose() is called for all open files.
*
*    Since information about the file is kept in the file descriptor, these
*    routines aren't reentrant with respect to a single file.  Several file
*    descriptors can be used simultaneously, one per thread, without
*    interference.
*
*    Although these routines do a reasonable amount of error checking, the
*    user must bear a significant amount of responsibility in abiding by
*    the rules.  Some cautions for the user include:
*    o  don't use pointers to file descriptors after bfClose() is called
*    o  don't exceed the block size for the file.  BF_MAX_BLOCK_SIZE is the
*       largest a block will ever be, but BfBlockSize(pDesc) returns the
*       actual block size for a particular file.
*
*    Users are strongly encouraged to use the BfXxx() macros when dealing
*    with block0 or the file descriptor.
*
* INCLUDE FILES
*    In general, use of the block file routines requires using several
*    include files.  The set used differs between SunOS and VxWorks:
*
*		   SunOs	   VxWorks
*
*		<bfDefs.h>	<bfDefs.h>
*		<stdio.h>	<vxWorks.h>
*		<sys/file.h>	<ioLib.h>
*				<stdioLib.h>
*
* BUGS
* o	routines aren't reentrant with respect to a single file
* o	some operations require an operating system file I/O call when, with
*	appropriate enhancements, such I/O could be avoided, or at least
*	deferred
* o	a list of open block files should be kept for a task, and an exit
*	handler should be established for a task, so that cleanup is assured
* o	the issue of blocking signals during critical operations should be
*	addressed
* o	error handling is rudimentary: there are no individualized error
*	codes; most error messages don't include file name; error messages
*	are sent to stdout; etc.
* o	these routines use blocking I/O.  Under SunOS, non-blocking I/O
*	might be better.
*	
*-***************************************************************************/


/*----------------------------------------------------------------------------
*    synopsis summaries
*	< indicates input arg to function
*	> indicates output arg from function
*	<> indicates arg both input to and output from function
*
*	except as noted, 'int' functions return OK or ERROR
*	blockNum must be 'BF_BLKNUM'
*
*     char *bfRingFgets(>pBuf,      <bufDim,  <>pBfDesc			)
*						returns pBuf or NULL
*       int bfRingFputs(<pBuf,     <>pBfDesc				)
*						returns last character or EOF
*       int BfRingGetc(<>pDesc						)
*						returns character or EOF
*       int BfRingPutc( <c,        <>pBfDesc				)
*						returns character or EOF
*
*BF_BLKNUM bfAcquire( <>pBfDesc,  <>pBfBlock				)
*		returns blockNum or ERROR
*	int bfClose(   <>ppBfDesc					)
*  BF_DESC *bfCreate(	<name,	    <type, <nblk, <blkSize, <flags, <maxNblk )
*	int bfDumpChar(	<pBfDesc,   <blockNum				)
*	int bfDumpHex(	<pBfDesc,   <blockNum				)
*	int bfInfo(	<name						)
*  BF_DESC *bfOpen(	<name,	    <mode, <type, <blkSize		)
*	int bfRead(	<pBfDesc,  <>pBfBlock, <blockNum		)
*	int bfRelease( <>pBfDesc,  <>pBfBlock, <blockNum		)
*	int bfWrite(	<pBfDesc,  <>pBfBlock, <blockNum		)
*---------------------------------------------------------------------------*/
#define BF_TEXT_GLBLSOURCE	/* must precede incl bfDefs.h */
#include <genDefs.h>
#include <bfDefs.h>
#include <ctype.h>

#ifdef vxWorks
#   include <vxWorks.h>
#   include <ioLib.h>
#   include <stdioLib.h>
    typedef long off_t;			/* for lseek() */
#else
#   include <stdio.h>
#   include <sys/types.h>
#   include <sys/file.h>
#endif

#ifdef LINT
    char	*glBfTypeKw[];

    main()
    {
	char	*pBuf;
	BF_DESC	*pDesc;
	BF_BLOCK *pBlock;

	exit(1);

	pBuf = NULL;
	pDesc = NULL;
	pBlock = NULL;

	(void) bfRingFgets(pBuf, 0, pDesc);
	(void) bfRingFputs(pBuf, pDesc);
	(void) bfClose(&pDesc);
	pDesc = bfCreate(" ", 0, (BF_BLKNUM)0, 0, (long)0, (BF_BLKNUM)0);
	(void) bfInfo(" ");
	(void) bfRelease(pDesc, pBlock, (BF_BLKNUM)0);
    }
#endif

BF_DESC *bfMallocDesc()
{  return (BF_DESC *)malloc(sizeof(BF_DESC));  }
void bfFreeDesc(ptr)
BF_DESC *ptr;
{  free(ptr);  }

BF_BLOCK0 *bfMallocBlock0()
{  return (BF_BLOCK0 *)malloc(sizeof(BF_BLOCK0));  }
void bfFreeBlock0(ptr)
BF_BLOCK0 *ptr;
{  free(ptr);  }


/*+/subr**********************************************************************
* NAME	bfRingFgets - get a line from a ring file
*
* DESCRIPTION
*	Read the next line, including the newline, from the ring file into
*	the caller's buffer.  The last character placed in the caller's buffer
*	is always followed by '\0'.
*
*	At most bufDim-1 characters are transferred; if no newline was
*	encountered, then the remaining characters will be read with the
*	next call.
*
*	Due to the nature of ring files, the first call to bfRingFgets()
*	may encounter a partial line.  If this occurs, the partial line
*	is discarded and the first full line is returned.
*
* RETURNS
*	pBuf, or
*	NULL if an error occurs or EOF is encountered
*
*-*/
char *
bfRingFgets(pBuf, bufDim, pDesc)
char	*pBuf;		/* buffer to receive up to 'bufDim-1' characters */
int	bufDim;		/* dimension of buffer */
BF_DESC *pDesc;		/* block file descriptor */
{
    int		c;		/* character from file */
    char	*pBuf1;		/* temporary buffer pointer */

    pBuf1 = pBuf;
    while (--bufDim > 0  &&  (c = BfRingGetc(pDesc)) != EOF) {
	if ((*pBuf1++ = c) == '\n')
	    break;
    }
    *pBuf1 = '\0';
    return ((c == EOF  &&  pBuf1 == pBuf) ? NULL : pBuf);
}

/*+/subr**********************************************************************
* NAME	bfRingFillRbufAndGetc - get next block and return a character
*
* DESCRIPTION
*	Reads the next readable block from the ring file and returns the
*	first readable character from the block.  (If the present block, if
*	any, contains unread characters, then those characters will be
*	skipped.)
*
*	Due to the nature of ring files, the first call to bfRingFillRbuf()
*	may encounter a partial line.  If this occurs, the partial line
*	is discarded and the character is obtained from the first full line.
*
*	The macro BfRingGetc(pDesc) is the recommended method for reading
*	a ring file a character at a time.  That macro calls this routine
*	as necessary.
*
*	The routine bfRingFgets() is available for reading a ring file a line
*	at a time.
*
* RETURNS
*	character, or
*	EOF if an error occurs or EOF is encountered
*
*-*/
int
bfRingFillRbufAndGetc(pDesc)
BF_DESC *pDesc;		/* IO block file descriptor for ring file */
{
    int		c;		/* character to return to caller */
    int	stat;

/*----------------------------------------------------------------------------
*    first, decide which block to read for more data.  If this is the first
*    time a read has been done, use the ringHead block; otherwise, use the
*    flink from the present block (if the present block is ringTail, then
*    there is no next block).
*---------------------------------------------------------------------------*/
    if (pDesc->readCount < -9)
	pDesc->readBlock = BfB0RingHead(pDesc);
    else {
	if (pDesc->readBlock == BfB0RingTail(pDesc))
	    return EOF;
	else
	    pDesc->readBlock = pDesc->pReadBuf->flink;
    }
/*----------------------------------------------------------------------------
*    if the block number to read is zero, then there is nothing to do but
*    return EOF.  If the block number isn't zero, read the block; otherwise,
*    return EOF.  Once the block has been read, check lastByte; if it indicates
*    no data, return EOF.
*
*    When the first read is to be done, a buffer is obtained from malloc,
*    with the pointer stored in the descriptor.  This buffer will be free'd
*    when the bfClose() is called.
*
*---------------------------------------------------------------------------*/
    if (pDesc->readBlock > 0) {
	if (pDesc->pReadBuf == NULL) {
	    if ((pDesc->pReadBuf =
		    (BF_BLOCK *)malloc((unsigned)BfB0BlockSize(pDesc))) ==
								NULL) {
		(void)fprintf(stderr,
				"bfRingFillRbufAndGetc: can't get memory\n");
		return EOF;
	    }
	}
	stat = bfRead(pDesc, pDesc->pReadBuf, pDesc->readBlock);
	if (stat != OK)
	    c = EOF;
	else {
	    if (pDesc->pReadBuf->lastByte < BF_BLOCK_DATA)
		c = EOF;
	    else {
/*----------------------------------------------------------------------------
*    set the read pointer and readCount.  If this is the first read for the
*    file, then start reading at firstByte; succeeding blocks start reading
*    at BF_BLOCK_DATA, in order to read any overflow bytes from previous
*    blocks.  (If the block for the first read for the file contains only
*    overflow bytes, then EOF is returned.)
*
*    If the file isn't organized by lines, then reading always starts at
*    BF_BLOCK_DATA.
*
*---------------------------------------------------------------------------*/
		int i=0;

		if (pDesc->readCount < -9) {	/* first read for file */
		    i = pDesc->pReadBuf->firstByte;
		    if (i == 0)			/* file not line-oriented */
			i = BF_BLOCK_DATA;
		}
		else
		    i = BF_BLOCK_DATA;

		if (i < BF_BLOCK_DATA) {	/* only overflow bytes */
		    pDesc->pRead = NULL;
		    pDesc->readCount = 0;
		    c = EOF;
		}
		else {
		    pDesc->pRead = (char *)pDesc->pReadBuf + i - 1;
		    pDesc->readCount = pDesc->pReadBuf->lastByte - i + 1;

		    c = (int)*(char *)(++pDesc->pRead);
		    pDesc->readCount--;
		}
	    }
	}
    }
    else
	c = EOF;

    return c;
}

/*+/subr**********************************************************************
* NAME	bfRingFputs - put a line into a ring file
*
* DESCRIPTION
*	Write the line, including the newline, into the ring file.  The
*	'\0' at the end of the caller's line isn't written into the file.
*
*	If this is the first line written to a block, then firstByte is
*	set to point to this line.
*
* RETURNS
*	last character scanned from 'line' (typically '\0'), or
*	EOF, if any error occurs
*
*-*/
int
bfRingFputs(line, pDesc)
char	*line;		/* I line to write */
BF_DESC	*pDesc;		/* IO pointer to descriptor */
{
    if (*line != '\0') {
	if (BfRingPutc(*line, pDesc) == EOF)
	    return EOF;
	if (pDesc->pWriteBuf->firstByte == 0) {
	    pDesc->pWriteBuf->firstByte = pDesc->pWrite -
						(char *)pDesc->pWriteBuf;
	}
    }
    else
	return *line;

    while (*(++line) != '\0') {
	if (BfRingPutc(*line, pDesc) == EOF)
	    return EOF;
    }
    return *line;
}

/*+/subr**********************************************************************
* NAME	bfRingFlushWbufAndPutc - flush write buffer and write a character
*
* DESCRIPTION
*	Write a block to the ring file, get the next block for writing, and
*	put the character into the block.  The file descriptor must have
*	a pointer to the memory buffer for the block at entry (except
*	immediately after bfOpen() ); at exit, a pointer to the memory buffer
*	for the new block has been placed in the file descriptor.
*
*	For the first write following bfOpen(), this routine obtains a
*	memory buffer for the block and places the pointer into the descriptor.
*	In this case, there is no buffer to flush.  Succeeding calls to this
*	routine will function as described above.
*
*	When this routine is called the first time following bfOpen(), it
*	attempts to begin writing in the ringTail block, if there is one.
*	When additional blocks are needed for writing, attempts are made to
*	acquire them in the following order:
*	o   try to get a block from the free block list
*	o   try to get a block by expanding the file if it is BF_FLAG_GROW
*	o   follow the current block's flink, if it isn't zero
*	o   if the current block's flink is zero, then grab the ringHead
*	    block for writing and move ringHead forward to its flink
*
*	The macro BfRingFputc(c, pDesc) is the recommended method for
*	writing a ring file a character at a time.  That macro calls this
*	routine as necessary.
*
*	The routine bfRingFputs() is available for writing a ring file
*	a line at a time.
*
* RETURNS
*	character written, or
*	EOF if an error occurs
*
* BUGS
* o	does a malloc/free for every call
*
* EXAMPLE
*
*-*/
int
bfRingFlushWbufAndPutc(c, pDesc)
char	c;		/* I character to write */
BF_DESC *pDesc;		/* IO pointer to ring file descriptor */
{
    int		stat;		/* status return from calls */
    BF_BLKNUM	flink;		/* link to next write block */
    BF_BLOCK   *pBuf;		/* next write block */

    if ((pBuf = (BF_BLOCK *)malloc((unsigned)BfB0BlockSize(pDesc))) == NULL) {
	(void)fprintf(stderr, "bfRingFlushWbufAndPutc: can't get memory\n");
	return EOF;
    }

    if (pDesc->writeBlock == 0) {
/*----------------------------------------------------------------------------
*	this is the first write for the file.  Get a block for writing by
*	trying first the ringTail block and then, if there isn't a ringTail,
*	acquiring a block.  If the ringTail block is full, make it look like
*	the 'old' block, so that a new block will be acquired by the normal
*	mechanism.
*---------------------------------------------------------------------------*/
	if ((flink = BfB0RingTail(pDesc)) > 0) {
	    stat = bfRead(pDesc, pBuf, flink);
	    if (stat != OK) {
		(void)free((char *)pBuf);
		return EOF;
	    }
	    if (pBuf->lastByte >= BfB0BlockSize(pDesc)-1) {
		pDesc->pWriteBuf = pBuf;	/* masquerade as old */
		pDesc->writeBlock = flink;
	    }
	}
	else {
	    flink = bfAcquire(pDesc, pBuf);
	    if (flink == ERROR) {
		(void)free((char *)pBuf);
		return EOF;
	    }
	}
    }
    if (pDesc->writeBlock != 0) {
/*----------------------------------------------------------------------------
*	the current block is full.  Obtain a new block.  Attempts are made
*	in the following order:
*	o  try to get a block from bfAcquire(), which will either remove
*	   a block from the free block list or which will expand the file.
*	   If neither succeeds, then:
*	o  follow the block's flink, if the flink isn't zero.  If the flink
*	   is zero, then:
*	o  use the block which is currently the ringHead
*	In the latter two cases, the block must be made to appear empty by
*	setting firstByte and lastByte appropriately.
*---------------------------------------------------------------------------*/
	int	readFlag;

	if ((flink = bfAcquire(pDesc, pBuf)) > 0)
	    readFlag = 0;
	else {
	    if ((flink = pDesc->pWriteBuf->flink) > 0)
		readFlag = 1;
	    else {
	        if ((flink = BfB0RingHead(pDesc)) > 0)
		    readFlag = 1;
		else {
		    free((char *)pBuf);
		    return EOF;		/* no new block available */
		}
	    }
	}
	if (readFlag) {		/* only have flink; get the actual block */
	    stat = bfRead(pDesc, pBuf, flink);
	    if (stat != OK) {
		free((char *)pBuf);
		return EOF;
	    }
	    pBuf->firstByte = 0;
	    pBuf->lastByte = BF_BLOCK_DATA - 1;
	}
    }
/*----------------------------------------------------------------------------
* 'flink' is the number of a new block; 'pBuf' has its memory address.  Wrap
*    up processing on the 'old' block and then install the new one.
*
*    first, install the proper flink into the current block.  If the new block
*    was obtained via bfAcquire(), then it is necessary to splice the new
*    block into the flink chain.
*
*    then, update the lastByte in the block to correspond with the pointer
*    in the descriptor.
*
*    finally, write the current block.  (In some cases there isn't a current
*    block to write; this will usually be the case on the first call after
*    a file has been opened.)  After writing the current block, free the
*    buffer.
*---------------------------------------------------------------------------*/
    if (pDesc->pWriteBuf != NULL) {
	if (pDesc->pWriteBuf->flink == flink)
	    ;					/* old block's flink OK */
	else if (pDesc->pWriteBuf->flink == 0)
	    pDesc->pWriteBuf->flink = flink;	/* link old to new */
	else {
	    pBuf->flink = pDesc->pWriteBuf->flink;	/* splice flinks */
	    pDesc->pWriteBuf->flink = flink;
	}

	pDesc->pWriteBuf->lastByte = pDesc->pWrite - (char *)pDesc->pWriteBuf;

	stat = bfWrite(pDesc, pDesc->pWriteBuf, pDesc->writeBlock);
	if (stat != OK) {
	    free((char *)pBuf);
	    return EOF;
	}
	free((char *)pDesc->pWriteBuf);
    }

/*----------------------------------------------------------------------------
* the current block has been dealt with.  Install the new one into the
*    descriptor.  (In the descriptor is the address of the memory that was
*    malloc'ed above.)
*---------------------------------------------------------------------------*/
    if (BfB0RingHead(pDesc) == 0)
	BfB0RingHead(pDesc) = flink;
    else if (BfB0RingHead(pDesc) == flink)
	BfB0RingHead(pDesc) = pBuf->flink;

    BfB0RingTail(pDesc) = flink;
    pDesc->writeBlock = flink;
    pDesc->pWriteBuf = pBuf;
    pDesc->writeCount = BfB0BlockSize(pDesc) - pBuf->lastByte - 1;
    pDesc->pWrite = (char *)pDesc->pWriteBuf + pBuf->lastByte;

/*----------------------------------------------------------------------------
* FINALLY (whew!), store the character that was sent in!
*---------------------------------------------------------------------------*/
    *(++pDesc->pWrite) = c;
    --pDesc->writeCount;

    return c;
}

/*+/subr**********************************************************************
* NAME	bfAcquire - acquire a block for use
*
* DESCRIPTION
*	Acquires a block for use by the caller, placing the block in the
*	caller's buffer.
*
*	bfAcquire() will acquire the block from the free block list, if any
*	free blocks are available.  If no free blocks are available, then
*	the file is extended, if the BF_FLAG_GROW flag is set and the maximum
*	size won't be violated.  Otherwise, no block is obtained.
*
*	The block number returned by bfAcquire() is the value which must
*	be used with bfRead(), bfWrite(), etc.
*
*	'firstByte' is set to 0 .  'lastByte' points one character before
*	the first available byte location.  'flink' is set to 0 .
*
*	The free block list on disk isn't updated by bfAcquire(), so it is
*	extremely important that bfClose() be called before exiting.
*
* RETURNS
*	block number or
*	ERROR (no error message is printed if reason for not being able to
*		acquire a block is either: no free block or can't grow file)
*
* BUGS
* o	should maintain an 'in memory' list of free blocks and replenish
*	the list as necessary.  This would avoid I/O delays on bfAcquire()
*	calls.
* o	should be able to get a free block without needing to do disk I/O
* o	multiple, simultaneous calls to this routine could result in giving
*	the same block to several callers.  The callers must presently
*	cooperate to prevent simultaneous calls.
*
* SEE ALSO
*	bfRelease()
*
* EXAMPLE
*	BF_DESC	*pDesc;		pointer to descriptor
*	BF_BLOCK *pBuf;		pointer to buffer containing block
*	BF_BLKNUM blockNum;	block number of acquired block
*
*	pDesc = bfOpen("myFile", O_RDWR, BF_TYPE_UNDEF);
*	pBuf = (BF_BLOCK *)malloc(BfB0BlockSize(pDesc));
*	blockNum = bfAcquire(pDesc, pBuf);
*	pBuf->firstByte = pBuf->lastByte + 1;
*	for (i=0; i<nBytes; i++)
*	    ((char *)pBuf)[++pBuf->lastByte] = myData[i];
*	stat = bfWrite(pDesc, pBuf, blockNum, 0);
*	stat = bfClose(pDesc);
*	free((char *)pBuf);
*
*-*/
BF_BLKNUM
bfAcquire(pDesc, pBuf)
BF_DESC	*pDesc;		/* IO pointer to descriptor */
BF_BLOCK  *pBuf;	/* IO buffer ptr; buf size is BfB0BlockSize(pDesc) */
{
    int		stat;		/* status from calls */
    int		blockNum;	/* number of acquired block */

    assert(pDesc != NULL);
    assert(pBuf != NULL);

    if (!BfWriteable(pDesc)) {
	(void)fprintf(stderr, "bfAcquire: file is O_RDONLY\n");
	return ERROR;
    }

    if (BfB0Flink(pDesc) == 0) {
/*----------------------------------------------------------------------------
* NO FREE BLOCK -- try to extend the file
*---------------------------------------------------------------------------*/
	if (  BfB0CurrentSize(pDesc) >= BfB0MaxSize(pDesc) ||
	      (BfB0Flags(pDesc) & BF_FLAG_GROW) == 0  ) {
	    (void)fprintf(stderr, "bfAcquire: file maxSize reached\n");
	    return ERROR;
	}
	else
	    blockNum = BfB0CurrentSize(pDesc)++;
    }
    else {
/*----------------------------------------------------------------------------
* USE A FREE BLOCK
*---------------------------------------------------------------------------*/
	off_t	offset;		/* offset into file, for lseek() */
	off_t	lseek();	/* lseek function */

	blockNum = BfB0Flink(pDesc);
	offset = blockNum * BfB0BlockSize(pDesc);
	stat = lseek((int)pDesc->fd, offset, L_SET);
	if (stat == ERROR) {
	    (void)fprintf(stderr, "bfAcquire: error on lseek for free block\n");
	    return ERROR;
	}
	stat = read((int)pDesc->fd, (char *)pBuf, (int)BfB0BlockSize(pDesc));
	if (stat != BfB0BlockSize(pDesc)) {
	    (void)fprintf(stderr, "bfAcquire: error reading free block\n");
	    return ERROR;
	}

/*----------------------------------------------------------------------------
*    move the free block's flink into block0 and decrement the free block count
*---------------------------------------------------------------------------*/
	BfB0Flink(pDesc) = pBuf->flink;
	BfB0CurrentFree(pDesc)--;
    }

    pBuf->flink = 0;
    pBuf->firstByte = 0;
    pBuf->lastByte = BF_BLOCK_DATA - 1;

    return blockNum;
}

/*+/subr**********************************************************************
* NAME	bfClose - close a block file
*
* DESCRIPTION
*	Close a file.  The actions taken are:
*	o  if pDesc->writeBlock > 0, write the block
*	o  if pDesc->pReadBuf != NULL, free the buffer
*	o  if pDesc->pWriteBuf != NULL, free the buffer
*	o  write block0
*	o  if the file was opened O_RDWR, delete the write lock file
*	o  free the descriptor and store null in the caller's pointer
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	writing the block and freeing the buffers are somewhat controversial.
*	If bfClose() free's the blocks, that may interfere with the caller's
*	memory management.  If the caller free's the buffers in an exit
*	handler, that may interfere with bfClose() writing the block.  There
*	is also a potential conflict between exit handlers.  The caller won't
*	know how large to make the buffer until after bfOpen() has been
*	called, and thus may be inclined to wait until after bfOpen() to
*	register the block with the caller's exit handler, which might result
*	in the caller's exit handler having precedence over one possibly
*	established by bfOpen(); if this scenario were acted out, the buffer
*	would be free'd before bfClose() got a chance to write it.
*
* SEE ALSO
*	bfOpen()
*
*-*/
int
bfClose(ppDesc)
BF_DESC	**ppDesc;	/* IO pointer to pointer to descriptor */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status */
    off_t	offset;		/* offset into file, for lseek() */
    off_t	lseek();	/* lseek function */
    BF_DESC    *pDesc;		/* pointer to descripter */

    assert(*ppDesc != NULL);

    pDesc = *ppDesc;

/*----------------------------------------------------------------------------
*    first, see if a partial block needs to be written; write if needed.  If
*    read and/or write buffers are registered in descriptor, release them.
*---------------------------------------------------------------------------*/
    if (pDesc->writeBlock > 0) {
	pDesc->pWriteBuf->lastByte = pDesc->pWrite -
						(char *)pDesc->pWriteBuf;
	stat = bfWrite(pDesc, pDesc->pWriteBuf, pDesc->writeBlock);
	if (stat != OK) {
	    (void)fprintf(stderr, "bfClose: error writing partial block\n");
	    retStat = ERROR;
	}
    }
    if (pDesc->pWriteBuf != NULL)
	free((char *)pDesc->pWriteBuf);
    if (pDesc->pReadBuf != NULL)
	free((char *)pDesc->pReadBuf);

/*----------------------------------------------------------------------------
*    next, position file at beginning and write block0
*---------------------------------------------------------------------------*/
    if (BfWriteable(pDesc)) {
	offset = 0;
	stat = lseek((int)pDesc->fd, offset, L_SET);
	if (stat >= 0) {
	    stat = write((int)pDesc->fd, (char *)pDesc->pBlock0,
						(int)BfB0BlockSize(pDesc));
	    if (stat != BfB0BlockSize(pDesc)) {
		perror("bfClose: error writing block0");
		retStat = ERROR;
	    }
	}
	else {
	    perror("bfClose: error on lseek");
	    retStat = ERROR;
	}
    }

/*----------------------------------------------------------------------------
*    then close the file
*---------------------------------------------------------------------------*/
    stat = close((int)pDesc->fd);
    if (stat != 0)
	retStat = ERROR;

/*----------------------------------------------------------------------------
*    if the file wasn't read only, delete the lock file
*---------------------------------------------------------------------------*/
    if (BfWriteable(pDesc)) {
	char	lockName[GEN_FNAME_DIM+5];/* name of lock file for write acc.*/

	(void) sprintf(lockName, "%s.lock", pDesc->name);
	if ((stat = unlink(lockName)) != 0) {
	    perror("bfClose: error deleting lock file");
	    retStat = ERROR;
	}
    }

/*----------------------------------------------------------------------------
*    and, finally, free the memory for the descriptor and put NULL in the ptr
*---------------------------------------------------------------------------*/
    if (pDesc->pBlock0 != NULL)
	bfFreeBlock0(pDesc->pBlock0);
    if (pDesc != NULL)
	bfFreeDesc(pDesc);
    *ppDesc = NULL;

    return retStat;
}

/*+/subr**********************************************************************
* NAME	bfCreate - create and initialize a block file
*
* DESCRIPTION
*	Creates and initializes a block file.  If the file already exists,
*	nothing is done.
*
*	The file is created with the specified number of blocks, with all
*	except the first one on the free block list.  The specified type,
*	flags, block size, and maximum file size will be installed in block 0.
*
*	File mode is set to -rw-rw-r--.  Owner and group for the file are
*	set by the operating system.
*
* RETURNS
*	pointer to descriptor, or
*	NULL
*
* BUGS
* o	there is a potential race when creating the file.  This routine
*	checks to see if the file already exists and balks if it does.  But
*	the file could be created between the check and the create done here.
*	Under UNIX, O_CREAT | O_EXCL avoids the race, but VxWorks can't
*	handle O_EXCL.
* o	this routine creates a file by successive writes.  Some file system
*	protocols require creating an empty file of the desired size before
*	any writing is done.
*
* EXAMPLE
*	BF_DESC *pDesc;		pointer to descriptor
*
*	pDesc = bfCreate("myFile", BF_TYPE_RING, 12, 1000, BF_FLAG_GROW, 100);
*	BfB0RingHead(pDesc) = 0;
*	BfB0RingTail(pDesc) = 0;
*	stat = bfClose(pDesc)
*
*-*/
BF_DESC *
bfCreate(name, type, nBlocks, blockSize, fileFlags, maxBlocks)
char	*name;		/* I name of file */
int	type;		/* I file type, one of the BF_TYPE_xxx */
BF_BLKNUM nBlocks;	/* I number of blocks; must be < BF_MAX_BLOCKS */
unsigned blockSize;	/* I size of blocks; must be >= BF_MIN_BLOCK_SIZE
				and <= BF_MAX_BLOCK_SIZE */
long	fileFlags;	/* I flags, one of the BF_FLAG_xxx */
BF_BLKNUM maxBlocks;	/* I size limit in blocks; must be <= BF_MAX_BLOCKS */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status */
    BF_DESC	*pDesc;		/* block file descriptor */
    int		i;		/* temp for loops */
    int		fd;		/* temp for file descriptor */

    assert(type >= (int)BF_TYPE_UNDEF && type < (int)BF_TYPE_PAST);
    assert(nBlocks >= 1 && nBlocks <= BF_MAX_BLOCKS);
    assert(blockSize >= BF_MIN_BLOCK_SIZE && blockSize <= BF_MAX_BLOCK_SIZE);
    assert(maxBlocks >= BF_MIN_BLOCKS && maxBlocks <= BF_MAX_BLOCKS);

    if ((fd = open(name, O_RDWR, 0664)) >= 0) {
	(void)close(fd);
	(void)fprintf(stderr, "bfCreate: can't create--file already exists\n");
	retStat = ERROR;
    }

    if (retStat == OK) {
	pDesc = bfOpen(name, O_RDWR | O_CREAT, type, (int)blockSize);
	if (pDesc == NULL) {
	    perror("bfCreate: error creating file");
	    retStat = ERROR;
	}
    }

    if (retStat == OK) {
/*----------------------------------------------------------------------------
*    set up block 0; special fields are the caller's responsibility
*---------------------------------------------------------------------------*/
	if (nBlocks > 1) {
	    BfB0Flink(pDesc) = 1;
	    BfB0CurrentSize(pDesc) = nBlocks;
	    BfB0CurrentFree(pDesc) = nBlocks - 1;
	}
	BfB0Flags(pDesc) = fileFlags;
	BfB0MaxSize(pDesc) = maxBlocks;

/*----------------------------------------------------------------------------
*    write out block 0.
*
*    write out the free block list.  Each block except the last has a flink
*    to the next; the last block has a flink of 0.
*
*    If an error occurs, the write process is aborted and an error message is
*    printed.
*---------------------------------------------------------------------------*/
	stat = write((int)pDesc->fd, (char *)pDesc->pBlock0, (int)blockSize);
	if (stat != blockSize) {
	    retStat = ERROR;
	    perror("bfCreate: error writing block0");
	}
	if (retStat == OK && nBlocks > 1) {
	    BF_BLOCK *pBuf;

	    pBuf = (BF_BLOCK *)malloc(blockSize);
	    if (pBuf == NULL) {
		retStat = ERROR;
		(void)fprintf(stderr,
			"bfCreate: can't get memory for writing free blocks\n");
	    }
	    else {
		pBuf->firstByte = 0;
		pBuf->lastByte = 0;
		for (i=1; i++<nBlocks; ) {
		    pBuf->flink = (i != nBlocks ? i : 0);
		    stat = write((int)pDesc->fd, (char *)pBuf, (int)blockSize);
		    if (stat != blockSize) {
			retStat = ERROR;
			perror("bfCreate: error writing free block list");
			break;
		    }
		free((char *)pBuf);
		}
	    }
	}
    }

    if (retStat == OK)
	return pDesc;
    else
	return NULL;
}

/*+/subr**********************************************************************
* NAME	bfDumpChar - dump, as text, contents of block from block file
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
int
bfDumpChar(pBfDesc, blockNum)
BF_DESC	*pBfDesc;	/* I pointer to descriptor */
BF_BLKNUM blockNum;	/* I block number */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status to caller */
    char	*pBuf;		/* buffer pointer */

    if ((pBuf = (char *)malloc((unsigned)BfB0BlockSize(pBfDesc))) == NULL) {
	(void)printf("bfDumpHex: can't malloc buffer\n");
	retStat = ERROR;
    }
    if (retStat == OK) {
	if ((stat = bfRead(pBfDesc, (BF_BLOCK *)pBuf, blockNum)) != OK) {
	    (void)printf("bfDumpHex: error reading block\n");
	    retStat = ERROR;
	}
    }
    if (retStat == OK) {
	char *pBuf1;
	char c;
	int i,j,last;

	(void)printf("block:%d flink:%d firstByte:%d lastByte:%d\n",                                blockNum,
		    ((BF_BLOCK *)pBuf)->flink,
		    ((BF_BLOCK *)pBuf)->firstByte,
		    ((BF_BLOCK *)pBuf)->lastByte);
	pBuf1 = pBuf;
	last = ((BF_BLOCK *)pBuf)->lastByte;
	for (i=0; i<=last; ) {
	    (void)printf("%04x \"", i);
	    for (j=0; j<64; j++) {
		(void)printf("%c",
			(isascii(c=pBuf1[i++])&&isprint(c))?c:'.');
		if (i>last)
		    break;
	    }
	    (void)printf("\"\n");
	}
    }
    if (pBuf != NULL)
	(void)free(pBuf);

    return retStat;
}

/*+/subr**********************************************************************
* NAME	bfDumpHex - dump, in hex, contents of block from block file
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
int
bfDumpHex(pBfDesc, blockNum)
BF_DESC	*pBfDesc;	/* I pointer to descriptor */
BF_BLKNUM blockNum;	/* I block number */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status to caller */
    char	*pBuf;		/* buffer pointer */

    if ((pBuf = (char *)malloc((unsigned)BfB0BlockSize(pBfDesc))) == NULL) {
	(void)printf("bfDumpHex: can't malloc buffer\n");
	retStat = ERROR;
    }
    if (retStat == OK) {
	if ((stat = bfRead(pBfDesc, (BF_BLOCK *)pBuf, blockNum)) != OK) {
	    (void)printf("bfDumpHex: error reading block\n");
	    retStat = ERROR;
	}
    }
    if (retStat == OK) {
	char *pBuf1;
	int i,j,last;

	(void)printf("block:%d flink:%d firstByte:%d lastByte:%d\n",                                blockNum,
		    ((BF_BLOCK *)pBuf)->flink,
		    ((BF_BLOCK *)pBuf)->firstByte,
		    ((BF_BLOCK *)pBuf)->lastByte);
	pBuf1 = pBuf;
	last = ((BF_BLOCK *)pBuf)->lastByte;
	for (i=0; i<=last; ) {
	    (void)printf("%04x", i);
	    for (j=0; j<8; j++) {
		(void)printf(" ");
		(void)printf("%02x", ((int)pBuf1[i++])&0xff);
		(void)printf("%02x", ((int)pBuf1[i++])&0xff);
		(void)printf("%02x", ((int)pBuf1[i++])&0xff);
		(void)printf("%02x", ((int)pBuf1[i++])&0xff);
		if (i>last)
		    break;
	    }
	    (void)printf("\n");
	}
    }
    if (pBuf != NULL)
	(void)free(pBuf);

    return retStat;
}

/*+/subr**********************************************************************
* NAME	bfInfo - print information from block0 of block file
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
int
bfInfo(name)
char	*name;		/* I name of file */
{
    int		stat;		/* status from calls */
    int		retStat=OK;	/* return status */
    BF_DESC	*pDesc;		/* block file descriptor */
    long	blockSize;	/* size of blocks in this file */
    off_t	offset;		/* offset into file, for lseek() */
    off_t	lseek();	/* lseek function */
    int		fd;

    if ((pDesc = bfMallocDesc()) == NULL) {
	retStat = ERROR;
	(void)fprintf(stderr, "bfInfo: can't get memory for bfDesc\n");
    }
    if (retStat == OK) {
	if ((pDesc->pBlock0 = bfMallocBlock0()) == NULL) {
	    retStat = ERROR;
	    (void)fprintf(stderr, "bfInfo: can't get memory for block0\n");
	}
    }

    if (retStat == OK) {
	fd = open(name, O_RDWR, 0664);
	pDesc->fd = fd;
	if (pDesc->fd < 0) {
	    perror("bfInfo: error opening file");
	    retStat = ERROR;
	}
    }

/*----------------------------------------------------------------------------
*    first, read only the first few bytes of the record, mostly to find out
*    how big the blocks are.
*---------------------------------------------------------------------------*/
    if (retStat == OK) {
	stat = read((int)pDesc->fd, (char *)pDesc->pBlock0, BF_MIN_BLOCK_SIZE);
	if (stat != BF_MIN_BLOCK_SIZE) {
	    (void)fprintf(stderr, "bfInfo: error reading block0 preamble\n");
	    retStat = ERROR;
	}
	else {
	    blockSize = BfB0BlockSize(pDesc);
	    (void)printf("BLOCK 0: blockSize is %d\n", blockSize);
	}
    }

    if (retStat == OK) {
	offset = 0;
	stat = lseek((int)pDesc->fd, offset, L_SET);
	if (stat == ERROR) {
	    perror("bfInfo: error on lseek");
	    retStat = ERROR;
	}
	if (retStat == OK) {
	    stat = read((int)pDesc->fd, (char *)pDesc->pBlock0, (int)blockSize);
	    if (stat != blockSize) {
		(void)fprintf(stderr, "bfInfo: error reading block0\n");
		retStat = ERROR;
	    }
	    else {
		(void)printf("BLOCK 0: type, flink, fByte, lByte %s %d %d %d\n",
		    BfTypeToText(BfB0FileType(pDesc)),
		    BfB0Flink(pDesc), BfB0FirstByte(pDesc),
		    BfB0LastByte(pDesc));
		(void)printf("maxSize, currSize, currFree %d %d %d\n",
		    BfB0MaxSize(pDesc), BfB0CurrentSize(pDesc),
		    BfB0CurrentFree(pDesc));
		(void)printf("free block list: ");
		while (BfB0Flink(pDesc) != 0) {
		    (void)printf("%d ", BfB0Flink(pDesc));
		    offset = BfB0Flink(pDesc) * blockSize;
		    stat = lseek((int)pDesc->fd, offset, L_SET);
		    if (stat == ERROR) {
			perror("bfInfo: error on lseek");
			retStat = ERROR;
			break;
		    }
		    else {
			stat = read((int)pDesc->fd, (char *)pDesc->pBlock0,
								(int)blockSize);
			if (stat != blockSize) {
			    (void)fprintf(stderr,
					"bfInfo: error reading block%d\n",
							offset/blockSize);
			    break;
			}
		    }
		}
		(void)printf("\n");
	    }
	}
    }


    if (pDesc->fd >= 0)
	(void)close((int)pDesc->fd);
    if (pDesc->pBlock0 != NULL)
	bfFreeBlock0(pDesc->pBlock0);
    if (pDesc != NULL)
	bfFreeDesc(pDesc);

    return retStat;
}

/*+/subr**********************************************************************
* NAME	bfOpen - open or create a block file
*
* DESCRIPTION
*	Open a block file.  The actions taken are:
*	o  if the file type doesn't match the caller's type, return with NULL.
*	   (If the caller's type is BF_TYPE_UNDEF, no type check is made.)
*	o  if the file is being opened O_RDWR, a lock file is created.  (If
*	   the lock file already exists, the open isn't done.)  The lock file
*	   has the same 'name' as the file, but with the extension of .lock
*	   added.
*	o  a block of memory is obtained to hold a file descriptor and block0
*	o  block0 is read
*	o  the file descriptor is set up
*
*	This routine can also be used to create a block file, if the open
*	mode is (O_RDWR & O_CREAT).  In this case, the 'blkSize' argument
*	must have a meaningful value.  The actions taken are:
*	o  a lock file is created, as described above
*	o  the file is created, with flags of 0664 (rw-rw-r--)
*	o  a block of memory is obtained to hold a file descriptor and block0
*	o  an 'empty' block0 is created, with no free blocks and 0 for flags
*	o  the file descriptor is set up
*
* RETURNS
*	pointer to descriptor, or
*	NULL
*
* BUGS
* o	the flags used in creating the file with O_CREAT, and the flags
*	used in creating the lock file, may be system dependent
* o	this routine should probably grab a few of the free blocks and
*	store them into the descriptor, which should have a place to 'store'
*	them.
* o	there is a potential race when locking against multiple writers.
*	Another instance of bfOpen() could check for the existance of the
*	lock file at the same time this one does, both could find the lock
*	file non-existant, and both could 'create' it.
*
* SEE ALSO
*	bfClose(), bfCreate()
*
* EXAMPLE
*	BF_DESC *pDesc;		pointer to file descriptor
*
*	pDesc = bfOpen("myFile", O_RDONLY, BF_TYPE_UNDEF, 0);
*	if (pDesc == NULL) {
*	    printf("error opening: myFile\n");
*	    exit(1);
*	}
*
*-*/
BF_DESC *
bfOpen(name, mode, type, blkSize)
char	*name;		/* I name of file */
int	mode;		/* I mode for open; O_RDWR or O_RDONLY.  O_CREAT 
			can be specified in conjunction with O_RDWR */
int	type;		/* I desired type of file; one of BF_TYPE_xxx */
int	blkSize;	/* I blockSize for new file if mode includes
			O_CREAT; ignored if mode doesn't include O_CREAT */
{
    int		stat;		/* status from calls */
    BF_DESC	*pDesc;		/* block file descriptor */
    char	buffer[BF_MIN_BLOCK_SIZE];	/* 'starter' buffer */
    long	blockSize;	/* size of blocks in this file */
    int		fd;		/* temp for fd from open */
    long	flags;		/* flags for block file descriptor */
    off_t	offset;		/* offset into file, for lseek() */
    off_t	lseek();	/* lseek function */
    char	lockName[GEN_FNAME_DIM+5];/* name of lock file for write acc.*/

    assert(type >= (int)BF_TYPE_UNDEF && type < (int)BF_TYPE_PAST);

/*----------------------------------------------------------------------------
*    first, open the file in the desired mode; if O_RDWR, set the flag
*---------------------------------------------------------------------------*/
    if (strlen(name) >= GEN_FNAME_DIM) {
	(void)fprintf(stderr, "bfOpen: file name too long %s\n", name);
	return NULL;
    }

    if ((mode & O_CREAT) != 0)
	assert(blkSize >= BF_MIN_BLOCK_SIZE && blkSize <= BF_MAX_BLOCK_SIZE);

    if (mode == O_RDONLY)
	flags = 0;
    else if (mode == O_RDWR || mode == (O_RDWR | O_CREAT) ) {
	flags = BF_DESC_WRITE;
	(void) sprintf(lockName, "%s.lock", name);
	if ((fd = open(lockName, O_RDONLY)) >= 0) {
	    (void)close(fd);
	    (void)fprintf(stderr,
			"bfOpen: %s locked against write access\n", name);
	    return NULL;
	}
	else {
	    if ((fd = open(lockName, O_CREAT, 0664)) < 0) {
		perror("bfOpen: error creating write lock file");
		return NULL;
	    }
	    (void)close(fd);
	}
    }
    else {
	(void)fprintf(stderr, "bfOpen: illegal mode\n", name);
	return NULL;
    }
    if ((fd = open(name, mode, 0664)) < 0)
	goto errorExit;

/*----------------------------------------------------------------------------
*    now, find out how big blocks are so we'll know how big to make the buffer
*---------------------------------------------------------------------------*/
    if ((mode & O_CREAT) != 0)
	blockSize = blkSize;
    else {
/*----------------------------------------------------------------------------
*	read block0 from file
*---------------------------------------------------------------------------*/
	stat = read(fd, buffer, BF_MIN_BLOCK_SIZE);
	if (stat != BF_MIN_BLOCK_SIZE) {
	    perror("bfOpen: error reading block0 preamble\n");
	    (void)close(fd);
	    goto errorExit;
	}
	blockSize = ((BF_BLOCK0 *)buffer)->fileInfo.blockSize;
    }

/*----------------------------------------------------------------------------
*    finally, malloc for the descriptor and for block0
*    set up the descriptor
*---------------------------------------------------------------------------*/
    if ((pDesc = bfMallocDesc()) == NULL) {
	(void)fprintf(stderr, "bfOpen: can't get memory for descriptor\n");
	(void)close(fd);
	goto errorExit;
    }
    else {
	if ((pDesc->pBlock0 = bfMallocBlock0()) == NULL) {
	    (void)fprintf(stderr, "bfOpen: can't get memory for block0\n");
	    bfFreeDesc(pDesc);
	    (void)close(fd);
	    goto errorExit;
	}
    }
    pDesc->fd = fd;
    pDesc->flags = flags;
    pDesc->readCount = -10;
    pDesc->readBlock = 0;
    pDesc->pReadBuf = NULL;
    pDesc->writeCount = -10;
    pDesc->writeBlock = 0;
    pDesc->pWriteBuf = NULL;
    (void)strcpy(pDesc->name, name);	/* length already checked above */

/*----------------------------------------------------------------------------
*    now, get block0 into memory.  If we're using O_CREAT, then one must be
*    created; otherwise, read it from the file.
*---------------------------------------------------------------------------*/
    if ((mode & O_CREAT) != 0) {
/*----------------------------------------------------------------------------
*	set up block 0; special fields are the caller's responsibility
*---------------------------------------------------------------------------*/
	BfB0Flink(pDesc) = 0;
	BfB0FirstByte(pDesc) = BF_BLOCK_DATA;
	BfB0LastByte(pDesc) = blockSize - 1;
	BfB0FileType(pDesc) = type;
	BfB0BlockSize(pDesc) = blockSize;
	BfB0Flags(pDesc) = 0;
	BfB0MaxSize(pDesc) = 1;
	BfB0CurrentSize(pDesc) = 1;
	BfB0CurrentFree(pDesc) = 0;
    }
    else {
/*----------------------------------------------------------------------------
*	read block0 from file
*---------------------------------------------------------------------------*/
	offset = 0;
	stat = lseek((int)pDesc->fd, offset, L_SET);
	if (stat == ERROR) {
	    perror("bfOpen: error on lseek");
	    goto errorExit0;
	}
	stat = read((int)pDesc->fd, (char *)pDesc->pBlock0, (int)blockSize);
	if (stat != blockSize) {
	    perror("bfOpen: error reading block0\n");
	    goto errorExit0;
	}
    }

/*----------------------------------------------------------------------------
*    do additional checks
*---------------------------------------------------------------------------*/
    if (type != (int)BF_TYPE_UNDEF && type != BfB0FileType(pDesc)) {
	(void)fprintf(stderr, "bfOpen: file type mismatch for %s\n", name);
	goto errorExit0;
    }

    return pDesc;

errorExit0:
    (void)close(fd);
    bfFreeBlock0(pDesc->pBlock0);
    bfFreeDesc(pDesc);
errorExit:
    if ((flags & BF_DESC_WRITE) != 0) {
	if ((stat = unlink(lockName)) != 0)
	    perror("bfOpen: error deleting lock file");
    }
    return NULL;
}

/*+/subr**********************************************************************
* NAME	bfRead - read a block from a block file
*
* DESCRIPTION
*	Reads a block from the file into the caller's buffer.  (None of the
*	read-related fields in the descriptor are updated; these fields
*	must be set by the caller, if desired.)
*
*	For files of type BF_TYPE_RING, the macro BfRingGetc(pDesc) and the
*	function bfRingFgets(pBuf, bufDim, pDesc) are available as more
*	friendly tools for reading the data from blocks.
*
* RETURNS
*	OK, or
*	ERROR
*
* EXAMPLE
*	BF_DESC *pDesc;		file descriptor
*	BF_BLOCK *pBuf;		buffer pointer
*
*	pDesc = bfOpen("dataFile", O_RDONLY, BF_TYPE_UNDEF);
*	pBuf = (BF_BLOCK *)malloc(BfB0BlockSize(pDesc));
*	stat = bfRead(pDesc, pBuf, 1);
*	printf("contents of block 1:\n");
*	for (i=BF_BLOCK_DATA; i<pBuf->lastByte; i++)
*	    putc(((char *)pBuf)[i]);
*	stat = bfClose(pDesc);
*	free((char *)pBuf);
*
*-*/
int
bfRead(pDesc, pBuf, blockNum)
BF_DESC	*pDesc;		/* IO pointer to descriptor */
BF_BLOCK  *pBuf;	/* IO buffer pointer */
BF_BLKNUM blockNum;	/* I block number */
{
    int		stat;		/* status from calls */
    off_t	offset;		/* offset into file, for lseek() */
    off_t	lseek();	/* lseek function */

    assert(pDesc != NULL);
    assert(pBuf != NULL);
    assert(blockNum >= 0 && blockNum <= BfB0CurrentSize(pDesc));

/*----------------------------------------------------------------------------
*    read the block
*---------------------------------------------------------------------------*/
    offset = blockNum * BfB0BlockSize(pDesc);
    stat = lseek((int)pDesc->fd, offset, L_SET);
    if (stat == ERROR) {
	(void)fprintf(stderr, "bfRead: error on lseek\n");
	return ERROR;
    }
    stat = read((int)pDesc->fd, (char *)pBuf, (int)BfB0BlockSize(pDesc));
    if (stat != BfB0BlockSize(pDesc)) {
	(void)fprintf(stderr, "bfRead: error reading block\n");
	return ERROR;
    }

    return OK;
}

/*+/subr**********************************************************************
* NAME	bfRelease - release a block to the free block list of a block file
*
* DESCRIPTION
*	Adds the caller's block to the head of the free block list.
*
*	The information in the caller's buffer is destroyed.
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	should provide a mechanism whereby no disk write is involved, or
*	at least provide a way to delay the write.
* o	should maintain an 'in memory' list of free blocks and replenish
*	the list as necessary.  This would avoid I/O delays on bfAcquire()
*	calls.
* o	multiple, simultaneous calls to this routine could result in various
*	kinds of scrambling of the free block list.  Callers must presently
*	cooperate to prevent simultaneous calls.
*
* SEE ALSO
*	bfAcquire()
*
*-*/
int
bfRelease(pDesc, pBuf, blockNum)
BF_DESC	*pDesc;		/* IO pointer to descriptor */
BF_BLOCK  *pBuf;	/* IO buffer pointer */
BF_BLKNUM blockNum;	/* I number of the block */
{
    int		stat;		/* status from calls */
    off_t	offset;		/* offset into file, for lseek() */
    off_t	lseek();	/* lseek function */

    assert(pDesc != NULL);
    assert(pBuf != NULL);
    assert(blockNum >= 0 && blockNum <= BfB0CurrentSize(pDesc));

    if (!BfWriteable(pDesc)) {
	(void)fprintf(stderr, "bfRelease: file is O_RDONLY\n");
	return ERROR;
    }

/*----------------------------------------------------------------------------
*    link this block into the free block list, set firstByte and lastByte
*    to 0, write the released block, and free the block's memory
*---------------------------------------------------------------------------*/
    pBuf->flink = BfB0Flink(pDesc);
    pBuf->firstByte = 0;
    pBuf->lastByte = 0;
    offset = blockNum * BfB0BlockSize(pDesc);
    stat = lseek((int)pDesc->fd, offset, L_SET);
    if (stat == ERROR) {
	perror("bfRelease: error on lseek");
	return ERROR;
    }
    stat = write((int)pDesc->fd, (char *)pBuf, (int)BfB0BlockSize(pDesc));
    if (stat != BfB0BlockSize(pDesc)) {
	(void)fprintf(stderr, "bfRelease: error writing released block\n");
	return ERROR;
    }

/*----------------------------------------------------------------------------
*    adjust the free block information in block0
*---------------------------------------------------------------------------*/
    BfB0CurrentFree(pDesc)++;
    BfB0Flink(pDesc) = blockNum;

    return OK;
}

/*+/subr**********************************************************************
* NAME	bfWrite - write a block to a block file
*
* DESCRIPTION
*	The block is written to the file, with the flink, firstByte, and
*	lastByte fields written as set by the caller.
*
*	For files of type BF_TYPE_RING, the macro BfRingPutc(c, pDesc) and the
*	function bfRingFputs(pBuf, pDesc) are available as more
*	friendly tools for writing the data to blocks.
*
* RETURNS
*	OK, or
*	ERROR
*
* EXAMPLE
*	For an example of using bfWrite, see bfAcquire().
*
*-*/
int
bfWrite(pDesc, pBuf, blockNum)
BF_DESC	*pDesc;		/* IO pointer to descriptor */
BF_BLOCK  *pBuf;	/* IO buffer pointer */
BF_BLKNUM blockNum;	/* I number of the block */
{
    int		stat;		/* status from calls */
    off_t	offset;		/* offset into file, for lseek() */
    off_t	lseek();	/* lseek function */

    assert(pDesc != NULL);
    assert(pBuf != NULL);
    assert(blockNum >= 0 && blockNum <= BfB0CurrentSize(pDesc));

    if (!BfWriteable(pDesc)) {
	(void)fprintf(stderr, "bfWrite: file is O_RDONLY\n");
	return ERROR;
    }

/*----------------------------------------------------------------------------
*    write the block and, if keepFlag isn't set, free the buffer
*---------------------------------------------------------------------------*/
    offset = blockNum * BfB0BlockSize(pDesc);
    stat = lseek((int)pDesc->fd, offset, L_SET);
    if (stat == ERROR) {
	perror("bfWrite: error on lseek");
	return ERROR;
    }
    stat = write((int)pDesc->fd, (char *)pBuf, (int)BfB0BlockSize(pDesc));
    if (stat != BfB0BlockSize(pDesc)) {
	(void)fprintf(stderr, "bfWrite: error writing block\n");
	return ERROR;
    }

    return OK;
}
