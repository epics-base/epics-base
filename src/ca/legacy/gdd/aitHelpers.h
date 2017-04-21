/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef aitHelpersInclude
#define aitHelpersInclude

/*
 * Authors: Jeff Hill and Jim Kowalkowski
 * Date: 6/20/96
 */

#include <string.h>
#include <limits.h>
#ifndef assert // allows use of epicsAssert.h
#include <assert.h> 
#endif

#include "shareLib.h"

#define NSecPerSec 1000000000u
#define NSecPerUSec 1000u
#define SecPerMin 60u

inline char* strDup(const char* x)
{
	char* y = new char[strlen(x)+1];
	strcpy(y,x);
	return y;
}

struct timespec;
struct epicsTimeStamp;
class epicsTime;
class gdd;

class epicsShareClass aitTimeStamp {
	friend inline aitTimeStamp operator+ (const aitTimeStamp &lhs, const aitTimeStamp &rhs);
	friend inline aitTimeStamp operator- (const aitTimeStamp &lhs, const aitTimeStamp &rhs);
	friend inline int operator>= (const aitTimeStamp &lhs, const aitTimeStamp &rhs);
	friend class gdd;
public:
	aitTimeStamp () : tv_sec(0u), tv_nsec(0u) {}
	aitTimeStamp (const aitTimeStamp &t) : tv_sec(t.tv_sec), tv_nsec(t.tv_nsec) {}
	aitTimeStamp (const unsigned long tv_secIn, const unsigned long tv_nsecIn) :
			tv_sec(tv_secIn), tv_nsec(tv_nsecIn) 
	{
		if (tv_nsecIn>=NSecPerSec) {
			this->tv_sec += this->tv_nsec/NSecPerSec;
			this->tv_nsec = this->tv_nsec%NSecPerSec;
		}
	}

	//
	// fetched as fields so this file is not required 
	// to include os dependent struct timeval
	//
	void getTV(long &tv_secOut, long &uSecOut) const
	{
		assert (this->tv_sec<=LONG_MAX);
		tv_secOut = (long) this->tv_sec;
		assert (this->tv_nsec<=LONG_MAX);
		uSecOut = (long) this->tv_nsec/NSecPerUSec;
	}

	//
	// for use when loading struct timeval
	//
	void get(unsigned long &tv_secOut, unsigned long &tv_nsecOut) const
	{
		tv_secOut = this->tv_sec;
		tv_nsecOut = this->tv_nsec;
	}

	operator double() const
	{
		return ((double)this->tv_nsec)/NSecPerSec+this->tv_sec;
	}

	operator float() const
	{
		return ((float)this->tv_nsec)/NSecPerSec+this->tv_sec;
	}

	//
	// convert to and from POSIX timespec format
	//
	operator struct timespec () const;
	void get (struct timespec &) const;
	aitTimeStamp (const struct timespec &ts);
	aitTimeStamp operator = (const struct timespec &rhs);

	//
	// convert to and from EPICS epicsTimeStamp format
	//
	operator struct epicsTimeStamp () const;
	void get (struct epicsTimeStamp &) const;
	aitTimeStamp (const epicsTimeStamp &ts);
	aitTimeStamp operator = (const epicsTimeStamp &rhs);
	
	// conversion to from epicsTime
	aitTimeStamp (const epicsTime &ts);
	aitTimeStamp operator = (const epicsTime &rhs);
	operator epicsTime () const;

	static aitTimeStamp getCurrent();

//private:
	unsigned long tv_sec;
	unsigned long tv_nsec;
private:
	static const unsigned epicsEpochSecPast1970; 
};

inline aitTimeStamp operator+ (const aitTimeStamp &lhs, const aitTimeStamp &rhs)
{
	return aitTimeStamp(lhs.tv_sec + rhs.tv_sec, lhs.tv_nsec + rhs.tv_nsec);	
}

//
// like data type unsigned this assumes that the lhs > rhs
// (otherwise we assume tv_sec wrap around)
//
inline aitTimeStamp operator- (const aitTimeStamp &lhs, const aitTimeStamp &rhs)
{
	unsigned long tv_nsec, tv_sec;

	if (lhs.tv_sec<rhs.tv_sec) {
		//
		// wrap around
		//
		tv_sec = 1 + lhs.tv_sec + (ULONG_MAX - rhs.tv_sec);
	}
	else {
		tv_sec = lhs.tv_sec - rhs.tv_sec;
	}

	if (lhs.tv_nsec<rhs.tv_nsec) {
		//
		// Borrow
		//
		tv_nsec = 1 + lhs.tv_nsec + (NSecPerSec - rhs.tv_nsec);	
		tv_sec--;
	}
	else {
		tv_nsec = lhs.tv_nsec - rhs.tv_nsec;	
	}
	return aitTimeStamp(tv_sec, tv_nsec);	
}

