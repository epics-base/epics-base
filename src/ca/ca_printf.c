static char *sccsId = "@(#)ca_printf.c	1.2\t7/27/92";

#if defined(UNIX) || defined(VMS)
#	include <stdio.h>
#else
#  if defined(vxWorks)
# 	include <vxWorks.h>
#  endif
#endif
#include <varargs.h>


/*
 *
 *
 *	ca_printf()
 *
 *	Dump error messages to the appropriate place
 *
 */
int
ca_printf(va_alist)
va_dcl
{
	va_list		args;
	char		*pformat;
	int		status;

	va_start(args);

	pformat = va_arg(args, char *);

#	if defined(UNIX) || defined(VMS)
	{
		status = vfprintf(
				stderr,
				pformat,
				args);
	}
#	else
#	   if defined(vxWorks)
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
				logMsgArgs[5],
				logMsgArgs[6]);
			
	}
#	   else
		#### dont compile in this case ####
#	   endif
#	endif

	va_end(args);

	return status;
}
