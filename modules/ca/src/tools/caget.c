/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2006 Diamond Light Source Ltd.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
*     Synchrotronstrahlung.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
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
 *  2009/03/31 Larry Hoff (BNL)
 *     Added field separators
 *  2009/04/01 Ralph Lange (HZB/BESSY)
 *     Added support for long strings (array of char) and quoting of nonprintable characters
 *
 */

#include <stdio.h>
#include <string.h>
#include <epicsStdlib.h>
#include <epicsString.h>

#include <alarm.h>
#include <cadef.h>
#include <epicsGetopt.h>
#include "epicsVersion.h"
#include "epicsMutex.h"
#include "epicsExit.h"
#include "epicsSpin.h"

#include "tool_lib.h"

#define VALID_DOUBLE_DIGITS 18  /* Max usable precision for a double */
#define PEND_EVENT_SLICES 5     /* No. of pend_event slices for callback requests */

/* Different output formats */
typedef enum { plain, terse, all, specifiedDbr } OutputT;

/* Different request types */
typedef enum { get, callback } RequestT;

// Struct to hold data used by caget threads
typedef struct
{
    pv pv_data;
    RequestT request;
    OutputT format;
    chtype dbrType;
    unsigned long reqElems;
    epicsMutexId printMutex;
    epicsMutexId resultMutex;
} t_thread_data;

static int nConn = 0;           /* Number of connected PVs */
static int nRead = 0;           /* Number of channels that were read */
static int floatAsString = 0;   /* Flag: fetch floats as string */
static int pvs_with_trouble = 0; /* Counter for PVs with result not zero */

static void usage (void)
{
    fprintf (stderr, "\nUsage: caget [options] <PV name> ...\n\n"
    "  -h: Help: Print this message\n"
    "  -V: Version: Show EPICS and CA versions\n"
    "Channel Access options:\n"
    "  -w <sec>: Wait time, specifies CA timeout, default is %f second(s)\n"
    "  -c: Asynchronous get (use ca_get_callback and wait for completion)\n"
    "  -p <prio>: CA priority (0-%u, default 0=lowest)\n"
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
    "  -S:         Print array of char as a string (long string)\n"
    "Floating point type format:\n"
    "  Default: Use %%g format\n"
    "  -e <nr>: Use %%e format, with a precision of <nr> digits\n"
    "  -f <nr>: Use %%f format, with a precision of <nr> digits\n"
    "  -g <nr>: Use %%g format, with a precision of <nr> digits\n"
    "  -s:      Get value as string (honors server-side precision)\n"
    "  -lx:     Round to long integer and print as hex number\n"
    "  -lo:     Round to long integer and print as octal number\n"
    "  -lb:     Round to long integer and print as binary number\n"
    "Integer number format:\n"
    "  Default: Print as decimal number\n"
    "  -0x: Print as hex number\n"
    "  -0o: Print as octal number\n"
    "  -0b: Print as binary number\n"
    "Alternate output field separator:\n"
    "  -F <ofs>: Use <ofs> as an alternate output field separator\n"
    "\nExample: caget -a -f8 my_channel another_channel\n"
    "  (uses wide output format, doubles are printed as %%f with precision of 8)\n\n"
             , DEFAULT_TIMEOUT, CA_PRIORITY_MAX);
}


/*+**************************************************************************
 *
 * Function:    event_handler
 *
 * Description: CA event_handler for request type callback
 *              Allocates the dbr structure and copies the data
 *
 * Arg(s) In:   args  -  event handler args (see CA manual)
 *
 **************************************************************************-*/

static void event_handler (evargs args)
{
    pv* ppv = args.usr;

    ppv->status = args.status;
    if (args.status == ECA_NORMAL)
    {
        ppv->dbrType = args.type;
        ppv->value   = calloc(1, dbr_size_n(args.type, args.count));
        memcpy(ppv->value, args.dbr, dbr_size_n(args.type, args.count));
        ppv->nElems = args.count;
        nRead++;
    }
}


/*+**************************************************************************
 *
 * Function:    caget
 *
 * Description: Issue read requests, wait for incoming data
 *              and print the data according to the selected format
 *
 * Arg(s) In:   pv_data   -  pv structure
 *              request   -  Request type
 *              format    -  Output format
 *              dbrType   -  Requested dbr type
 *              reqElems  -  Requested number of (array) elements
 *              printMutex     -  Mutex to avoid commingling of outputs from several PVs
 *
 * Return(s):   Error code: 0 = OK, 1 = Error
 *
 **************************************************************************-*/

