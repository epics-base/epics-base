
#include "server.h"

//
// casCtx::show()
//
void casCtx::show (unsigned level) const
{
	printf ( "casCtx at %p\n", 
        static_cast <const void *> ( this ) );
	if (level >= 3u) {
		printf ("\tpMsg = %p\n", 
            static_cast <const void *> ( &this->msg ) );
		printf ("\tpData = %p\n", 
            static_cast <void *> ( pData ) );
		printf ("\tpCAS = %p\n", 
            static_cast <void *> ( pCAS ) );
		printf ("\tpClient = %p\n", 
            static_cast <void *> ( pClient ) );
		printf ("\tpChannel = %p\n", 
            static_cast <void *> ( pChannel ) );
		printf ("\tpPV = %p\n", 
            static_cast <void *> ( pPV ) );
	}
}

