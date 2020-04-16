/*************************************************************************\
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef DBADDRMODIFIER_H
#define DBADDRMODIFIER_H

#include "dbAddr.h"

/** @brief Generic API for address modifiers. */

/** @brief Type of functions that handle an address modifier.
 *
 * This function should write the value in 'pbuffer' to the target 'pAddr',
 * according to the number of requested elements 'nRequest', the requested type
 * 'dbrType', and the address modifier private data 'pvt'. It may also take
 * into account the actual number of elements 'available' in the target, and
 * the 'offset', both of which usually result from a previous call to
 * 'get_array_info'.
 * The targeted record must be locked. Furthermore, the lock must not be given
 * up between the call to 'get_array_info' and call to this function, otherwise
 * 'offset' and 'available' may be out of sync.
 *
 * @param paddr         Target (field) address
 * @param dbrType       Requested (element) type
 * @param pbuffer       Data requested to be written
 * @param nRequest      Number of elements in pbuffer
 * @param pvt           Private modifier data
 * @param offset        Current offset in the target field
 * @param available     Current number of elements in the target field
 * @return              Status
 */
typedef long dbHandleAddrModifier(DBADDR *paddr, short dbrType,
    const void *pbuffer, long nRequest, void *pvt, long offset,
    long available);

/** @brief Structure of an address modifier.
 *
 * This will normally be allocated by user code. An address modifier
 * implementation should supply a function to create the private data 'pvt'
 * and initialize 'handle' with a function to handle the modifier.
 */
typedef struct {
    void *pvt;                      /** @brief Private modifier data */
    dbHandleAddrModifier *handle;   /** @brief Function to handle the modifier */
} dbAddrModifier;

#endif /* DBADDRMODIFIER_H */
