
#include "exServer.h"
#include "fdManager.h"

//
// main()
// (example single threaded ca server tool main loop)
//
extern int main (int argc, const char **argv)
{
	osiTime     begin (osiTime::getCurrent());
	exServer    *pCAS;
	unsigned    debugLevel = 0u;
	double      executionTime;
	char        pvPrefix[128] = "";
	unsigned    aliasCount = 1u;
	unsigned    scanOnAsUnsignedInt = true;
    bool        scanOn;
	bool        forever = aitTrue;
	int         i;

	for (i=1; i<argc; i++) {
		if (sscanf(argv[i], "-d\t%u", &debugLevel)==1) {
			continue;
		}
		if (sscanf(argv[i],"-t %lf", &executionTime)==1) {
			forever = aitFalse;
			continue;
		}
		if (sscanf(argv[i],"-p %127s", pvPrefix)==1) {
			continue;
		}
		if (sscanf(argv[i],"-c %u", &aliasCount)==1) {
			continue;
		}
		if (sscanf(argv[i],"-s %u", &scanOnAsUnsignedInt)==1) {
			continue;
		}
		printf ("\"%s\"?\n", argv[i]);
		printf (
"usage: %s [-d<debug level> -t<execution time> -p<PV name prefix> -c<numbered alias count>] -s<1=scan on (default), 0=scan off]>\n", 
			argv[0]);

		return (1);
	}

    if (scanOnAsUnsignedInt) {
        scanOn = true;
    }
    else {
        scanOn = false;
    }

	pCAS = new exServer (pvPrefix, aliasCount, scanOn);
	if (!pCAS) {
		return (-1);
	}

	pCAS->setDebugLevel(debugLevel);

	if (forever) {
		//
		// loop here forever
		//
		while (aitTrue) {
			fileDescriptorManager.process(1000.0);
		}
	}
	else {
		double delay = osiTime::getCurrent() - begin;
		//
		// loop here untill the specified execution time
		// expires
		//
		while (delay < executionTime) {
			fileDescriptorManager.process(delay);
			delay = osiTime::getCurrent() - begin;
		}
	}
	pCAS->show(2u);
	delete pCAS;
	return (0);
}

