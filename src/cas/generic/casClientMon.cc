/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */


#include "server.h"

#include "casChannelIIL.h"


//
// casClientMon::casClientMon()
//
casClientMon::casClientMon(casChannelI &chan,  caResId clientIdIn,
	const unsigned long count, const unsigned type,
	const casEventMask &maskIn, epicsMutex &mutexIn) : 
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
caStatus casClientMon::callBack ( const smartConstGDDPointer &value )
{
	casCoreClient & client = this->getChannel().getClient();
	caStatus status;
	caHdrLargeArray msg;

	//
	// reconstruct the msg header
	//
	msg.m_cmmd = CA_PROTO_EVENT_ADD;
	msg.m_postsize = 0u;
	unsigned type = this->getType();
	assert ( type <= 0xffff );
	msg.m_dataType = static_cast <ca_uint16_t> ( type );
	unsigned long count = this->getCount();
	assert ( count <= 0xffffffff );
	msg.m_count = static_cast <ca_uint32_t> ( count );
	msg.m_cid = this->getChannel().getSID();
	msg.m_available = this->getClientId();

	status = client.monitorResponse ( this->getChannel(),
		msg, value, S_cas_success );
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

