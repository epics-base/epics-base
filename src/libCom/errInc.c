/* errInc.c  */
/* share/epicsH $Id$ */
/* defines headers containing status codes for epics */

/* common to epics Unix and vxWorks */
#include "calink.h"
#include "sdrHeader.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "dbRecType.h"
#include "dbRecords.h"
#include "devLib.h"
#include "drvEpvxi.h"
#include "devSup.h"
#include "drvSup.h"
#include "recSup.h"
#include "tsDefs.h"
#include "arAccessLib.h"
#include "sydDefs.h"
#include "drvGpibErr.h"
#include "drvBitBusErr.h"
#ifdef VXLIST
/* epics vxWorks  only*/
#endif VXLIST
