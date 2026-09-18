/* asLibPvt.h - Private Access Security test interfaces */

#ifndef INC_asLibPvt_H
#define INC_asLibPvt_H

#include "libComAPI.h"
#include "epicsTime.h"
#include "osiSock.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (epicsStdCall *asHagResolver)(const char *name, unsigned short port,
    struct sockaddr_in *address);
typedef epicsUInt64 (*asHagClock)(void);

/* Private deterministic test seams.  This header is not installed. */
LIBCOM_API void asTestSetHagResolver(asHagResolver resolver);
LIBCOM_API void asTestSetHagClock(asHagClock clock);
LIBCOM_API void asTestResetHagHooks(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_asLibPvt_H */
