/*************************************************************************\
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef WRAPARRAYINDICES_H
#define WRAPARRAYINDICES_H

#include "dbCoreAPI.h"

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
DBCORE_API
long wrapArrayIndices(long *start, long increment, long *end, long no_elements);

#endif /* WRAPARRAYINDICES_H */
