/*
 *	$Id$
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
 * Revision 1.2  1997/04/10 19:33:58  jhill
 * API changes
 *
 * Revision 1.1  1996/11/02 01:01:05  jhill
 * installed
 *
 *
 *
 */

//
// EPICS
// (some of these are included from casdef.h but
// are included first here so that they are included
// once only before epicsExportSharedSymbols is defined)
//
#include "alarm.h"		// EPICS alarm severity/condition 
#include "errMdef.h"	// EPICS error codes 
#include "gdd.h" 		// EPICS data descriptors 

#define epicsExportSharedSymbols
#include "casdef.h"

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncReadIO::~casAsyncReadIO()
{
}

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncWriteIO::~casAsyncWriteIO()
{
}

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncPVExistIO::~casAsyncPVExistIO()
{
}

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncPVCreateIO::~casAsyncPVCreateIO()
{
}

