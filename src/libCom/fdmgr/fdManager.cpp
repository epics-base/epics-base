
//
// $Id$
//
//
// $Log$
// Revision 1.2  1996/09/04 21:50:16  jhill
// added hashed fd to fdi convert
//
// Revision 1.1  1996/08/13 22:48:23  jhill
// dfMgr =>fdManager
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
#include "osiTimer.h"
#include "fdManager.h"

#if !defined(__SUNPRO_CC)
        //
        // From Stroustrups's "The C++ Programming Language"
        // Appendix A: r.14.9
        //
        // This explicitly instantiates the template class's member
        // functions into "fdManager.o"
        //
#	include <resourceLib.cc>
        template class resTable <fdReg, fdRegId>;
#endif

fdManager fileDescriptorManager;

inline int selectErrno()
{
	return errno;
}

//
// fdManager::fdManager()
//
fdManager::fdManager()
{
	size_t	i;

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
fdManager::~fdManager()
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
}

//
// fdManager::process()
//
void fdManager::process (const osiTime &delay)
{
	tsDLFwdIter<fdReg> regIter(this->regList);
	osiTime minDelay;
	osiTime zeroDelay;
	fdReg *pReg;
	struct timeval tv;
	int status;

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

	while ( (pReg=regIter()) ) {
		FD_SET(pReg->getFD(), &this->fdSets[pReg->getType()]); 
	}
	minDelay.getTV (tv.tv_sec, tv.tv_usec);
	status = select (this->maxFD, &this->fdSets[fdrRead], 
		&this->fdSets[fdrWrite], &this->fdSets[fdrExcp], &tv);
	staticTimerQueue.process();
	if (status==0) {
		this->processInProg = 0;
		return;
	}
	else if (status<0) {
		if (selectErrno() == EINTR) {
			this->processInProg = 0;
			return;
		}
		else {
			fprintf(stderr, 
			"fdManager: select failed because \"%s\"\n",
				strerror(selectErrno()));
		}
	}

	//
	// Look for activity
	//
	regIter.reset();
	while ( (pReg=regIter()) ) {
		if (FD_ISSET(pReg->getFD(), &this->fdSets[pReg->getType()])) {
			FD_CLR(pReg->getFD(), &this->fdSets[pReg->getType()]);
			regIter.remove();
			this->activeList.add(*pReg);
			pReg->state = fdrActive;
		}
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
		if (this->pCBReg == pReg) {
			this->pCBReg = 0;
			if (pReg->onceOnly) {
				pReg->destroy();
			}
			else {
				this->regList.add(*pReg);
				pReg->state = fdrPending;
			}
		}
		else {
			//
			// no recursive calls  to process allowed
			//
			assert(this->pCBReg == 0);
		}
	}
	this->processInProg = 0;
}

//
// fdReg::destroy()
// (default destroy method)
//
void fdReg::destroy()
{
	delete this;
}

//
// fdReg::~fdReg()
//
fdReg::~fdReg()
{
	fileDescriptorManager.removeReg(*this);
}

//
// fdReg::show()
//
void fdReg::show(unsigned level) const
{
	printf ("fdReg at %x\n", (unsigned) this);
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
	printf ("fdRegId at %x\n", (unsigned) this);
	if (level>1u) {
		printf ("\tfd = %d, type = %d\n",
			this->fd, this->type);
	}
}


