/* $Id$
 *
 * Definitions for macro substitution library (macLib)
 *
 * William Lupton, W. M. Keck Observatory
 */

/*
 * EPICS include files needed by this file
 */
#include "ellLib.h"
#include "epicsPrint.h"
#include "errMdef.h"

/*
 * Standard FALSE and TRUE macros
 */
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/*
 * Magic number for validating context.
 */
#define MAC_MAGIC 0xbadcafe	/* ...sells sub-standard coffee? */

/*
 * Maximum size of macro name or value string (simpler to make fixed)
 */
#define MAC_SIZE 256

/*
 * Macros passing __FILE__ and __LINE__ to what will be macErrPrintf()
 */
#define macErrMessage0( _status, _format ) \
	errPrintf( _status, __FILE__, __LINE__, _format )

#define macErrMessage1( _status, _format, _arg1 ) \
	errPrintf( _status, __FILE__, __LINE__, _format, _arg1 )

#define macErrMessage2( _status, _format, _arg1, _arg2 ) \
	errPrintf( _status, __FILE__, __LINE__, _format, _arg1, _arg2 )

#define macErrMessage3( _status, _format, _arg1, _arg2, _arg3 ) \
	errPrintf( _status, __FILE__, __LINE__, _format, _arg1, _arg2, _arg3 )

/*
 * Special characters
 */
#define MAC_STARTCHARS	"$"	/* characters that indicate macro ref */
#define MAC_LEFTCHARS	"{("	/* characters to left of macro name */
#define MAC_RIGHTCHARS	"})"	/* characters to right of macro name */
#define MAC_SEPARCHARS	","	/* characters separating macro defns */
#define MAC_ASSIGNCHARS	"="	/* characters denoting values assignment */
#define MAC_ESCAPECHARS "\\"	/* characters escaping the next char */

typedef struct {
    char *start;
    char *left;
    char *right;
    char *separ;
    char *assign;
    char *escape;
} MAC_CHARS;

/*
 * Macro substitution context. One of these contexts is allocated each time
 * macCreateHandle() is called
 */
typedef struct {
    long	magic;		/* magic number (used for authentication) */
    MAC_CHARS	chars;		/* special characters */
    long	dirty;		/* values need expanding from raw values? */
    long	level;		/* scoping level */
    long	debug;		/* debugging level */
    ELLLIST	list;		/* macro name / value list */
} MAC_HANDLE;

/*
 * Entry in linked list of macro definitions
 */
typedef struct mac_entry {
    ELLNODE	node;		/* prev and next pointers */
    char	*name;		/* macro name */
    char	*rawval;	/* raw (unexpanded) value */
    char	*value;		/* expanded macro value */
    long	length;		/* length of value */
    long	error;		/* error expanding value? */
    long	visited;	/* ever been visited? */
    long	special;	/* special (internal) entry? */
    long	level;		/* scoping level */
} MAC_ENTRY;

/*
 * Function prototypes (core library)
 */
long				/* 0 = OK; <0 = ERROR */
macCreateHandle(
    MAC_HANDLE	**handle,	/* address of variable to receive pointer */
				/* to new macro substitution context */

    char	*pairs[]	/* pointer to NULL-terminated array of */
				/* {name,value} pair strings; a NULL */
				/* value implies undefined; a NULL */
				/* argument implies no macros */
);

long				/* #chars copied, <0 if any macros are */
				/* undefined */
macExpandString(
    MAC_HANDLE	*handle,	/* opaque handle */

    char	*src,		/* source string */

    char	*dest,		/* destination string */

    long	maxlen		/* maximum number of characters to copy */
				/* to destination string */
);

long				/* length of value */
macPutValue(
    MAC_HANDLE	*handle,	/* opaque handle */

    char	*name,		/* macro name */

    char	*value		/* macro value */
);

long				/* #chars copied (<0 if undefined) */
macGetValue(
    MAC_HANDLE	*handle,	/* opaque handle */

    char	*name,		/* macro name or reference */

    char	*value,		/* string to receive macro value or name */
				/* argument if macro is undefined */

    long	maxlen		/* maximum number of characters to copy */
				/* to value */
);

long				/* 0 = OK; <0 = ERROR */
macDeleteHandle(
    MAC_HANDLE	*handle		/* opaque handle */
);

long				/* 0 = OK; <0 = ERROR */
macPushScope(
    MAC_HANDLE	*handle		/* opaque handle */
);

long				/* 0 = OK; <0 = ERROR */
macPopScope(
    MAC_HANDLE	*handle		/* opaque handle */
);

long				/* 0 = OK; <0 = ERROR */
macReportMacros(
    MAC_HANDLE	*handle		/* opaque handle */
);

/*
 * Function prototypes (utility library)
 */
long				/* #defns encountered; <0 = ERROR */
macParseDefns(
    MAC_HANDLE	*handle,	/* opaque handle; can be NULL if default */
				/* special characters are to be used */

    char	*defns,		/* macro definitions in "a=xxx,b=yyy" */
				/* format */

    char	**pairs[]	/* address of variable to receive pointer */
				/* to NULL-terminated array of {name, */
				/* value} pair strings; all storage is */
				/* allocated contiguously */
);

long				/* #macros defined; <0 = ERROR */
macInstallMacros(
    MAC_HANDLE	*handle,	/* opaque handle */

    char	*pairs[]	/* pointer to NULL-terminated array of */
				/* {name,value} pair strings; a NULL */
				/* value implies undefined; a NULL */
				/* argument implies no macros */
);

/* $Log$
 * Revision 1.1  1996/07/10 14:49:51  mrk
 * added macLib
 *
 * Revision 1.8  1996/06/26  09:43:16  wlupton
 * first released version
 *
 */
