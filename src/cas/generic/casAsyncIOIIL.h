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
	client.lock();
}

//
// void casAsyncIOI::unlock()
//
inline void casAsyncIOI::unlock()
{
	client.unlock();
}

#endif // casAsyncIOIIL_h

