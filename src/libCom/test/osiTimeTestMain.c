#include <stdio.h>

int osiTimeTest (void);

int main (int argc, char **argv)
{
#ifdef HP_UX
	_main();
#endif
	return osiTimeTest ();
}
