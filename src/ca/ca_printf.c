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

static char *sccsId = "%W% %G%";

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
#ifdef __STDC__
int ca_printf(char *pformat, ...)
#else
int ca_printf(va_alist)
va_dcl
#endif
{
	va_list		args;
	int		status;

	va_start(args, pformat);

#if defined(vxWorks)
	{
		int	logMsgArgs[6];
		int	i;

		for(i=0; i< NELEMENTS(logMsgArgs); i++){
			logMsgArgs[i] = va_arg(args, int);	
		}

		status = logMsg(
				pformat,
				logMsgArgs[0],
				logMsgArgs[1],
				logMsgArgs[2],
				logMsgArgs[3],
				logMsgArgs[4],
				logMsgArgs[5]);
			
	}
#else
	status = vfprintf(
			stderr,
			pformat,
			args);
#endif 

	va_end(args);

	return status;
}
