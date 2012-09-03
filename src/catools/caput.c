/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2006 Diamond Light Source Ltd.
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
 *  2006/01/17 Malcolm Walters (Tessella/Diamond Light Source)
 *     Added put_callback option - heavily based on caget
 *  2008/03/06 Andy Foster (OSL/Diamond Light Source)
 *     Remove timeout dependency of ca_put_callback by using an EPICS event
 *     (semaphore), i.e. remove ca_pend_event time slicing.
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

#include <cadef.h>
#include <epicsGetopt.h>
#include <epicsEvent.h>
#include <epicsString.h>

#include "tool_lib.h"

#define VALID_DOUBLE_DIGITS 18  /* Max usable precision for a double */

/* Different output formats */
typedef enum { plain, terse, all } OutputT;

/* Different request types */
typedef enum { get, callback } RequestT;

/* Valid EPICS string */
typedef char EpicsStr[MAX_STRING_SIZE];

static int nConn = 0;           /* Number of connected PVs */
static epicsEventId epId;

void usage (void)
{
    fprintf (stderr, "\nUsage: caput [options] <PV name> <PV value>\n"
    "       caput -a [options] <PV name> <no of values> <PV value> ...\n\n"
    "  -h: Help: Print this message\n"
    "Channel Access options:\n"
    "  -w <sec>:  Wait time, specifies CA timeout, default is %f second(s)\n"
    "  -c: Asynchronous put (use ca_put_callback and wait for completion)\n"
    "  -p <prio>: CA priority (0-%u, default 0=lowest)\n"
    "Format options:\n"
    "  -t: Terse mode - print only sucessfully written value, without name\n"
    "  -l: Long mode \"name timestamp value stat sevr\" (read PVs as DBR_TIME_xxx)\n"
    "Enum format:\n"
    "  Default: Auto - try value as ENUM string, then as index number\n"
    "  -n: Force interpretation of values as numbers\n"
    "  -s: Force interpretation of values as strings\n"
    "Arrays:\n"
    "  -a: Put array\n"
    "      Value format: number of requested values, then list of values\n"
    "  -S: Put string as an array of char (long string)\n"
    "\nExample: caput my_channel 1.2\n"
    "  (puts 1.2 to my_channel)\n\n"
             , DEFAULT_TIMEOUT, CA_PRIORITY_MAX);
}


/*+**************************************************************************
 *
 * Function:    put_event_handler
 *
 * Description: CA event_handler for request type callback
 *              Sets status flags and marks as done.
 *
 * Arg(s) In:   args  -  event handler args (see CA manual)
 *
 **************************************************************************-*/

void put_event_handler ( struct event_handler_args args )
{
    /* Retrieve pv from event handler structure */
    pv* pPv = args.usr;

    /* Store status, then give EPICS event */
    pPv->status = args.status;
    epicsEventSignal( epId );
}


/*+**************************************************************************
 *
 * Function:	caget
 *
 * Description:	Issue read request, wait for incoming data
 * 		and print the data
 *
 * Arg(s) In:	pvs       -  Pointer to an array of pv structures
 *              nPvs      -  Number of elements in the pvs array
 *              format    -  Output format
 *              dbrType   -  Requested dbr type
 *              reqElems  -  Requested number of (array) elements
 *
 * Return(s):	Error code: 0 = OK, 1 = Error
 *
 **************************************************************************-*/

