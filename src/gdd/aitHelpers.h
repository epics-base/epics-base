#ifndef aitHelpersInclude
#define aitHelpersInclude

/*
 * Authors: Jeff Hill and Jim Kowalkowski
 * Date: 6/20/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.6  1996/08/14 12:30:10  jbk
 * fixes for converting aitString to aitInt8* and back
 * fixes for managing the units field for the dbr types
 *
 * Revision 1.5  1996/08/12 15:37:46  jbk
 * Re-added the installString() function I took out.
 *
 * Revision 1.4  1996/08/09 02:28:09  jbk
 * rewrite of aitString class - more intuitive now, I think
 *
 * Revision 1.3  1996/08/06 19:14:09  jbk
 * Fixes to the string class.
 * Changes units field to a aitString instead of aitInt8.
 *
 * Revision 1.2  1996/06/26 21:00:05  jbk
 * Fixed up code in aitHelpers, removed unused variables in others
 * Fixed potential problem in gddAppTable.cc with the map functions
 *
 * Revision 1.1  1996/06/25 19:11:31  jbk
 * new in EPICS base
 *
 *
 */

#include <stdio.h>
#include <limits.h>
#ifndef assert // allows use of epicsAssert.h
#include <assert.h> 
#endif

const unsigned NSecPerSec = 1000000000u;
const unsigned NSecPerUSec = 1000u;
const unsigned SecPerMin = 60u;

class aitTimeStamp {
	friend aitTimeStamp operator+ (const aitTimeStamp &lhs, const aitTimeStamp &rhs);
	friend aitTimeStamp operator- (const aitTimeStamp &lhs, const aitTimeStamp &rhs);
	friend int operator>= (const aitTimeStamp &lhs, const aitTimeStamp &rhs);
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
	void getTV(long &tv_secOut, long &uSecOut) {
		assert (this->tv_sec<=LONG_MAX);
		tv_secOut = (long) this->tv_sec;
		assert (this->tv_nsec<=LONG_MAX);
		uSecOut = (long) this->tv_nsec/NSecPerUSec;
	}
	//
	// for use when loading struct timeval
	//
	void get(unsigned long &tv_secOut, unsigned long &tv_nsecOut) {
		tv_secOut = this->tv_sec;
		tv_nsecOut = this->tv_nsec;
	}

	operator double()
	{
		return ((double)this->tv_nsec)/NSecPerSec+this->tv_sec;
	}

	operator float()
	{
		return ((float)this->tv_nsec)/NSecPerSec+this->tv_sec;
	}

