

#include <osiTime.h>
#include <winsock.h>

void main ()
{
	unsigned i;
	osiTime begin = osiTime::getCurrent();
	const unsigned iter = 1000000u;

	for (i=0;i<iter;i++) {
		osiTime tmp = osiTime::getCurrent();
	}

	osiTime end = osiTime::getCurrent();

	printf("elapsed per call to osiTime::getCurrent() = %f\n", ((double) (end-begin))/iter);
}