inline int operator>= (const aitTimeStamp &lhs, const aitTimeStamp &rhs)
{
	int rc=0;

	if (lhs.tv_sec>rhs.tv_sec)
		rc=1;
	else if(lhs.tv_sec==rhs.tv_sec)
		if(lhs.tv_nsec>=rhs.tv_nsec)
			rc=1;
	return rc;
}


// ---------------------------------------------------------------------------
// very simple class for string storage (for now)
//
//
enum aitStrType {
		aitStrRefConstImortal, // any constant string that always exists - ie "abc"
		aitStrRefConst, // user provides constant string buffer for life of aitSting
		aitStrRef, // user provides modifiable string buffer for life of aitSting
		aitStrCopy}; // aitSting copies into internal storage
class epicsShareClass aitString
{
public:
	inline aitString(void);
	inline aitString(const aitString* p);
	inline aitString(const aitString& p);
	inline aitString(const char* p, aitStrType type=aitStrCopy);
	inline aitString(const char* p, aitStrType type, unsigned strLength);
	inline aitString(const char* p, aitStrType type, unsigned strLength, unsigned bufSize);
	
	inline ~aitString(void); // free up string is required

	inline void clear(void); // clear everything, free string if required
	void dump(void) const;
	void dump(const char* id) const;

	// casts from aitString to other things - pulls info out of aitString
	inline operator aitUint16(void) const	{ return (aitUint16)this->len; }
	inline operator aitUint32(void) const	{ return (aitUint32)this->len; }
	inline operator aitInt32(void) const		{ return (aitInt32)this->len; }
	inline operator const char*(void) const	{ return this->str; }
	inline operator char*(void) const		
	{ 
		assert(type!=aitStrRefConst && type!=aitStrRefConstImortal); 
		return str; 
	}
	inline int isConstant(void) const;

	inline aitUint32 length(void) const	{ return (aitUint32)this->len; }
	inline const char* string(void) const	{ return this->str; }

	// completely reset the aitString to a new value
	// - the same as copy()
	inline aitString& operator=(const aitString& pString);
	inline aitString& operator=(const aitString* pString);
	inline aitString& operator=(const char* pString);

	inline int copy(const aitString* pString);
	inline int copy(const aitString& pString);
	inline int copy(const char* pString);
	inline int copy(const char* pString, unsigned stringLength);
	int copy(const char* pString, unsigned stringLength, unsigned bufSize);

	//
	// make ait string point at string in application's modifiable buffer
	//
	inline int installBuf(const char* pString);
	inline int installBuf(const char* pString, unsigned stringLength);
	inline int installBuf(const char* pString, unsigned stringLength, unsigned bufSize);

	//
	// make ait string point at constant string in application's constant buffer
	//
	inline int installConstBuf(const char* pString);
	inline int installConstBuf(const char* pString, unsigned stringLength);
	inline int installConstBuf(const char* pString, unsigned stringLength, unsigned bufSize);

	//
	// make ait string point at constant string in application's constant buffer
	// that exists forever (such as "abc")
	//
	inline int installConstImortalBuf(const char* pString);
	inline int installConstImortalBuf(const char* pString, unsigned stringLength);
	inline int installConstImortalBuf(const char* pString, unsigned stringLength, unsigned bufSize);

	inline void extractString(char* to_here, unsigned bufSize);

	// take the aitString array, and put it and all the string into buf,
	// return the total length the data copied
	static aitUint32 totalLength(aitString* array,aitIndex arraySize);
	static aitUint32 stringsLength(aitString* array,aitIndex arraySize);
	static aitIndex compact(aitString* array, aitIndex arraySize,
		void* buf, aitIndex bufSize);

	//
	// for gdd's use only! (to construct class inside union)
	// (it is possible to leak memory if this is called on
	// an already constructed aitString). This practice should
	// be eliminated by deriving from aitString and replacing
	// the new operator?
	//
	inline void init(void);

private:

	char* str;
	unsigned len:14;		// actual length of string
	unsigned bufLen:14;	// length of string buffer
	unsigned type:4;		// aitStrType goes here

	void mallocFailure(void);
	inline aitStrType getType(void) const { return (aitStrType)type; }
	int init(const char* p, aitStrType type, unsigned strLength, unsigned bufSize);
};

inline void aitString::init(void) 
{ 
	this->str=(char *)"";
	this->len=0u;
	this->bufLen=1u;
	this->type=aitStrRefConstImortal;
}

inline int aitString::isConstant(void) const
{
	return ( (getType()==aitStrRefConst||getType()==aitStrRefConstImortal) && this->str)?1:0;
}