static int caget (pv pv_data, RequestT request, OutputT format,
           chtype dbrType, unsigned long reqElems, epicsMutexId printMutex)
{
    unsigned int i;
    int n, result;

    unsigned long nElems;

                        /* Set up pv_data structure */
                        /* -------------------- */

    /* Get natural type and array count */
    nElems         = ca_element_count(pv_data.chid);
    pv_data.dbfType = ca_field_type(pv_data.chid);
    pv_data.dbrType = dbrType;

    /* Set up value structures */
    if (format != specifiedDbr)
    {
        pv_data.dbrType = dbf_type_to_DBR_TIME(pv_data.dbfType); /* Use native type */
        if (dbr_type_is_ENUM(pv_data.dbrType))                  /* Enums honour -n option */
        {
            if (enumAsNr) pv_data.dbrType = DBR_TIME_INT;
            else          pv_data.dbrType = DBR_TIME_STRING;
        }
        else if (floatAsString &&
                 (dbr_type_is_FLOAT(pv_data.dbrType) || dbr_type_is_DOUBLE(pv_data.dbrType)))
        {
            pv_data.dbrType = DBR_TIME_STRING;
        }
    }

                        /* Issue CA request */
                        /* ---------------- */

    if (ca_state(pv_data.chid) == cs_conn)
    {
        nConn++;
        pv_data.onceConnected = 1;
        if (request == callback)
        {
            /* Event handler will allocate value and set nElems */
            pv_data.reqElems = reqElems > nElems ? nElems : reqElems;
            result = ca_array_get_callback(pv_data.dbrType,
                                           pv_data.reqElems,
                                           pv_data.chid,
                                           event_handler,
                                           (void*)&pv_data);
        } else {
            /* We allocate value structure and set nElems */
            pv_data.nElems = reqElems && reqElems < nElems ? reqElems : nElems;
            pv_data.value = calloc(1, dbr_size_n(pv_data.dbrType, pv_data.nElems));
            if (!pv_data.value) {
                // Lock printMutex before printing to avoid having multiple
                // threads printing at the same time
                epicsMutexLock(printMutex);
                fprintf(stderr,"Memory allocation failed\n");
                epicsMutexUnlock(printMutex);
                return 1;
            }
            result = ca_array_get(pv_data.dbrType,
                                  pv_data.nElems,
                                  pv_data.chid,
                                  pv_data.value);
        }
        pv_data.status = result;
    } else {
        pv_data.status = ECA_DISCONN;
    }

    if (!nConn) return 1;              /* No connection? We're done. */

                                /* Wait for completion */
                                /* ------------------- */

    result = ca_pend_io(caTimeout);
    if (result == ECA_TIMEOUT) {
        // Lock printMutex before printing to avoid having multiple
        // threads printing at the same time
        epicsMutexLock(printMutex);
        fprintf(stderr, "Read operation timed out: some PV data was not read.\n");
        epicsMutexUnlock(printMutex);
    }

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
            if (nRead < nConn) {
                // Lock printMutex before printing to avoid having multiple
                // threads printing at the same time
                epicsMutexLock(printMutex);
                fprintf(stderr, "Read operation timed out: some PV data was not read.\n");
                epicsMutexUnlock(printMutex);
            }
        } else {
            /* For 0 timeout keep waiting until all are done */
            while (nRead < nConn) {
                  ca_pend_event(1.0);
            }
        }
    }

                            /* Print the data */
                            /* -------------- */

    // Lock printMutex before printing to avoid having multiple
    // threads printing at the same time
    epicsMutexLock(printMutex);
    switch (format) {
    case plain:             /* Emulate old caget behavior */
        if (pv_data.nElems <= 1 && fieldSeparator == ' ') printf("%-30s", pv_data.name);
        else                                               printf("%s", pv_data.name);
        printf("%c", fieldSeparator);
    case terse:
        if (pv_data.status == ECA_DISCONN)
            printf("*** not connected\n");
        else if (pv_data.status == ECA_NORDACCESS)
            printf("*** no read access\n");
        else if (pv_data.status != ECA_NORMAL)
            printf("*** CA error %s\n", ca_message(pv_data.status));
        else if (pv_data.value == 0)
            printf("*** no data available (timeout)\n");
        else
        {
            if (charArrAsStr && dbr_type_is_CHAR(pv_data.dbrType) && (reqElems || pv_data.nElems > 1)) {
                dbr_char_t *s = (dbr_char_t*) dbr_value_ptr(pv_data.value, pv_data.dbrType);
                int dlen = epicsStrnEscapedFromRawSize((char*)s, strlen((char*)s));
                char *d = calloc(dlen+1, sizeof(char));
                if(d) {
                    epicsStrnEscapedFromRaw(d, dlen+1, (char*)s, strlen((char*)s));
                    printf("%s", d);
                    free(d);
                } else {
                    fprintf(stderr,"Failed to allocate space for escaped string\n");
                }
            } else {
                if (reqElems || pv_data.nElems > 1) printf("%lu%c", pv_data.nElems, fieldSeparator);
                for (i=0; i<pv_data.nElems; ++i) {
                    if (i) printf ("%c", fieldSeparator);
                    printf("%s", val2str(pv_data.value, pv_data.dbrType, i));
                }
            }
            printf("\n");
        }
        break;
    case all:
        print_time_val_sts(&pv_data, reqElems);
        break;
    case specifiedDbr:
        printf("%s\n", pv_data.name);
        if (pv_data.status == ECA_DISCONN)
            printf("    *** not connected\n");
        else if (pv_data.status == ECA_NORDACCESS)
            printf("    *** no read access\n");
        else if (pv_data.status != ECA_NORMAL)
            printf("    *** CA error %s\n", ca_message(pv_data.status));
        else
        {
            printf("    Native data type: %s\n",
                   dbf_type_to_text(pv_data.dbfType));
            printf("    Request type:     %s\n",
                   dbr_type_to_text(pv_data.dbrType));
            if (pv_data.dbrType == DBR_CLASS_NAME)
                printf("    Class Name:       %s\n",
                       *((dbr_string_t*)dbr_value_ptr(pv_data.value,
                                                      pv_data.dbrType)));
            else {
                printf("    Element count:    %lu\n"
                       "    Value:            ",
                       pv_data.nElems);
                if (charArrAsStr && dbr_type_is_CHAR(pv_data.dbrType) && (reqElems || pv_data.nElems > 1)) {
                    dbr_char_t *s = (dbr_char_t*) dbr_value_ptr(pv_data.value, pv_data.dbrType);
                    int dlen = epicsStrnEscapedFromRawSize((char*)s, strlen((char*)s));
                    char *d = calloc(dlen+1, sizeof(char));
                    if(d) {
                        epicsStrnEscapedFromRaw(d, dlen+1, (char*)s, strlen((char*)s));
                        printf("%s", d);
                        free(d);
                    } else {
                        fprintf(stderr,"Failed to allocate space for escaped string\n");
                    }
                } else {
                    for (i=0; i<pv_data.nElems; ++i) {
                        if (i) printf ("%c", fieldSeparator);
                        printf("%s", val2str(pv_data.value, pv_data.dbrType, i));
                    }
                }
                printf("\n");
                if (pv_data.dbrType > DBR_DOUBLE) /* Extended type extra info */
                    printf("%s\n", dbr2str(pv_data.value, pv_data.dbrType));
            }
        }
        break;
    default :
        break;
    }

    epicsMutexUnlock(printMutex);

    return 0;
}

