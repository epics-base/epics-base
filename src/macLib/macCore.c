/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Implementation of core macro substitution library (macLib)
 *
 * The implementation is fairly unsophisticated and linked lists are
 * used to store macro values. Typically there will will be only a
 * small number of macros and performance won't be a problem. Special
 * measures are taken to avoid unnecessary expansion of macros whose
 * definitions reference other macros. Whenever a macro is created,
 * modified or deleted, a "dirty" flag is set; this causes a full
 * expansion of all macros the next time a macro value is read
 *
 * Original Author: William Lupton, W. M. Keck Observatory
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "errlog.h"
#include "dbmf.h"
#include "macLib.h"


/*** Local structure definitions ***/

/*
 * Entry in linked list of macro definitions
 */
typedef struct mac_entry {
    ELLNODE     node;           /* prev and next pointers */
    char        *name;          /* entry name */
    char        *type;          /* entry type */
    char        *rawval;        /* raw (unexpanded) value */
    char        *value;         /* expanded macro value */
    size_t      length;         /* length of value */
    int         error;          /* error expanding value? */
    int         visited;        /* ever been visited? */
    int         special;        /* special (internal) entry? */
    int         level;          /* scoping level */
} MAC_ENTRY;


/*** Local function prototypes ***/

/*
 * These static functions peform low-level operations on macro entries
 */
static MAC_ENTRY *first   ( MAC_HANDLE *handle );
static MAC_ENTRY *last    ( MAC_HANDLE *handle );
static MAC_ENTRY *next    ( MAC_ENTRY  *entry );
static MAC_ENTRY *previous( MAC_ENTRY  *entry );

static MAC_ENTRY *create( MAC_HANDLE *handle, const char *name, int special );
static MAC_ENTRY *lookup( MAC_HANDLE *handle, const char *name, int special );
static char      *rawval( MAC_HANDLE *handle, MAC_ENTRY *entry, const char *value );
static void       delete( MAC_HANDLE *handle, MAC_ENTRY *entry );
static long       expand( MAC_HANDLE *handle );
static void       trans ( MAC_HANDLE *handle, MAC_ENTRY *entry, int level,
                          const char *term, const char **rawval, char **value,
                          char *valend );
static void       refer ( MAC_HANDLE *handle, MAC_ENTRY *entry, int level,
                          const char **rawval, char **value, char *valend );

static void cpy2val( const char *src, char **value, char *valend );
static char *Strdup( const char *string );


/*** Constants ***/

/*
 * Magic number for validating context.
 */
#define MAC_MAGIC 0xbadcafe     /* ...sells sub-standard coffee? */

/*
 * Flag bits
 */
#define FLAG_SUPPRESS_WARNINGS  0x1
#define FLAG_USE_ENVIRONMENT    0x80


/*** Library routines ***/

/*
 * Create a new macro substitution context and return an opaque handle
 * associated with the new context. Also optionally load an initial set
 * of macro definitions
 */
long                            /* 0 = OK; <0 = ERROR */
epicsShareAPI macCreateHandle(
    MAC_HANDLE  **pHandle,      /* address of variable to receive pointer */
                                /* to new macro substitution context */

    const char * pairs[] )      /* pointer to NULL-terminated array of */
                                /* {name,value} pair strings; a NULL */
                                /* value implies undefined; a NULL */
                                /* argument implies no macros */
{
    MAC_HANDLE *handle;         /* pointer to macro substitution context */

    /* ensure NULL handle pointer is returned on error */
    *pHandle = NULL;

    /* allocate macro substitution context */
    handle = ( MAC_HANDLE * ) dbmfMalloc( sizeof( MAC_HANDLE ) );
    if ( handle == NULL ) {
        errlogPrintf( "macCreateHandle: failed to allocate context\n" );
        return -1;
    }

    /* initialize context */
    handle->magic = MAC_MAGIC;
    handle->dirty = FALSE;
    handle->level = 0;
    handle->debug = 0;
    handle->flags = 0;
    ellInit( &handle->list );

    /* use environment variables if so specified */
    if (pairs && pairs[0] && !strcmp(pairs[0],"") && pairs[1] && !strcmp(pairs[1],"environ") && !pairs[3]) {
        handle->flags |= FLAG_USE_ENVIRONMENT;
    }
    else {
        /* if supplied, load macro definitions */
        for ( ; pairs && pairs[0]; pairs += 2 ) {
            if ( macPutValue( handle, pairs[0], pairs[1] ) < 0 ) {
                dbmfFree( handle );
                return -1;
            }
        }
    }

    /* set returned handle pointer */
    *pHandle = handle;

    return 0;
}

