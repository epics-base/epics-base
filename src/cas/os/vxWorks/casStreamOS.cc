//
// casStreamOS.cc
// $Id$
//
//
// $Log$
// Revision 1.6  1998/09/24 20:53:54  jhill
// cosmetic
//
// Revision 1.5  1997/08/05 00:47:27  jhill
// fixed warnings
//
// Revision 1.4  1997/06/30 23:38:47  jhill
// use %p for pointers
//
// Revision 1.3  1996/11/02 00:55:01  jhill
// many improvements
//
// Revision 1.2  1996/09/16 18:27:11  jhill
// vxWorks port changes
//
// Revision 1.1  1996/09/04 22:06:46  jhill
// installed
//
// Revision 1.1.1.1  1996/06/20 00:28:06  jhill
// ca server installation
//
//
//

//
// CA server
// 
#include<server.h>
#include <task_params.h> // EPICS task priorities


//
// casStreamOS::eventSignal()
//
void casStreamOS::eventSignal()
{
        STATUS  st;
 
        st = semGive(this->eventSignalSem);
        assert (st==OK);
}

//
// casStreamOS::eventFlush()
//
void casStreamOS::eventFlush()
{
	this->flush();
}


//
// casStreamOS::casStreamOS()
//
casStreamOS::casStreamOS(caServerI &cas, casMsgIO &ioIn) : 
	casStrmClient(cas, ioIn),
	eventSignalSem(NULL),
	clientTId(NULL),
	eventTId(NULL)
{
    caStatus status;
    
    this->eventSignalSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
    if (this->eventSignalSem == NULL) {
        throw S_cas_noMemory;
    }
    
    //
    // init the base classes
    //
    status = this->casStrmClient::init();
    if (status) {
        throw status;
    }
}

//
// casStreamOS::~casStreamOS()
//
casStreamOS::~casStreamOS()
{
	//
	// attempt to flush out any remaining messages
	//
	this->flush();

        if (taskIdVerify(this->clientTId)==OK &&
		this->clientTId != taskIdSelf()) {
                taskDelete(this->clientTId);
        }
        if (taskIdVerify(this->eventTId)==OK &&
		this->eventTId != taskIdSelf()) {
                taskDelete(this->eventTId);
        }
	if (this->eventSignalSem) {
		semDelete(this->eventSignalSem);
	}
}

//
// casStreamOS::show()
//
void casStreamOS::show(unsigned level)
{
	this->casStrmClient::show(level);
	printf("casStreamOS at %p\n", this);
	if (taskIdVerify(this->clientTId)==OK) {
		taskShow(this->clientTId, level);
	}
	if (taskIdVerify(this->eventTId)==OK) {
		printf("casStreamOS task id %x\n", this->eventTId);
	}
	if (this->eventSignalSem) {
		semShow(this->eventSignalSem, level);
	}
}


//
// casClientStart ()
//
caStatus casStreamOS::start()
{
        //
        // no (void *) vxWorks task arg
        //
        assert (sizeof(int) >= sizeof(this));
 
        this->clientTId = taskSpawn(
                        CA_CLIENT_NAME,
                        CA_CLIENT_PRI,
                        CA_CLIENT_OPT,
                        CA_CLIENT_STACK,
                        (FUNCPTR) casStrmServer, // get your act together wrs
                        (int) this,  // get your act together wrs
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL);
        if (this->clientTId==ERROR) {
                return S_cas_noMemory;
        }
        this->eventTId = taskSpawn(
                        CA_EVENT_NAME,
                        CA_CLIENT_PRI,
                        CA_CLIENT_OPT,
                        CA_CLIENT_STACK,
                        (FUNCPTR) casStrmEvent, // get your act together wrs
                        (int) this,  // get your act together wrs
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL);
        if (this->eventTId==ERROR) {
                return S_cas_noMemory;
        }
	return S_cas_success;
}

//
// casStreamOS::processInput()
//
casProcCond casStreamOS::processInput()
{
	caStatus status;

#	ifdef DEBUG
	printf(
		"Resp bytes to send=%d, Req bytes pending %d\n", 
		this->outBuf::bytesPresent(),
		this->inBuf::bytesPresent());
#	endif

	status = this->processMsg();
	switch (status) {
	case S_cas_ioBlocked:
	case S_cas_success:
		return casProcOk;
	default:
		errMessage (status,
			"unexpected error processing client's input");
		return casProcDisconnect;
	}
}

//
// casStrmServer()
//
int casStrmServer (casStreamOS *pStrmOS)
{
	inBuf::fillCondition fillCond;
	casProcCond procCond;
 
        //
        // block for the next message until the connection closes
        //
        while (TRUE) {
		//
		// copy in new messages 
		//
		fillCond = pStrmOS->fill();
		procCond = pStrmOS->processInput();
		if (fillCond == casFillDisconnect ||
			procCond == casProcDisconnect) {
			delete pStrmOS;
			//
			// NO CODE HERE
			// (see delete above)
			//
			return OK;
		}	
		//
		// force the output buffer to flush prior to 
		// blocking for more input (if no input bytes are
		// pending)
		//
		if (pStrmOS->bytesAvailable()<=0u) {
			pStrmOS->flush();
		}
        }

}

//
// casStrmEvent()
//
int casStrmEvent(casStreamOS *pStrmOS)
{
	STATUS status;
	casProcCond cond;

	//
	// Wait for event queue entry
	//
	while (TRUE) {
                status = semTake(pStrmOS->eventSignalSem, WAIT_FOREVER);
                assert (status==OK);
 
                cond = pStrmOS->casEventSys::process();
                if (cond != casProcOk) {
                        printf("Stream event sys process failed\n");
                }
	}
}

