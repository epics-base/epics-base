/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef inBufILh
#define inBufILh

//
// inBuf::bytesPresent()
//
inline bufSizeT inBuf::bytesPresent () const 
{
	return this->bytesInBuffer-this->nextReadIndex;
}

//
// inBuf::bytesAvailable()
//
inline bufSizeT inBuf::bytesAvailable () const 
{
	bufSizeT bp;
	bp = this->bytesPresent ();
	bp += this->client.incomingBytesPresent ();
	return bp;
}

//
// inBuf::full()
//
inline bool inBuf::full () const
{
	if (this->bufSize-this->bytesPresent()<this->ioMinSize) {
		return true;
	}
	return false;
}

//
// inBuf::msgPtr()
//
inline char *inBuf::msgPtr () const 
{
	return &this->pBuf[this->nextReadIndex];
}

//
// inBuf::removeMsg()
//
inline void inBuf::removeMsg ( bufSizeT nBytes ) 
{
	this->nextReadIndex += nBytes;
	assert ( this->nextReadIndex <= this->bytesInBuffer );
}

//
// inBufCtx::inBufCtx ()
//
inline inBufCtx::inBufCtx () :
    stat (pushCtxNoSpace) {}

//
// inBufCtx::inBufCtx ()
//
inline inBufCtx::inBufCtx (const inBuf &inBufIn) :
    stat (pushCtxSuccess), pBuf (inBufIn.pBuf), 
    bufSize (inBufIn.bufSize), bytesInBuffer (inBufIn.bytesInBuffer),
    nextReadIndex (inBufIn.nextReadIndex) {}

//
// inBufCtx::pushResult
// 
inline inBufCtx::pushCtxResult inBufCtx::pushResult () const
{
    return this->stat;
}

#endif // inBufILh

