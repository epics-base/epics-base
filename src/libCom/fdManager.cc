
//
// $Id$
//
//
// $Log$
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
	FD_ZERO (&this->read);
	FD_ZERO (&this->write);
	FD_ZERO (&this->exception);
	this->maxFD = 0;
	this->processInProg = 0;
	this->pCBReg = 0;
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
		FD_SET(pReg->fd, &pReg->fdSet); 
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
	status = select (this->maxFD, &this->read, 
			&this->write, &this->exception, &tv);
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
		if (FD_ISSET(pReg->fd, &pReg->fdSet)) {
			FD_CLR(pReg->fd, &pReg->fdSet);
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
		printf ("\tfd = %d, fdSet at %x, state = %d, onceOnly = %d\n",
			this->fd, (unsigned)&this->fdSet, 
			this->state, this->onceOnly);
	}
}


