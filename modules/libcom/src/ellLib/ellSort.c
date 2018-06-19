/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc., as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2016 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Use of mergesort algorithm based on analysis by
 * http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
 */
#include <stdlib.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "ellLib.h"

static void ellMoveN(ELLLIST* pTo, ELLLIST* pFrom, int count )
{
    for(;count && ellCount(pFrom); count--) {
        ELLNODE *node = ellGet(pFrom);
        ellAdd(pTo, node);
    }
}

/* Stable (MergeSort) to given list.
 * The comparison function cmp(A,B) is expected
 * to return -1 for A<B, 0 for A==B, and 1 for A>B.
 */
void ellSortStable(ELLLIST *pList, pListCmp cmp)
{
    ELLLIST INP, P, Q;
    size_t insize = 1; /* initial sub-list size */
    if(ellCount(pList)<=1)
        return;

    ellInit(&INP);
    ellInit(&P);
    ellInit(&Q);

    /* Process is to iteratively sort
     * a sequence of sub-lists of size 'insize'
     */

    while(insize < ellCount(pList)) {

        assert(ellCount(&INP)==0);

        /* shift previous results to inputs */
        ellConcat(&INP, pList);

        while(ellCount(&INP))
        {
            ELLNODE *p, *q;

            /* Pull out the next pair of sub-lists */
            ellMoveN(&Q, &INP, insize);
            ellMoveN(&P, &INP, insize);

            /* merge these sub-lists */
            while((p=ellFirst(&P)) && (q=ellFirst(&Q)))
            {
                if((*cmp)(p,q) < 0) {
                    ellAdd(pList, ellGet(&P));
                } else {
                    ellAdd(pList, ellGet(&Q));
                }
            }

            /* concatenate any remaining to result */
            if(ellFirst(&P))
                ellConcat(pList, &P);
            else if(ellFirst(&Q))
                ellConcat(pList, &Q);

            assert(!ellFirst(&P) && !ellFirst(&Q));
        }

        insize *= 2;
    }

}
