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
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	5-88
 *
 */
#define GLBLSOURCE

static char *sccsId = "@(#) $Id$";

#include <vxWorks.h>
#include <types.h>
#include <socket.h>
#include <in.h>

#include "ellLib.h"
#include "db_access.h"
#include "server.h"

