/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
*     Synchrotronstrahlung.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* 
 *  $Id$
 *
 *  Author: Ralph Lange (BESSY)
 *
 */

#include <stdio.h>
#include <epicsStdlib.h>
#include <string.h>

#include <cadef.h>
#include <epicsGetopt.h>

#include "tool_lib.h"

#define VALID_DOUBLE_DIGITS 18  /* Max usable precision for a double */

static unsigned long reqElems = 0;
static unsigned long eventMask = DBE_VALUE | DBE_ALARM;   /* Event mask used */
static int floatAsString = 0;                             /* Flag: fetch floats as string */
static int nConn = 0;                                     /* Number of connected PVs */


void usage (void)
{
    fprintf (stderr, "\nUsage: camonitor [options] <PV name> ...\n\n"
    "  -h: Help: Print this message\n"
    "Channel Access options:\n"
    "  -w <sec>:  Wait time, specifies longer CA timeout, default is %f second\n"
    "  -m <mask>: Specify CA event mask to use, with <mask> being any combination of\n"
    "             'v' (value), 'a' (alarm), 'l' (log). Default: va\n"
    "Timestamps:\n"
    "  Default: Print absolute timestamps (as reported by CA)\n"
    "  -r: Relative timestamps (time elapsed since start of program)\n"
    "  -i: Incremental timestamps (time elapsed since last update)\n"
    "  -I: Incremental timestamps (time elapsed since last update for this channel)\n"
    "Enum format:\n"
    "  -n: Print DBF_ENUM values as number (default are enum string values)\n"
    "Arrays: Value format: print number of requested values, then list of values\n"
    "  Default:    Print all values\n"
    "  -# <count>: Print first <count> elements of an array\n"
    "Floating point type format:\n"
    "  Default: Use %%g format\n"
    "  -e <nr>: Use %%e format, with a precision of <nr> digits\n"
    "  -f <nr>: Use %%f format, with a precision of <nr> digits\n"
    "  -g <nr>: Use %%g format, with a precision of <nr> digits\n"
    "  -s:      Get value as string (may honour server-side precision)\n"
    "Integer number format:\n"
    "  Default: Print as decimal number\n"
    "  -0x: Print as hex number\n"
    "  -0o: Print as octal number\n"
    "  -0b: Print as binary number\n"
    "\nExample: camonitor -f8 my_channel another_channel\n"
    "  (doubles are printed as %%f with precision of 8)\n\n"
	, DEFAULT_TIMEOUT);
}



/*+**************************************************************************
 *
 * Function:	event_handler
 *
 * Description:	CA event_handler for request type callback
 * 		Allocates the dbr structure and copies the data
 *
 * Arg(s) In:	args  -  event handler args (see CA manual)
 *
 **************************************************************************-*/

void event_handler (evargs args)
{
    pv* pv = args.usr;

    pv->status = args.status;
    if (args.status == ECA_NORMAL)
    {
        pv->dbrType = args.type;
        memcpy(pv->value, args.dbr, dbr_size_n(args.type, args.count));

        print_time_val_sts(pv, pv->reqElems);
    }
}


/*+**************************************************************************
 *
 * Function:	connection_handler
 *
 * Description:	CA connection_handler 
 *
 * Arg(s) In:	args  -  connection_handler_args (see CA manual)
 *
 **************************************************************************-*/

void connection_handler ( struct connection_handler_args args )
{
    pv *ppv = ( pv * ) ca_puser ( args.chid );
    if ( args.op == CA_OP_CONN_UP ) {
        int dbrType;
                                /* Set up pv structure */
                                /* ------------------- */

                                /* Get natural type and array count */
        ppv->nElems  = ca_element_count(ppv->chid);
        ppv->dbfType = ca_field_type(ppv->chid);

                                /* Set up value structures */
        dbrType = dbf_type_to_DBR_TIME(ppv->dbfType); /* Use native type */
        if (dbr_type_is_ENUM(dbrType))                  /* Enums honour -n option */
        {
            if (enumAsNr) dbrType = DBR_TIME_INT;
            else          dbrType = DBR_TIME_STRING;
        }
        
        else if (floatAsString &&
                 (dbr_type_is_FLOAT(dbrType) || dbr_type_is_DOUBLE(dbrType)))
        {
            dbrType = DBR_TIME_STRING;
        }
                                /* Adjust array count */
        if (reqElems == 0 || ppv->nElems < reqElems)
            reqElems = ppv->nElems;
                                /* Remember dbrType and reqElems */
        ppv->dbrType  = dbrType;
        ppv->reqElems = reqElems;

        ppv->onceConnected = 1;
        nConn++;
                                /* Issue CA request */
                                /* ---------------- */
        /* install monitor once with first connect */
        if ( ! ppv->value ) {
                                    /* Allocate value structure */
            ppv->value = calloc(1, dbr_size_n(dbrType, reqElems));           
            if ( ppv->value ) {
                ppv->status = ca_create_subscription(dbrType,
                                                reqElems,
                                                ppv->chid,
                                                eventMask,
                                                event_handler,
                                                (void*)ppv,
                                                NULL);
                if ( ppv->status != ECA_NORMAL ) {
                    free ( ppv->value );
                }
            }
        }
    }
    else if ( args.op == CA_OP_CONN_DOWN ) {
        nConn--;
        ppv->status = ECA_DISCONN;
        print_time_val_sts(ppv, ppv->reqElems);
    }
}


