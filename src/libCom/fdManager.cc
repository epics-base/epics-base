/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// $Id$
//
//

//
//
// NOTES: 
// 1) This library is not thread safe
//
//

//
// ANSI C
//
#include <assert.h>
#include <errno.h>
#include <string.h>

//	Both the functions in osiTimer and fdManager are
//	implemented in this DLL -> define epicsExportSharesSymbols
#define epicsExportSharedSymbols
#define instantiateRecourceLib
#include "osiTimer.h"
#include "fdManager.h"
#include "osiSleep.h"
#include "bsdSocketResource.h"
 
//
// if the compiler supports explicit instantiation of
// template member functions
//
#if defined(EXPL_TEMPL)
        //
        // From Stroustrups's "The C++ Programming Language"
        // Appendix A: r.14.9
        //
        // This explicitly instantiates the template class's member
        // functions used by fdManager 
        //
		//
		// instantiated by "fdManager fileDescriptorManager;" statement below?
		// (according to ms vis C++)
		//
        template class resTable <fdReg, fdRegId>;
#endif

epicsShareDef fdManager fileDescriptorManager;

//
// fdManager::fdManager()
//
epicsShareFunc fdManager::fdManager()
{
	size_t	i;

	assert (bsdSockAttach());

	for (i=0u; i<sizeof(this->fdSets)/sizeof(this->fdSets[0u]); i++) {
		FD_ZERO(&this->fdSets[i]);
	}
	this->maxFD = 0;
	this->processInProg = 0u;
	this->pCBReg = 0;
	//
	// should throw an exception here 
	// when most compilers are implementing
	// exceptions
	//
	assert (this->fdTbl.init(0x100)>=0);
}

//
// fdManager::~fdManager()
//
epicsShareFunc fdManager::~fdManager()
{
	fdReg	*pReg;

	while ( (pReg = this->regList.get()) ) {
		pReg->state = fdrLimbo;
		pReg->destroy();
	}
	while ( (pReg = this->activeList.get()) ) {
		pReg->state = fdrLimbo;
		pReg->destroy();
	}
	bsdSockRelease();
}

//
// fdManager::process()
//
epicsShareFunc void fdManager::process (const osiTime &delay)
{
	static const tsDLIterBD<fdReg> eol; // end of list
	tsDLIterBD<fdReg> iter;
	tsDLIterBD<fdReg> tmp;
	osiTime minDelay;
	osiTime zeroDelay;
	fdReg *pReg;
	struct timeval tv;
	int status;
	int ioPending = 0;

	//
	// no recursion 
	//
	if (this->processInProg) {
		return;
	}
	this->processInProg = 1;

	//
	// One shot at expired timers prior to going into
	// select. This allows zero delay timers to arm
	// fd writes. We will never process the timer queue
	// more than once here so that fd activity get serviced
	// in a reasonable length of time.
	//
	minDelay = staticTimerQueue.delayToFirstExpire();
	if (zeroDelay>=minDelay) {
		staticTimerQueue.process();
		minDelay = staticTimerQueue.delayToFirstExpire();
	} 

	if (minDelay>=delay) {
		minDelay = delay;
	}

	for (iter=this->regList.first(); iter!=eol; ++iter) {
		FD_SET(iter->getFD(), &this->fdSets[iter->getType()]); 
		ioPending = 1;
	}

	tv.tv_sec = minDelay.getSecTruncToLong ();
	tv.tv_usec = minDelay.getUSecTruncToLong ();

	/*
 	 * win32 requires this (others will
	 * run faster with this installed)
	 */
	if (!ioPending) {
		/*
		 * recover from subtle differences between
		 * windows sockets and UNIX sockets implementation
		 * of select()
		 */
		if (tv.tv_sec!=0 || tv.tv_usec!=0) {
			osiSleep (tv.tv_sec, tv.tv_usec);
		}
		status = 0;
	}
	else {
		status = select (this->maxFD, &this->fdSets[fdrRead], 
			&this->fdSets[fdrWrite], &this->fdSets[fdrExcp], &tv);
	}

	staticTimerQueue.process();
	if (status==0) {
		this->processInProg = 0;
		return;
	}
	else if (status<0) {
		if (SOCKERRNO != SOCK_EINTR) {
			fprintf(stderr, 
			"fdManager: select failed because errno=%d=\"%s\"\n",
				SOCKERRNO, SOCKERRSTR);
		}
		this->processInProg = 0;
		return;	
    }

	//
	// Look for activity
	//
	iter=this->regList.first();
	while (iter!=eol) {
		tmp = iter;
		tmp++;
		if (FD_ISSET(iter->getFD(), &this->fdSets[iter->getType()])) {
			FD_CLR(iter->getFD(), &this->fdSets[iter->getType()]);
			this->regList.remove(*iter);
			this->activeList.add(*iter);
			iter->state = fdrActive;
		}
		iter=tmp;
	}

	//
	// I am careful to prevent problems if they access the
	// above list while in a "callBack()" routine
	//
	while ( (pReg = this->activeList.get()) ) {
		pReg->state = fdrLimbo;

		//
		// Tag current reg so that we
		// can detect if it was deleted 
		// during the call back
		//
		this->pCBReg = pReg;
		pReg->callBack();
		if (this->pCBReg != NULL) {
			//
			// check only after we see that it is non-null so
			// that we dont trigger bounds-checker dangling pointer 
			// error
			//
			assert (this->pCBReg==pReg);
			this->pCBReg = 0;
			if (pReg->onceOnly) {
				pReg->destroy();
			}
			else {
				this->regList.add(*pReg);
				pReg->state = fdrPending;
			}
		}
	}
	this->processInProg = 0;
}

