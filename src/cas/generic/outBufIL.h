/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef outBufILh
#define outBufILh

//
// outBuf::bytesPresent()
// number of bytes in the output queue
//
inline bufSizeT outBuf::bytesPresent() const
{
	return this->stack;
}

//
// outBuf::bytesFree()
// number of bytes unused in the output queue
//
inline bufSizeT outBuf::bytesFree() const
{
	return this->bufSize - this->stack;
}

//
// outBuf::clear()
//
inline void outBuf::clear()
{
	this->stack = 0u;
}

#endif // outBufILh

