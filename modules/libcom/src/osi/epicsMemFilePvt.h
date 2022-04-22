/*************************************************************************\
* Copyright (c) 2022 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef EPICSMEMFILEPVT_H
#define EPICSMEMFILEPVT_H

#include <ellLib.h>

#include "epicsStdio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* For use by osdMemOpen() implementations to ensure that epicsIMF* remains
 * valid if epicsMemUnmount() called before fclose()
 */
void epicsIMFInc(const epicsIMF *mf);
void epicsIMFDec(const epicsIMF *mf);

/* Called by epicsMemOpen(), should set errno when returning NULL.
 * "!binary" is text mode.
 */
FILE* osdMemOpen(const epicsIMF* file, int binary);

/* Uncompress into pre-allocated buffer.
 * Caller must already know uncompressed size.
 * Return zero on success
 */
int osiUncompressZ(const char* inbuf, size_t inlen,
                   char* outbuf, size_t outlen);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // EPICSMEMFILEPVT_H
