/* $Id$
 * $Log$*/

#include    <version.h>

    char *epicsRelease= "@(#)EPICS IOC CORE $Date$";

coreRelease()
{
    printf("############################################################################\n###  %s\n###%s\n############################################################################\n", epicsRelease, epicsVersion);
}
