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
 * Revision 1.4  1997/06/13 09:15:54  jhill
 * connect proto changes
 *
 * Revision 1.3  1997/04/10 19:33:58  jhill
 * API changes
 *
 * Revision 1.2  1996/11/02 00:54:01  jhill
 * many improvements
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */


#include "server.h"
#include "casEventSysIL.h" // casEventSys in line func

//
// casChanDelEv()
//
casChanDelEv::~casChanDelEv()
{
}

//
// casChanDelEv()
//
caStatus casChanDelEv::cbFunc(casEventSys &eSys)
{
	caStatus status;
	status = eSys.getCoreClient().disconnectChan(this->id);
	if (status == S_cas_success) {
		delete this;
	}
	return status;
}

