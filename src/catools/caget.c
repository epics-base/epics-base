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
 *  Author: Ralph Lange (BESSY)
 *
 *  Modification History
 *  2006/01/17 Malcolm Walters (Tessella/Diamond Light Source)
 *     Fixed problem with "-c -w 0" hanging forever
 *  2008/04/16 Ralph Lange (BESSY)
 *     Updated usage info
 *
 */

#include <stdio.h>
#include <epicsStdlib.h>
#include <string.h>

#include <alarm.h>
#include <cadef.h>
#include <epicsGetopt.h>

#include "tool_lib.h"

#define VALID_DOUBLE_DIGITS 18  /* Max usable precision for a double */
#define PEND_EVENT_SLICES 5     /* No. of pend_event slices for callback requests */

/* Different output formats */
typedef enum { plain, terse, all, specifiedDbr } OutputT;

/* Different request types */
typedef enum { get, callback } RequestT;

static int nConn = 0;           /* Number of connected PVs */
static int nRead = 0;           /* Number of channels that were read */
static int floatAsString = 0;   /* Flag: fetch floats as string */


void usage (void)
{
    fprintf (stderr, "\nUsage: caget [options] <PV name> ...\n\n"
    "  -h: Help: Print this message\n"
    "Channel Access options:\n"
    "  -w <sec>: Wait time, specifies CA timeout, default is %f second(s)\n"
    "  -c: Asynchronous get (use ca_get_callback and wait for completion)\n"
    "Format options:\n"
    "      Default output format is \"name value\"\n"
    "  -t: Terse mode - print only value, without name\n"
    "  -a: Wide mode \"name timestamp value stat sevr\" (read PVs as DBR_TIME_xxx)\n"
    "  -d <type>: Request specific dbr type; use string (DBR_ prefix may be omitted)\n"
    "      or number of one of the following types:\n"
    " DBR_STRING     0  DBR_STS_FLOAT    9  DBR_TIME_LONG   19  DBR_CTRL_SHORT    29\n"
    " DBR_INT        1  DBR_STS_ENUM    10  DBR_TIME_DOUBLE 20  DBR_CTRL_INT      29\n"
    " DBR_SHORT      1  DBR_STS_CHAR    11  DBR_GR_STRING   21  DBR_CTRL_FLOAT    30\n"
    " DBR_FLOAT      2  DBR_STS_LONG    12  DBR_GR_SHORT    22  DBR_CTRL_ENUM     31\n"
    " DBR_ENUM       3  DBR_STS_DOUBLE  13  DBR_GR_INT      22  DBR_CTRL_CHAR     32\n"
    " DBR_CHAR       4  DBR_TIME_STRING 14  DBR_GR_FLOAT    23  DBR_CTRL_LONG     33\n"
    " DBR_LONG       5  DBR_TIME_INT    15  DBR_GR_ENUM     24  DBR_CTRL_DOUBLE   34\n"
    " DBR_DOUBLE     6  DBR_TIME_SHORT  15  DBR_GR_CHAR     25  DBR_STSACK_STRING 37\n"
    " DBR_STS_STRING 7  DBR_TIME_FLOAT  16  DBR_GR_LONG     26  DBR_CLASS_NAME    38\n"
    " DBR_STS_SHORT  8  DBR_TIME_ENUM   17  DBR_GR_DOUBLE   27\n"
    " DBR_STS_INT    8  DBR_TIME_CHAR   18  DBR_CTRL_STRING 28\n"
    "Enum format:\n"
    "  -n: Print DBF_ENUM value as number (default is enum string)\n"
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
    "\nExample: caget -a -f8 my_channel another_channel\n"
    "  (uses wide output format, doubles are printed as %%f with precision of 8)\n\n"
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
    pv* ppv = args.usr;

    ppv->status = args.status;
    if (args.status == ECA_NORMAL)
    {
        ppv->dbrType = args.type;
        ppv->value   = calloc(1, dbr_size_n(args.type, args.count));
        memcpy(ppv->value, args.dbr, dbr_size_n(args.type, args.count));
        ppv->onceConnected = 1;
        nRead++;
    }
}



/*+**************************************************************************
 *
 * Function:	caget
 *
 * Description:	Issue read requests, wait for incoming data
 * 		and print the data according to the selected format
 *
 * Arg(s) In:	pvs       -  Pointer to an array of pv structures
 *              nPvs      -  Number of elements in the pvs array
 *              request   -  Request type
 *              format    -  Output format
 *              dbrType   -  Requested dbr type
 *              reqElems  -  Requested number of (array) elements
 *
 * Return(s):	Error code: 0 = OK, 1 = Error
 *
 **************************************************************************-*/
 
int caget (pv *pvs, int nPvs, RequestT request, OutputT format,
           chtype dbrType, unsigned long reqElems)
{
    unsigned int i;
    int n, result;

    for (n = 0; n < nPvs; n++) {

                                /* Set up pvs structure */
                                /* -------------------- */

                                /* Get natural type and array count */
        pvs[n].nElems  = ca_element_count(pvs[n].chid);
        pvs[n].dbfType = ca_field_type(pvs[n].chid);

                                /* Set up value structures */
        if (format != specifiedDbr)
        {
            dbrType = dbf_type_to_DBR_TIME(pvs[n].dbfType); /* Use native type */
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
        }
                                /* Adjust array count */
        if (reqElems == 0 || pvs[n].nElems < reqElems){
            pvs[n].reqElems = pvs[n].nElems; /* Use full number of points */
        } else {
            pvs[n].reqElems = reqElems; /* Limit to specified number */
        }

                                /* Remember dbrType */
        pvs[n].dbrType  = dbrType;

                                /* Issue CA request */
                                /* ---------------- */

        if (ca_state(pvs[n].chid) == cs_conn)
        {
            nConn++;
            pvs[n].onceConnected = 1;
            if (request == callback)
            {                          /* Event handler will allocate value */
                result = ca_array_get_callback(dbrType,
                                               pvs[n].reqElems,
                                               pvs[n].chid,
                                               event_handler,
                                               (void*)&pvs[n]);
            } else {
                                       /* Allocate value structure */
                pvs[n].value = calloc(1, dbr_size_n(dbrType, pvs[n].reqElems));
                result = ca_array_get(dbrType,
                                      pvs[n].reqElems,
                                      pvs[n].chid,
                                      pvs[n].value);
            }
            pvs[n].status = result;
        } else {
            pvs[n].status = ECA_DISCONN;
        }
    }
    if (!nConn) return 1;              /* No connection? We're done. */

                                /* Wait for completion */
                                /* ------------------- */

    result = ca_pend_io(caTimeout);
    if (result == ECA_TIMEOUT)
        fprintf(stderr, "Read operation timed out: some PV data was not read.\n");

    if (request == callback)    /* Also wait for callbacks */
    {
        if (caTimeout != 0)
        {
            double slice = caTimeout / PEND_EVENT_SLICES;
            for (n = 0; n < PEND_EVENT_SLICES; n++)
            {
                ca_pend_event(slice);
                if (nRead >= nConn) break;
            }
            if (nRead < nConn)
                fprintf(stderr, "Read operation timed out: some PV data was not read.\n");
        } else {
            /* For 0 timeout keep waiting until all are done */
            while (nRead < nConn) {
                  ca_pend_event(1.0);
            }
        }
    }

                                /* Print the data */
                                /* -------------- */

    for (n = 0; n < nPvs; n++) {
        reqElems = pvs[n].reqElems;

        switch (format) {
        case plain:             /* Emulate old caget behaviour */
            if (reqElems <= 1) printf("%-30s ", pvs[n].name);
            else               printf("%s", pvs[n].name);
        case terse:
            if (pvs[n].status == ECA_DISCONN)
                printf("*** not connected\n");
            else if (pvs[n].status == ECA_NORDACCESS)
                printf("*** no read access\n");
            else if (pvs[n].status != ECA_NORMAL)
                printf("*** CA error %s\n", ca_message(pvs[n].status));
            else if (pvs[n].value == 0)
                printf("*** no data available (timeout)\n");
            else
            {
                if (reqElems > 1) printf(" %lu ", reqElems);
                for (i=0; i<reqElems; ++i) {
                    if (i) printf (" ");
                    printf("%s", val2str(pvs[n].value, pvs[n].dbrType, i));
                }
                printf("\n");
            }
            break;
        case all:
            print_time_val_sts(&pvs[n], reqElems);
            break;
        case specifiedDbr:
            printf("%s\n", pvs[n].name);
            if (pvs[n].status == ECA_DISCONN)
                printf("    *** not connected\n");
            else if (pvs[n].status == ECA_NORDACCESS)
                printf("    *** no read access\n");
            else if (pvs[n].status != ECA_NORMAL)
                printf("    *** CA error %s\n", ca_message(pvs[n].status));
            else
            {
                printf("    Data type:      %s (native: %s)\n",
                       dbr_type_to_text(pvs[n].dbrType),
                       dbf_type_to_text(pvs[n].dbfType));
                if (pvs[n].dbrType == DBR_CLASS_NAME)
                    printf("    Class Name:     %s\n",
                           *((dbr_string_t*)dbr_value_ptr(pvs[n].value,
                                                          pvs[n].dbrType)));
                else {
                    printf("    Element count:  %lu\n"
                           "    Value:          ",
                           reqElems);
                    for (i=0; i<reqElems; ++i) {     /* Print value(s) */
                        if (i) printf (" ");
                        printf(" %s", val2str(pvs[n].value, pvs[n].dbrType, i));
                    }
                    printf("\n");
                    if (pvs[n].dbrType > DBR_DOUBLE) /* Extended type extra info */
                        printf("%s\n", dbr2str(pvs[n].value, pvs[n].dbrType));
                }
            }
            break;
        default :
            break;
        }
    }
    return 0;
}



/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	caget main()
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

void complainIfNotPlainAndSet (OutputT *current, const OutputT requested)
{
    if (*current != plain) 
        fprintf(stderr,
                "Options t,d,a are mutually exclusive. "
                "('caget -h' for help.)\n");
    *current = requested;
}

int main (int argc, char *argv[])
{
    int n = 0;
    int result;                 /* CA result */
    OutputT format = plain;     /* User specified format */
    RequestT request = get;     /* User specified request type */

    int count = 0;              /* 0 = not specified by -# option */
    int opt;                    /* getopt() current option */
    int type = -1;              /* getopt() data type argument */
    int digits = 0;             /* getopt() no. of float digits */

    int nPvs;                   /* Number of PVs */
    pv* pvs = 0;                /* Array of PV structures */

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":taicnhse:f:g:#:d:0:w:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 't':               /* Terse output mode */
            complainIfNotPlainAndSet(&format, terse);
            break;
        case 'a':               /* Wide output mode */
            complainIfNotPlainAndSet(&format, all);
            break;
        case 'c':               /* Callback mode */
            request = callback;
            break;
        case 'd':               /* Data type specification */
            complainIfNotPlainAndSet(&format, specifiedDbr);
                                /* Argument (type) may be text or number */
            if (sscanf(optarg, "%d", &type) != 1)
            {
                dbr_text_to_type(optarg, type);
                if (type == -1)                   /* Invalid? Try prefix DBR_ */
                {
                    char str[30] = "DBR_";
                    strncat(str, optarg, 25);
                    dbr_text_to_type(str, type);
                }
            }
            if (type < DBR_STRING       || type > DBR_CLASS_NAME
                || type == DBR_PUT_ACKT || type == DBR_PUT_ACKS)
            {
                fprintf(stderr, "Requested dbr type out of range "
                        "or invalid - ignored. ('caget -h' for help.)\n");
                format = plain;
            }
            break;
        case 'n':               /* Print ENUM as index numbers */
            enumAsNr = 1;
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
            if (sscanf(optarg,"%d", &count) != 1)
            {
                fprintf(stderr, "'%s' is not a valid array element count "
                        "- ignored. ('caget -h' for help.)\n", optarg);
                count = 0;
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
            type = DBR_LONG;
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
        fprintf(stderr, "No pv name specified. ('caget -h' for help.)\n");
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

    for (n = 0; optind < argc; n++, optind++)
        pvs[n].name = argv[optind] ;       /* Copy PV names from command line */

    connect_pvs(pvs, nPvs);

                                /* Read and print data */

    result = caget(pvs, nPvs, request, format, type, count);

                                /* Shut down Channel Access */
    ca_context_destroy();

    return result;
}
