/* epicsReadline.h */
/* Author:  Eric Norum Date: 18DEC2000 */

/*
 * Wrappers around readline calls to work around the fact that
 * on many machines readline.h declares functions with old-style
 * (i.e. non-prototype) syntax.  The old syntax is incompatible
 * with C++.
 */

#ifndef _EPICS_READLINE_H_
#define _EPICS_READLINE_H_

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc char * epicsShareAPI epics_readline (const char *prompt);
epicsShareFunc void epicsShareAPI epics_stifle_history (int n);
epicsShareFunc void epicsShareAPI epics_add_history (const char *line);
epicsShareFunc void epicsShareAPI epics_bind_keys (void);

#ifdef __cplusplus
}
#endif

#endif /* _EPICS_READLINE_H_ */
