/* base/src/libCompat  $Id$ */
/*-----------------------------------------------------------------------------
 * Copyright (c) 1993  Southeastern Universities Research Association,
 *             Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 * CEBAF Control Systems Group, 12000 Jefferson Ave., Newport News, VA 23606
 * Email:   Tel: (804) 249-7066  Fax: (804) 249-7049
 *-----------------------------------------------------------------------------

		            COPYRIGHT AND LICENSE

Copyright (c) 1991, 1992  Southeastern Universities Research Association,
		          Continuous Electron Beam Accelerator Facility,
			  12000 Jefferson Avenue, Newport News, VA 23606 

This material resulted from work developed under a United States Government 
Contract and is subject to the following license:

The Government retains a paid-up, nonexclusive, irrevocable worldwide license
to reproduce, prepare derivative works, perform publicly and display publicly
by or for the Government including the right to distribute to other Government
contractors. 


                  DISCLAIMER AND LIMITATION OF WARRANTY.                    
                                                                            
         ALL SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY. THERE           
         ARE NO WARRANTIES EXPRESS OR IMPLIED, INCLUDING ANY IMPLIED        
         WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR            
         PURPOSE. THERE IS NO WARRANTY THAT USE WILL NOT INFRINGE           
         ANY PATENT, COPYRIGHT OR TRADEMARK.                                
                                                                            
In consideration of the use of the software and other materials, user agrees
that neither the Government nor SURA/CEBAF will be liable for any damages with 
respect to such use, and user shall hold both the Government and SURA/CEBAF
harmless from and indemnify them against any and all liability for damages
arising out of the use of such software and other materials. In no event shall
the Government or SURA/CEBAF be liable whether arising under contract, tort,
strict liability or otherwise for any incidental, indirect or consequential loss
or damage of any nature arising at any time from any cause whatsoever. In
addition, the Government and SURA/CEBAF assume no obligation for defending
against third party claims or threats of claims arising as a result of user's
use of the software or materials either as delivered to user or as modified by
user.  

 *-----------------------------------------------------------------------------
 * 
 * Description:
 *		$Id$
 *	
 * Author: Pratik Gupta  , CEBAF Control Systems Group 
 * 
 * Revision History: 
 *   $Log$ 
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


