//
// $Id$
//
// casOSD.h - Channel Access Server OS Dependent for posix
// 
//
// Some BSD calls have crept in here
//
// $Log$
// Revision 1.4  1997/06/13 09:16:17  jhill
// connect proto changes
//
// Revision 1.3  1996/11/02 00:55:00  jhill
// many improvements
//
// Revision 1.2  1996/09/16 18:27:10  jhill
// vxWorks port changes
//
// Revision 1.1  1996/09/04 22:06:46  jhill
// installed
//
// Revision 1.1.1.1  1996/06/20 00:28:06  jhill
// ca server installation
//
//

#ifndef includeCASOSDH 
#define includeCASOSDH 

#include "osiMutex.h"
#include "osiTimer.h"

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

	inline caServerI * operator -> ();

private:
	caServerI	&cas;
	casBeaconTimer	*pBTmr;
};

//
// casDGIntfOS
//
class casDGIntfOS : public casDGIO {
public:
        casDGIntfOS(casDGClient &client);
        ~casDGIntfOS();
 
        caStatus start();
 
        void show(unsigned level) const;
 
        void recvCB();
        void sendCB();
private:
        casDGClient     &client;
	int		tid;
};

//
// casIntfOS
//
class casIntfOS : public casIntfIO, public tsDLNode<casIntfOS>
{
public:
        casIntfOS (caServerI &casIn) :
                cas (casIn), pRdReg (NULL) {}
        caStatus init(const caNetAddr &addr, casDGClient &dgClientIn,
                        int autoBeaconAddr, int addConfigBeaconAddr);
        ~casIntfOS();
 
        void recvCB ();
        void sendCB () {}; // NOOP satifies template
 
        casDGIO *newDGIO (casDGClient &dgClientIn) const;
private:
        caServerI       &cas;
	int		tid;
};

// no additions below this line
#endif // includeCASOSDH 

