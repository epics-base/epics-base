
#include "server.h"

//
// casCtx::show()
//
void casCtx::show (unsigned level) const
{
	printf ("casCtx at %p\n", this);
	if (level >= 3u) {
		printf ("\tpMsg = %p\n", &this->msg);
		printf ("\tpData = %p\n", pData);
		printf ("\tpCAS = %p\n", pCAS);
		printf ("\tpClient = %p\n", pClient);
		printf ("\tpChannel = %p\n", pChannel);
		printf ("\tpPV = %p\n", pPV);
	}
}

