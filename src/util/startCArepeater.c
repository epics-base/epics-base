/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	05-03-91
 */
/*+/mod***********************************************************************
* TITLE	startCArepeater.c - start up the Channel Access repeater task
*
* DESCRIPTION
*	This program exists to avoid having the UNIX Channel Access
*	repeater process masquerade as the process which caused it to
*	be created.  For example, if AR is the first Channel Access client
*	started after booting the workstation, then there will exist the
*	"real" AR process as well as the CA repeater process whose name
*	would also be "AR", since it is produced via a fork.
*
*	To avoid this problem, the script which starts AR first runs this
*	program.  Other programs which care could adopt the same strategy.
*
*-***************************************************************************/
#include <stdio.h>
#include <cadef.h>
int main()
{
    ca_task_initialize();
    return 0;
}
