/************************************************************************/
/*                                                                      */
/*                            L O S  A L A M O S                        */
/*                      Los Alamos National Laboratory                  */
/*                       Los Alamos, New Mexico 87545                   */
/*                                                                      */
/*      Copyright, 1986, The Regents of the University of California.   */
/*                                                                      */
/*	Author								*/
/*	------								*/
/*	Jeff Hill							*/
/*                                                                      */
/*      History                                                         */
/*      -------                                                         */
/*                                                                      */
/*_begin                                                                */
/************************************************************************/
/*                                                                      */
/*      Title:  channel access TCPIP interface include file             */
/*      File:   ca_printf.c	                                       	*/
/*      Environment: VMS, UNIX, VRTX                                    */
/*                                                                      */
/*                                                                      */
/*      Purpose                                                         */
/*      -------                                                         */
/*                                                                      */
/*                                                                      */
/*                                                                      */
/*      Special comments                                                */
/*      ------- --------                                                */
/*                                                                      */
/************************************************************************/
/*_end                              					*/

static char *sccsId = "@(#) $Id$";

#include <stdio.h>
#include <stdarg.h>

#ifdef vxWorks
# 	include <vxWorks.h>
#	include <logLib.h>
#endif /*vxWorks*/



/*
 *
 *
 *	ca_printf()
 *
 *	Dump error messages to the appropriate place
 *
 */
int ca_printf(char *pformat, ...)
{
	va_list		args;
	int		status;

	va_start(args, pformat);

#if defined(vxWorks)
	status = mprintf(pformat, args);
#else
	status = vfprintf(stderr, pformat, args);
#endif 

	va_end(args);

	return status;
}
