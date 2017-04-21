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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef casCtxh
#define casCtxh

#include "caHdrLargeArray.h"

class casStrmClient;

class casCtx {
public:
	casCtx();
	const caHdrLargeArray * getMsg () const;
	void * getData () const;
	class caServerI * getServer () const;
	class casCoreClient * getClient () const;
	class casPVI * getPV () const;
	class casChannelI * getChannel () const;
	void setMsg ( const caHdrLargeArray &, void * pBody );
	void setServer ( class caServerI * p );
	void setClient ( class casCoreClient * p );
	void setPV ( class casPVI * p );
	void setChannel ( class casChannelI * p );
	void show ( unsigned level ) const;
private:
	caHdrLargeArray msg;	// ca message header
	void * pData; // pointer to data following header
	caServerI * pCAS;
	casCoreClient * pClient;
	casChannelI * pChannel;
	casPVI * pPV;
	unsigned nAsyncIO; // checks for improper use of async io
    friend class casStrmClient;
};

inline const caHdrLargeArray * casCtx::getMsg() const 
{	
	return & this->msg;
}

inline void * casCtx::getData() const 
{
	return this->pData;
}

inline caServerI * casCtx::getServer() const 
{
	return this->pCAS;
}

inline casCoreClient * casCtx::getClient() const 
{
	return this->pClient;
}

inline casPVI * casCtx::getPV() const 
{
	return this->pPV;
}

inline casChannelI * casCtx::getChannel() const 
{	
	return this->pChannel;
}

inline void casCtx::setMsg ( const caHdrLargeArray & msgIn, void * pBody )
{
	this->msg = msgIn;
    this->pData = pBody;
}

inline void casCtx::setServer(caServerI *p)
{
	this->pCAS = p;
}

inline void casCtx::setClient(casCoreClient *p) 
{
	this->pClient = p;
}

inline void casCtx::setPV(casPVI *p) 
{
	this->pPV = p;
}

inline void casCtx::setChannel(casChannelI *p) 
{
	this->pChannel = p;
}

#endif // casCtxh
