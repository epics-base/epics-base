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
 * .02  07-04-92	jrw	Moved tty-specific stuff to devTy232.h
 *
 * This file contains the common parts of the RS-232 device support info.
 */

#ifndef	EPICS_DRVRS232_H
#define EPICS_DRVRS232_H

typedef struct {
  int	cmd;
  void	*pparm;
} ioctlCommand;

#define	IOCTL232_BREAK	1

#endif