/*
 * Turn on/off suppression of printed warnings from macLib calls
 * for the given handle
 */
void
epicsShareAPI macSuppressWarning(
    MAC_HANDLE  *handle,        /* opaque handle */
    int         suppress        /* 0 means issue, 1 means suppress */
)
{
    if ( handle && handle->magic == MAC_MAGIC ) {
        handle->flags = (handle->flags & ~FLAG_SUPPRESS_WARNINGS) |
                        (suppress ? FLAG_SUPPRESS_WARNINGS : 0);
    }
}

/*
 * Expand a string that may contain macro references and return the
 * expanded string
 *
 * This is a very basic and powerful routine. It's basically a wrapper
 * around the the translation "engine" trans()
 */
long                            /* strlen(dest), <0 if any macros are */
                                /* undefined */
epicsShareAPI macExpandString(
    MAC_HANDLE  *handle,        /* opaque handle */

    const char  *src,           /* source string */

    char        *dest,          /* destination string */

    long        capacity )      /* capacity of destination buffer (dest) */
{
    MAC_ENTRY entry;
    const char *s;
    char *d;
    long length;

    /* check handle */
    if ( handle == NULL || handle->magic != MAC_MAGIC ) {
        errlogPrintf( "macExpandString: NULL or invalid handle\n" );
        return -1;
    }

    /* debug output */
    if ( handle->debug & 1 )
        printf( "macExpandString( %s, capacity = %ld )\n", src, capacity );

    /* Check size */
    if (capacity <= 1)
        return -1;

    /* expand raw values if necessary */
    if ( expand( handle ) < 0 )
        errlogPrintf( "macExpandString: failed to expand raw values\n" );

    /* fill in necessary fields in fake macro entry structure */
    entry.name  = (char *) src;
    entry.type  = "string";
    entry.error = FALSE;

    /* expand the string */
    s  = src;
    d  = dest;
    *d = '\0';
    trans( handle, &entry, 0, "", &s, &d, d + capacity - 1 );

    /* return +/- #chars copied depending on successful expansion */
    length = d - dest;
    length = ( entry.error ) ? -length : length;

    /* debug output */
    if ( handle->debug & 1 )
        printf( "macExpandString() -> %ld\n", length );

    return length;
}

/*
 * Define the value of a macro. A NULL value deletes the macro if it
 * already existed
 */
long                            /* strlen(value) */
epicsShareAPI macPutValue(
    MAC_HANDLE  *handle,        /* opaque handle */

    const char  *name,          /* macro name */

    const char  *value )        /* macro value */
{
    MAC_ENTRY   *entry;         /* pointer to this macro's entry structure */

    /* check handle */
    if ( handle == NULL || handle->magic != MAC_MAGIC ) {
        errlogPrintf( "macPutValue: NULL or invalid handle\n" );
        return -1;
    }

    if ( handle->debug & 1 )
        printf( "macPutValue( %s, %s )\n", name, value ? value : "NULL" );

    /* handle NULL value case: if name was found, delete entry (may be
       several entries at different scoping levels) */
    if ( value == NULL ) {
        /* 
         * FIXME: shouldn't be able to delete entries from lower scopes
         * NOTE: when this is changed, this functionality of removing
         * a macro from all scopes will still be needed by iocshEnvClear
         */
        while ( ( entry = lookup( handle, name, FALSE ) ) != NULL ) {
            int done = strcmp(entry->type, "environment variable") == 0;
            delete( handle, entry );
            
            if (done)
                break;
        }
        
        return 0;
    }

    /* look up macro name */
    entry = lookup( handle, name, FALSE );

    /* new entry must be created if macro doesn't exist or if it only
       exists at a lower scoping level */
    if ( entry == NULL || entry->level < handle->level ) {
        entry = create( handle, name, FALSE );
        if ( entry == NULL ) {
            errlogPrintf( "macPutValue: failed to create macro %s = %s\n",
                            name, value );
            return -1;
        } else {
            entry->type = "macro";
        }
    }

    /* copy raw value */
    if ( rawval( handle, entry, value ) == NULL ) {
        errlogPrintf( "macPutValue: failed to copy macro %s = %s\n",
                        name, value ) ;
        return -1;
    }

    /* return length of value */
    return strlen( value );
}

