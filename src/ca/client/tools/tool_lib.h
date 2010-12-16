/*************************************************************************\
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
 *  2009/03/31 Larry Hoff (BNL)
 *     Added field separators
 *
 */

#ifndef INCLtool_libh
#define INCLtool_libh

#include <epicsTime.h>

/* Convert status and severity to strings */
#define stat_to_str(stat)                                       \
        ((stat) >= 0 && (stat) <= (signed)lastEpicsAlarmCond) ? \
        epicsAlarmConditionStrings[stat] : "??"

#define sevr_to_str(stat)                                       \
        ((stat) >= 0 && (stat) <= (signed)lastEpicsAlarmSev) ?  \
        epicsAlarmSeverityStrings[stat] : "??"

#define stat_to_str_unsigned(stat)              \
        ((stat) <= lastEpicsAlarmCond) ?        \
        epicsAlarmConditionStrings[stat] : "??"

#define sevr_to_str_unsigned(stat)              \
        ((stat) <= lastEpicsAlarmSev) ?         \
        epicsAlarmSeverityStrings[stat] : "??"

/* The different versions are necessary because stat and sevr are
 * defined unsigned in CA's DBR_STSACK structure and signed in all the
 * others. Some compilers generate warnings if you check an unsigned
 * being >=0 */


#define DEFAULT_CA_PRIORITY 0  /* Default CA priority */
#define DEFAULT_TIMEOUT 1.0     /* Default CA timeout */

#ifndef _WIN32
#  define LINE_BUFFER(stream) setvbuf(stream, NULL, _IOLBF, BUFSIZ)
#else
/* Windows doesn't support line mode, turn buffering off completely */
#  define LINE_BUFFER(stream) setvbuf(stream, NULL, _IONBF, 0)
#endif


/* Type of timestamp */
typedef enum { absolute, relative, incremental, incrementalByChan } TimeT;

/* Output formats for integer data types */
typedef enum { dec, bin, oct, hex } IntFormatT;

/* Structure representing one PV (= channel) */
typedef struct 
{
    char* name;
    chid  chid;
    long  dbfType;
    long  dbrType;
    unsigned long nElems;       // True length of data in value
    unsigned long reqElems;     // Requested length of data
    int status;
    void* value;
    epicsTimeStamp tsPreviousC;
    epicsTimeStamp tsPreviousS;
    char firstStampPrinted;
    char onceConnected;
} pv;


extern TimeT tsType;        /* Timestamp type flag (-t option) */
extern int tsSrcServer;     /* Timestamp source flag (-t option) */
extern int tsSrcClient;     /* Timestamp source flag (-t option) */
extern IntFormatT outTypeI; /* Flag used for -0.. output format option */
extern IntFormatT outTypeF; /* Flag used for -l.. output format option */
extern int enumAsNr;        /* Used for -n option (get DBF_ENUM as number) */
extern int charArrAsStr;    /* used for -S option - treat char array as (long) string */
extern double caTimeout;    /* Wait time default (see -w option) */
extern char dblFormatStr[]; /* Format string to print doubles (see -e -f option) */
extern char fieldSeparator; /* Output field separator */
extern capri caPriority;    /* CA priority */

extern char *val2str (const void *v, unsigned type, int index);
extern char *dbr2str (const void *value, unsigned type);
extern void print_time_val_sts (pv *pv, unsigned long reqElems);
extern int  create_pvs (pv *pvs, int nPvs, caCh *pCB );
extern int  connect_pvs (pv *pvs, int nPvs );

/*
 * no additions below this endif
 */
#endif /* ifndef INCLtool_libh */
