
// Author: Jim Kowalkowski
// Date: 6/20/96
//
// $Id$
//
// $Log$
// Revision 1.11  1999/05/10 23:38:33  jhill
// convert to and from other time stamp formats
//
// Revision 1.10  1999/05/03 16:20:51  jhill
// allow aitTimeStamp to convert to TS_STAMP (without binding to libCom)
//
// Revision 1.9  1998/05/05 21:08:26  jhill
// fixed warning
//
// Revision 1.8  1997/08/05 00:51:05  jhill
// fixed problems in aitString and the conversion matrix
//
// Revision 1.7  1997/06/25 06:17:33  jhill
// fixed warnings
//
// Revision 1.6  1997/04/23 17:12:54  jhill
// fixed export of symbols from WIN32 DLL
//
// Revision 1.5  1996/08/23 20:29:34  jbk
// completed fixes for the aitString and fixed string management
//
// Revision 1.4  1996/08/22 21:05:38  jbk
// More fixes to make strings and fixed string work better.
//
// Revision 1.3  1996/08/09 02:28:08  jbk
// rewrite of aitString class - more intuitive now, I think
//
// Revision 1.2  1996/08/06 19:14:06  jbk
// Fixes to the string class.
// Changes units field to a aitString instead of aitInt8.
//
// Revision 1.1  1996/06/25 19:11:30  jbk
// new in EPICS base
//
//

#define epicsExportSharedSymbols
#include "aitTypes.h"
#include "aitHelpers.h"

//
// 1/1/90 20 yr (5 leap) of seconds
//
const unsigned aitTimeStamp::epicsEpochSecPast1970 = 7305 * 86400; 

void aitString::mallocFailure(void)
{
	str=(char *)"";
	len=0u;
	bufLen=1u;
	type=aitStrRefConstImortal;
	fprintf(stderr,"aitString: no pool => continuing with zero char str\n");
}

void aitString::dump(const char* p) const
{
	fprintf(stderr,"<%s>:",p);
	dump();
}

void aitString::dump(void) const
{
	fprintf(stderr,"this=%p ", this);
	if(str) fprintf(stderr,"string=%p<%s>, ",str,str);
	else	fprintf(stderr,"no string present, ");
	fprintf(stderr,"length=%u, ",len);
	fprintf(stderr,"buf length=%u, ",bufLen);
	if(type==aitStrRefConstImortal) fprintf(stderr,"type=Imortal Constant Reference\n");
	else if(type==aitStrRefConst) fprintf(stderr,"type=Constant Reference\n");
	else if(type==aitStrRef) fprintf(stderr,"type=Reference\n");
	else if(type==aitStrCopy) fprintf(stderr,"type=Allocated\n");
	else fprintf(stderr,"type=Invalid\n");
}

aitIndex aitString::compact(aitString* array, aitIndex arraySize,
		void* buf, aitIndex bufSize)
{
	aitIndex i;
	aitUint32 pos;
	char* ptr=(char*)buf;
	aitString* str=(aitString*)buf;

	// copy the array first
	pos=sizeof(aitString)*arraySize;
	if(bufSize<pos) return 0;

	for(i=0;i<arraySize;i++) str[i].init();

	for(i=0;i<arraySize;i++)
	{
		if((pos+str[i].length())>bufSize) break; // quick exit from loop
		if(array[i].string())
		{
			memcpy(&ptr[pos],array[i].string(),array[i].length()+1u);
			str[i].installBuf(&ptr[pos], array[i].length(), array[i].length()+1u);
			pos+=str[i].length()+1;
		}
	}
	return pos;
}

aitUint32 aitString::totalLength(aitString* array, aitIndex arraySize)
{
	aitIndex i;
	aitUint32 tot;

	tot=sizeof(aitString)*arraySize;
	for(i=0;i<arraySize;i++)
		tot+=array[i].length()+1; // include the NULL

	return tot;
}

aitUint32 aitString::stringsLength(aitString* array, aitIndex arraySize)
{
	aitIndex i;
	aitUint32 tot;

	for(tot=0,i=0;i<arraySize;i++)
		tot+=array[i].length()+1; // include the NULL

	return tot;
}

