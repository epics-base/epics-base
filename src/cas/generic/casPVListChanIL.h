/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
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
 *
 * History
 * $Log$
 * Revision 1.5  1997/04/10 19:34:17  jhill
 * API changes
 *
 * Revision 1.4  1997/01/10 21:17:59  jhill
 * code around gnu g++ inline bug when -O isnt used
 *
 * Revision 1.3  1997/01/09 22:29:17  jhill
 * installed hostBuild branch
 *
 * Revision 1.2  1996/12/06 22:36:20  jhill
 * use destroyInProgress flag now functional nativeCount()
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casPVListChanIL_h
#define casPVListChanIL_h

#include "casPVIIL.h"

//
// empty for now since casPVListChan::casPVListChan()
// causes undefined sym when it is inline under g++ 2.7.x
// (without -O)
//

#endif // casPVListChanIL_h

