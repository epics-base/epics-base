
#include <exServer.h>
#include <taskLib.h>

//
// main()
//
int excas (unsigned debugLevel=0u, unsigned delaySec=0)
{
	osiTime		begin(osiTime::getCurrent());
	exServer	*pCAS;

	pCAS = new exServer(32u,5u,500u);
	if (!pCAS) {
		return (-1);
	}

	pCAS->setDebugLevel(debugLevel);

	if (delaySec==0u) {
		//
		// loop here forever
		//
		while (aitTrue) {
			taskDelay(10);
		}
	}
	else {
		osiTime total( ((float)delaySec) );
		osiTime delay(osiTime::getCurrent() - begin);
		//
		// loop here untill the specified execution time
		// expires
		//
		while (delay < total) {
			taskDelay(10);
			delay = osiTime::getCurrent() - begin;
		}
	}
	delete pCAS;
	return (0);
}

