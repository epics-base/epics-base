
#include "exServer.h"
#include "fdManager.h"

//
// main()
// (example single threaded ca server tool main loop)
//
extern int main (int argc, const char **argv)
{
	osiTime		begin(osiTime::getCurrent());
	exServer	*pCAS;
	unsigned 	debugLevel = 0u;
	float		executionTime;
	char		pvPrefix[128] = "";
	unsigned	aliasCount = 1u;
	aitBool		forever = aitTrue;
	int		i;

	for (i=1; i<argc; i++) {
		if (sscanf(argv[i], "-d %u", &debugLevel)==1) {
			continue;
		}
		if (sscanf(argv[i],"-t %f", &executionTime)==1) {
			forever = aitFalse;
			continue;
		}
		if (sscanf(argv[i],"-p %127s", pvPrefix)==1) {
			continue;
		}
		if (sscanf(argv[i],"-c %u", &aliasCount)==1) {
			continue;
		}
		printf (
"usage: %s [-d<debug level> -t<execution time> -p<PV name prefix> -c<numbered alias count>]\n", 
			argv[0]);

		return (1);
	}

	pCAS = new exServer(pvPrefix, aliasCount);
	if (!pCAS) {
		return (-1);
	}

	pCAS->setDebugLevel(debugLevel);

	if (forever) {
		osiTime	delay(1000u,0u);
		//
		// loop here forever
		//
		while (aitTrue) {
			fileDescriptorManager.process(delay);
		}
	}
	else {
		osiTime total(executionTime);
		osiTime delay(osiTime::getCurrent() - begin);
		//
		// loop here untime the specified execution time
		// expires
		//
		while (delay < total) {
			fileDescriptorManager.process(delay);
			delay = osiTime::getCurrent() - begin;
		}
	}
	pCAS->show(2u);
	delete pCAS;
	return (0);
}