int caget (pv *pvs, int nPvs, OutputT format,
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
        pvs[n].dbrType = dbrType;

                                /* Set up value structures */
        pvs[n].dbrType = dbf_type_to_DBR_TIME(pvs[n].dbfType); /* Use native type */
        if (dbr_type_is_ENUM(pvs[n].dbrType))             /* Enums honour -n option */
        {
            if (enumAsNr) pvs[n].dbrType = DBR_TIME_INT;
            else          pvs[n].dbrType = DBR_TIME_STRING;
        }

        if (reqElems == 0 || pvs[n].nElems < reqElems)    /* Adjust array count */
            pvs[n].reqElems = pvs[n].nElems;
        else
            pvs[n].reqElems = reqElems;

                                /* Issue CA request */
                                /* ---------------- */

        if (ca_state(pvs[n].chid) == cs_conn)
        {
            nConn++;
            pvs[n].onceConnected = 1;
                                   /* Allocate value structure */
            pvs[n].value = calloc(1, dbr_size_n(pvs[n].dbrType, pvs[n].reqElems));
            if(!pvs[n].value){
                fprintf(stderr,"Allocation failed\n");
                exit(1);
            }
            result = ca_array_get(pvs[n].dbrType,
                                  pvs[n].reqElems,
                                  pvs[n].chid,
                                  pvs[n].value);
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
        fprintf(stderr, "Read operation timed out: PV data was not read.\n");

                                /* Print the data */
                                /* -------------- */

    for (n = 0; n < nPvs; n++) {

        switch (format) {
        case plain:             /* Emulate old caput behaviour */
            if (pvs[n].reqElems <= 1 && fieldSeparator == ' ') printf("%-30s", pvs[n].name);
            else                                               printf("%s", pvs[n].name);
            printf("%c", fieldSeparator);
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
                if (charArrAsStr && dbr_type_is_CHAR(pvs[n].dbrType) && (reqElems || pvs[n].reqElems > 1)) {
                    dbr_char_t *s = (dbr_char_t*) dbr_value_ptr(pvs[n].value, pvs[n].dbrType);
                    int dlen = epicsStrnEscapedFromRawSize((char*)s, strlen((char*)s));
                    char *d = calloc(dlen+1, sizeof(char));
                    if(!d){
                        fprintf(stderr,"Allocation failed\n");
                        exit(1);
                    }
                    epicsStrnEscapedFromRaw(d, dlen+1, (char*)s, strlen((char*)s));
                    printf("%s", d);
                    free(d);
                } else {
                    if (reqElems || pvs[n].nElems > 1) printf("%lu%c", pvs[n].reqElems, fieldSeparator);
                    for (i=0; i<pvs[n].reqElems; ++i) {
                        if (i) printf ("%c", fieldSeparator);
                        printf("%s", val2str(pvs[n].value, pvs[n].dbrType, i));
                    }
                }
                printf("\n");
            }
            break;
        case all:
            print_time_val_sts(&pvs[n], reqElems);
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
 * Description:	caput main()
 * 		Evaluate command line options, set up CA, connect the
 * 		channel, put and print the data
 *
 * Arg(s) In:	[options] <pv-name> <pv-value> ...
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Standard return code (0=success, 1=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    int i;
    int result;                 /* CA result */
    OutputT format = plain;     /* User specified format */
    RequestT request = get;     /* User specified request type */
    int isArray = 0;            /* Flag for array operation */
    int enumAsString = 0;       /* Force ENUM values to be strings */

    int count = 1;
    int opt;                    /* getopt() current option */
    chtype dbrType = DBR_STRING;
    char *pend;
    EpicsStr *sbuf;
    double *dbuf;
    char *cbuf = 0;
    char *ebuf = 0;
    void *pbuf;
    int len = 0;
    int waitStatus;
    struct dbr_gr_enum bufGrEnum;

    int nPvs;                   /* Number of PVs */
    pv* pvs;                /* Array of PV structures */

    LINE_BUFFER(stdout);        /* Configure stdout buffering */
    putenv("POSIXLY_CORRECT="); /* Behave correct on GNU getopt systems */

    while ((opt = getopt(argc, argv, ":cnlhatsS#:w:p:F:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'n':               /* Force interpret ENUM as index number */
            enumAsNr = 1;
            enumAsString = 0;
            break;
        case 's':               /* Force interpret ENUM as menu string */
            enumAsString = 1;
            enumAsNr = 0;
            break;
        case 'S':               /* Treat char array as (long) string */
            charArrAsStr = 1;
            isArray = 0;
            break;
        case 't':               /* Select terse output format */
            format = terse;
            break;
        case 'l':               /* Select long output format */
            format = all;
            break;
        case 'a':               /* Select array mode */
            isArray = 1;
            charArrAsStr = 0;
            break;
        case 'c':               /* Select put_callback mode */
            request = callback;
            break;
        case 'w':               /* Set CA timeout value */
            if(epicsScanDouble(optarg, &caTimeout) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('caput -h' for help.)\n", optarg);
                caTimeout = DEFAULT_TIMEOUT;
            }
            break;
        case '#':               /* Array count */
            if (sscanf(optarg,"%d", &count) != 1)
            {
                fprintf(stderr, "'%s' is not a valid array element count "
                        "- ignored. ('caput -h' for help.)\n", optarg);
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
        case 'F':               /* Store this for output and tool_lib formatting */
            fieldSeparator = (char) *optarg;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('caput -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('caput -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    nPvs = argc - optind;       /* Remaining arg list are PV names and values */

    if (nPvs < 1) {
        fprintf(stderr, "No pv name specified. ('caput -h' for help.)\n");
        return 1;
    }
    if (nPvs == 1) {
        fprintf(stderr, "No value specified. ('caput -h' for help.)\n");
        return 1;
    }

    nPvs = 1;                   /* One PV - the rest is value(s) */

    epId = epicsEventCreate(epicsEventEmpty);  /* Create empty EPICS event (semaphore) */

                                /* Start up Channel Access */

    result = ca_context_create(ca_enable_preemptive_callback);
    if (result != ECA_NORMAL) {
        fprintf(stderr, "CA error %s occurred while trying "
                "to start channel access.\n", ca_message(result));
        return 1;
    }
                                /* Allocate PV structure array */

    pvs = calloc (nPvs, sizeof(pv));
    if (!pvs) {
        fprintf(stderr, "Memory allocation for channel structure failed.\n");
        return 1;
    }
                                /* Connect channels */

    pvs[0].name = argv[optind] ;   /* Copy PV name from command line */

    result = connect_pvs(pvs, nPvs); /* If the connection fails, we're done */
    if (result) {
        ca_context_destroy();
        return result;
    }

                                /* Get values from command line */
    optind++;

    if (isArray) {
        optind++;               /* In case of array skip first value (nr
                                 * of elements) - actual number of values is used */
        count = argc - optind;

    } else {                    /* Concatenate the remaining line to one string
                                 * (sucks but is compatible to the former version) */
        for (i = optind; i < argc; i++) {
            len += strlen(argv[i]);
            len++;
        }
        cbuf = calloc(len, sizeof(char));
        if (!cbuf) {
            fprintf(stderr, "Memory allocation failed.\n");
            return 1;
        }
        strcpy(cbuf, argv[optind]);

        if (argc > optind+1) {
            for (i = optind + 1; i < argc; i++) {
                strcat(cbuf, " ");
                strcat(cbuf, argv[i]); 
            }
        }

        if ((argc - optind) >= 1)
            count = 1;
        argv[optind] = cbuf;
    }

    sbuf = calloc (count, sizeof(EpicsStr));
    dbuf = calloc (count, sizeof(double));
    if(!sbuf || !dbuf) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

                                /*  ENUM? Special treatment */

    if (ca_field_type(pvs[0].chid) == DBR_ENUM) {

                                /* Get the ENUM strings */

        result = ca_array_get (DBR_GR_ENUM, 1, pvs[0].chid, &bufGrEnum);
        result = ca_pend_io(caTimeout);
        if (result == ECA_TIMEOUT) {
            fprintf(stderr, "Read operation timed out: ENUM data was not read.\n");
            return 1;
        }

        if (enumAsNr) {         /* Interpret values as numbers */

            for (i = 0; i < count; ++i) {
                dbuf[i] = epicsStrtod(*(argv+optind+i), &pend);
                if (*(argv+optind+i) == pend) { /* Conversion didn't work */
                    fprintf(stderr, "Enum index value '%s' is not a number.\n",
                            *(argv+optind+i));
                    return 1;
                }
                if (dbuf[i] >= bufGrEnum.no_str) {
                    fprintf(stderr, "Warning: enum index value '%s' may be too large.\n",
                            *(argv+optind+i));
                }
            }
            dbrType = DBR_DOUBLE;

        } else {                /* Interpret values as strings */

            for (i = 0; i < count; ++i) {
                epicsStrnRawFromEscaped(sbuf[i], sizeof(EpicsStr), *(argv+optind+i), sizeof(EpicsStr));
                *( sbuf[i]+sizeof(EpicsStr)-1 ) = '\0';
                dbrType = DBR_STRING;

                                /* Compare to ENUM strings */
                for (len = 0; len < bufGrEnum.no_str; len++)
                    if (!strcmp(sbuf[i], bufGrEnum.strs[len]))
                        break;

                if (len >= bufGrEnum.no_str) {
                                         /* Not a string? Try as number */
                    dbuf[i] = epicsStrtod(sbuf[i], &pend);
                    if (sbuf[i] == pend || enumAsString) {
                        fprintf(stderr, "Enum string value '%s' invalid.\n", sbuf[i]);
                        return 1;
                    }
                    if (dbuf[i] >= bufGrEnum.no_str) {
                        fprintf(stderr, "Warning: enum index value '%s' may be too large.\n", sbuf[i]);
                    }
                    dbrType = DBR_DOUBLE;
                }
            }
        }

    } else {                    /* Not an ENUM */

        if (charArrAsStr) {
            count = len;
            dbrType = DBR_CHAR;
            ebuf = calloc(strlen(cbuf)+1, sizeof(char));
            if(!ebuf) {
                fprintf(stderr, "Memory allocation failed\n");
                return 1;
            }
            epicsStrnRawFromEscaped(ebuf, strlen(cbuf)+1, cbuf, strlen(cbuf));
        } else {
            for (i = 0; i < count; ++i) {
                epicsStrnRawFromEscaped(sbuf[i], sizeof(EpicsStr), *(argv+optind+i), sizeof(EpicsStr));
                *( sbuf[i]+sizeof(EpicsStr)-1 ) = '\0';
            }
            dbrType = DBR_STRING;
        }
    }

                                /* Read and print old data */
    if (format != terse) {
        printf("Old : ");
        result = caget(pvs, nPvs, format, 0, 0);
    }

                                /* Write new data */
    if (dbrType == DBR_STRING) pbuf = sbuf;
    else if (dbrType == DBR_CHAR) pbuf = ebuf;
    else pbuf = dbuf;

    if (request == callback) {
        /* Use callback version of put */
        pvs[0].status = ECA_NORMAL;   /* All ok at the moment */
        result = ca_array_put_callback (
            dbrType, count, pvs[0].chid, pbuf, put_event_handler, (void *) pvs);
    } else {
        /* Use standard put with defined timeout */
        result = ca_array_put (dbrType, count, pvs[0].chid, pbuf);
    }
    result = ca_pend_io(caTimeout);
    if (result == ECA_TIMEOUT) {
        fprintf(stderr, "Write operation timed out: Data was not written.\n");
        return 1;
    }
    if (request == callback) {   /* Also wait for callbacks */
        waitStatus = epicsEventWaitWithTimeout( epId, caTimeout );
        if (waitStatus)
            fprintf(stderr, "Write callback operation timed out\n");

        /* retrieve status from callback */
        result = pvs[0].status;
    }

    if (result != ECA_NORMAL) {
        fprintf(stderr, "Error occured writing data.\n");
        return 1;
    }

                                /* Read and print new data */
    if (format != terse)
        printf("New : ");

    result = caget(pvs, nPvs, format, 0, 0);

                                /* Shut down Channel Access */
    ca_context_destroy();

    return result;
}
