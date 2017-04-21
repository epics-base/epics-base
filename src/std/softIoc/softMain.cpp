/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2003 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to the Software License Agreement
* found in the file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author: Andrew Johnson	Date: 2003-04-08 */

/* Usage:
 *  softIoc [-D softIoc.dbd] [-h] [-S] [-s] [-a ascf]
 *	[-m macro=value,macro2=value2] [-d file.db]
 *	[-x prefix] [st.cmd]
 *
 *  If used the -D option must come first, and specify the
 *  path to the softIoc.dbd file.  The compile-time install
 *  location is saved in the binary as a default.
 *
 *  Usage information will be printed if -h is given, then
 *  the program will exit normally.
 *
 *  The -S option prevents an interactive shell being started
 *  after all arguments have been processed.
 *
 *  Previous versions accepted a -s option to cause a shell
 *  to be started; this option is still accepted but ignored
 *  since a command shell is now started by default.
 *
 *  Access Security can be enabled with the -a option giving
 *  the name of the configuration file; if any macros were
 *  set with -m before the -a option was given, they will be
 *  used as access security substitution macros.
 *
 *  Any number of -m and -d arguments can be interspersed;
 *  the macros are applied to the following .db files.  Each
 *  later -m option causes earlier macros to be discarded.
 *
 *  The -x option loads the softIocExit.db with the macro
 *  IOC set to the string provided.  This database contains
 *  a subroutine record named $(IOC):exit which has its field
 *  SNAM set to "exit".  When this record is processed, the
 *  subroutine that runs will call epicsExit() with the value
 *  of the field A determining whether the exit status is
 *  EXIT_SUCCESS if (A == 0.0) or EXIT_FAILURE (A != 0.0).
 *
 *  A st.cmd file is optional.  If any databases were loaded
 *  the st.cmd file will be run *after* iocInit.  To perform
 *  iocsh commands before iocInit, all database loading must
 *  be performed by the script itself, or by the user from
 *  the interactive IOC shell.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "registryFunction.h"
#include "epicsThread.h"
#include "epicsExit.h"
#include "epicsStdio.h"
#include "dbStaticLib.h"
#include "subRecord.h"
#include "dbAccess.h"
#include "asDbLib.h"
#include "iocInit.h"
#include "iocsh.h"
#include "epicsInstallDir.h"

extern "C" int softIoc_registerRecordDeviceDriver(struct dbBase *pdbbase);

#define DBD_FILE EPICS_BASE "/dbd/softIoc.dbd"
#define EXIT_FILE EPICS_BASE "/db/softIocExit.db"

const char *arg0;
const char *base_dbd = DBD_FILE;
const char *exit_db = EXIT_FILE;


static void exitSubroutine(subRecord *precord) {
    epicsExitLater((precord->a == 0.0) ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void usage(int status) {
    printf("Usage: %s [-D softIoc.dbd] [-h] [-S] [-a ascf]\n", arg0);
    puts("\t[-m macro=value,macro2=value2] [-d file.db]");
    puts("\t[-x prefix] [st.cmd]");
    puts("Compiled-in path to softIoc.dbd is:");
    printf("\t%s\n", base_dbd);
    epicsExit(status);
}


int main(int argc, char *argv[])
{
    char *dbd_file = const_cast<char*>(base_dbd);
    char *macros = NULL;
    char xmacro[PVNAME_STRINGSZ + 4];
    int startIocsh = 1;	/* default = start shell */
    int loadedDb = 0;
    
    arg0 = strrchr(*argv, '/');
    if (!arg0) {
	arg0 = *argv;
    } else {
	++arg0;	/* skip the '/' */
    }
    
    --argc, ++argv;
    
    /* Do this here in case the dbd file not available */
    if (argc>0 && **argv=='-' && (*argv)[1]=='h') {
	usage(EXIT_SUCCESS);
    }
    
    if (argc>1 && **argv=='-' && (*argv)[1]=='D') {
	dbd_file = *++argv;
	argc -= 2;
	++argv;
    }
    
    if (dbLoadDatabase(dbd_file, NULL, NULL)) {
	epicsExit(EXIT_FAILURE);
    }
    
    softIoc_registerRecordDeviceDriver(pdbbase);
    registryFunctionAdd("exit", (REGISTRYFUNCTION) exitSubroutine);

    while (argc>1 && **argv == '-') {
	switch ((*argv)[1]) {
	case 'a':
	    if (macros) asSetSubstitutions(macros);
	    asSetFilename(*++argv);
	    --argc;
	    break;
	
	case 'd':
	    if (dbLoadRecords(*++argv, macros)) {
		epicsExit(EXIT_FAILURE);
	    }
	    loadedDb = 1;
	    --argc;
	    break;
	
	case 'h':
	    usage(EXIT_SUCCESS);
	
	case 'm':
	    macros = *++argv;
	    --argc;
	    break;
	
	case 'S':
	    startIocsh = 0;
	    break;
	
	case 's':
	    break;
	
	case 'x':
	    epicsSnprintf(xmacro, sizeof xmacro, "IOC=%s", *++argv);
	    if (dbLoadRecords(exit_db, xmacro)) {
		epicsExit(EXIT_FAILURE);
	    }
	    loadedDb = 1;
	    --argc;
	    break;
	
	default:
	    printf("%s: option '%s' not recognized\n", arg0, *argv);
	    usage(EXIT_FAILURE);
	}
	--argc;
	++argv;
    }
    
    if (argc>0 && **argv=='-') {
	switch((*argv)[1]) {
	case 'a':
	case 'd':
	case 'm':
	case 'x':
	    printf("%s: missing argument to option '%s'\n", arg0, *argv);
	    usage(EXIT_FAILURE);
	
	case 'h':
	    usage(EXIT_SUCCESS);
	
	case 'S':
	    startIocsh = 0;
	    break;
	
	case 's':
	    break;
	
	default:
	    printf("%s: option '%s' not recognized\n", arg0, *argv);
	    usage(EXIT_FAILURE);
	}
	--argc;
	++argv;
    }
    
    if (loadedDb) {
	iocInit();
	epicsThreadSleep(0.2);
    }
    
    /* run user's startup script */
    if (argc>0) {
	if (iocsh(*argv)) epicsExit(EXIT_FAILURE);
	epicsThreadSleep(0.2);
	loadedDb = 1;	/* Give it the benefit of the doubt... */
    }
    
    /* start an interactive shell if it was requested */
    if (startIocsh) {
	iocsh(NULL);
    } else {
	if (loadedDb) {
	    epicsThreadExitMain();
	} else {
	    printf("%s: Nothing to do!\n", arg0);
	    usage(EXIT_FAILURE);
	}
    }
    epicsExit(EXIT_SUCCESS);
    /*Note that the following statement will never be executed*/
    return 0;
}