/*
 * Return the value of a macro
 */
long                            /* strlen(value), <0 if undefined */
epicsShareAPI macGetValue(
    MAC_HANDLE  *handle,        /* opaque handle */

    const char  *name,          /* macro name or reference */

    char        *value,         /* string to receive macro value or name */
                                /* argument if macro is undefined */

    long        capacity )      /* capacity of destination buffer (value) */
{
    MAC_ENTRY   *entry;         /* pointer to this macro's entry structure */
    long        length;         /* number of characters returned */

    /* check handle */
    if ( handle == NULL || handle->magic != MAC_MAGIC ) {
        errlogPrintf( "macGetValue: NULL or invalid handle\n" );
        return -1;
    }

    /* debug output */
    if ( handle->debug & 1 )
        printf( "macGetValue( %s )\n", name );

    /* look up macro name */
    entry = lookup( handle, name, FALSE );

    /* if capacity <= 1 or VALUE == NULL just return -1 / 0 for undefined /
       defined macro */
    if ( capacity <= 1 || value == NULL ) {
        return ( entry == NULL ) ? -1 : 0;
    }

    /* if not found, copy name to value and return minus #chars copied */
    if ( entry == NULL ) {
        strncpy( value, name, capacity );
        return ( value[capacity-1] == '\0' ) ? - (long) strlen( name ) : -capacity;
    }

    /* expand raw values if necessary; if fail (can only fail because of
       memory allocation failure), return same as if not found */
    if ( expand( handle ) < 0 ) {
        errlogPrintf( "macGetValue: failed to expand raw values\n" );
        strncpy( value, name, capacity );
        return ( value[capacity-1] == '\0' ) ? - (long) strlen( name ) : -capacity;
    }

    /* copy value and return +/- #chars copied depending on successful
       expansion */
    strncpy( value, entry->value, capacity );
    length = ( value[capacity-1] == '\0' ) ? entry->length : capacity;

    return ( entry->error ) ? -length : length;
}

/*
 * Free up all storage associated with and delete a macro substitution
 * context
 */
long                            /* 0 = OK; <0 = ERROR */
epicsShareAPI macDeleteHandle(
    MAC_HANDLE  *handle )       /* opaque handle */
{
    MAC_ENTRY *entry, *nextEntry;

    /* check handle */
    if ( handle == NULL || handle->magic != MAC_MAGIC ) {
        errlogPrintf( "macDeleteHandle: NULL or invalid handle\n" );
        return -1;
    }

    /* debug output */
    if ( handle->debug & 1 )
        printf( "macDeleteHandle()\n" );

    /* delete all entries */
    for ( entry = first( handle ); entry != NULL; entry = nextEntry ) {
        nextEntry = next( entry );
        delete( handle, entry );
    }

    /* clear magic field and free context structure */
    handle->magic = 0;
    dbmfFree( handle );

    return 0;
}

/*
 * Mark the start of a new scoping level
 */
