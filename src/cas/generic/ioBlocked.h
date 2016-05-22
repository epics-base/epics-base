
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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef ioBlockedh
#define ioBlockedh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_ioBlockedh
#   undef epicsExportSharedSymbols
#endif

#include "tsDLList.h"

#ifdef epicsExportSharedSymbols_ioBlockedh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class ioBlocked : public tsDLNode < ioBlocked > {
friend class ioBlockedList;
public:
	ioBlocked ();
	virtual ~ioBlocked ();
    bool isBlocked ();
private:
	class ioBlockedList * pList;
	virtual void ioBlockedSignal ();
};

class ioBlockedList : private tsDLList<ioBlocked> {
friend class ioBlocked;
public:
	ioBlockedList ();
	virtual ~ioBlockedList ();
	void signal ();
	void addItemToIOBLockedList ( ioBlocked & item );
	ioBlockedList ( const ioBlockedList & );
	ioBlockedList & operator = ( const ioBlockedList & );
};

inline bool ioBlocked :: isBlocked ()
{
    return this->pList != NULL;
}

#endif // ioBlockedh
 
