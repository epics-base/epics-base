/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

// Author: Jim Kowalkowski
// Date: 6/20/96
//
// $Id$
//
#define epicsExportSharedSymbols
#include "aitTypes.h"
#include "aitHelpers.h"

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