long                            /* 0 = OK; <0 = ERROR */
epicsShareAPI macPushScope(
    MAC_HANDLE  *handle )       /* opaque handle */
{
    MAC_ENTRY *entry;

    /* check handle */
    if ( handle == NULL || handle->magic != MAC_MAGIC ) {
        errlogPrintf( "macPushScope: NULL or invalid handle\n" );
        return -1;
    }

    /* debug output */
    if ( handle->debug & 1 )
        printf( "macPushScope()\n" );

    /* increment scoping level */
    handle->level++;

    /* create new "special" entry of name "<scope>" */
    entry = create( handle, "<scope>", TRUE );
    if ( entry == NULL ) {
        handle->level--;
        errlogPrintf( "macPushScope: failed to push scope\n" );
        return -1;
    } else {
        entry->type = "scope marker";
    }

    return 0;
}

/*
 * Pop all macros defined since the last call to macPushScope()
 */
long                            /* 0 = OK; <0 = ERROR */
epicsShareAPI macPopScope(
    MAC_HANDLE  *handle )       /* opaque handle */
{
    MAC_ENTRY *entry, *nextEntry;

    /* check handle */
    if ( handle == NULL || handle->magic != MAC_MAGIC ) {
        errlogPrintf( "macPopScope: NULL or invalid handle\n" );
        return -1;
    }

    /* debug output */
    if ( handle->debug & 1 )
        printf( "macPopScope()\n" );

    /* check scoping level isn't already zero */
    if ( handle->level == 0 ) {
        errlogPrintf( "macPopScope: no scope to pop\n" );
        return -1;
    }

    /* look up most recent scope entry */
    entry = lookup( handle, "<scope>", TRUE );
    if ( entry == NULL ) {
        errlogPrintf( "macPopScope: no scope to pop\n" );
        return -1;
    }

    /* delete scope entry and all macros defined since it */
    for ( ; entry != NULL; entry = nextEntry ) {
        nextEntry = next( entry );
        delete( handle, entry );
    }

    /* decrement scoping level */
    handle->level--;

    return 0;
}

/*
 * Report macro details to standard output
 */
long                            /* 0 = OK; <0 = ERROR */
epicsShareAPI macReportMacros(
    MAC_HANDLE  *handle )       /* opaque handle */
{
    const char *format = "%-1s %-16s %-16s %s\n";
    MAC_ENTRY *entry;

    /* check handle */
    if ( handle == NULL || handle->magic != MAC_MAGIC ) {
        errlogPrintf( "macReportMacros: NULL or invalid handle\n" );
        return -1;
    }

    /* expand raw values if necessary; report but ignore failure */
    if ( expand( handle ) < 0 )
        errlogPrintf( "macGetValue: failed to expand raw values\n" );

    /* loop through macros, reporting names and values */
    printf( format, "e", "name", "rawval", "value" );
    printf( format, "-", "----", "------", "-----" );
    for ( entry = first( handle ); entry != NULL; entry = next( entry ) ) {

        /* differentiate between "special" (scope marker) and ordinary
           entries */
        if ( entry->special )
            printf( format, "s", "----", "------", "-----" );
        else
            printf( format, entry->error ? "*" : " ", entry->name,
                         entry->rawval ? entry->rawval : "",
                         entry->value  ? entry->value  : "");
    }

    return 0;
}

/******************** beginning of static functions ********************/

/*
 * Return pointer to first macro entry (could be preprocessor macro)
 */
static MAC_ENTRY *first( MAC_HANDLE *handle )
{
    return ( MAC_ENTRY * ) ellFirst( &handle->list );
}

/*
 * Return pointer to last macro entry (could be preprocessor macro)
 */
static MAC_ENTRY *last( MAC_HANDLE *handle )
{
    return ( MAC_ENTRY * ) ellLast( &handle->list );
}

/*
 * Return pointer to next macro entry (could be preprocessor macro)
 */
static MAC_ENTRY *next( MAC_ENTRY *entry )
{
    return ( MAC_ENTRY * ) ellNext( ( ELLNODE * ) entry );
}

/*
 * Return pointer to previous macro entry (could be preprocessor macro)
 */
static MAC_ENTRY *previous( MAC_ENTRY *entry )
{
    return ( MAC_ENTRY * ) ellPrevious( ( ELLNODE * ) entry );
}

/*
 * Create new macro entry (can assume it doesn't exist)
 */
