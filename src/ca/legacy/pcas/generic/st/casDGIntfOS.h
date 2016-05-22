/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casDGIntfOSh
#define casDGIntfOSh 

#include "casDGIntfIO.h"
#include "casDGIOWakeup.h"
#include "casDGEvWakeup.h"

class casDGIntfOS : public casDGIntfIO {
    friend class casDGReadReg;
    friend class casDGBCastReadReg;
    friend class casDGWriteReg;
    friend class casDGEvWakeup;
    friend class casDGIOWakeup;
    friend class casStreamEvWakeup;
public:
    casDGIntfOS ( caServerI &, clientBufMemoryManager &,
        const caNetAddr & addr, bool autoBeaconAddr = true, 
        bool addConfigBeaconAddr = false);
	virtual ~casDGIntfOS ();
	virtual void show (unsigned level) const;
private:
    casDGIOWakeup ioWk;
    casDGEvWakeup evWk;
	class casDGReadReg * pRdReg;
	class casDGBCastReadReg * pBCastRdReg; // fix for solaris bug
	class casDGWriteReg * pWtReg;

    void armRecv ();
    void armSend ();

    void disarmRecv ();
    void disarmSend ();

    void recvCB ( inBufClient::fillParameter parm );
	void sendCB ();

	void sendBlockSignal ();

	void ioBlockedSignal ();

	void eventSignal ();

	casDGIntfOS ( const casDGIntfOS & );
	casDGIntfOS & operator = ( const casDGIntfOS & );
};

#endif // casDGIntfOSh
