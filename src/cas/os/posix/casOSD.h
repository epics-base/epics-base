//
// $Id$
//
// casOSD.h - Channel Access Server OS Dependent for posix
// 
//
// Some BSD calls have crept in here
//
// $Log$
// Revision 1.2  1996/08/13 22:58:15  jhill
// fdMgr.h => fdmanager.h
//
// Revision 1.1.1.1  1996/06/20 00:28:06  jhill
// ca server installation
//
//

#ifndef includeCASOSDH 
#define includeCASOSDH 


#include <unistd.h>
#include <errno.h>

extern "C" {
//
// for htons() etc  
//
#	include <netinet/in.h>

//
// g++ 2.7.2 does not fix this file under sunos4 so I
// have provided the prototype elsewhere (in osiSock.h for now)
//
#if !(defined(SUNOS4) && defined(__GNUC__))
#	include <netdb.h>
#endif
} // extern "C"

#include <osiMutex.h>
#include <osiTimer.h>
#include <fdManager.h>

class caServerI;

class caServerOS;

//
// casBeaconTimer
//
class casBeaconTimer : public osiTimer {
public:
        casBeaconTimer (const osiTime &delay, caServerOS &osIn) : 
		osiTimer(delay), os (osIn) {}
        void expire();
	const osiTime delay();
	osiBool again();
	const char *name()
	{
		return "casBeaconTimer";
	}
private:
        caServerOS      &os;
 
};

class casServerReg;

//
// caServerOS
//
class caServerOS {
	friend class casServerReg;
public:
	caServerOS (caServerI &casIn) : 
		cas (casIn), pRdReg (NULL), pBTmr (NULL) {}
	caStatus init ();
	~caServerOS ();

        caStatus start ();
        //caStatus process (const caTime &delay);
        //caStatus installTimer (const caTime &delay,
	//		void (*pFunc)(void *pParam), void *pParam,
        //		caServerTimerId &id);
        //caStatus deleteTimer (const caServerTimerId id);

	void recvCB ();
	void sendCB () {}; // NOOP satifies template

	inline caServerI * operator -> ();

	int getFD();
private:
	caServerI	&cas;
	casServerReg	*pRdReg;
	casBeaconTimer	*pBTmr;

};

class casServerReg : public fdReg {
public:
        casServerReg (caServerOS &osIn) :
                os (osIn), fdReg (osIn.getFD(), fdrRead) {}
        ~casServerReg ();
private:
        caServerOS &os;
 
        void callBack ();
};

// no additions below this line
#endif // includeCASOSDH 