static MAC_ENTRY *create( MAC_HANDLE *handle, const char *name, int special )
{
    ELLLIST   *list  = &handle->list;
    MAC_ENTRY *entry = ( MAC_ENTRY * ) dbmfMalloc( sizeof( MAC_ENTRY ) );

    if ( entry != NULL ) {
        entry->name   = Strdup( name );
        if ( entry->name == NULL ) {
            dbmfFree( entry );
            entry = NULL;
        }
        else {
            entry->type    = "";
            entry->rawval  = NULL;
            entry->value   = NULL;
            entry->length  = 0;
            entry->error   = FALSE;
            entry->visited = FALSE;
            entry->special = special;
            entry->level   = handle->level;

            ellAdd( list, ( ELLNODE * ) entry );
        }
    }

    return entry;
}

/*
 * Look up macro entry with matching "special" attribute by name
 */
static MAC_ENTRY *lookup( MAC_HANDLE *handle, const char *name, int special )
{
    MAC_ENTRY *entry;

    if ( handle->debug & 2 )
        printf( "lookup-> level = %d, name = %s, special = %d\n",
                handle->level, name, special );

    /* search backwards so scoping works */
    for ( entry = last( handle ); entry != NULL; entry = previous( entry ) ) {
        if ( entry->special != special )
            continue;
        if ( strcmp( name, entry->name ) == 0 )
            break;
    }
    if ( (special == FALSE) && (entry == NULL) &&
         (handle->flags & FLAG_USE_ENVIRONMENT) ) {
        char *value = getenv(name);
        if (value) {
            entry = create( handle, name, FALSE );
            if ( entry ) {
                entry->type = "environment variable";
                if ( rawval( handle, entry, value ) == NULL )
                    entry = NULL;
            }
        }
    }

    if ( handle->debug & 2 )
        printf( "<-lookup level = %d, name = %s, result = %p\n",
                handle->level, name, entry );

    return entry;
}

/*
 * Copy raw value to macro entry
 */
static char *rawval( MAC_HANDLE *handle, MAC_ENTRY *entry, const char *value )
{
    if ( entry->rawval != NULL )
        dbmfFree( entry->rawval );
    entry->rawval = Strdup( value );

    handle->dirty = TRUE;

    return entry->rawval;
}

/*
 * Delete a macro entry; requires re-expansion of macro values since this
 * macro may be referenced by another one
 */
static void delete( MAC_HANDLE *handle, MAC_ENTRY *entry )
{
    ELLLIST *list = &handle->list;

    ellDelete( list, ( ELLNODE * ) entry );

    dbmfFree( entry->name );
    if ( entry->rawval != NULL )
        dbmfFree( entry->rawval );
    if ( entry->value != NULL )
        free( entry->value );
    dbmfFree( entry );

    handle->dirty = TRUE;
}

/*
 * Expand macro definitions (expensive but done very infrequently)
 */
static long expand( MAC_HANDLE *handle )
{
    MAC_ENTRY *entry;
    const char *rawval;
    char      *value;

    if ( !handle->dirty )
        return 0;

    for ( entry = first( handle ); entry != NULL; entry = next( entry ) ) {

        if ( handle->debug & 2 )
            printf( "\nexpand %s = %s\n", entry->name,
                entry->rawval ? entry->rawval : "" );

        if ( entry->value == NULL ) {
            if ( ( entry->value = malloc( MAC_SIZE + 1 ) ) == NULL ) {  
                return -1;
            }
        }

        /* start at level 1 so quotes and escapes will be removed from
           expanded value */
        rawval = entry->rawval;
        value  = entry->value;
        *value = '\0';
        entry->error  = FALSE;
        trans( handle, entry, 1, "", &rawval, &value, entry->value + MAC_SIZE );
        entry->length = value - entry->value;
        entry->value[MAC_SIZE] = '\0';
    }

    handle->dirty = FALSE;

    return 0;
}

/*
 * Translate raw macro value (recursive). This is by far the most complicated
 * of the macro routines and calls itself recursively both to translate any
 * macros referenced in the name and to translate the resulting name
 */
