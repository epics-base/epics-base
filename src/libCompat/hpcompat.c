/*************************************************************************\
* Copyright (c) 1993  Southeastern Universities Research Association,
*             Continuous Electron Beam Accelerator Facility
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/libCompat  $Id$ */
 *-----------------------------------------------------------------------------
 * 
 * Description:
 *		$Id$
 *	
 * Author: Pratik Gupta  , CEBAF Control Systems Group 
 * 
 * Revision History: 
 *   $Log$
 *   Revision 1.1  1994/06/27 23:51:19  mcn
 *   HP compatibility library
 * 
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/param.h>

void dummy()
{}

#ifdef _HPUX_SOURCE

static char     *pSccsId = "@(#) $Id$ 05/04/94 Author: Pratik Gupta";

#define nusec_MAX_LIMIT	4000000
int usleep (_nusecs)
unsigned int _nusecs;
{
	struct timeval	tval;

	if( (_nusecs == (unsigned long) 0)
		|| _nusecs > (unsigned long) nusec_MAX_LIMIT )
	{
	errno = ERANGE;         /* value out of range */
	perror( "usleep time out of range ( 0 -> 4000000 ) " );
	return -1;
	}


	tval.tv_sec = _nusecs/1000000;
	tval.tv_usec = _nusecs % 1000000;
	if (select(0,NULL,NULL,NULL,&tval) < 0)
	{
		 perror( "usleep (select) failed" );
		 return -1;
	 }
}

/* This is an incomplete implementation of the realpath() function */

char * realpath(_getpath, _respath)
char * _getpath;
char * _respath;
{
struct stat buf;

if (stat(_getpath,&buf) < 0 )
	{
	return NULL ;
	};
lstat(_getpath,&buf);

if (S_ISLNK(buf.st_mode) )
	readlink(_getpath,_respath,MAXPATHLEN);
else 
	strcpy(_respath,_getpath);


return 0;
}

#endif


