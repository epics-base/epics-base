//
// $Id$
//
// casOSD.h - Channel Access Server OS Dependent for posix
// 
//
// Some BSD calls have crept in here
//
// $Log$
// Revision 1.1  1996/09/04 22:06:46  jhill
// installed
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
#	include <ioLib.h>
} // extern "C"

#include <osiMutex.h>
#include <osiTimer.h>

class caServerI;

class caServerOS;

//
// casBeaconTimer
//
class casBeaconTimer : public osiTimer {
public:
        casBeaconTimer (const osiTime &delay, caServerOS &osIn) : 
		os (osIn), osiTimer(delay) {}
        void expire();
	const osiTime delay();
	osiBool again();
	const char *name()
	{
		return "casBeaconTimer";
	}
private:
        caServerOS      &os;
	int		taskId; 
};

class casServerReg;

class caServerOS;

//
// vxWorks task entry
//
void caServerEntry(caServerI *pCAS);

//
// caServerOS
//
class caServerOS {
	friend class casServerReg;
	friend void caServerEntry(caServerI *pCAS);
public:
	caServerOS (caServerI &casIn) : 
		cas (casIn), pBTmr (NULL), tid(ERROR) {}
	caStatus init ();
	~caServerOS ();

        //caStatus start ();

	inline caServerI * operator -> ();

	//int getFD();

private:
	caServerI	&cas;
	casBeaconTimer	*pBTmr;
	int		tid;
};

// no additions below this line
#endif // includeCASOSDH 

