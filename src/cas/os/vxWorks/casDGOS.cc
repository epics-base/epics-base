
/*
 *
 * casDGOS.c
 * $Id$
 *
 *
 * $Log$
 * Revision 1.2  1996/09/16 18:27:09  jhill
 * vxWorks port changes
 *
 * Revision 1.1  1996/09/04 22:06:45  jhill
 * installed
 *
 * Revision 1.1.1.1  1996/06/20 00:28:06  jhill
 * ca server installation
 *
 *
 */

//
// CA server
// 
#include <taskLib.h> // vxWorks

#include <server.h>
#include <casClientIL.h> // casClient inline func
#include <task_params.h> // EPICS task priorities

//
// casDGOS::eventSignal()
//
void casDGOS::eventSignal()
{
	STATUS	st;

	st = semGive(this->eventSignalSem);
	assert (st==OK);
}

//
// casDGOS::eventFlush()
//
void casDGOS::eventFlush()
{
	this->flush();
}


//
// casDGOS::casDGOS()
//
casDGOS::casDGOS(caServerI &cas) : 
	eventSignalSem(NULL),
	casDGClient(cas),
	clientTId(ERROR),
	eventTId(ERROR)
{
}

//
// casDGOS::init()
//
caStatus casDGOS::init()
{
	caStatus status;

	this->eventSignalSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if (this->eventSignalSem == NULL) {
		return S_cas_noMemory;
	}

	//
	// init the base classes
	//
	status = this->casDGClient::init();

	return status;
}


//
// casDGOS::~casDGOS()
//
casDGOS::~casDGOS()
{
	if (taskIdVerify(this->clientTId)==OK) {
		taskDelete(this->clientTId);
	}
	if (taskIdVerify(this->eventTId)==OK) {
		taskDelete(this->eventTId);
	}
	if (this->eventSignalSem) {
		semDelete(this->eventSignalSem);
	}
}

//
// casDGOS::show()
//
void casDGOS::show(unsigned level)
{
	this->casDGClient::show(level);
	printf ("casDGOS at %p\n", this);
	if (taskIdVerify(this->clientTId) == OK) {
		taskShow(this->clientTId, level);
	}
	if (taskIdVerify(this->eventTId) == OK) {
		printf("casDGOS task id = %x\n", this->eventTId);
	}
	if (this->eventSignalSem) {
		semShow(this->eventSignalSem, level);
	}
}


/*
 * casClientStart ()
 */
caStatus casDGOS::start()
{
	//
	// no (void *) vxWorks task arg
	//
	assert (sizeof(int) >= sizeof(this));

	this->clientTId = taskSpawn(
			CAST_SRVR_NAME,
                  	CAST_SRVR_PRI,
                  	CAST_SRVR_OPT,
                  	CAST_SRVR_STACK,
			(FUNCPTR) casDGServer, // get your act together wrs 
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
                  	CAST_SRVR_STACK,
			(FUNCPTR) casDGEvent, // get your act together wrs 
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


/*
 * casDGOS::processInput ()
 * - a noop
 */
casProcCond casDGOS::processInput ()
{
	return casProcOk;
}

//
// casDGServer()
//
int casDGServer (casDGOS *pDGOS)
{
	//
	// block for the next DG until the connection closes
	//
	while (TRUE) {
		pDGOS->process();
	}
}

//
// casDGEvent()
//
int casDGEvent (casDGOS *pDGOS)
{
	STATUS status;
	casProcCond cond;

	//
	// Wait for event queue entry
	//
	while (TRUE) {
		status = semTake(pDGOS->eventSignalSem, WAIT_FOREVER);
		assert (status!=OK);

		cond = pDGOS->eventSysProcess();
		if (cond != casProcOk) {
			printf("DG event sys process failed\n");
		}
	}
}

