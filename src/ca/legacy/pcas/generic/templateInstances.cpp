/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#define epicsExportSharedSymbols
#include "ipIgnoreEntry.h"
#include "casChannelI.h"

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class resTable < ipIgnoreEntry, ipIgnoreEntry >;
template class resTable < casChannelI, chronIntId >;
template class resTable < casEventMaskEntry, stringId >;
template class chronIntIdResTable < casChannelI >;
template class tsFreeList < casMonEvent, 1024, epicsMutexNOOP >;
template class tsFreeList < casMonitor, 1024, epicsMutex >;


#ifdef _MSC_VER
#   pragma warning ( pop )
#endif