static void complainIfNotPlainAndSet (OutputT *current, const OutputT requested)
{
    if (*current != plain)
        fprintf(stderr,
                "Options t,d,a are mutually exclusive. "
                "('caget -h' for help.)\n");
    *current = requested;
}
/*
void test(void* thread_data_void) {
printf("called\n");
    //t_thread_data* thread_data = (t_thread_data*)thread_data_void;
    //epicsSpinLock(thread_data->spin);
    //epicsMutexUnlock(thread_data->printMutex_lock_main);
printf("End of test\n");
}
*/

// Create a thread name based on the PV index in the order sent as argument for caget
void thread_name (int pv_num, char* th_name) {
    // Threads will be named caget0, caget1, ..., caget42, ...
    char pv_num_str[3];
    sprintf(pv_num_str, "%d", pv_num);
    strcpy(th_name, "caget");
    strcat(th_name, pv_num_str);
}

// Function that runs inside each thread, calling the caget function
void caget_caller (void* thread_data_void)
{
    int result;                 /* CA result */
    t_thread_data* thread_data = (t_thread_data*)thread_data_void;
    result = connect_pvs(&thread_data->pv_data, 1);

                                /* Read and print data */
    if (!result)
        result = caget(thread_data->pv_data, thread_data->request, thread_data->format, thread_data->dbrType, thread_data->reqElems, thread_data->printMutex);

    epicsMutexLock(thread_data->resultMutex);
    pvs_with_trouble += result;
    epicsMutexUnlock(thread_data->resultMutex);

    free(thread_data);
}