static void trans( MAC_HANDLE *handle, MAC_ENTRY *entry, int level,
                   const char *term, const char **rawval, char **value,
                   char *valend )
{
    char quote;
    const char *r;
    char *v;
    int discard;
    int macRef;

    /* return immediately if raw value is NULL */
    if ( *rawval == NULL ) return;

    /* discard quotes and escapes if level is > 0 (i.e. if these aren't
       the user's quotes and escapes) */
    discard = ( level > 0 );

    /* debug output */
    if ( handle->debug & 2 )
        printf( "trans-> entry = %p, level = %d, capacity = %u, discard = %s, "
        "rawval = %s\n", entry, level, (unsigned int)(valend - *value), discard ? "T" : "F", *rawval );

    /* initially not in quotes */
    quote = 0;

    /* scan characters until hit terminator or end of string */
    for ( r = *rawval, v = *value; strchr( term, *r ) == NULL; r++ ) {

        /* handle quoted characters (quotes are discarded if in name) */
        if ( quote ) {
            if ( *r == quote ) {
                quote = 0;
                if ( discard ) continue;
            }
        }
        else if ( *r == '"' || *r == '\'' ) {
            quote = *r;
            if ( discard ) continue;
        }

        /* macro reference if '$' followed by '(' or '{' */
        macRef = ( *r == '$' &&
                   *( r + 1 ) != '\0' &&
                   strchr( "({", *( r + 1 ) ) != NULL );

        /* macros are not expanded in single quotes */
        if ( macRef && quote != '\'' ) {
            /* Handle macro reference */
            refer ( handle, entry, level, &r, &v, valend );
        }

        else {
            /* handle escaped characters (escape is discarded if in name) */
            if ( *r == '\\' && *( r + 1 ) != '\0' ) {
                if ( v < valend && !discard ) *v++ = '\\';
                if ( v < valend ) *v++ = *++r;
            }

            /* copy character to output */
            else {
                if ( v < valend ) *v++ = *r;
            }

            /* ensure string remains properly terminated */
            if ( v <= valend ) *v   = '\0';
        }
    }

    /* debug output */
    if ( handle->debug & 2 )
        printf( "<-trans level = %d, length = %4u, value  = %s\n",
                     level, (unsigned int)(v - *value), *value );

    /* update pointers to next characters to scan in raw value and to fill
       in in output value (if at end of input, step back so terminator is
       still there to be seen) */
    *rawval = ( *r == '\0' ) ? r - 1 : r;
    *value  = v;

    return;
}

/*
 * Expand a macro reference, handling default values and defining scoped
 * macros as encountered. This code used to part of trans(), but was
 * pulled out for ease of understanding.
 */
