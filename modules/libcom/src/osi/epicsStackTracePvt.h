/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */ 

#ifndef INC_epicsStackTracePvt_H
#define INC_epicsStackTracePvt_H

#include "shareLib.h"

typedef struct epicsSymbol {
	const char *f_nam;  /* file where the symbol is defined  */
	const char *s_nam;  /* symbol name                       */
	void       *s_val;  /* symbol value                      */	
} epicsSymbol;

#ifdef __cplusplus
extern "C" {
#endif

/* Take a snapshot of the stack into 'buf' (limited to buf_sz entries)
 * RETURNS: actual number of entries in 'buf'
 */
epicsShareFunc int epicsBackTrace(void **buf, int buf_sz);

/* Find symbol closest to 'addr'.
 * 
 * If successful the routine fills in the members of *sym_p but
 * note that 'f_nam' and/or 's_nam' may be NULL if the address
 * cannot be resolved.
 *
 * RETURNS: 0 on success, nonzero on failure (not finding an address
 *          is not considered an error).
 */
epicsShareFunc int epicsFindAddr(void *addr, epicsSymbol *sym_p);

/* report supported features (as reported by epicsStackTraceGetFeatures) */
epicsShareFunc int epicsFindAddrGetFeatures();

#ifdef __cplusplus
}
#endif

#endif
