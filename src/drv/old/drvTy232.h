/* share/epicsH $Id$ */
/*
 *      Author: John Winans
 *      Date:   04-16-92
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1988, 1989, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * All rights reserved. No part of this publication may be reproduced,
 * stored in a retrieval system, transmitted, in any form or by any
 * means,  electronic, mechanical, photocopying, recording, or otherwise
 * without prior written permission of Los Alamos National Laboratory
 * and Argonne National Laboratory.
 *
 * Modification Log:
 * -----------------
 * .01	04-16-92	jrw	Initial release
 */

#ifndef	EPICS_DEVTY232_H
#define EPICS_DEVTY232_H

/******************************************************************************
 *
 * Additional fields needed for the msgParmBlock structure.
 *
 ******************************************************************************/
typedef struct {
  int   timeWindow;             /* Clock ticks to skip after a timeout */
  int   dmaTimeout;             /* Clock ticks to wait for DMA to complete */
  int	flags;			/* set to FALSE if does NOT echo characters */
  int	eoi;			/* eoi char value or -1 if none */
  int	baud;			/* baud rate to run the interface */
  int	ttyOptions;		/* ioctl options for the serial port */
} devTy232ParmBlock;

#define	ECHO	1		/* Device echos characters sent to it */
		/* The CRLF option is only valid when ECHO is set */
#define	CRLF	2		/* Device does CR -> CR LF expansion */
#define	KILL_CRLF 4		/* Remove all CR and LF characters from input */

typedef struct {
  CALLBACK	callback;	/* Used to do the ioctl when the dog wakes up */
  WDOG_ID	doggie;		/* For I/O timing */
  int		dogAbort;	/* Used to flag a timeout */
  int		link;		/* The tty card/link number */
  int		port;		/* The tty port number on that card */
  int		ttyFd;		/* The open file descriptor */
	/* The pparmBlock is used to make sure only 1 device type is requested */
	/* on one single port. */
  msgParmBlock	*pparmBlock;
} devTy232Link;

#ifndef DRVTY232_C
extern
#endif
msgDrvBlock drv232Block;

#endif
