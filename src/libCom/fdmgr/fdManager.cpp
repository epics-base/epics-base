
//
// $Id$
//
//
// $Log$
// Revision 1.12  1998/02/05 21:12:09  jhill
// removed questionable inline
//
// Revision 1.11  1997/08/05 00:37:00  jhill
// removed warnings
//
// Revision 1.10  1997/06/25 05:45:49  jhill
// cleaned up pc port
//
// Revision 1.9  1997/06/13 09:39:09  jhill
// fixed warnings
//
// Revision 1.8  1997/05/29 21:37:38  tang
// add ifdef for select call to support HP-UX
//
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
#define instantiateRecourceLib
#include "osiTimer.h"
#include "fdManager.h"
 
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
	tv.tv_sec = minDelay.getSecTruncToLong ();
	tv.tv_usec = minDelay.getUSecTruncToLong ();
	status = select (this->maxFD, &this->fdSets[fdrRead], 
		&this->fdSets[fdrWrite], &this->fdSets[fdrExcp], &tv);
	staticTimerQueue.process();
	if (status==0) {
		this->processInProg = 0;
		return;
	}
	else if (status<0) {
		if (SOCKERRNO == SOCK_EINTR) {
			this->processInProg = 0;
			return;
		}
		else {
			fprintf(stderr, 
			"fdManager: select failed because errno=%d=\"%s\"\n",
				SOCKERRNO, SOCKERRSTR);
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

