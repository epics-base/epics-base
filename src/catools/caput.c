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
#include <stdlib.h>
#include <string.h>

#include <cadef.h>
#include <epicsGetopt.h>

#include "tool_lib.h"

#define VALID_DOUBLE_DIGITS 18  /* Max usable precision for a double */

/* Different output formats */
typedef enum { plain, terse } OutputT;

/* Valid EPICS string */
typedef char EpicsStr[MAX_STRING_SIZE];

static int nConn = 0;           /* Number of connected PVs */


void usage (void)
{
    fprintf (stderr, "\nUsage: caput [options] <PV name> <PV value>\n"
    "       caput -a [options] <PV name> <no of values> <PV value> ...\n\n"
    "  -h: Help: Print this message\n"
    "Channel Access options:\n"
    "  -w <sec>:  Wait time, specifies longer CA timeout, default is %f second\n"
    "Format options:\n"
    "  -t: Terse mode - print only sucessfully written value, without name\n"
    "Enum format:\n"
    "  Default: Auto - try value as ENUM string, then as index number\n"
    "  -n: Force interpretation of values as numbers\n"
    "  -s: Force interpretation of values as strings\n"
    "Arrays:\n"
    "  -a: Put array\n"
    "      Value format: number of requested values, then list of values\n"
    "\nExample: caput my_channel 1.2\n"
    "  (puts 1.2 to my_channel)\n\n"
	, DEFAULT_TIMEOUT);
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

                                /* Set up value structures */
        dbrType = dbf_type_to_DBR(pvs[n].dbfType); /* Use native type */
        if (dbr_type_is_ENUM(dbrType))             /* Enums honour -n option */
        {
            if (enumAsNr) dbrType = DBR_INT;
            else          dbrType = DBR_STRING;
        }

                                /* Adjust array count */
        if (reqElems == 0 || pvs[n].nElems < reqElems)
            reqElems = pvs[n].nElems;
                                /* Remember dbrType and reqElems */
        pvs[n].dbrType  = dbrType;
        pvs[n].reqElems = reqElems;

                                /* Issue CA request */
                                /* ---------------- */

        if (ca_state(pvs[n].chid) == cs_conn)
        {
            nConn++;
                                   /* Allocate value structure */
            pvs[n].value = calloc(1, dbr_size_n(dbrType, reqElems));
            result = ca_array_get(dbrType,
                                  reqElems,
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
        reqElems = pvs[n].reqElems;

        switch (format) {
        case plain:             /* Emulate old caput behaviour */
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
                for (i=0; i<reqElems; ++i)
                    printf("%s ", val2str(pvs[n].value, pvs[n].dbrType, i));
                printf("\n");
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
    int n = 0;
    int i;
    int result;                 /* CA result */
    OutputT format = plain;     /* User specified format */
    int isArray = 0;            /* Flag for array operation */
    int enumAsString = 0;       /* Force ENUM values to be strings */

    int count = 1;
    int opt;                    /* getopt() current option */
    chtype dbrType;
    char *pend;
    EpicsStr *sbuf;
    double *dbuf;
    void *pbuf;
    int len;
    EpicsStr bufstr;
    struct dbr_gr_enum bufGrEnum;

    int nPvs;                   /* Number of PVs */
    pv* pvs = 0;                /* Array of PV structures */

    setvbuf(stdout,NULL,_IOLBF,0);   /* Set stdout to line buffering */
    putenv("POSIXLY_CORRECT=");      /* Behave correct on GNU getopt systems */

    while ((opt = getopt(argc, argv, ":nhats#:w:")) != -1) {
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
        case 't':               /* Select terse output format */
            format = terse;
            break;
        case 'a':               /* Select array mode */
            isArray = 1;
            break;
        case 'w':               /* Set CA timeout value */
            if(sscanf(optarg,"%lf", &caTimeout) != 1)
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

    if (nPvs < 1)
    {
        fprintf(stderr, "No pv name specified. ('caput -h' for help.)\n");
        return 1;
    }
    if (nPvs == 1)
    {
        fprintf(stderr, "No value specified. ('caput -h' for help.)\n");
        return 1;
    }

    nPvs = 1;                   /* One PV - the rest is value(s) */

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
        fprintf(stderr, "Memory allocation for channel structure failed.\n");
        return 1;
    }
                                /* Connect channels */

    pvs[0].name = argv[optind] ;   /* Copy PV name from command line */

    connect_pvs(pvs, nPvs);

                                /* Get values from command line */
    optind++;

    if (isArray) {
        optind++;               /* In case of array skip first value (nr
                                 * of elements) - actual number of values is used */
        count = argc - optind;

    } else {                    /* Concatenate the remaining line to one string
                                 * (sucks but is compatible to the former version) */
        len = strlen(argv[optind]);

        if (len < MAX_STRING_SIZE) {
	    strcpy(bufstr, argv[optind]);

            if (argc > optind+1) {
                for (i = optind + 1; i < argc; i++) {
                    len += strlen(argv[i]);
                    if (len < MAX_STRING_SIZE - 1) {
                        strcat(bufstr, " ");
                        strcat(bufstr, argv[i]); 
                    }
                }
            }
        }
        
        if ((argc - optind) >= 1)
            	count = 1;	
		argv[optind] = bufstr;
    }

    sbuf = calloc (count, sizeof(EpicsStr));
    dbuf = calloc (count, sizeof(double));
    dbrType = DBR_STRING;

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
                dbuf[i] = strtod(*(argv+optind+i), &pend);
                if (*(argv+optind+i) == pend) { /* Conversion didn't work */
                    fprintf(stderr, "Enum index value '%s' is not a number.\n",
                            *(argv+optind+i));
                    return 1;
                }
                if (dbuf[i] >= bufGrEnum.no_str) {
                    fprintf(stderr, "Enum index value '%s' too large.\n",
                            *(argv+optind+i));
                    return 1;
                }
            }
            dbrType = DBR_DOUBLE;

        } else {                /* Interpret values as strings */
            
            for (i = 0; i < count; ++i) {
                len = strlen(*(argv+optind+i));
                if (len >= sizeof(EpicsStr)) /* Too long? Cut at max length */
                    *( *(argv+optind+i)+sizeof(EpicsStr)-1 ) = 0;

                                /* Compare to ENUM strings */
                for (len = 0; len < bufGrEnum.no_str; len++)
                    if (!strcmp(*(argv+optind+i), bufGrEnum.strs[len]))
                        break;

                if (len >= bufGrEnum.no_str) {
                                         /* Not a string? Try as number */
                    dbuf[i] = strtod(*(argv+optind+i), &pend);
                    if (*(argv+optind+i) == pend || enumAsString) {
                        fprintf(stderr, "Enum string value '%s' invalid.\n",
                                *(argv+optind+i));
                        return 1;
                    }
                    if (dbuf[i] >= bufGrEnum.no_str) {
                        fprintf(stderr, "Enum index value '%s' too large.\n",
                                *(argv+optind+i));
                        return 1;
                    }
                    dbrType = DBR_DOUBLE;
                } else {
                    strcpy (sbuf[i], *(argv+optind+i));
                    dbrType = DBR_STRING;
                }
            }
        }

    } else {                    /* Not an ENUM */
        
        for (i = 0; i < count; ++i) {
            len=strlen(*(argv+optind+i));
            if (len >= sizeof(EpicsStr)) /* Too long? Cut at max length */
                *( *(argv+optind+i)+sizeof(EpicsStr)-1 ) = 0;
            strcpy (sbuf[i], *(argv+optind+i));
        }
        dbrType = DBR_STRING;
    }

                                /* Read and print old data */
    if (format == plain) {
        printf("Old : ");
        result = caget(pvs, nPvs, format, 0, count);
    }

                                /* Write new data */

    if (dbrType == DBR_STRING) pbuf = sbuf;
    else pbuf = dbuf;

    result = ca_array_put (dbrType, count, pvs[0].chid, pbuf);
    result = ca_pend_io(caTimeout);
    if (result == ECA_TIMEOUT) {
        fprintf(stderr, "Write operation timed out: Data was not written.\n");
        return 1;
    }

                                /* Read and print new data */
    if (format == plain)
        printf("New : ");

    result = caget(pvs, nPvs, format, 0, count);

                                /* Shut down Channel Access */
    ca_context_destroy();

    return result;
}
