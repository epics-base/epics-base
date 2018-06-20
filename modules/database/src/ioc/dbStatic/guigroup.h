/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
	these are used in the pmt (prompt) field of the record support
	ascii files.  They represent field groupings for dct tools
*/

#ifndef __gui_group_h__
#define __gui_group_h__

#error As of Base 3.15.4, the promptgroup implementation has changed. \
    This header file (guigroup.h) is invalid and will be removed shortly. \
    Instead, you should include dbStaticLib.h, parse the DBD, \
    and use dbGetPromptGroupNameFromKey() and dbGetPromptGroupKeyFromName() \
    that have been added to dbStaticLib. \
    More details in the 3.15.4 release notes and the AppDev Guide.

#endif /*__gui_group_h__*/
