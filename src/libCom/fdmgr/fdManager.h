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
 * Revision 1.2  1996/09/04 21:50:16  jhill
 * added hashed fd to fdi convert
 *
 * Revision 1.1  1996/08/13 22:48:21  jhill
 * dfMgr =>fdManager
 *
 *
 */

#ifndef fdManagerH_included
#define fdManagerH_included

#include <stdio.h>

#include "tsDLList.h"
#include "resourceLib.h"
#include "osiTime.h"
#include "osiSock.h"

enum fdRegType {fdrRead, fdrWrite, fdrExcp, fdRegTypeNElem};
enum fdRegState {fdrActive, fdrPending, fdrLimbo};

class fdRegId  
{
public:
	fdRegId (const int fdIn, const fdRegType typeIn) :
		fd(fdIn), type(typeIn) {}

	int getFD()
	{
		return this->fd;
	}

	fdRegType getType()
	{
		return this->type;
	}

        int operator == (const fdRegId &idIn)
        {
                return this->fd == idIn.fd && this->type==idIn.type;
        }

        resTableIndex resourceHash(unsigned nBitsId) const
        {
                unsigned        src = (unsigned) this->fd;
                resTableIndex   hashid;
 
                hashid = src;
                src = src >> nBitsId;
                while (src) {
                        hashid = hashid ^ src;
                        src = src >> nBitsId;
                }
		hashid = hashid ^ this->type;

                //
                // the result here is always masked to the
                // proper size after it is returned to the resource class
                //
                return hashid;
        }

	virtual void show(unsigned level) const;
private:
        const int	fd;
	const fdRegType	type;
};

//
// fdReg
// file descriptor registration
//
class epicsShareClass fdReg : public tsDLNode<fdReg>, public fdRegId,
	public tsSLNode<fdReg> {
        friend class fdManager;
public:
	inline fdReg (const int fdIn, const fdRegType typ, 
			const unsigned onceOnly=0);
	virtual ~fdReg ();

	virtual void show(unsigned level) const;
private:

        //
        // called when there is activity on the fd
	// NOTES
	// 1) the fdManager will call this only once during the
	// lifetime of a fdReg object if the constructor
	// specified "onceOnly"
        //
        virtual void callBack ()=0;

	//
	// Called by the file descriptor manager:
	// 1) If the fdManager is deleted and there are still 
	// fdReg objects attached
	// 2) Immediately after calling "callBack()" if
	// the constructor specified "onceOnly" 
	//
	// fdReg::destroy() does a "delete this"
	//
	virtual void destroy ();

	unsigned char 	state; // fdRegState goes here
	unsigned char	onceOnly;
};
 
class epicsShareClass fdManager {
friend class fdReg;
public:
        fdManager();
        ~fdManager();
        void process (const osiTime &delay);

	//
	// returns NULL if the fd is unknown
	//
	inline fdReg *lookUpFD(const int fd, const fdRegType type);
private:
        tsDLList<fdReg>	regList;
        tsDLList<fdReg>	activeList;
	fd_set		fdSets[fdRegTypeNElem];
        int             maxFD;
	unsigned	processInProg;
	//
	// Set to fdreg when in call back
	// and nill ortherwise
	//
	fdReg		*pCBReg; 

	inline void installReg (fdReg &reg);
        inline void removeReg (fdReg &reg);
	resTable<fdReg,fdRegId> fdTbl;
};

epicsShareExtern fdManager fileDescriptorManager;

//
// lookUpFD()
//
inline fdReg *fdManager::lookUpFD(const int fd, const fdRegType type)
{
	if (fd<0) {
		return NULL;
	}
	fdRegId id (fd,type);
	return this->fdTbl.lookup(id); 
}

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

//
// fdManager::installReg()
//
inline void fdManager::installReg (fdReg &reg)
{
	int status;

       	this->maxFD = fdManagerMaxInt(this->maxFD, reg.getFD()+1);
       	this->regList.add(reg);
	reg.state = fdrPending;
	status = this->fdTbl.add(reg);
	if (status) {
		fprintf (stderr, 
			"**** Warning - duplicate fdReg object\n");
		fprintf (stderr, 
			"**** will not be seen by fdManager::lookUpFD()\n");
	}
}
 
//
// fdManager::removeReg()
//
inline void fdManager::removeReg(fdReg &reg)
{
	fdReg *pItemFound;

        //
        // signal fdManager that the fdReg was deleted
        // during the call back
        //
        if (this->pCBReg == &reg) {
                this->pCBReg = 0;
        }
        FD_CLR(reg.getFD(), &this->fdSets[reg.getType()]);
	pItemFound = this->fdTbl.remove(reg);
	assert (pItemFound==&reg);
	switch (reg.state) {
	case fdrActive:
        	this->activeList.remove(reg);
		break;
	case fdrPending:
        	this->regList.remove(reg);
		break;
	case fdrLimbo:
		break;
	default:
		assert(0);
	}
	reg.state = fdrLimbo;
}

//
// fdReg::fdReg()
//
inline fdReg::fdReg (const int fdIn, const fdRegType typIn, 
		const unsigned onceOnlyIn) : 
	fdRegId(fdIn,typIn), state(fdrLimbo), onceOnly(onceOnlyIn)
{
	assert (fdIn>=0);
        if (!FD_IN_FDSET(fdIn)) {
		fprintf (stderr, "%s: fd > FD_SETSIZE ignored\n", 
			__FILE__);
		return;
	}
	fileDescriptorManager.installReg(*this);
}

#endif // fdManagerH_included
 
