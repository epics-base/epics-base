#ifndef INCgsd_sync_defsh
#define INCgsd_sync_defsh
/*	$Id$
 *	Author:	Gale Slentz
 *	Date:	09-18-90
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
 * .01	09-18-90	ges	initial version
 *
 */
/*+/mod***********************************************************************
* TITLE	gsd_sync_defs.h - defines for gsd_sync.c
*
* DESCRIPTION
*	Contains the definitions that a user of gsd_sync.c
*	(for silo of synchronous data) would need to use.
*
*-***************************************************************************/
#define	PREVIOUS_SYNC_DATA	0
#define	NEXTSET_SYNC_DATA	1
#define USEC_TIME_OUT		100	/* struct timeval's timeout 100usecs */
#define VOID			void

struct gsd_sync_data {
	char *pName;	/* ptr to channel name string */
	chid pChid;	/* Channel Access id */
	chtype time_type;	/* DBR_TIME_xxxx type specifier for data */
	int count;	/* element count */
	int svalid;	/* valid data flag */
	VOID *pSdata;	/* ptr to DBR_TIME_XXXX structure */
};

#endif /* INCgsd_sync_defsh */
