/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	05-03-91
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	05-03-91	rac	initial version
 * .02	07-30-91	rac	installed in SCCS
 *
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
