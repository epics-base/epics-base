
#include <stdio.h>
#include <time.h>

#include "osiTime.h"
#include "osiThread.h"
#include "epicsAssert.h"


int main ()
{
	unsigned i;
	osiTime begin = osiTime::getCurrent();
	const unsigned iter = 100000u;
	TS_STAMP stamp;
	struct timespec ts;
	struct tm tmAnsi;
	tm_nano_sec ansiDate;
    unsigned long nanoSec;
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

    tsStampToTM (&tmAnsi, &nanoSec, &stamp);
	printf ("TS_STAMP = %s %lu nSec \n", asctime(&tmAnsi), nanoSec);

	printf ("struct tm = %s %f\n", asctime(&ansiDate.ansi_tm), 
		ansiDate.nSec/(double)osiTime::nSecPerSec);
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
		threadSleep (10.0);
	}
#endif

    printf ("test complete\n");
    
	return 0;
}
