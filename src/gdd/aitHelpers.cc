
// Author: Jim Kowalkowski
// Date: 6/20/96
//
// $Id$
//
// $Log$
// Revision 1.2  1996/08/06 19:14:06  jbk
// Fixes to the string class.
// Changes units field to a aitString instead of aitInt8.
//
// Revision 1.1  1996/06/25 19:11:30  jbk
// new in EPICS base
//
//

#include "aitTypes.h"
#include "aitHelpers.h"

void aitString::mallocFailure(void)
{
	str="";
	len=0u;
	type=aitStrConst;
	fprintf(stderr,"aitString: no pool => continuing with nill str\n");
}

void aitString::dump(const char* p) const
{
	fprintf(stderr,"<%s>:",p);
	dump();
}

void aitString::dump(void) const
{
	fprintf(stderr,"this=%8.8x ",(int)this);
	if(str) fprintf(stderr,"string=%8.8x<%s>, ",(int)str,str);
	else	fprintf(stderr,"no string present, ");
	fprintf(stderr,"length=%d, ",(int)len);
	if(type==aitStrConst) fprintf(stderr,"type=Constant\n");
	else fprintf(stderr,"type=Allocated\n");
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
	memcpy(ptr,(char*)array,pos);

	for(i=0;i<arraySize;i++)
	{
		if(str[i].string())
		{
			memcpy(&ptr[pos],str[i].string(),str[i].length()+1); // include NULL
			str[i]=&ptr[pos];
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

