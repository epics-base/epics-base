
//
// $Id$
//
//
// $Log$
// Revision 1.1  1996/08/13 22:48:23  jhill
// dfMgr =>fdManager
//
//

//
//
// NOTES: 
// 1) This isnt intended for a multi-threading environment
//
//

//
// ANSI C
//
#include <assert.h>
#include <errno.h>
#include <string.h>

#if __GNUC__
	extern "C" {
#		include <sys/types.h>
		int select (int, fd_set *, fd_set *, 
				fd_set *, struct timeval *);
		int bzero (char *b, int length);
	} //extern "C"
#endif // __GNUC__

#include <fdManager.h>
#include <osiTimer.h>

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
	tsDLIter<fdReg> regIter(this->regList);
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

	while ( (pReg=regIter()) ) {
		FD_SET(pReg->getFD(), &this->fdSets[pReg->getType()]); 
	}

	//
	// dont bother calling select if the delay to
	// the first timer expire is zero
	//
	while (1) {
		minDelay = staticTimerQueue.delayToFirstExpire();
		if (zeroDelay>=minDelay) {
			staticTimerQueue.process();
		} 
		else {
			break;
		}
	}

	if (minDelay>=delay) {
		minDelay = delay;
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
void fdReg::show(unsigned level)
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
void fdRegId::show(unsigned level)
{
	printf ("fdRegId at %x\n", (unsigned) this);
	if (level>1u) {
		printf ("\tfd = %d, type = %d\n",
			this->fd, this->type);
	}
}


