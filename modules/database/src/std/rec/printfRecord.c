/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Printf record type */
/*
 * Author: Andrew Johnson
 * Date:   2012-09-18
 */

#include <stddef.h>
#include <string.h>

#include "dbDefs.h"
#include "errlog.h"
#include "alarm.h"
#include "cantProceed.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "epicsMath.h"
#include "epicsStdio.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "printfRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"


/* Flag bits */
#define F_CHAR   1
#define F_SHORT  2
#define F_LONG   4
#define F_LEFT   8
#define F_BADFMT 0x10
#define F_BADLNK 0x20
#define F_BAD (F_BADFMT | F_BADLNK)

#define GET_PRINT(VALTYPE, DBRTYPE) \
    VALTYPE val; \
    int ok; \
\
    if (dbLinkIsConstant(plink)) \
        ok = recGblInitConstantLink(plink++, DBRTYPE, &val); \
    else \
        ok = ! dbGetLink(plink++, DBRTYPE, &val, 0, 0); \
    if (ok) \
        added = epicsSnprintf(pval, vspace + 1, format, val); \
    else \
        flags |= F_BADLNK

static void doPrintf(printfRecord *prec)
{
    const char *pfmt = prec->fmt;
    DBLINK *plink = &prec->inp0;
    int linkn = 0;
    char *pval = prec->val;
    int vspace = prec->sizv - 1;
    int ch;

    while (vspace > 0 && (ch = *pfmt++)) {
        if (ch != '%') {
            /* Copy literal strings directly into prec->val */
            *pval++ = ch;
            --vspace;
        }
        else {
            char format[20];
            char *pformat = format;
            int width = 0;
            int precision = 0;
            int *pnum = &width;
            int flags = 0;
            int added = 0;
            int cont = 1;

            /* The format directive parsing here is not comprehensive,
             * in most cases we just copy each directive into format[]
             * and get epicsSnprintf() do all the work.  We do replace
             * all variable-length field width or precision '*' chars
             * with an integer read from the next input link, and we
             * also convert %ls (long string) directives ourself, so
             * we need to know the width, precision and justification.
             */

            *pformat++ = ch; /* '%' */
            while (cont && (ch = *pfmt++)) {
                *pformat++ = ch;
                switch (ch) {
                case '+': case ' ': case '#':
                    break;
                case '-':
                    flags |= F_LEFT;
                    break;
                case '.':
                    pnum = &precision;
                    break;
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    *pnum = *pnum * 10 + ch - '0';
                    break;
                case '*':
                    if (*pnum) {
                        flags |= F_BADFMT;
                    }
                    else if (linkn++ < PRINTF_NLINKS) {
                        epicsInt16 i;
                        int ok;

                        if (dbLinkIsConstant(plink))
                            ok = recGblInitConstantLink(plink++, DBR_SHORT, &i);
                        else
                            ok = ! dbGetLink(plink++, DBR_SHORT, &i, 0, 0);
                        if (ok) {
                            *pnum = i;
                            added = epicsSnprintf(--pformat, 6, "%d", i);
                            pformat += added;
                        }
                        else /* No more LNKn fields */
                            flags |= F_BADLNK;
                    }
                    else
                        flags |= F_BADLNK;
                    break;
                case 'h':
                    if (flags & F_SHORT)
                        flags = (flags & ~F_SHORT) | F_CHAR;
                    else
                        flags |= F_SHORT;
                    break;
                case 'l':
                    flags |= F_LONG;
                    break;
                default:
                    if (strchr("diouxXeEfFgGcs%", ch) == NULL)
                        flags |= F_BADFMT;
                    cont = 0;
                    break;
                }
            }
            if (!ch)        /* End of format string */
                break;

            if (flags & F_BAD)
                goto bad_format;

            *pformat = 0;   /* Terminate our format string */

            if (width < 0) {
                width = -width;
                flags |= F_LEFT;
            }
            if (precision < 0)
                precision = 0;

            if (ch == '%') {
                added = epicsSnprintf(pval, vspace + 1, "%s", format);
            }
            else if (linkn++ >= PRINTF_NLINKS) {
                /* No more LNKn fields */
                flags |= F_BADLNK;
            }
            else
                switch (ch) { /* Conversion character */
                case 'c': case 'd': case 'i':
                    if (ch == 'c' || flags & F_CHAR) {
                        GET_PRINT(epicsInt8, DBR_CHAR);
                    }
                    else if (flags & F_SHORT) {
                        GET_PRINT(epicsInt16, DBR_SHORT);
                    }
                    else { /* F_LONG has no real effect */
                        GET_PRINT(epicsInt32, DBR_LONG);
                    }
                    break;

                case 'o': case 'x': case 'X': case 'u':
                    if (flags & F_CHAR) {
                        GET_PRINT(epicsUInt8, DBR_UCHAR);
                    }
                    else if (flags & F_SHORT) {
                        GET_PRINT(epicsUInt16, DBR_USHORT);
                    }
                    else { /* F_LONG has no real effect */
                        GET_PRINT(epicsUInt32, DBR_ULONG);
                    }
                    break;

                case 'e': case 'E':
                case 'f': case 'F':
                case 'g': case 'G':
                    if (flags & F_SHORT) {
                        GET_PRINT(epicsFloat32, DBR_FLOAT);
                    }
                    else {
                        GET_PRINT(epicsFloat64, DBR_DOUBLE);
                    }
                    break;

                case 's':
                    if (flags & F_LONG) {
                        long n = vspace + 1;
                        long status;

                        if (precision && n > precision)
                            n = precision + 1;
                            /* If set, precision is the maximum number of
                             * characters to be printed from the string.
                             * It does not limit the field width however.
                             */
                        if (dbLinkIsConstant(plink)) {
                            epicsUInt32 len = n;
                            status = dbLoadLinkLS(plink++, pval, n, &len);
                            n = len;
                        }
                        else
                            status = dbGetLink(plink++, DBR_CHAR, pval, 0, &n);
                        if (status)
                            flags |= F_BADLNK;
                        else {
                            int padding;

                            /* Terminate string and measure its length */
                            pval[n] = 0;
                            added = strlen(pval);
                            padding = width - added;

                            if (padding > 0) {
                                if (flags & F_LEFT) {
                                    /* add spaces on RHS */
                                    if (width > vspace)
                                        padding = vspace - added;
                                    memset(pval + added, ' ', padding);
                                }
                                else {
                                    /* insert spaces on LHS */
                                    int trunc = width - vspace;

                                    if (trunc < added) {
                                        added -= trunc;
                                        memmove(pval + padding, pval, added);
                                    }
                                    else {
                                        padding = vspace;
                                        added = 0;
                                    }
                                    memset(pval, ' ', padding);
                                }
                                added += padding;
                            }
                        }
                    }
                    else {
                        char val[MAX_STRING_SIZE];
                        int ok;

                        if (dbLinkIsConstant(plink))
                            ok = recGblInitConstantLink(plink++, DBR_STRING, val);
                        else
                            ok = ! dbGetLink(plink++, DBR_STRING, val, 0, 0);
                        if (ok)
                            added = epicsSnprintf(pval, vspace + 1, format, val);
                        else
                            flags |= F_BADLNK;
                    }
                    break;

                default:
                    errlogPrintf("printfRecord: Unexpected conversion '%s'\n",
                        format);
                    flags |= F_BADFMT;
                    break;
                }

            if (flags & F_BAD) {
    bad_format:
                added = epicsSnprintf(pval, vspace + 1, "%s",
                    flags & F_BADLNK ? prec->ivls : format);
            }

            if (added <= vspace) {
                pval += added;
                vspace -= added;
            }
            else {
                /* Output was truncated */
                pval += vspace;
                vspace = 0;
            }
        }
    }
    *pval++ = 0;  /* Terminate the VAL string */
    prec->len = pval - prec->val;
}


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct printfRecord *prec = (struct printfRecord *)pcommon;
    printfdset *pdset;

    if (pass == 0) {
        size_t sizv = prec->sizv;

        if (sizv < 16) {
            sizv = 16;  /* Enforce a minimum size for the VAL field */
            prec->sizv = sizv;
        }

        prec->val = callocMustSucceed(1, sizv, "printf::init_record");
        prec->len = 0;
        return 0;
    }

    pdset = (printfdset *) prec->dset;
    if (!pdset)
        return 0;       /* Device support is optional */

    if (pdset->number < 5) {
        recGblRecordError(S_dev_missingSup, prec, "printf::init_record");
        return S_dev_missingSup;
    }

    if (pdset->init_record) {
        long status = pdset->init_record(prec);
        if (status)
            return status;
    }

    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct printfRecord *prec = (struct printfRecord *)pcommon;
    int pact = prec->pact;
    printfdset *pdset;
    long status = 0;
    epicsUInt16 events;

    if (!pact) {
        doPrintf(prec);

        prec->udf = FALSE;
        recGblGetTimeStamp(prec);
    }

    /* Call device support */
    pdset = (printfdset *) prec->dset;
    if (pdset &&
        pdset->number >= 5 &&
        pdset->write_string) {
        status = pdset->write_string(prec);

        /* Asynchronous if device support set pact */
        if (!pact && prec->pact)
            return status;
    }

    prec->pact = TRUE;

    /* Post monitor */
    events = recGblResetAlarms(prec);
    db_post_events(prec, prec->val, events | DBE_VALUE | DBE_LOG);
    db_post_events(prec, &prec->len, events | DBE_VALUE | DBE_LOG);

    /* Wrap up */
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}

static long cvt_dbaddr(DBADDR *paddr)
{
    printfRecord *prec = (printfRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == printfRecordVAL) {
        paddr->pfield         = prec->val;
        paddr->no_elements    = 1;
        paddr->field_type     = DBF_STRING;
        paddr->dbr_field_type = DBF_STRING;
        paddr->field_size     = prec->sizv;
    }
    else
        errlogPrintf("printfRecord::cvt_dbaddr called for %s.%s\n",
            prec->name, paddr->pfldDes->name);
    return 0;
}

static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    printfRecord *prec = (printfRecord *) paddr->precord;

    *no_elements = prec->len;
    *offset = 0;
    return 0;
}


/* Create Record Support Entry Table */

#define report NULL
#define initialize NULL
/* init_record */
/* process */
#define special NULL
#define get_value NULL
/* cvt_dbaddr */
/* get_array_info */
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset printfRSET = {
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double
};
epicsExportAddress(rset, printfRSET);

