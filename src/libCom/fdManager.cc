
//
// $Id$
//
//
// $Log$
// Revision 1.7  1997/05/27 14:53:11  tang
// fd_set cast in select for both Hp and Sun
//
// Revision 1.6  1997/05/08 19:49:12  tang
// added int * cast in select for HP port compatibility
//
// Revision 1.5  1997/04/23 17:22:57  jhill
// fixed WIN32 DLL symbol exports
//
// Revision 1.4  1997/04/10 19:45:33  jhill
// API changes and include with  not <>
//
// Revision 1.3  1996/11/02 02:04:42  jhill
// fixed several subtle bugs
//
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
#define instantiateStringIdFastHash
#include "resourceLib.cc"
 
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
	static const tsDLIterBD<fdReg> eol; // end of list
	tsDLIterBD<fdReg> iter;
	tsDLIterBD<fdReg> tmp;
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

	for (iter=this->regList.first(); iter!=eol; ++iter) {
		FD_SET(iter->getFD(), &this->fdSets[iter->getType()]); 
	}
	minDelay.getTV (tv.tv_sec, tv.tv_usec);
#ifdef __hpux
	status = select (this->maxFD, (int *)&this->fdSets[fdrRead], 
		(int *)&this->fdSets[fdrWrite], (int *)&this->fdSets[fdrExcp], &tv);
#else
	status = select (this->maxFD, (fd_set *)&this->fdSets[fdrRead], 
		(fd_set *)&this->fdSets[fdrWrite], (fd_set *)&this->fdSets[fdrExcp], &tv);
#endif

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


