/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 * Revision 1.6  1997/06/13 09:15:55  jhill
 * connect proto changes
 *
 * Revision 1.5  1997/04/10 19:34:03  jhill
 * API changes
 *
 * Revision 1.4  1996/11/02 00:54:07  jhill
 * many improvements
 *
 * Revision 1.3  1996/09/16 18:23:59  jhill
 * vxWorks port changes
 *
 * Revision 1.2  1996/08/13 22:53:14  jhill
 * changes for MVC++
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#include "server.h"
#include "caServerIIL.h"	// caServerI in line func
#include "casAsyncIOIIL.h"	// casAsyncIOI in line func
#include "casEventSysIL.h"	// casEventSys in line func
#include "casCtxIL.h"		// casCtx in line func
#include "inBufIL.h"		// inBuf in line func
#include "outBufIL.h"		// outBuf in line func

//
// casCoreClient::init()
//
caStatus casCoreClient::init()
{
        if (this->osiMutex::init()) {
                return S_cas_noMemory;
        }
        return this->casEventSys::init();
}

//
// casCoreClient::casCoreClient()
//
casCoreClient::casCoreClient (caServerI &serverInternal) : casEventSys(*this)
{
	assert(&serverInternal);
	ctx.setServer(&serverInternal);
	ctx.setClient(this);
}
 
//
// casCoreClient::~casCoreClient()
//
casCoreClient::~casCoreClient()
{
        if (this->ctx.getServer()->getDebugLevel()>0u) {
                ca_printf ("CAS: Connection Terminated\n");
        }

	this->osiLock();
	tsDLIterBD<casAsyncIOI>   iterIO(this->ioInProgList.first());
	tsDLIterBD<casAsyncIOI>   tmpIO;
	tsDLIterBD<casAsyncIOI>   eolIO;

        //
        // cancel any pending asynchronous IO
        //
        while (iterIO!=eolIO) {
                //
                // destructor removes from this list
                //
		tmpIO = iterIO;
		++tmpIO;
		iterIO->destroy();
		iterIO = tmpIO;
        }

	this->osiUnlock();
}

//
// casCoreClient::destroy()
//
void casCoreClient::destroy()
{
	delete this;
}

//
// casCoreClient::disconnectChan()
//
caStatus casCoreClient::disconnectChan(caResId)
{
	printf ("Disconnect Chan issued for inappropriate client type?\n");
	return S_cas_success;
}

void casCoreClient::show (unsigned level) const
{
	printf ("Core client\n");
	this->casEventSys::show (level);
	printf ("\t%d io ops in progess\n", this->ioInProgList.count());
	this->ctx.show (level);
}

//
// one of these for each CA request type that has
// asynchronous completion
//
caStatus casCoreClient::asyncSearchResponse(casDGIntfIO &,
		const caNetAddr &, const caHdr &, const pvExistReturn &)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::createChanResponse(const caHdr &, const pvCreateReturn &)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readResponse(casChannelI *, const caHdr &, 
	gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readNotifyResponse(casChannelI *, const caHdr &, 
	gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeResponse(casChannelI *, const caHdr &, 
	const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeNotifyResponse(casChannelI *, const caHdr &, 
	const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::monitorResponse(casChannelI *, const caHdr &, 
	gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::accessRightsResponse(casChannelI *)
{
	return S_casApp_noSupport;
}

//
// casCoreClient::installChannel()
//
void casCoreClient::installChannel(casChannelI &)
{
	assert(0); // dont install channels on the wrong type of client
}
 
//
// casCoreClient::removeChannel()
//
void casCoreClient::removeChannel(casChannelI &)
{
	assert(0); // dont install channels on the wrong type of client
}

//
// casCoreClient::fetchRespAddr()
//
caNetAddr casCoreClient::fetchRespAddr()
{
	return caNetAddr(); // sets addr type to UDF
}

//
// casCoreClient::fetchOutIntf()
//
casDGIntfIO* casCoreClient::fetchOutIntf()
{
	return NULL;
}