/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	camonitor main()
 * 		Evaluate command line options, set up CA, connect the
 * 		channels, collect and print the data as requested
 *
 * Arg(s) In:	[options] <pv-name> ...
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Standard return code (0=success, 1=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    int returncode = 0;
    int n = 0;
    int result;                 /* CA result */

    int opt;                    /* getopt() current option */
    int digits = 0;             /* getopt() no. of float digits */

    int nPvs;                   /* Number of PVs */
    pv* pvs = 0;                /* Array of PV structures */

    setvbuf(stdout,NULL,_IOLBF,0);   /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":nhriIm:se:f:g:#:d:0:w:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'n':               /* Print ENUM as index numbers */
            enumAsNr=1;
            break;
        case 'r':               /* Select relative timestamps */
            tsType = relative;
            break;
        case 'i':               /* Select incremental timestamps */
            tsType = incremental;
            break;
        case 'I':               /* Select incremental timestamps (by channel) */
            tsType = incrementalByChan;
            break;
        case 'w':               /* Set CA timeout value */
            if(epicsScanDouble(optarg, &caTimeout) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('caget -h' for help.)\n", optarg);
                caTimeout = DEFAULT_TIMEOUT;
            }
            break;
        case '#':               /* Array count */
            if (sscanf(optarg,"%ld", &reqElems) != 1)
            {
                fprintf(stderr, "'%s' is not a valid array element count "
                        "- ignored. ('caget -h' for help.)\n", optarg);
                reqElems = 0;
            }
            break;
        case 'm':               /* Select CA event mask */
            eventMask = 0;
            {
                int i = 0;
                char c, err = 0;
                while ((c = optarg[i++]) && !err)
                    switch (c) {
                    case 'v': eventMask |= DBE_VALUE; break;
                    case 'a': eventMask |= DBE_ALARM; break;
                    case 'l': eventMask |= DBE_LOG; break;
                    default :
                        fprintf(stderr, "Invalid argument '%s' "
                                "for option '-m' - ignored.\n", optarg);
                        eventMask = DBE_VALUE | DBE_ALARM;
                        err = 1;
                    }
            }
            break;
        case 's':               /* Select string dbr for floating type data */
            floatAsString = 1;
            break;
        case 'e':               /* Select %e/%f/%g format, using <arg> digits */
        case 'f':
        case 'g':
            if (sscanf(optarg, "%d", &digits) != 1)
                fprintf(stderr, 
                        "Invalid precision argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            else
            {
                if (digits>=0 && digits<=VALID_DOUBLE_DIGITS)
                    sprintf(dblFormatStr, "%%-.%d%c", digits, opt);
                else
                    fprintf(stderr, "Precision %d for option '-%c' "
                            "out of range - ignored.\n", digits, opt);
            }
            break;
        case '0':               /* Select integer format */
            switch ((char) *optarg) {
            case 'x': outType = hex; break;    /* 0x print Hex */
            case 'b': outType = bin; break;    /* 0b print Binary */
            case 'o': outType = oct; break;    /* 0o print Octal */
            default :
                fprintf(stderr, "Invalid argument '%s' "
                        "for option '-0' - ignored.\n", optarg);
            }
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('caget -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('caget -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    nPvs = argc - optind;       /* Remaining arg list are PV names */

    if (nPvs < 1)
    {
        fprintf(stderr, "No pv name specified. ('camonitor -h' for help.)\n");
        return 1;
    }
                                /* Start up Channel Access */

    result = ca_context_create(ca_disable_preemptive_callback);
    if (result != ECA_NORMAL) {
        fprintf(stderr, "CA error %s occurred while trying "
                "to start channel access '%s'.\n", ca_message(result), pvs[n].name);
        return 1;
    }
                                /* Allocate PV structure array */

    pvs = calloc (nPvs, sizeof(pv));
    if (!pvs)
    {
        fprintf(stderr, "Memory allocation for channel structures failed.\n");
        return 1;
    }
                                /* Connect channels */

                                      /* Copy PV names from command line */
    for (n = 0; optind < argc; n++, optind++)
    {
        pvs[n].name   = argv[optind];
    }
                                      /* Create CA connections */
    returncode = create_pvs(pvs, nPvs, connection_handler);
    if ( returncode ) {
        return returncode;
    }
                                      /* Check for channels that didn't connect */
    ca_pend_event(caTimeout);
    for (n = 0; n < nPvs; n++)
    {
        if (!pvs[n].onceConnected)
            print_time_val_sts(&pvs[n], pvs[n].reqElems);
    }

                                /* Read and print data forever */
    ca_pend_event(0);

                                /* Shut down Channel Access */
    ca_context_destroy();

    return result;
}
