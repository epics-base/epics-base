
#include <stdio.h>

#include "tsDefs.h"
#include "osiTime.h"
#include "osiSleep.h"

int main ()
{
	unsigned i;
	osiTime begin = osiTime::getCurrent();
	const unsigned iter = 100000u;
	TS_STAMP stamp;
	char stampText[128];

	for (i=0;i<iter;i++) {
		osiTime tmp = osiTime::getCurrent();
	}

	osiTime end = osiTime::getCurrent();

	osiTime diff = end - begin;
	printf ("elapsed per call to osiTime::getCurrent() = %f\n", 
		((double) diff)/iter);

	stamp = osiTime::getCurrent();

	tsStampToText (&stamp, TS_TEXT_MMDDYY, stampText);

	printf ("osiTime::getCurrent() = %s\n", stampText);

	osiTime copy = end;
	assert (copy==end);
	assert (copy<=end);
	assert (copy>=end);
	assert (end>begin);
	assert (end>=begin);
	assert (begin<end);
	assert (begin<=end);
	assert (begin!=end);
	assert (end-begin==diff);
	begin += diff;
	assert (begin==end);
	begin -= diff;
	assert (begin+diff==end);

	//
	// examine the drift
	//
#if 0
	while (1) {
		osiTime::synchronize();
		osiSleep (10 /* sec */, 0 /* uSec */);
	}
#endif

	return 0;
}