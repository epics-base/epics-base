/* share/epicsH $Id$ */
/*
 *      Author: John Winans
 *      Date:   5-21-92
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
 * .01	05-21-92	jrw	Initial release
 * .02	02-19-92	joh	cpu independent clk rate
 */

#ifndef	DRVBB232_H
#define DRVBB232_H

/******************************************************************************
 *
 * Additional fields needed for the msgParmBlock structure.
 *
 ******************************************************************************/
typedef struct {
  int   dmaTimeout;             /* Clock ticks to wait for DMA to complete */
  int	baud;			/* baud rate to run the interface */
} drvBB232ParmBlock;

typedef struct {
  int		link;		/* The BB card/link number */
  int		node;		/* the bug's node number */
  int		port;		/* The tty port number on that card */
	/* The pparmBlock is used to make sure only 1 device type is requested */
	/* on one single port. */
  msgParmBlock	*pparmBlock;
} drvBB232Link;

#ifndef DRVBB232_C
extern
#endif
msgDrvBlock drvBB232Block;

#define	BB232_DEFAULT_AGE	(sysClkRateGet())	/* 1 second */

#define BB_232_EOI		0x20
#endif
