/*************************************************************************\
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "arrayRangeModifier.h"
#include "dbAccessDefs.h"
#include "dbConvertFast.h"
#include "dbConvert.h"

typedef struct {
    long start;
    long incr;
    long end;
} arrayRangeModifier;

long wrapArrayIndices(long *start, const long increment,
    long *end, const long no_elements)
{
    if (*start < 0) *start = no_elements + *start;
    if (*start < 0) *start = 0;
    if (*start > no_elements) *start = no_elements;

    if (*end < 0) *end = no_elements + *end;
    if (*end < 0) *end = 0;
    if (*end >= no_elements) *end = no_elements - 1;

    if (*end - *start >= 0)
        return 1 + (*end - *start) / increment;
    else
        return 0;
}

static long handleArrayRangeModifier(DBADDR *paddr, short dbrType, const void
    *pbuffer, long nRequest, void *pvt, long offset, long available)
{
    arrayRangeModifier *pmod = (arrayRangeModifier *)pvt;
    long status = 0;
    long start = pmod->start;
    long end = pmod->end;
    /* Note that this limits the return value to be <= available */
    long n = wrapArrayIndices(&start, pmod->incr, &end, available);

    assert(pmod->incr > 0);
    if (pmod->incr > 1) {
        long i, j;
        long (*putCvt) (const void *from, void *to, const dbAddr * paddr) =
            dbFastPutConvertRoutine[dbrType][paddr->field_type];
        short dbr_size = dbValueSize(dbrType);
        if (nRequest > n)
            nRequest = n;
        for (i = 0, j = (offset + start) % paddr->no_elements; i < nRequest;
            i++, j = (j + pmod->incr) % paddr->no_elements) {
            status = putCvt(pbuffer + (i * dbr_size),
                paddr->pfield + (j * paddr->field_size), paddr);
        }
    } else {
        offset = (offset + start) % paddr->no_elements;
        if (nRequest > n)
            nRequest = n;
        if (paddr->no_elements <= 1) {
            status = dbFastPutConvertRoutine[dbrType][paddr->field_type] (pbuffer,
                paddr->pfield, paddr);
        } else {
            if (paddr->no_elements < nRequest)
                nRequest = paddr->no_elements;
            status = dbPutConvertRoutine[dbrType][paddr->field_type] (paddr,
                pbuffer, nRequest, paddr->no_elements, offset);
        }
    }
    return status;
}

long createArrayRangeModifier(dbAddrModifier *pmod, long start, long incr, long end)
{
    arrayRangeModifier *pvt =
        (arrayRangeModifier *) malloc(sizeof(arrayRangeModifier));
    if (incr <= 0) {
        return S_db_errArg;
    }
    if (!pvt) {
        return S_db_noMemory;
    }
    pvt->start = start;
    pvt->incr = incr;
    pvt->end = end;
    pmod->pvt = pvt;
    pmod->handle = handleArrayRangeModifier;
    return 0;
}

void deleteArrayRangeModifier(dbAddrModifier *pmod)
{
    if (pmod->pvt)
        free(pmod->pvt);
}
