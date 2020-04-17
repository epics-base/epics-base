/*************************************************************************\
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef ARRAYRANGEMODIFIER_H
#define ARRAYRANGEMODIFIER_H

#include "dbAddrModifier.h"
#include "shareLib.h"

/** @brief The array range address modifier. */

/** @brief Given a number of elements, convert negative start and end indices
 *         to non-negative ones by counting from the end of the array.
 *
 * @param start         Pointer to possibly negative start index
 * @param increment     Increment
 * @param end           Pointer to possibly negative end index
 * @param no_elements   Number of array elements
 * @return              Final number of elements
 */
epicsShareFunc
long wrapArrayIndices(long *start, long increment, long *end, long no_elements);

/** @brief Create an array range modifier from start index, increment, and end index
 *
 * @param pmod          Pointer to address modifier (user allocated)
 * @param start         Start index (possibly negative)
 * @param incr          Increment (must be positive)
 * @param end           End index (possibly negative)
 * @return              Status
 */
epicsShareFunc
long createArrayRangeModifier(dbAddrModifier *pmod, long start, long incr, long end);

/** @brief Alternative creation function that parses the data from a string.
 *
 * If successful, advances 'pstring' to point after the recognized address modifier.
 *
 * @param pmod          Pointer to uninitialized address modifier (user allocated)
 * @param pstring       Pointer to string to parse (in/out)
 * @return              0 (success) or -1 (failure)
 */
epicsShareFunc
long parseArrayRange(dbAddrModifier *pmod, const char **pstring);

/** @brief Extract the private data
 *
 * The 'pmod' argument must point to a valid array range address modifier.
 * The other aruments must point to appropriately sized storage.
 *
 * @param pmod          Pointer to uninitialized address modifier (user allocated)
 * @param pstart        Start index (out)
 * @param pincr         Increment (out)
 * @param pend          End index (out)
 */
epicsShareFunc
void getArrayRange(dbAddrModifier *pmod, long *pstart, long *pincr, long *pend);

/** @brief Free private memory associated with an array range modifier
 *
 * Note this does not free 'pmod' which is always user allocated.
 *
 * @param pmod          Pointer to address modifier (user allocated)
 */
epicsShareFunc
void deleteArrayRangeModifier(dbAddrModifier *pmod);

#endif /* ARRAYRANGEMODIFIER_H */