inline void aitString::clear(void)
{
	if(this->str && this->type==aitStrCopy) delete [] this->str;
	this->init();
}

inline void aitString::extractString(char* p, unsigned bufLength)
{ 
	if (bufLength==0u) {
		return;
	}
	else if (this->str) {
		strncpy(p,this->str,bufLength);
		p[bufLength-1u]='\0';
	}
	else {
		p[0u] = '\0';
	}
}

//
// make ait string point at string in application's buffer
//
inline int aitString::installBuf(const char* pString, unsigned strLengthIn, unsigned bufSizeIn)
{
	if (this->type==aitStrCopy) {
		delete [] this->str;
	}
	this->str = (char *) pString;
	this->bufLen = bufSizeIn;
	this->type = aitStrRef;
	this->len = strLengthIn;
	return 0;
}

inline int aitString::installBuf(const char* pString)
{
    unsigned strLengthIn = (unsigned) strlen(pString);
	return this->installBuf(pString, strLengthIn, strLengthIn+1u);
}

inline int aitString::installBuf(const char* pString, unsigned strLengthIn)
{
	return this->installBuf(pString, strLengthIn, strLengthIn+1u);
}


//
// make ait string point at constant string in application's buffer
//
inline int aitString::installConstBuf(const char* pString, unsigned strLengthIn, unsigned bufSizeIn)
{
	if (this->type==aitStrCopy) {
		delete [] this->str;
	}
	this->str = (char *) pString;
	this->bufLen = bufSizeIn;
	this->type = aitStrRefConst;
	this->len = strLengthIn;
	return 0;
}

inline int aitString::installConstBuf(const char* pString)
{
    unsigned strLengthIn = (unsigned) strlen(pString);
	return this->installConstBuf(pString, strLengthIn, strLengthIn+1u);
}

inline int aitString::installConstBuf(const char* pString, unsigned strLengthIn)
{
	return this->installConstBuf(pString, strLengthIn, strLengthIn+1u);
}

//
// make ait string point at constant string in application's buffer
// that always exists (such as "abc")
//
inline int aitString::installConstImortalBuf(const char* pString, 
					unsigned strLengthIn, unsigned bufSizeIn)
{
	if (this->type==aitStrCopy) {
		delete [] this->str;
	}
	this->str = (char *) pString;
	this->bufLen = bufSizeIn;
	this->type = aitStrRefConstImortal;
	this->len = strLengthIn;
	return 0;
}

inline int aitString::installConstImortalBuf(const char* pString)
{
    unsigned strLengthIn = (unsigned) strlen(pString);
	return this->installConstImortalBuf(pString, strLengthIn, strLengthIn+1u);
}

inline int aitString::installConstImortalBuf(const char* pString, unsigned strLengthIn)
{
	return this->installConstImortalBuf(pString, strLengthIn, strLengthIn+1u);
}


inline int aitString::copy(const char* pString, unsigned stringLength) 
{
	return this->copy(pString, stringLength, 
        this->bufLen>(stringLength+1u) ? this->bufLen : (stringLength+1u) );
}

inline int aitString::copy(const char* p)
{
    return this->copy(p, (unsigned) strlen(p));
}

inline int aitString::copy(const aitString* p)
{
	if (p->type==aitStrRefConstImortal) {
		//
		// fast reference if the string is constant and it
		// exists forever
		//
		return this->installConstImortalBuf(p->str, p->len, p->len+1u);
	}
	return this->copy(p->str, p->len);
}

inline int aitString::copy(const aitString& p)
{
	return this->copy(&p);
}

inline aitString& aitString::operator=(const aitString& p)
	{ this->copy(p); return *this; }
inline aitString& aitString::operator=(const aitString* p)
	{ this->copy(p); return *this; }
inline aitString& aitString::operator=(const char* p)
	{ this->copy(p); return *this; }

inline aitString::~aitString(void)
{
	// dump("~aitString");
	this->clear();
}

inline aitString::aitString(void) 
{ 
	this->init(); 
}

inline aitString::aitString(const char* p, aitStrType typeIn)
{
    unsigned strLengthIn = (unsigned) strlen(p);
	this->init(p, typeIn, strLengthIn, strLengthIn+1u);
}

inline aitString::aitString(const char* p, aitStrType typeIn, unsigned strLengthIn)
{
	this->init(p, typeIn, strLengthIn, strLengthIn+1u);
}

inline aitString::aitString(const char* p, aitStrType typeIn, unsigned strLength, unsigned bufSize)
{
	this->init(p,typeIn,strLength,bufSize);
}

inline aitString::aitString(const aitString* p)	
{ 
	this->init(); 
	this->copy(p); 
}

inline aitString::aitString(const aitString& p)	
{ 
	this->init(); 
	this->copy(p);
}

#endif // aitHelpersInclude
