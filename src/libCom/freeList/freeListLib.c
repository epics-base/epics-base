/* $Id$ */
/* Author:  Marty Kraimer Date:    04-19-94 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 *
 * Modification Log:
 * -----------------
 * .01  04-19-94	mrk	Initial Implementation
 */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <freeList.h>

void freeListInitPvt(void **ppvt,int size,int nmalloc)
{
    FREELISTPVT	*pfl;
    pfl = (void *)calloc((size_t)1,(size_t)sizeof(FREELISTPVT));
    if(!pfl) {
#ifdef vxWorks
	taskSuspend(0);
#else
	abort();
#endif
    }
    pfl->size = size;
    pfl->nmalloc = nmalloc;
    pfl->head = NULL;
#ifdef vxWorks
    FASTLOCKINIT(&pfl->lock);
#endif
    *ppvt = (void *)pfl;
    return;
}

void *freeListCalloc(void *pvt)
{
    FREELISTPVT *pfl = pvt;
    void	*ptemp;

    ptemp = freeListMalloc(pvt);
    if(ptemp) memset((char *)ptemp,0,pfl->size);
    return(ptemp);
}

void *freeListMalloc(void *pvt)
{
    FREELISTPVT *pfl = pvt;
    void	*ptemp;
    void	**ppnext;
    int		i;

#ifdef vxWorks
    FASTLOCK(&pfl->lock);
#endif
    ptemp = pfl->head;
    if(!ptemp) {
	ptemp = (void *)malloc(pfl->nmalloc*pfl->size);
	if(!ptemp) {
#ifdef vxWorks
	    FASTUNLOCK(&pfl->lock);
#endif
	    return(ptemp);
	}
	for(i=0; i<pfl->nmalloc; i++) {
	    ppnext = ptemp;
	    *ppnext = pfl->head;
	    pfl->head = ptemp;
	    ptemp = ((char *)ptemp) + pfl->size;
	}
	ptemp = pfl->head;
    }
    ppnext = pfl->head;
    pfl->head = *ppnext;
#ifdef vxWorks
    FASTUNLOCK(&pfl->lock);
#endif
    return(ptemp);
}

void freeListFree(void *pvt,void*pmem)
{
    FREELISTPVT	*pfl = pvt;
    void	**ppnext;

#ifdef vxWorks
    FASTLOCK(&pfl->lock);
#endif
    ppnext = pmem;
    *ppnext = pfl->head;
    pfl->head = pmem;
#ifdef vxWorks
    FASTUNLOCK(&pfl->lock);
#endif
}
