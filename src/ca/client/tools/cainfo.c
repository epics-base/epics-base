/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
*     Synchrotronstrahlung.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author: Ralph Lange (BESSY)
 *
 *  Modification History
 *  2008/04/16 Ralph Lange (BESSY)
 *     Updated usage info
 *  2009/04/01 Ralph Lange (HZB/BESSY)
 *     Clarified output for native data type
 *
 */

#include <stdio.h>
#include <epicsStdlib.h>

#include <cadef.h>
#include <epicsGetopt.h>

#include "tool_lib.h"

static unsigned statLevel = 0;  /* ca_client_status() interest level */


void usage (void)
{
    fprintf (stderr, "\nUsage: cainfo [options] <PV name> ...\n\n"
    "  -h: Help: Print this message\n"
    "Channel Access options:\n"
    "  -w <sec>:   Wait time, specifies CA timeout, default is %f second(s)\n"
    "  -s <level>: Call ca_client_status with the specified interest level\n"
    "  -p <prio>:  CA priority (0-%u, default 0=lowest)\n"
    "\nExample: cainfo my_channel another_channel\n\n"
             , DEFAULT_TIMEOUT, CA_PRIORITY_MAX);
}



/*+**************************************************************************
 *
 * Function:	cainfo
 *
 * Description:	Print CA info data or call ca_client_status
 *
 * Arg(s) In:	pvs       -  Pointer to an array of pv structures
 *              nPvs      -  Number of elements in the pvs array
 *
 * Return(s):	Error code: 0 = OK, 1 = Error
 *
 **************************************************************************-*/
 
int cainfo (pv *pvs, int nPvs)
{
    int n;
    long  dbfType;
    long  dbrType;
    unsigned long nElems;
    enum channel_state state;
    char *stateStrings[] = {
        "never connected", "previously connected", "connected", "closed" };
    char *boolStrings[] = { "no ", "" };

    if (statLevel) {
        ca_client_status(statLevel);

    } else {

        for (n = 0; n < nPvs; n++) {

                                /* Print the status data */
                                /* --------------------- */

            state   = ca_state(pvs[n].chid);
            nElems  = ca_element_count(pvs[n].chid);
            dbfType = ca_field_type(pvs[n].chid);
            dbrType = dbf_type_to_DBR(dbfType);

            printf("%s\n"
                   "    State:            %s\n"
                   "    Host:             %s\n"
                   "    Access:           %sread, %swrite\n"
                   "    Native data type: %s\n"
                   "    Request type:     %s\n"
                   "    Element count:    %lu\n"
                   , pvs[n].name,
                   stateStrings[state],
                   ca_host_name(pvs[n].chid),
                   boolStrings[ca_read_access(pvs[n].chid)],
                   boolStrings[ca_write_access(pvs[n].chid)],
                   dbf_type_to_text(dbfType),
                   dbr_type_to_text(dbrType),
                   nElems
                );
        }
    }

    return 0;
}



/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	cainfo main()
 * 		Evaluate command line options, set up CA, connect the
 * 		channels, print the data as requested
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
    int n;
    int result;                 /* CA result */

    int opt;                    /* getopt() current option */

    int nPvs;                   /* Number of PVs */
    pv* pvs;                    /* Array of PV structures */

    LINE_BUFFER(stdout);        /* Configure stdout buffering */

    while ((opt = getopt(argc, argv, ":nhw:s:p:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'w':               /* Set CA timeout value */
            if(epicsScanDouble(optarg, &caTimeout) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('cainfo -h' for help.)\n", optarg);
                caTimeout = DEFAULT_TIMEOUT;
            }
            break;
        case 's':               /* ca_client_status interest level */
            if (sscanf(optarg,"%du", &statLevel) != 1)
            {
                fprintf(stderr, "'%s' is not a valid interest level "
                        "- ignored. ('cainfo -h' for help.)\n", optarg);
                statLevel = 0;
            }
            break;
        case 'p':               /* CA priority */
            if (sscanf(optarg,"%u", &caPriority) != 1)
            {
                fprintf(stderr, "'%s' is not a valid CA priority "
                        "- ignored. ('cainfo -h' for help.)\n", optarg);
                caPriority = DEFAULT_CA_PRIORITY;
            }
            if (caPriority > CA_PRIORITY_MAX) caPriority = CA_PRIORITY_MAX;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('cainfo -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('cainfo -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    nPvs = argc - optind;       /* Remaining arg list are PV names */

    if (!statLevel && nPvs < 1)
    {
        fprintf(stderr, "No pv name specified. ('cainfo -h' for help.)\n");
        return 1;
    }
                                /* Start up Channel Access */

    result = ca_context_create(ca_disable_preemptive_callback);
    if (result != ECA_NORMAL) {
        fprintf(stderr, "CA error %s occurred while trying "
                "to start channel access.\n", ca_message(result));
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

    for (n = 0; optind < argc; n++, optind++)
        pvs[n].name = argv[optind] ;       /* Copy PV names from command line */

    result = connect_pvs(pvs, nPvs);

                                /* Print data */
    if (!result)
        result = cainfo(pvs, nPvs);

                                /* Shut down Channel Access */
    ca_context_destroy();

    return result;
}
