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
// inBuf::inBuf()
//
inline inBuf::inBuf(osiMutex &mutexIn, unsigned bufSizeIn) :
        mutex(mutexIn), pBuf(NULL), bufSize(bufSizeIn),
        bytesInBuffer(0u), nextReadIndex(0u)
{
}

//
// inBuf::init()
//
inline caStatus inBuf::init()
{
        this->pBuf = new char [this->bufSize];
        if (!this->pBuf) {
                this->bufSize = 0u;
                return S_cas_noMemory;
        }
        return S_cas_success;
}

//
// inBuf::bytesPresent()
//
inline bufSizeT inBuf::bytesPresent() const 
{
	return this->bytesInBuffer-this->nextReadIndex;
}

//
// inBuf::bytesAvailable()
//
inline bufSizeT inBuf::bytesAvailable() const 
{
	bufSizeT bp;
	bp = this->bytesPresent();
	bp += this->incommingBytesPresent();
	return bp;
}

//
// inBuf::full()
//
inline aitBool inBuf::full() const
{
	if (this->bytesPresent()>=this->bufSize) {
		return aitTrue;
	}
	return aitFalse;
}

//
// inBuf::clear()
//
inline void inBuf::clear()
{
	this->bytesInBuffer = 0u;
	this->nextReadIndex = 0u;
}

//
// inBuf::msgPtr()
//
inline char *inBuf::msgPtr() const 
{
	return &this->pBuf[this->nextReadIndex];
}

//
// inBuf::removeMsg()
//
inline void inBuf::removeMsg(unsigned nBytes) 
{
	this->nextReadIndex += nBytes;
	assert(this->nextReadIndex<=this->bytesInBuffer);
}

#endif // inBufILh

