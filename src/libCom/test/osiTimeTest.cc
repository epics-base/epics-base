
#include <stdio.h>

#include "tsDefs.h"
#include "osiTime.h"

int main ()
{
	unsigned i;
	osiTime begin = osiTime::getCurrent();
	const unsigned iter = 1000000u;
	TS_STAMP stamp;
	char stampText[128];

	for (i=0;i<iter;i++) {
		osiTime tmp = osiTime::getCurrent();
	}

	osiTime end = osiTime::getCurrent();

	printf ("elapsed per call to osiTime::getCurrent() = %f\n", 
		((double) (end-begin))/iter);

	stamp = osiTime::getCurrent();

	tsStampToText (&stamp, TS_TEXT_MMDDYY, stampText);

	printf ("osiTime::getCurrent() = %s\n", stampText);

	return 0;
}