/* $Id$
 * $Log$
 * Revision 1.1  1994/07/17  06:55:41  bordua
 * Initial version.
 **/

#include    <version.h>

    char *epicsRelease= "@(#)EPICS IOC CORE $Date$";

coreRelease()
{
    printf("############################################################################\n###  %s\n###%s\n############################################################################\n", epicsRelease, epicsReleaseVersion);
}
