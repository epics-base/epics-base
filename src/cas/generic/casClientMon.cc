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
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */


#include "server.h"

#include "casChannelIIL.h"


//
// casClientMon::casClientMon()
//
casClientMon::casClientMon(casChannelI &chan,  caResId clientIdIn,
	const unsigned long count, const unsigned type,
	const casEventMask &maskIn, osiMutex &mutexIn) : 
	casMonitor(clientIdIn, chan, count, type, maskIn, mutexIn)
{
}


//
// casClientMon::~casClientMon()
//
casClientMon::~casClientMon()
{
}


//
// casClientMon::callBack()
//
caStatus casClientMon::callBack(gdd &value)
{
        casCoreClient	&client = this->getChannel().getClient();
        caStatus	status;
	caHdr		msg;

	//
	// reconstruct the msg header
	//
        msg.m_cmmd = CA_PROTO_EVENT_ADD;
        msg.m_postsize = 0u;
        msg.m_type = this->getType();
        msg.m_count = this->getCount();
        msg.m_cid = this->getChannel().getSID();
        msg.m_available = this->getClientId();

	status = client.monitorResponse (&this->getChannel(),
			msg, &value, S_cas_success);
	return status;
}

//
// casClientMon::destroy()
//
// this class is always created inside the server
// lib with new
//
void casClientMon::destroy()
{
	delete this;
}

//
// casClientMon::resourceType()
//
casResType casClientMon::resourceType() const
{
	return casClientMonT;
}

