/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef INCgsd_sync_defsh
#define INCgsd_sync_defsh
/*	$Id$
 *	Author:	Gale Slentz
 *	Date:	09-18-90
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
