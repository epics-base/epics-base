
/*  
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "resourceLib.h"

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class intId < unsigned, 8u, sizeof(unsigned)*CHAR_BIT >;

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif
