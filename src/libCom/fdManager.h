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
 * 	File descriptor management C++ class library
 * 	(for multiplexing IO in a single threaded environment)
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 */

#ifndef fdManagerH_included
#define fdManagerH_included

#include <stdio.h>

#include "shareLib.h" // reset share lib defines
#include "tsDLList.h"
#include "resourceLib.h"
#include "osiTime.h"
#include "osiSock.h"

enum fdRegType {fdrRead, fdrWrite, fdrExcp, fdRegTypeNElem};
enum fdRegState {fdrActive, fdrPending, fdrLimbo};

class epicsShareClass fdRegId  
{
public:
	fdRegId (const SOCKET fdIn, const fdRegType typeIn) :
		fd(fdIn), type(typeIn) {}

	SOCKET getFD ()
	{
		return this->fd;
	}

	fdRegType getType ()
	{
		return this->type;
	}

	int operator == (const fdRegId &idIn)
	{
	    return this->fd == idIn.fd && this->type==idIn.type;
	}

	resTableIndex resourceHash (unsigned nBitsId) const;

	virtual void show (unsigned level) const;
private:
	const SOCKET	fd;
	const fdRegType	type;
};

//
// fdReg
// file descriptor registration
//
class fdReg : public tsDLNode<fdReg>, public fdRegId, public tsSLNode<fdReg> {
        friend class fdManager;
public:
	epicsShareFunc fdReg (const SOCKET fdIn, const fdRegType typ, 
			const unsigned onceOnly=0);
	epicsShareFunc virtual ~fdReg ();

	epicsShareFunc virtual void show(unsigned level) const;
	
	//
	// Called by the file descriptor manager:
	// 1) If the fdManager is deleted and there are still 
	// fdReg objects attached
	// 2) Immediately after calling "callBack()" if
	// the constructor specified "onceOnly" 
	//
	// fdReg::destroy() does a "delete this"
	//
	epicsShareFunc virtual void destroy ();
private:

	//
	// called when there is activity on the fd
	// NOTES
	// 1) the fdManager will call this only once during the
	// lifetime of a fdReg object if the constructor
	// specified "onceOnly"
	//
	epicsShareFunc virtual void callBack ()=0;

	unsigned char 	state; // fdRegState goes here
	unsigned char	onceOnly;
};

//
// fdManager
// file descriptor manager
//
class fdManager {
friend class fdReg;
public:
	epicsShareFunc fdManager();
	epicsShareFunc ~fdManager();
	epicsShareFunc void process (const osiTime &delay);

	//
	// returns NULL if the fd is unknown
	//
	epicsShareFunc fdReg *lookUpFD(const SOCKET fd, const fdRegType type);
private:
	tsDLList<fdReg>	regList;
	tsDLList<fdReg>	activeList;
	resTable<fdReg,fdRegId> fdTbl;
	fd_set		fdSets[fdRegTypeNElem];

	int             maxFD;
	unsigned	processInProg;
	//
	// Set to fdreg when in call back
	// and nill otherwise
	//
	fdReg		*pCBReg; 

	epicsShareFunc void installReg (fdReg &reg);
	void removeReg (fdReg &reg);
};

epicsShareExtern fdManager fileDescriptorManager;

//
// fdManagerMaxInt ()
//
inline int fdManagerMaxInt (int a, int b)
{
        if (a>b) {
                return a;
        }
        else {
                return b;
        }
}
 
#endif // fdManagerH_included
 
