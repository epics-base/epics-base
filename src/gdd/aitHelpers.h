#ifndef aitHelpersInclude
#define aitHelpersInclude

/*
 * Authors: Jeff Hill and Jim Kowalkowski
 * Date: 6/20/96
 *
 * $Id$
 *
 * $Log$
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
private:
	enum aitStrType {aitStrMalloc, aitStrConst};

	int set(const char* p)
	{
		int rc=0;
		if(p)
		{
			len=strlen(p); 
			str=new char[len+1];
			if(str)
			{
				strcpy(str, p);
				type=aitStrMalloc;
			}
			else
			{
				// If malloc fails set it to
				// an empty constant str
				str = "";
				len = 0u;
				type = aitStrConst;
				printf("aitString: no pool => continuing with nill str\n");
				rc=-1;
			}
		}
		else
		{
			str=NULL;
			len=0u; 
			type=aitStrConst;
		}
		return rc;
	}

	void cset(const char* p)
	{
		str=(char*)p;
		type=aitStrConst;
		if(str)
			len=strlen(str);
		else
			len = 0u;
	}

public:

	aitString(void)			 { cset((char*)NULL); }
	aitString(const char* x) { cset(x); }
	aitString(char* x)		 { cset(x); }
	~aitString(void)		 { clear(); }

	operator aitUint16(void)		{ return len; }
	operator const char*(void)		{ return str; }
	aitUint32 length(void)			{ return len; }
	const char* string(void) const	{ return str; }
	void force(char* x)				{ str=x; }
	void force(unsigned char* x)	{ str=(char*)x; }
	void force(unsigned long x)		{ str=(char*)x; }

	void clear(void)
	{
		if (str && type==aitStrMalloc)
		{
			delete [] str;
			type=aitStrConst;
			str=NULL;
			len=0;
		}
	}


	int installString(const char* p);
	int installString(char* p);

	void copy(const char* p)			{ clear(); cset(p); }
	void copy(char* p)					{ clear(); cset(p); }
	void replaceData(const char* p)		{ strncpy(str,p,len+1); }

	aitString& operator=(const char* p) { this->copy(p); return *this; }
	aitString& operator=(char* p)		{ this->copy(p); return *this; }

	// take the aitString array, and put it and all the string into buf,
	// return the total length the data copied
	static aitUint32 totalLength(aitString* array,aitIndex arraySize);
	static aitUint32 stringsLength(aitString* array,aitIndex arraySize);
	static aitIndex compact(aitString* array, aitIndex arraySize,
		void* buf, aitIndex bufSize);

private:
	char* str;
	aitUint16 len;
	aitUint16 type;	// aitStrType goes here
};

#endif // aitHelpersInclude
