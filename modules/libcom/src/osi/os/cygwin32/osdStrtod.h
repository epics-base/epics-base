/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * This header fragment is intended to be included as part of epicsString.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * epicsStrtod() for systems with broken strtod() routine
 */
LIBCOM_API double epicsStrtod(const char *str, char **endp); 

#ifdef __cplusplus
}
#endif
