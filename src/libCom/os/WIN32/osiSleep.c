
#include <windows.h>

#define epicsExportSharedSymbols
#include "osiSleep.h"

epicsShareFunc void epicsShareAPI osiSleep (unsigned sec, unsigned uSec)
{
	unsigned milliSec;

	milliSec = uSec / 1000;
	milliSec += sec * 1000;

	if (milliSec==0u) {
		milliSec = 1;
	}

	Sleep (milliSec); // time-out interval in milliseconds 
}