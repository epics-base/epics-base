
/* globalsource.c */
/* share/src/rsrv $Id$ */

/*
*******************************************************************************
**                              GTA PROJECT
**      Copyright 1988, The Regents of the University of California.
**                      Los Alamos National Laboratory
**                      Los Alamos New Mexico 87845
**      inmsgtask0.c - GTA request server message reader task.
**      Sun UNIX 4.2 Release 3.4
**      Bob Dingler February 11, 1988
*******************************************************************************
*/
#define GLBLSOURCE


#include <vxWorks.h>
#include <lstLib.h>
#include <types.h>
#include <socket.h>
#include <in.h>
#include <db_access.h>
#include <server.h>
