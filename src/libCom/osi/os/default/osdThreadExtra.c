/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author:  Ralph Lange   Date:  26 Jun 2012 */

/* Null default thread hooks for all platforms that do not do anything special */

EPICS_THREAD_HOOK_ROUTINE epicsThreadDefaultStartHook;
EPICS_THREAD_HOOK_ROUTINE epicsThreadDefaultExitHook;
