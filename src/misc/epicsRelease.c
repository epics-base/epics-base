/* $Id$
 * $Log$
 * Revision 1.4  1994/08/18  04:34:42  bordua
 * Added some spaces to make output look good.
 *
 * Revision 1.3  1994/07/17  10:37:48  bordua
 * Changed to use epicsReleaseVersion as a string.
 *
 * Revision 1.2  1994/07/17  08:26:28  bordua
 * Changed epicsVersion to epicsReleaseVersion.
 *
 * Revision 1.1  1994/07/17  06:55:41  bordua
 * Initial version.
 **/

#include    <epicsVersion.h>

    char *epicsRelease= "@(#)EPICS IOC CORE $Date$";
    char *epicsRelease1 = epicsReleaseVersion;
coreRelease()
{
    printf("############################################################################\n###  %s\n###  %s\n############################################################################\n", epicsRelease, epicsRelease1);
}