	static aitTimeStamp getCurrent();
// private:
	unsigned long tv_sec;
	unsigned long tv_nsec;
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
		tv_sec = lhs.tv_sec + (ULONG_MAX - rhs.tv_sec);
	}
	else {
		tv_sec = lhs.tv_sec - rhs.tv_sec;
	}

	if (lhs.tv_nsec<rhs.tv_nsec) {
		//
		// Borrow
		//
		tv_nsec = lhs.tv_nsec + NSecPerSec - rhs.tv_nsec;	
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
class aitString
{
public:
	aitString(char* p);				// copy the user's string
	aitString(aitString* p);		// copy the user's string
	aitString(aitString& p);		// copy the user's string

	aitString(const char* p);		// reference a user's constant string
	aitString(const aitString* p);	// reference a user's constant string
	aitString(const aitString& p);	// reference a user's constant string

	aitString(void);
	~aitString(void); // free up string is required

	void clear(void); // clear everything, free string if required
	void dump(void) const;
	void dump(const char* id) const;

	// casts from aitString to other things - pulls info out of aitString
	operator aitUint16(void) const		{ return (aitUint16)len; }
	operator aitUint32(void) const		{ return (aitUint32)len; }
	operator aitInt32(void) const		{ return (aitInt32)len; }
	operator const char*(void) const	{ return str; }
	operator char*(void) const			{ return str; }
	int isConstant(void) const;

	aitUint32 length(void) const	{ return (aitUint32)len; }
	const char* string(void) const	{ return str; }

	// completely reset the aitString to a new value
	aitString& operator=(aitString& p);
	aitString& operator=(aitString* p);
	aitString& operator=(char* p);
	aitString& operator=(const aitString& p);
	aitString& operator=(const aitString* p);
	aitString& operator=(const char* p);

	// change strings into the aitString (actually makes new strings)
	int copy(aitString* p);
	int copy(aitString& p);
	int copy(char* p);
	int copy(const aitString* p);
	int copy(const aitString& p);
	int copy(const char* p);

	int installString(aitString* p);
	int installString(aitString& p);
	int installString(char* p);
	int installString(const aitString* p);
	int installString(const aitString& p);
	int installString(const char* p);

	// set data in the aitString and retrieve data from the aitString
	void replaceData(const char* p);
	void replaceData(const aitString* p);
	void replaceData(aitString& p);
	void replaceData(const aitString& p);
	void extractString(char* to_here);

	// special function to change the string - internal use with gdd library
	void force(char* x)				{ str=x; }
	void force(unsigned char* x)	{ str=(char*)x; }
	void force(unsigned long x)		{ str=(char*)x; }
	void forceConstant(void)		{ type=aitStrConst; }
	void init(void);

	// take the aitString array, and put it and all the string into buf,
	// return the total length the data copied
	static aitUint32 totalLength(aitString* array,aitIndex arraySize);
	static aitUint32 stringsLength(aitString* array,aitIndex arraySize);
	static aitIndex compact(aitString* array, aitIndex arraySize,
		void* buf, aitIndex bufSize);

private:
	enum aitStrType {aitStrMalloc, aitStrConst};

	char* str;
	aitUint16 len;
	aitUint16 type;	// aitStrType goes here

	void mallocFailure(void);
	int set(const char* p, aitUint32 len);
	int cset(const char* p, aitUint32 len);
	aitStrType getType(void) const { return (aitStrType)type; }
};

inline int aitString::isConstant(void) const
{
	return (getType()==aitStrConst && str)?1:0;
}

inline void aitString::clear(void)
{
	if(str && type==aitStrMalloc) delete [] str;
	type=aitStrConst;
	str=NULL;
	len=0;
}

inline int aitString::set(const char* p,aitUint32 l)
{
	int rc=0;
	clear();
	len=l;
	str=new char[len+1];
	if(str)
	{
		strcpy(str, p);
		type=aitStrMalloc;
	}
	else
	{
		mallocFailure();
		rc=-1;
	}
	return rc;
}

inline int aitString::cset(const char* p,aitUint32 l)
{
	clear();
	str=(char*)p;
	type=aitStrConst;
	len=l;
	return 0;
}

inline int aitString::copy(const char* p)
	{ return p?cset(p,strlen(p)):-1; }
inline int aitString::copy(char* p)
	{ return p?set(p,strlen(p)):-1; }
inline int aitString::copy(aitString* p)
	{ return p?set((char*)*p,p->length()):-1; }
inline int aitString::copy(aitString& p)
	{ return set((char*)p,p.length()); }
inline int aitString::copy(const aitString* p)
	{ return p?cset((const char*)*p,p->length()):-1; }
inline int aitString::copy(const aitString& p)
	{ return cset((const char*)p,p.length()); }

inline void aitString::replaceData(const char* p)
	{ if(p && str) strncpy(str,p,len); }
inline void aitString::replaceData(const aitString* p)
	{ if(p && str) strncpy(str,p->string(),len); }
inline void aitString::replaceData(aitString& p)
	{ if(str) strncpy(str,p.string(),len); }
inline void aitString::replaceData(const aitString& p)
	{ if(str) strncpy(str,p.string(),len); }
inline void aitString::extractString(char* p)
	{ if(p && str) strcpy(p,str); }

inline int aitString::installString(const char* p)
	{ int rc=0; if(isConstant()) replaceData(p); else rc=copy(p); return rc; }
inline int aitString::installString(char* p)
	{ int rc=0; if(isConstant()) replaceData(p); else rc=copy(p); return rc; }
inline int aitString::installString(aitString* p)
	{ int rc=0; if(isConstant()) replaceData(p); else rc=copy(p); return rc; }
inline int aitString::installString(aitString& p)
	{ int rc=0; if(isConstant()) replaceData(p); else rc=copy(p); return rc; }
inline int aitString::installString(const aitString* p)
	{ int rc=0; if(isConstant()) replaceData(p); else rc=copy(p); return rc; }
inline int aitString::installString(const aitString& p)
	{ int rc=0; if(isConstant()) replaceData(p); else rc=copy(p); return rc; }

inline aitString& aitString::operator=(const aitString& p)
	{ this->copy(p); return *this; }
inline aitString& aitString::operator=(const aitString* p)
	{ this->copy(p); return *this; }
inline aitString& aitString::operator=(aitString& p)
	{ this->copy(p); return *this; }
inline aitString& aitString::operator=(aitString* p)
	{ this->copy(p); return *this; }
inline aitString& aitString::operator=(const char* p)
	{ this->copy(p); return *this; }
inline aitString& aitString::operator=(char* p)
	{ this->copy(p); return *this; }

inline void aitString::init(void) { str=NULL; len=0u; type=aitStrConst; }

inline aitString::~aitString(void)
{
	// dump("~aitString");
	clear();
}
inline aitString::aitString(void) { init(); }

inline aitString::aitString(char* p)		{ init(); copy(p); }
inline aitString::aitString(aitString* p)	{ init(); copy(p); }
inline aitString::aitString(aitString& p)	{ init(); copy(p); }

inline aitString::aitString(const char* p)		{ init(); copy(p); }
inline aitString::aitString(const aitString* p)	{ init(); copy(p); }
inline aitString::aitString(const aitString& p)	{ init(); copy(p); }

#endif // aitHelpersInclude
