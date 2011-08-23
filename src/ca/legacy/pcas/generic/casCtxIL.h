/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casCtxILh
#define casCtxILh

#include "osiWireFormat.h"

//
// casCtx::casCtx()
//
inline casCtx::casCtx() :
	pData(NULL), pCAS(NULL), pClient(NULL),
	pChannel(NULL), pPV(NULL), nAsyncIO(0u)
{
	memset(&this->msg, 0, sizeof(this->msg));
}

//
// casCtx::getMsg()
//
inline const caHdrLargeArray * casCtx::getMsg() const 
{	
	return & this->msg;
}

//
// casCtx::getData()
//
inline void * casCtx::getData() const 
{
	return this->pData;
}

//
// casCtx::getServer()
//
inline caServerI * casCtx::getServer() const 
{
	return this->pCAS;
}

//
// casCtx::getClient()
//
inline casCoreClient * casCtx::getClient() const 
{
	return this->pClient;
}

//
// casCtx::getPV()
//
inline casPVI * casCtx::getPV() const 
{
	return this->pPV;
}

//
// casCtx::getChannel()
//
inline casChannelI * casCtx::getChannel() const 
{	
	return this->pChannel;
}

//
// casCtx::setMsg() 
//
inline void casCtx::setMsg ( const caHdrLargeArray & msgIn, void * pBody )
{
	this->msg = msgIn;
    this->pData = pBody;
}

//
// casCtx::setServer()
//
inline void casCtx::setServer(caServerI *p)
{
	this->pCAS = p;
}

//
// casCtx::setClient()
//
inline void casCtx::setClient(casCoreClient *p) 
{
	this->pClient = p;
}

//
// casCtx::setPV()
//
inline void casCtx::setPV(casPVI *p) 
{
	this->pPV = p;
}

//
// casCtx::setChannel()
//
inline void casCtx::setChannel(casChannelI *p) 
{
	this->pChannel = p;
}

#endif // casCtxILh

