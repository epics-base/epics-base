
#include <stdio.h>
#include <time.h>

#include <winsock.h>

#include "tsDefs.h"
#include "osiTime.h"
#include "osiSleep.h"
#include "epicsAssert.h"

#ifndef CLOCK_REALTIME

	//
	// this is part of the POSIX RT standard but some OS 
	// still do not define this in time.h 
	//
	struct timespec {
		time_t tv_sec; /* seconds since some epoch */
		long tv_nsec; /* nanoseconds within the second */
	};
#endif


int main ()
{
	unsigned i;
	osiTime begin = osiTime::getCurrent();
	const unsigned iter = 100000u;
	TS_STAMP stamp;
	struct timespec ts;
	struct tm tmAnsi;
	tm_nano_sec ansiDate;
	char stampText[128];
	double diff;

	for (i=0; i<iter; i++) {
		osiTime tmp = osiTime::getCurrent();
	}

	osiTime end = osiTime::getCurrent();

	diff = end - begin;
	printf ("elapsed per call to osiTime::getCurrent() = %f\n", 
		diff/iter);

	stamp = begin;
	ansiDate = begin;
	ts = begin;

	tsStampToText (&stamp, TS_TEXT_MMDDYY, stampText);
	printf ("TS_STAMP = %s\n", stampText);
	printf ("struct tm = %s %f\n", asctime(&ansiDate.ansi_tm), 
		ansiDate.nsec/(double)osiTime::nSecPerSec);
	tmAnsi = *localtime (&ts.tv_sec);
	printf ("struct timespec = %s %f\n", asctime(&ansiDate.ansi_tm), 
		ts.tv_nsec/(double)osiTime::nSecPerSec);
	begin.show (0);

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
	// test struct tm conversions
	//
	ansiDate = begin;
	begin = ansiDate;
	assert (begin+diff==end);

	//
	// test struct timespec conversion
	//
	ts = begin;
	begin = ts;
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

    printf ("test complete\n");
    
	return 0;
}