
#include "server.h"

//
// casCtx::show()
//
void casCtx::show (unsigned level) const
{
	printf ("casCtx at %lx\n", (unsigned long) this);
	if (level >= 3u) {
		printf ("\tpMsg = %lx\n", (unsigned long) &this->msg);
		printf ("\tpData = %lx\n", (unsigned long) pData);
		printf ("\tpCAS = %lx\n", (unsigned long) pCAS);
		printf ("\tpClient = %lx\n", (unsigned long) pClient);
		printf ("\tpChannel = %lx\n", (unsigned long) pChannel);
		printf ("\tpPV = %lx\n", (unsigned long) pPV);
	}
}

