/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMathTest.c
 *
 *      Author  Marty Kraimer
 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "epicsMath.h"

static char *truth[2] = {"false","true"};

int epicsMathTest ()
{
    double a,b,c;

    a=0.0; b=0.0; c=a/b;
    printf("a %e b %e c %e isnan %s isinf %s\n",
        a,b,c,truth[isnan(c) ? 1 : 0],truth[isinf(c) ? 1 : 0]);
    a=1e300; b=1e-300; c=a/b;
    printf("a %e b %e c %e isnan %s isinf %s\n",
        a,b,c,truth[isnan(c) ? 1 : 0],truth[isinf(c) ? 1 : 0]);
    a=-1e300; b=1e-300; c=a/b;
    printf("a %e b %e c %e isnan %s isinf %s\n",
        a,b,c,truth[isnan(c) ? 1 : 0],truth[isinf(c) ? 1 : 0]);
    a=0.0; b=1.0; c=a/b;
    printf("a %e b %e c %e isnan %s isinf %s\n",
        a,b,c,truth[isnan(c) ? 1 : 0],truth[isinf(c) ? 1 : 0]);
    a=1e300; b=1e300; c=a/b;
    printf("a %e b %e c %e isnan %s isinf %s\n",
        a,b,c,truth[isnan(c) ? 1 : 0],truth[isinf(c) ? 1 : 0]);
    return(0);
}
