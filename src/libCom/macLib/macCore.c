/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *
 * Implementation of core macro substitution library (macLib)
 *
 * Just about all functionality proposed in tech-talk on 19-Feb-96 has
 * been implemented with only minor departures. Missing features are:
 *
 * 1. All the attribute-setting routines (for setting and getting
 *    special characters etc)
 *
 * 2. Error message verbosity control (although a debug level is
 *    supported)
 *
 * 3. The macro substitution tool (although the supplied macTest
 *    program is in fact a simple version of this)
 *
 * The implementation is fairly unsophisticated and linked lists are
 * used to store macro values. Typically there will will be only a
 * small number of macros and performance won't be a problem. Special
 * measures are taken to avoid unnecessary expansion of macros whose
 * definitions reference other macros. Whenever a macro is created,
 * modified or deleted, a "dirty" flag is set; this causes a full
 * expansion of all macros the next time a macro value is read
 *
 * See the files README (original specification from tech-talk) and
 * NOTES (valid input file for the macTest program) for further
 * information
 *
 * Testing has been primarily under SunOS, but the code has been built
 * and macTest <NOTES run under vxWorks
 *
 * William Lupton, W. M. Keck Observatory
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "errlog.h"
#include "dbmf.h"
#include "macLib.h"

/*
 * Static function prototypes (these static functions provide an low-level
 * library operating on macro entries)
 */
static MAC_ENTRY *first   ( MAC_HANDLE *handle );
static MAC_ENTRY *last    ( MAC_HANDLE *handle );
static MAC_ENTRY *next    ( MAC_ENTRY  *entry );
static MAC_ENTRY *previous( MAC_ENTRY  *entry );

static MAC_ENTRY *create( MAC_HANDLE *handle, const char *name, long special );
static MAC_ENTRY *lookup( MAC_HANDLE *handle, const char *name, long special );
static char      *rawval( MAC_HANDLE *handle, MAC_ENTRY *entry, const char *value );
static void       delete( MAC_HANDLE *handle, MAC_ENTRY *entry );
static long       expand( MAC_HANDLE *handle );
static void       trans ( MAC_HANDLE *handle, MAC_ENTRY *entry, long level,
                        char *term, const char **rawval, char **value, char *valend );
static void cpy2val(const char *src, char **value, char *valend);

/*
 * Flag bits
 */
#define FLAG_SUPPRESS_WARNINGS  0x1
#define FLAG_USE_ENVIRONMENT    0x80

static char *Strdup( const char *string );

/*
 * Static variables
 */
static MAC_CHARS macDefaultChars = {
    MAC_STARTCHARS,
    MAC_LEFTCHARS,
    MAC_RIGHTCHARS,
    MAC_SEPARCHARS,
    MAC_ASSIGNCHARS,
    MAC_ESCAPECHARS
};

/*
 * Create a new macro substitution context and return an opaque handle
 * associated with the new context. Also optionally load an initial set
 * of macro definitions
 */
long                            /* 0 = OK; <0 = ERROR */
epicsShareAPI macCreateHandle(
    MAC_HANDLE  **pHandle,      /* address of variable to receive pointer */
                                /* to new macro substitution context */

    char        *pairs[] )      /* pointer to NULL-terminated array of */
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
    handle->chars = macDefaultChars;
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
void epicsShareAPI macSuppressWarning(
    MAC_HANDLE  *handle,        /* opaque handle */
    int         falseTrue       /*0 means issue, 1 means suppress*/
)
{
    if(handle) handle->flags = (handle->flags & ~FLAG_SUPPRESS_WARNINGS) |
                                    (falseTrue ? FLAG_SUPPRESS_WARNINGS : 0);
}

/*
 * Expand a string that may contain macro references and return the
 * expanded string
 *
 * This is a very basic and powerful routine. It's basically a wrapper
 * around the the translation "engine" trans()
 */
long                            /* #chars copied, <0 if any macros are */
                                /* undefined */
epicsShareAPI macExpandString(
    MAC_HANDLE  *handle,        /* opaque handle */

    const char  *src,           /* source string */

    char        *dest,          /* destination string */

    long        maxlen )        /* maximum number of characters to copy */
                                /* to destination string */
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
        printf( "macExpandString( %s, maxlen = %ld )\n", src, maxlen );

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
    trans( handle, &entry, 0, "", &s, &d, d + maxlen );

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
long                            /* length of value */
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
/* FIXME: don't loop or delete entries from any lower scopes */
        while ( ( entry = lookup( handle, name, FALSE ) ) != NULL )
            delete( handle, entry );
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
long                            /* #chars copied (<0 if undefined) */
epicsShareAPI macGetValue(
    MAC_HANDLE  *handle,        /* opaque handle */

    const char  *name,          /* macro name or reference */

    char        *value,         /* string to receive macro value or name */
                                /* argument if macro is undefined */

    long        maxlen )        /* maximum number of characters to copy */
                                /* to value */
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

    /* if maxlen <= 0 or VALUE == NULL just return -1 / 0 for undefined /
       defined macro */
    if ( maxlen <= 0 || value == NULL ) {
        return ( entry == NULL ) ? -1 : 0;
    }

    /* if not found, copy name to value and return minus #chars copied */
    if ( entry == NULL ) {
        strncpy( value, name, maxlen );
        return ( value[maxlen-1] == '\0' ) ? - (long) strlen( name ) : -maxlen;
    }

    /* expand raw values if necessary; if fail (can only fail because of
       memory allocation failure), return same as if not found */
    if ( expand( handle ) < 0 ) {
        errlogPrintf( "macGetValue: failed to expand raw values\n" );
        strncpy( value, name, maxlen );
        return ( value[maxlen-1] == '\0' ) ? - (long) strlen( name ) : -maxlen;
    }

    /* copy value and return +/- #chars copied depending on successful
       expansion */
