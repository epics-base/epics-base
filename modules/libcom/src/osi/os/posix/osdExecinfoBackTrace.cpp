/*
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * SPDX-License-Identifier: EPICS
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */

// pull in libc feature test macros
#include <stdlib.h>

// execinfo.h may not be present if uclibc is configured to omit backtrace()
// some C libraries, such as musl, don't have execinfo.h at all
#if defined(__has_include)
#  if __has_include(<execinfo.h>)
#    define HAS_EXECINFO 1
#  endif
#elif !defined(__UCLIBC_MAJOR__) || defined(__UCLIBC_HAS_EXECINFO__)
#  define HAS_EXECINFO 1
#endif

#if HAS_EXECINFO

#include <execinfo.h>

#endif

#include "epicsStackTracePvt.h"

int epicsBackTrace(void **buf, int buf_sz)
{
#if HAS_EXECINFO
    return backtrace(buf, buf_sz);
#else
    return -1;
#endif
}
