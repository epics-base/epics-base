#include <stdio.h>

extern "C" int epicsTimeTest (void);

int main (int , char **)
{
	return epicsTimeTest ();
}
