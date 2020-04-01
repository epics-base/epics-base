#ifndef DBADDRMODIFIER_H
#define DBADDRMODIFIER_H

#include "shareLib.h"

typedef struct {
    long start;
    long incr;
    long end;
} dbAddrModifier;

/** @brief The address modifier that modifies nothing. */
#define defaultAddrModifier (struct dbAddrModifier){0,1,-1}

/** @brief Given a number of elements, convert negative start and end indices
 *         to non-negative ones by counting from the end of the array.
 *
 * @param start         Pointer to possibly negative start index.
 * @param increment     Increment.
 * @param end           Pointer to possibly negative end index.
 * @param no_elements   Number of array elements.
 * @return              Final number of elements.
 */
epicsShareFunc long wrapArrayIndices(long *start, const long increment,
    long *end, const long no_elements);

#endif /* DBADDRMODIFIER_H */
