#ifndef epicsMathh
#define epicsMathh

#include <math.h>
#include <float.h>

#define isnan(D) _isnan(D)
#define isinf(D) ( !_finite(D) && !_isnan(D) ) 

#endif /* epicsMathh */
