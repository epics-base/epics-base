
#include <server.h>

//
// casCtx::show()
//
void casCtx::show (unsigned level) const
{
	printf ("casCtx at %x\n", (unsigned) this);
	if (level >= 1u) {
		printf ("\tpMsg = %x\n", (unsigned) &this->msg);
		printf ("\tpData = %x\n", (unsigned) pData);
		printf ("\tpCAS = %x\n", (unsigned) pCAS);
		printf ("\tpClient = %x\n", (unsigned) pClient);
		printf ("\tpChannel = %x\n", (unsigned) pChannel);
		printf ("\tpPV = %x\n", (unsigned) pPV);
	}
}

