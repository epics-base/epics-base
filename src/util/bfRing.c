/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	03-09-90
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
 * .01	03-09-90	rac	initial version
 * .02	07-30-91	rac	installed in SCCS
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	bfRing.c - fixed size ring file 'manager' for text
*
* DESCRIPTION
*	
*-***************************************************************************/

#include <bfDefs.h>

#ifdef vxWorks
#   include <vxWorks.h>
#   include <ioLib.h>		/* for O_RDWR and O_RDONLY definitions */
#   include <stdioLib.h>
#else
#   include <stdio.h>
#   include <sys/file.h>	/* for O_RDWR and O_RDONLY definitions */
#endif


#ifndef vxWorks
    main(argc, argv)
    int     argc;		/* number of command line args */
    char   *argv[];		/* command line args */
    {
	int	stat=OK;	/* status */
	char	*name;		/* pathname to file */
	char	*command;	/* command */
	int	nBytes=0;	/* number of bytes for file size */

	if (argc >= 3) {	/* must be command and >= 2 arg */
	    name = argv[1];
	    command = argv[2];
	}
	else
	    stat = ERROR;
	if (strcmp(command, "create") == 0) {
	    if (argc == 4) {
		if (sscanf(argv[3], "%d", &nBytes) != 1)
		    stat = ERROR;
	    }
	    else if (argc != 3)
		stat = ERROR;
	}
	if (stat == OK) {
	    stat = bfRing(name, command, nBytes);
	}
	if (stat != OK) {
	    printf("Usage: %s name command [nBytes]\n", argv[0]);
	    exit(1);
	}
	exit(0);
    }
#endif

/*+/subr**********************************************************************
* NAME	bfRing - implement ring files
*
* DESCRIPTION
*	Provide a 'shell' for using ring files
*
* RETURNS
*	OK, or
*	ERROR
*
*-*/
int
bfRing(name, command, nBytes)
char	*name;		/* file name */
char	*command;	/* command */
int	nBytes;		/* number of bytes, for 'create' */
{
    char        message[80];    /* temp for status message */
    int         stat;           /* status return */

    BF_DESC	*pDesc=NULL;	/* pointer to block file descriptor */

    BF_BLOCK	*pBuf;		/* buffer for last read or acquired block */
    long	blockNum;	/* blockNum of last read or acquired block */
    long	nBlocks;	/* number of blocks to create */
    long	blockSize;	/* size of blocks */

    if (strlen(name) >= GEN_FNAME_DIM) {
	printf("bfRing: file name too long\n");
	return ERROR;
    }

    if (strcmp(command, "create") == 0) {
/*----------------------------------------------------------------------------
* create - create and initialize a named block file for a ring file; the
*	file will be a BF_FLAG_GROW file, with no free blocks.
*---------------------------------------------------------------------------*/
	int	i;
	long	fileFlags;	/* flags */
	long	maxBlocks;	/* size limit */

	blockSize = 1000;
	if (nBytes < blockSize*2) {
	    printf("nBytes must be at least %d\n", blockSize*2);
	    return ERROR;
	}
	nBlocks = 1 + (nBytes-1)/blockSize; /* size for user info + block0 */

	pDesc = bfCreate(name, BF_TYPE_RING,
				1, blockSize, BF_FLAG_GROW, nBlocks);
	stat = bfClose(&pDesc);
    }
    else if (strcmp(command, "info") == 0) {
/*----------------------------------------------------------------------------
* info - print some information about the file
*---------------------------------------------------------------------------*/
        pDesc = bfOpen(name, O_RDONLY, BF_TYPE_RING, 0);
	if (pDesc == NULL) {
	    printf("bfRing: can't open %s\n", name);
	    return ERROR;
	}
	printf("bfRing: head, tail = %d %d\n",
		BfB0RingHead(pDesc), BfB0RingTail(pDesc));
    }
    else if (strcmp(command, "retrieve") == 0) {
/*----------------------------------------------------------------------------
* retrieve - read the information from the file and write to stdout
*---------------------------------------------------------------------------*/
	char	record[80];

        pDesc = bfOpen(name, O_RDONLY, BF_TYPE_RING, 0);
	if (pDesc == NULL) {
	    printf("bfRing: can't open %s\n", name);
	    return ERROR;
	}

	while (bfRingFgets(record, 80, pDesc) != NULL)
	    fputs(record, stdout);

	stat = bfClose(&pDesc);
    }
    else if (strcmp(command, "store") == 0) {
/*----------------------------------------------------------------------------
* store - store information from stdin into the file
*
* BUGS
* o	need to address races between storing and retrieving
*---------------------------------------------------------------------------*/
	int	i;
	long	flink;
	char	c;
	char	record[80];

        pDesc = bfOpen(name, O_RDWR, BF_TYPE_RING, 0);
	if (pDesc == NULL) {
	    printf("bfRing: can't open %s\n", name);
	    return ERROR;
	}

	while (fgets(record, 80, stdin) != NULL) {
	    if (bfRingFputs(record, pDesc) == EOF) {
		printf("error on bfRingFputs()\n");
		break;
	    }
	}

	stat = bfClose(&pDesc);
    }
    else if (strcmp(command, "unlock") == 0) {
/*----------------------------------------------------------------------------
* unlock - remove the file write lock for the file
*---------------------------------------------------------------------------*/
	if (strlen(name) > 0) {
	    char lockName[GEN_FNAME_DIM+5]; /* name of lock file */

	    sprintf(lockName, "%s.lock", name);
	    if ((stat = unlink(lockName)) != 0)
		perror("bfRing: removing lock file");
	}
	else {
	    printf("syntax error; ");
	    printf("usage: bfRing name unlock\n");
	}
    }
/*----------------------------------------------------------------------------
*
*---------------------------------------------------------------------------*/
    else {
	printf("Legal commands are:\n");
	printf("	name create [nBytes]\n");
	printf("	name info\n");
	printf("	name retrieve\n");
	printf("	name store <file\n");
	printf("	name unlock\n");
    }

    printf("\n");
    return OK;
}
