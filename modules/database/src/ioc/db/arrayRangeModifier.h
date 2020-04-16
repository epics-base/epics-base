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
 * @return              Array range modifier private data
 */
epicsShareFunc
long createArrayRangeModifier(dbAddrModifier *pmod, long start, long incr, long end);

/** @brief Free private memory associated with an array range modifier
 *
 * @param pmod          Pointer to address modifier (user allocated)
 */
epicsShareFunc
void deleteArrayRangeModifier(dbAddrModifier *pmod);

#endif /* ARRAYRANGEMODIFIER_H */
