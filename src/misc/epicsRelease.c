/* $Id$
 * $Log$
 * Revision 1.6  1995/02/13  16:46:04  jba
 * Removed date from epicsRelease.
 *
 * Revision 1.5  1994/10/05  18:28:17  jba
 * Renamed version.h to epicsVersion.h
 *
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

#include    <stdlib.h>
#include    <stdio.h>
#include    <epicsVersion.h>

    char *epicsRelease= "@(#)EPICS IOC CORE";
    char *epicsRelease1 = epicsReleaseVersion;
int coreRelease()
{
    printf("############################################################################\n###  %s\n###  %s\n############################################################################\n", epicsRelease, epicsRelease1);
    return(0);
}
