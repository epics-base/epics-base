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
 *
 */

#ifndef fdManagerH_included
#define fdManagerH_included

#include <tsDLList.h>
#include <osiTime.h>

#ifdef WIN32
#include <winsock.h>
#else
extern "C" {
#	include <sys/types.h>
#	include <sys/time.h>
} // extern "C"
#endif

#include <stdio.h>

enum fdRegType {fdrRead, fdrWrite, fdrExcp};
enum fdRegState {fdrActive, fdrPending, fdrLimbo};

//
// fdReg
// file descriptor registration
//
class fdReg : public tsDLNode<fdReg> {
        friend class fdManager;
public:
	fdReg (const int fdIn, const fdRegType typ, 
			const unsigned onceOnly=0);
	virtual ~fdReg ();

	virtual void show(unsigned level);
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

        const int	fd;
	fd_set		&fdSet;
	unsigned char 	state; // fdRegState goes here
	unsigned char	onceOnly;
};
 
class fdManager {
friend class fdReg;
public:
        fdManager();
        ~fdManager();
        void process (const osiTime &delay);
private:
        tsDLList<fdReg>	regList;
        tsDLList<fdReg>	activeList;
	fd_set		read;
	fd_set		write;
	fd_set		exception;
        int             maxFD;
	unsigned	processInProg;
	//
	// Set to fdreg when in call back
	// and nill ortherwise
	//
	fdReg		*pCBReg; 

	void installReg (fdReg &reg);
        void removeReg (fdReg &reg);
	fd_set *pFDSet (fdRegType typIn);
};

extern fdManager fileDescriptorManager;


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
// fdManager::pFDSet()
//
inline fd_set *fdManager::pFDSet (fdRegType typIn)
{
	fd_set *pSet;

	switch (typIn) {
	case fdrRead:
		pSet = &this->read;
		break;
	case fdrWrite:
		pSet = &this->write;
		break;
	case fdrExcp:
		pSet = &this->exception;
		break;
	default:
		assert(0);
	}
	return pSet;
}

//
// fdManager::installReg()
//
inline void fdManager::installReg (fdReg &reg)
{
       	this->maxFD = fdManagerMaxInt(this->maxFD, reg.fd+1);
       	this->regList.add(reg);
	reg.state = fdrPending;
}
 
//
// fdManager::removeReg()
//
inline void fdManager::removeReg(fdReg &reg)
{
        //
        // signal fdManager that the fdReg was deleted
        // during the call back
        //
        if (this->pCBReg == &reg) {
                this->pCBReg = 0;
        }
        FD_CLR(reg.fd, &reg.fdSet);
	switch (reg.state) {
	case fdrActive:
        	this->activeList.remove(reg);
		reg.state = fdrLimbo;
		break;
	case fdrPending:
        	this->regList.remove(reg);
		reg.state = fdrLimbo;
		break;
	case fdrLimbo:
		break;
	default:
		assert(0);
	}
}

//
// fdReg::fdReg()
//
inline fdReg::fdReg (const int fdIn, const fdRegType typIn, 
		const unsigned onceOnlyIn) : 
	fd(fdIn), fdSet(*fileDescriptorManager.pFDSet(typIn)),
	state(fdrLimbo), onceOnly(onceOnlyIn)
{
	assert (this->fd>=0);
        if (this->fd>FD_SETSIZE) {
		fprintf (stderr, "%s: fd > FD_SETSIZE ignored\n", 
			__FILE__);
		return;
	}
	fileDescriptorManager.installReg(*this);
}

#endif // fdManagerH_included
 