static void refer ( MAC_HANDLE *handle, MAC_ENTRY *entry, int level,
                    const char **rawval, char **value, char *valend )
{
    const char *r = *rawval;
    char *v = *value;
    char refname[MAC_SIZE + 1] = {'\0'};
    char *rn = refname;
    MAC_ENTRY *refentry;
    const char *defval = NULL;
    const char *macEnd;
    const char *errval = NULL;
    int pop = FALSE;

    /* debug output */
    if ( handle->debug & 2 )
        printf( "refer-> entry = %p, level = %d, capacity = %u, rawval = %s\n",
                entry, level, (unsigned int)(valend - *value), *rawval );

    /* step over '$(' or '${' */
    r++;
    macEnd = ( *r == '(' ) ? "=,)" : "=,}";
    r++;

    /* translate name (may contain macro references); truncated
       quietly if too long but always guaranteed zero-terminated */
    trans( handle, entry, level + 1, macEnd, &r, &rn, rn + MAC_SIZE );
    refname[MAC_SIZE] = '\0';

    /* Is there a default value? */
    if ( *r == '=' ) {
        MAC_ENTRY dflt;
        int flags = handle->flags;
        handle->flags |= FLAG_SUPPRESS_WARNINGS;

        /* store its location in case we need it */
        defval = ++r;

        /* Find the end, discarding its value */
        dflt.name = refname;
        dflt.type = "default value";
        dflt.error = FALSE;
        trans( handle, &dflt, level + 1, macEnd+1, &r, &v, v);

        handle->flags = flags;
    }

    /* extract and set values for any scoped macros */
    if ( *r == ',' ) {
        MAC_ENTRY subs;
        int flags = handle->flags;
        handle->flags |= FLAG_SUPPRESS_WARNINGS;

        subs.type = "scoped macro";
        subs.error = FALSE;

        macPushScope( handle );
        pop = TRUE;

        while ( *r == ',' ) {
            char subname[MAC_SIZE + 1] = {'\0'};
            char subval[MAC_SIZE + 1] = {'\0'};
            char *sn = subname;
            char *sv = subval;

            /* translate the macro name */
            ++r;
            subs.name = refname;

            trans( handle, &subs, level + 1, macEnd, &r, &sn, sn + MAC_SIZE );
            subname[MAC_SIZE] = '\0';

            /* If it has a value, translate that and assign it */
            if ( *r == '=' ) {
                ++r;
                subs.name = subname;

                trans( handle, &subs, level + 1, macEnd+1, &r, &sv,
                      sv + MAC_SIZE);
                subval[MAC_SIZE] = '\0';

                macPutValue( handle, subname, subval );
                handle->dirty = TRUE;   /* re-expand with new macro values */
            }
        }

        handle->flags = flags;
    }

    /* Now we can look up the translated name */
    refentry = lookup( handle, refname, FALSE );

    if ( refentry ) {
        if ( !refentry->visited ) {
            /* reference is good, use it */
            if ( !handle->dirty ) {
                /* copy the already-expanded value, merge any error status */
                cpy2val( refentry->value, &v, valend );
                entry->error = entry->error || refentry->error;
            } else {
                /* translate raw value */
                const char *rv = refentry->rawval;
                refentry->visited = TRUE;
                trans( handle, entry, level + 1, "", &rv, &v, valend );
                refentry->visited = FALSE;
            }
            goto cleanup;
        }
        /* reference is recursive */
        entry->error = TRUE;
        errval = ",recursive)";
        if ( (handle->flags & FLAG_SUPPRESS_WARNINGS) == 0 ) {
            errlogPrintf( "macLib: %s %s is recursive (expanding %s %s)\n",
                          entry->type, entry->name,
                          refentry->type, refentry->name );
        }
    } else {
        /* no macro found by this name */
        if ( defval ) {
            /* there was a default value, translate that instead */
            trans( handle, entry, level + 1, macEnd+1, &defval, &v, valend );
            goto cleanup;
        }
        entry->error = TRUE;
        errval = ",undefined)";
        if ( (handle->flags & FLAG_SUPPRESS_WARNINGS) == 0 ) {
            errlogPrintf( "macLib: macro %s is undefined (expanding %s %s)\n",
                        refname, entry->type, entry->name );
        }
    }

    /* Bad reference, insert $(name,errval) */
    if ( v < valend ) *v++ = '$';
    if ( v < valend ) *v++ = '(';
    cpy2val( refname, &v, valend );
    cpy2val( errval, &v, valend );

cleanup:
    if (pop) {
        macPopScope( handle );
    }

    /* debug output */
    if ( handle->debug & 2 )
        printf( "<-refer level = %d, length = %4u, value  = %s\n",
                     level, (unsigned int)(v - *value), *value );

    *rawval = r;
    *value = v;
    return;
}

/*
 * Copy a string, honoring the 'end of destination string' pointer
 * Returns with **value pointing to the '\0' terminator
 */
static void cpy2val(const char *src, char **value, char *valend)
{
    char *v = *value;
    while ((v < valend) && (*v = *src++)) { v++; }
    *v = '\0';
    *value = v;
}

/*
 * strdup() implementation which uses our own memory allocator
 */
static char *Strdup(const char *string )
{
    char *copy = dbmfMalloc( strlen( string ) + 1 );

    if ( copy != NULL )
        strcpy( copy, string );

    return copy;
}

