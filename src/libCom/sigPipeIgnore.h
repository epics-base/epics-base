
/*
 * install NOOP SIGPIPE handler
 *
 * escape into C to call signal because of a brain dead
 * signal() func proto supplied in signal.h by gcc 2.7.2
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc void epicsShareAPI installSigPipeIgnore (void);

#ifdef __cplusplus
}
#endif

