
//
// $Id$
//
// File descriptor management C++ class library
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

#include <fdMgr.h>
#include <osiTimer.h>

fdMgr fileDescriptorManager;

inline int selectErrno()
{
	return errno;
}

//
// fdMgr::fdMgr()
//
fdMgr::fdMgr()
{
	FD_ZERO (&this->read);
	FD_ZERO (&this->write);
	FD_ZERO (&this->exception);
	this->maxFD = 0;
	this->processInProg = 0;
}

//
// fdMgr::~fdMgr()
//
fdMgr::~fdMgr()
{
	fdReg	*pReg;

	while ( (pReg = this->regList.get()) ) {
		pReg->destroy();
	}
	while ( (pReg = this->activeList.get()) ) {
		pReg->destroy();
	}
}

//
// fdMgr::process()
//
void fdMgr::process (const osiTime &delay)
{
	tsDLIter<fdReg> iter(this->regList);
	osiTime minDelay;
	osiTime zeroDelay;
	fdReg *pReg;
	fdReg *pNextReg;
	struct timeval tv;
	int status;

	//
	// no recursion 
	//
	if (this->processInProg) {
		return;
	}
	this->processInProg = 1;

	while ( (pReg=iter()) ) {
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
			"fdMgr: select failed because \"%s\"\n",
				strerror(selectErrno()));
		}
	}

	//
	// Look for activity
	//
	iter.reset();
	pNextReg = iter();
	while ( (pReg=pNextReg) ) {
		pNextReg = iter();
		if (FD_ISSET(pReg->fd, &pReg->fdSet)) {
			FD_CLR(pReg->fd, &pReg->fdSet);
			this->regList.remove(*pReg);
			this->activeList.add(*pReg);
			pReg->state = fdrActive;
		}
	}

	//
	// prevent problems if they access the
	// above list while in a "callBack()" routine
	//
	while ( (pReg = this->activeList.first()) ) {

		pReg->callBack();

		//
		// Verify that the current item wasnt deleted
		// in "callBack()"
		//
		if (pReg == this->activeList.first()) {
			this->activeList.get();
			pReg->state = fdrLimbo;
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


