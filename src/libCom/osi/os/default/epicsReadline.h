#ifndef _EPICS_READLINE_H
#define _EPICS_READLINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <shareLib.h>
#include <stdio.h>

epicsShareFunc char * epicsShareAPI epicsReadline (FILE *fp, const char *prompt);
epicsShareFunc void epicsShareAPI epicsStifleHistory (int n);
epicsShareFunc void epicsShareAPI epicsAddHistory (const char *line);
epicsShareFunc void epicsShareAPI epicsBindKeys (void);

#ifdef __cplusplus
}
#endif

#endif /* _EPICS_READLINE_H */
