/* $Id$
 * $Log$
 * Revision 1.2  1994/07/17  08:26:28  bordua
 * Changed epicsVersion to epicsReleaseVersion.
 *
 * Revision 1.1  1994/07/17  06:55:41  bordua
 * Initial version.
 **/

#include    <version.h>

    char *epicsRelease= "@(#)EPICS IOC CORE $Date$";
    char *epicsRelease1 = epicsReleaseVersion;
coreRelease()
{
    printf("############################################################################\n###  %s\n###%s\n############################################################################\n", epicsRelease, epicsRelease1);
}
