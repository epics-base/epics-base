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

void epicsMathTest ()
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
}
