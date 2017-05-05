/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * This header is included by epicsString.h and epicsStdlib.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * epicsStrtod() for systems with broken strtod() routine
 */
double epicsStrtod(const char *str, char **endp);

/*
 * VxWorks doesn't provide these routines, so for now we do
 */

long long int strtoll(const char *nptr, char **endptr, int base);
unsigned long long int strtoull(const char *nptr, char **endptr, int base);

#ifdef __cplusplus
}
#endif
