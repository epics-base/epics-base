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
 * Revision 1.3  1996/09/16 18:23:58  jhill
 * vxWorks port changes
 *
 * Revision 1.2  1996/09/04 20:16:24  jhill
 * moved operator -> here
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casAsyncIOIIL_h
#define casAsyncIOIIL_h

//
// void casAsyncIOI::lock()
//
// NOTE:
// If this changed to use another lock other than the one in the
// client structure then the locking in casAsyncIOI::cbFunc()
// must be changed
//
inline void casAsyncIOI::lock()
{
	client.osiLock();
}

//
// void casAsyncIOI::unlock()
//
inline void casAsyncIOI::unlock()
{
	client.osiUnlock();
}

//
// casAsyncIO * casAsyncIOI::operator -> ()
//
inline casAsyncIO * casAsyncIOI::operator -> ()
{
	return &this->ioExternal;
}

//
// void casAsyncIOI::destroy ()
//
inline void casAsyncIOI::destroy ()
{
	this->serverDelete = TRUE;
	(*this)->destroy();
}

#endif // casAsyncIOIIL_h