//
// fdReg::destroy()
// (default destroy method)
//
epicsShareFunc void fdReg::destroy()
{
	delete this;
}

//
// fdReg::~fdReg()
//
epicsShareFunc fdReg::~fdReg()
{
	fileDescriptorManager.removeReg(*this);
}

//
// fdReg::show()
//
epicsShareFunc void fdReg::show(unsigned level) const
{
	printf ("fdReg at %p\n", (void *) this);
	if (level>1u) {
		printf ("\tstate = %d, onceOnly = %d\n",
			this->state, this->onceOnly);
	}
	this->fdRegId::show(level);
}

//
// fdRegId::show()
//
void fdRegId::show(unsigned level) const
{
	printf ("fdRegId at %p\n", this);
	if (level>1u) {
		printf ("\tfd = %d, type = %d\n",
			this->fd, this->type);
	}
}

//
// fdRegId::resourceHash()
//
resTableIndex fdRegId::resourceHash (unsigned) const
{
	resTableIndex hashid = (unsigned) this->fd;

	//
	// This assumes worst case hash table index width of 1 bit.
	// We will iterate this loop 5 times on a 32 bit architecture.
	//
	// A good optimizer will unroll this loop?
	// Experiments using the microsoft compiler show that this isnt 
	// slower than switching on the architecture size and urolling the
	// loop explicitly (that solution has resulted in portability
	// problems in the past).
	//
	for (unsigned i=(CHAR_BIT*sizeof(unsigned))/2u; i>0u; i >>= 1u) {
		hashid ^= (hashid>>i);
	}

	//
	// evenly distribute based on the type of interest also
	//
	hashid ^= this->type;

	//
	// the result here is always masked to the
	// proper size after it is returned to the resource class
	//
	return hashid;
}

//
// fdManager::installReg()
//
epicsShareFunc void fdManager::installReg (fdReg &reg)
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
void fdManager::removeReg(fdReg &reg)
{
	fdReg *pItemFound;

	pItemFound = this->fdTbl.remove(reg);
	if (pItemFound!=&reg) {
		fprintf(stderr, 
			"fdManager::removeReg() bad fd registration object\n");
		return;
	}

	//
	// signal fdManager that the fdReg was deleted
	// during the call back
	//
	if (this->pCBReg == &reg) {
		this->pCBReg = 0;
	}
	
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
		//
		// here if memory corrupted
		//
		assert(0);
	}
	reg.state = fdrLimbo;

	FD_CLR(reg.getFD(), &this->fdSets[reg.getType()]);
}

//
// lookUpFD()
//
epicsShareFunc fdReg *fdManager::lookUpFD(const SOCKET fd, const fdRegType type)
{
	if (fd<0) {
		return NULL;
	}
	fdRegId id (fd,type);
	return this->fdTbl.lookup(id); 
}

//
// fdReg::fdReg()
//
fdReg::fdReg (const SOCKET fdIn, const fdRegType typIn, 
		const unsigned onceOnlyIn) : 
	fdRegId(fdIn,typIn), state(fdrLimbo), onceOnly(onceOnlyIn)
{
	if (!FD_IN_FDSET(fdIn)) {
		fprintf (stderr, "%s: fd > FD_SETSIZE ignored\n", 
			__FILE__);
		return;
	}
	fileDescriptorManager.installReg(*this);
}