int aitString::copy(const char* p, unsigned newStrLength, unsigned bufSizeIn)
{
	if (newStrLength>=bufSizeIn) {
		return -1;
	}

	if (this->type==aitStrRefConst || this->type==aitStrRefConstImortal 
			|| bufSizeIn>this->bufLen) {

		char *pStrNew;
		pStrNew = new char [bufSizeIn];
		if (!pStrNew) {
			mallocFailure();
			return -1;
		}
		if (this->type==aitStrCopy) {
			delete [] this->str;
		}
		this->str = pStrNew;
		this->bufLen = bufSizeIn;
		this->type = aitStrCopy;
	}
	// test above verifies that bufLen exceeds 
	// length of p
	strncpy (this->str,p,this->bufLen);
	this->len = newStrLength;
	return 0;
}

int aitString::init(const char* p, aitStrType typeIn, unsigned strLengthIn, unsigned bufSizeIn)		
{ 
	int rc;

	this->init();
	switch (typeIn) {
	case aitStrCopy:
		rc = this->copy(p, strLengthIn, bufSizeIn);
		break;
	case aitStrRef:
		rc = this->installBuf(p, strLengthIn, bufSizeIn);
		break;
	case aitStrRefConst:
		rc = this->installConstBuf(p, strLengthIn, bufSizeIn);
		break;
	case aitStrRefConstImortal:
		rc = this->installConstImortalBuf(p, strLengthIn, bufSizeIn);
		break;
	default:
		rc=-1;
		break;
	}
	return rc;
}

//
// allow this module to include code that can convert
// to an EPICS time stamp, but dont force this library
// to link with libCom
//

struct TS_STAMP {
	aitUint32    secPastEpoch;   /* seconds since 0000 Jan 1, 1990 */
	aitUint32    nsec;           /* nanoseconds within second */
};

aitTimeStamp::operator struct TS_STAMP () const
{
	TS_STAMP ts;

	if (this->tv_sec>aitTimeStamp::epicsEpochSecPast1970) {
		ts.secPastEpoch = this->tv_sec - aitTimeStamp::epicsEpochSecPast1970;
		ts.nsec = this->tv_nsec;
	}
	else {
		ts.secPastEpoch = 0;
		ts.nsec = 0;
	}
	return ts;
}

void aitTimeStamp::get (struct TS_STAMP &ts) const
{
	if (this->tv_sec>aitTimeStamp::epicsEpochSecPast1970) {
		ts.secPastEpoch = this->tv_sec - aitTimeStamp::epicsEpochSecPast1970;
		ts.nsec = this->tv_nsec;
	}
	else {
		ts.secPastEpoch = 0;
		ts.nsec = 0;
	}
}

aitTimeStamp::aitTimeStamp (const struct TS_STAMP &ts) 
{
	this->tv_sec = ts.secPastEpoch + aitTimeStamp::epicsEpochSecPast1970;
	this->tv_nsec = ts.nsec;
}

aitTimeStamp aitTimeStamp::operator = (const struct TS_STAMP &rhs)
{
	this->tv_sec = rhs.secPastEpoch + aitTimeStamp::epicsEpochSecPast1970;
	this->tv_nsec = rhs.nsec;
	return *this;
}

//
// allow this module to include code that can convert
// to and from a POSIX timespec, but allow this library
// to compile on systems that dont support it
//
struct timespec
{
	unsigned long tv_sec;
	unsigned long tv_nsec;
};

aitTimeStamp::operator struct timespec () const
{
	struct timespec ts;
	ts.tv_sec = (unsigned long) this->tv_sec; 
	ts.tv_nsec = (unsigned long) this->tv_nsec; 
	return ts;
}

void aitTimeStamp::get (struct timespec &ts) const
{
	ts.tv_sec = (unsigned long) this->tv_sec; 
	ts.tv_nsec = (unsigned long) this->tv_nsec; 
}

aitTimeStamp::aitTimeStamp (const struct timespec &ts)
{
	this->tv_sec = (aitUint32) ts.tv_sec;
	this->tv_nsec = (aitUint32) ts.tv_nsec;
}

aitTimeStamp aitTimeStamp::operator = (const struct timespec &rhs)
{
	this->tv_sec = (aitUint32) rhs.tv_sec;
	this->tv_nsec = (aitUint32) rhs.tv_nsec;
	return *this;
}
