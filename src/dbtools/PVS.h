#ifndef __PVS_H
#define __PVS_H

#include "BSlib.h"

#define PVS_RETRY_COUNT     4
#define PVS_TRANSFER_SIZE   1024
#define PVS_UDP 17
#define PVS_TCP 6
#define PVS_UDP_PORT 50298
#define PVS_TCP_PORT 50299
#define PVS_UDP_CPORT 50300

#define PVS_Request (BS_LAST_VERB+1)
#define PVS_Data	(BS_LAST_VERB+2)
#define PVS_Alive   (BS_LAST_VERB+3)

#endif
