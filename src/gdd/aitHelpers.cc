
// Author: Jim Kowalkowski
// Date: 6/20/96
//
// $Id$
//
// $Log$
//

#include "aitTypes.h"
#include "aitHelpers.h"

aitIndex aitString::compact(aitString* array, aitIndex arraySize,
		void* buf, aitIndex bufSize)
{
	aitIndex i,j;
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