/* FIXME: nul-terminator */
    strncpy( value, entry->value, maxlen );
    length = ( value[maxlen-1] == '\0' ) ? entry->length : maxlen;

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
static MAC_ENTRY *create( MAC_HANDLE *handle, const char *name, long special )
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
static MAC_ENTRY *lookup( MAC_HANDLE *handle, const char *name, long special )
{
    MAC_ENTRY *entry;

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
 *
 * For now, use default special characters
 */
static void trans( MAC_HANDLE *handle, MAC_ENTRY *entry, long level,
                   char *term, const char **rawval, char **value, char *valend )
{
    char quote;
    const char *r, *r2;
    char *v, *n2;
    char name2[MAC_SIZE + 1];
    long discard;
    long macRef;
    char *macEnd;
    MAC_ENTRY *entry2;

    /* return immediately if raw value is NULL */
    if ( *rawval == NULL ) return;

    /* discard quotes and escapes if level is > 0 (i.e. if these aren't
       the user's quotes and escapes) */
    discard = ( level > 0 );

    /* debug output */
    if ( handle->debug & 2 )
        printf( "trans-> level = %ld, maxlen = %4u, discard = %s, "
        "rawval = %s\n", level, (unsigned int)(valend - *value), discard ? "T" : "F", *rawval );

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

        /* if not macro reference (macros are not expanded in single quotes) */
        if ( quote == '\'' || !macRef ) {

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

        /* macro reference */
        else {

            /* step over '$(' or '${' */
            r++;
            macEnd = ( *r == '(' ) ? "=)" : "=}";
            r++;

            /* translate name (may contain macro references); truncated
               quietly if too long but always guaranteed zero-terminated */
            n2 = name2;
            *n2 = '\0';
            trans( handle, entry, level + 1, macEnd, &r, &n2, n2 + MAC_SIZE );
            name2[MAC_SIZE] = '\0';

            /* look up the translated name */
            if ( ( entry2 = lookup( handle, name2, FALSE ) ) == NULL ) {
                /* not found */
                if ( *r == '=' ) {
                    /* there is a default value, translate that instead */
                    ++r;
                    trans( handle, entry, level + 1, ++macEnd, &r, &v, valend );
                } else {
                    /* flag the warning and insert $(name) */
                    if ( (handle->flags & FLAG_SUPPRESS_WARNINGS) == 0 ) {
                        entry->error = TRUE;
                        errlogPrintf( "macLib: macro %s is undefined (expanding %s %s)\n",
                                    name2, entry->type, entry->name );
                    }
                    if ( v < valend ) *v++ = '$';
                    if ( v < valend ) *v++ = '(';
                    cpy2val( name2, &v, valend );
                    if ( v < valend ) *v++ = ')';

                    /* ensure string remains properly terminated */
                    if ( v <= valend ) *v   = '\0';
                }
                continue;
            }

            /* if reference is recursive, flag an error and insert $(name) */
            if ( entry2->visited ) {
                entry->error = TRUE;
                errlogPrintf( "macLib: %s %s is recursive (expanding %s %s)\n",
                              entry->type, entry->name,
                              entry2->type, entry2->name );
                if ( v < valend ) *v++ = '$';
                if ( v < valend ) *v++ = '(';
                cpy2val( name2, &v, valend );
                if ( v < valend ) *v++ = ')';

                /* ensure string remains properly terminated */
                if ( v <= valend ) *v   = '\0';
                continue;
            }

            /* translate but discard any default value, supressing warnings */
            if ( *r == '=' ) {
                MAC_ENTRY dflt;
                int flags = handle->flags;
                
                ++r;
                handle->flags |= FLAG_SUPPRESS_WARNINGS;
                dflt.name = name2;
                dflt.type = "default value";
                dflt.error = FALSE;
                trans( handle, &dflt, level + 1, ++macEnd, &r, &v, v);
                handle->flags = flags;
            }

            /* if all expanded values are valid (not "dirty") copy the
               value directly */
            if ( !handle->dirty ) {
                cpy2val( entry2->value, &v, valend );
            }

            /* otherwise, translate raw value */
            else {
                r2 = entry2->rawval;
                entry2->visited = TRUE;
                trans( handle, entry, level + 1, "", &r2, &v, valend );
                entry2->visited = FALSE;
            }
        }
    }

    /* debug output */
    if ( handle->debug & 2 )
        printf( "<-trans level = %ld, length = %4u, value  = %s\n",
                     level, (unsigned int)(v - *value), *value );

    /* update pointers to next characters to scan in raw value and to fill
       in in output value (if at end of input, step back so terminator is
       still there to be seen) */
    *rawval = ( *r == '\0' ) ? r - 1 : r;
    *value  = v;

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

