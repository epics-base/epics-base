
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#define epicsExportSharedSymbols

#include "dbCAC.h"
#include "dbChannelIO.h"
#include "dbPutNotifyBlocker.h"


#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class tsFreeList < dbChannelIO >;
template class epicsSingleton < tsFreeList < dbChannelIO > >;
template class tsFreeList < dbPutNotifyBlocker, 1024 >;
template class epicsSingleton < tsFreeList < dbPutNotifyBlocker, 1024 > >;
template class resTable < dbBaseIO, chronIntId >;
template class chronIntIdResTable < dbBaseIO >;
template class tsFreeList < dbSubscriptionIO >;
template class epicsSingleton < tsFreeList < dbSubscriptionIO > >;

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif
