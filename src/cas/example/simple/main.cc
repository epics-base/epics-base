
#include <exServer.h>
#include <fdManager.h>

//
// main()
// (example single threaded ca server tool main loop)
//
int main (int argc, const char **argv)
{
	osiTime		begin(osiTime::getCurrent());
	exServer	*pCAS;
	unsigned 	debugLevel = 0u;
	float		executionTime;
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
		printf ("usage: %s -d<debug level> -t<execution time>\n", 
			argv[0]);
		return (1);
	}

	pCAS = new exServer(32u,5u,500u);
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
	delete pCAS;
	return (0);
}

