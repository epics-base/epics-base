#ifndef epicsMathh
#define epicsMathh

#include <math.h>
#include <ieeefp.h>
extern int isnan(double x);
#define isinf(x) (((x)==(x)) && !finite((x)))
/* same as (!isnan(x) && !finite(x)) */

#endif /* epicsMathh */