// Creates one thread per PV called with caget
void thread_builder (t_thread_data* thread_data, int pv_num)
{
    char th_name[9];
    thread_name(pv_num, th_name);
    epicsThreadOpts thread_opts = EPICS_THREAD_OPTS_INIT;
    // Must be joinable so the main thread can wait for all the threads to
    // complete their job.
    thread_opts.joinable = 1;
    epicsThreadCreateOpt(th_name, &caget_caller, thread_data, &thread_opts);
}


/***************************************************************************
 *
 * Function:    main
 *
 * Description: caget main()
 *              Evaluate command line options, set up CA, connect the
 *              channels, collect and print the data as requested
 *
 * Arg(s) In:   [options] <pv-name> ...
 *
 * Arg(s) Out:  none
 *
 * Return(s):   Return code = number of PVs that couldn't be reached
 *              (0=success, !0=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    int n;
    int result;                 /* CA result */
    OutputT format = plain;     /* User specified format */
    RequestT request = get;     /* User specified request type */
    IntFormatT outType;         /* Output type */

    int count = 0;              /* 0 = not specified by -# option */
    int opt;                    /* getopt() current option */
    int type = -1;              /* getopt() data type argument */
    int digits = 0;             /* getopt() no. of float digits */

    int nPvs;                   /* Number of PVs */
    pv* pvs;                    /* Array of PV structures */

    LINE_BUFFER(stdout);        /* Configure stdout buffering */

    use_ca_timeout_env ( &caTimeout);

    while ((opt = getopt(argc, argv, ":taicnhsSVe:f:g:l:#:d:0:w:p:F:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'V':
            printf( "\nEPICS Version %s, CA Protocol version %s\n", EPICS_VERSION_STRING, ca_version() );
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
                        "- ignored, using '%.1f'. ('caget -h' for help.)\n",
                        optarg, caTimeout);
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
        case 'p':               /* CA priority */
            if (sscanf(optarg,"%u", &caPriority) != 1)
            {
                fprintf(stderr, "'%s' is not a valid CA priority "
                        "- ignored. ('caget -h' for help.)\n", optarg);
                caPriority = DEFAULT_CA_PRIORITY;
            }
            if (caPriority > CA_PRIORITY_MAX) caPriority = CA_PRIORITY_MAX;
            break;
        case 's':               /* Select string dbr for floating type data */
            floatAsString = 1;
            break;
        case 'S':               /* Treat char array as (long) string */
            charArrAsStr = 1;
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
        case 'l':               /* Convert to long and use integer format */
        case '0':               /* Select integer format */
            switch ((char) *optarg) {
            case 'x': outType = hex; break;    /* x print Hex */
            case 'b': outType = bin; break;    /* b print Binary */
            case 'o': outType = oct; break;    /* o print Octal */
            default :
                outType = dec;
                fprintf(stderr, "Invalid argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            }
            if (outType != dec) {
              if (opt == '0') {
                type = DBR_LONG;
                outTypeI = outType;
              } else {
                outTypeF = outType;
              }
            }
            break;
        case 'F':               /* Store this for output and tool_lib formatting */
            fieldSeparator = (char) *optarg;
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

    // Mutex to avoid commingling of text output by several threads
    epicsMutexId printMutex = epicsMutexCreate();
    // Mutex to access result counter
    epicsMutexId resultMutex = epicsMutexCreate();
    for (int iii=0; iii<nPvs; ++iii) {
        t_thread_data* thread_data;
        thread_data = malloc(sizeof(t_thread_data));
        thread_data->pv_data = pvs[iii];
        thread_data->request = request;
        thread_data->format = format;
        thread_data->dbrType = type;
        thread_data->reqElems = count;
        thread_data->printMutex = printMutex;
        thread_data->resultMutex = resultMutex;
        thread_builder(thread_data, iii);
    }

    epicsThreadId thread_id;
    char th_name[9];

    // Wait for all threads to finish their business before ending the main thread
    for (int iii=0; iii<nPvs; ++iii) {
        thread_name(iii, th_name);
        thread_id = epicsThreadGetId(th_name);
        epicsThreadMustJoin(thread_id);
    }

    ca_context_destroy();
    epicsMutexDestroy(printMutex);
    epicsMutexDestroy(resultMutex);

    return pvs_with_trouble;
}
