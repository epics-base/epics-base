
#ifndef INCerrSymTblh
#define INCerrSymTblh 1

#ifndef NELEMENTS
#define NELEMENTS(array)                /* number of elements in an array */ \
                (sizeof (array) / sizeof ((array) [0]))
#endif
#define LOCAL static

typedef struct		/* ERRSYMBOL - entry in symbol table */
    {
    char *name;		/* pointer to symbol name */
    long errNum;	/* errMessage symbol number */
    } ERRSYMBOL;
typedef struct		/* ERRSYMTAB - symbol table */
    {
    int nsymbols;	/* current number of symbols in table */
    ERRSYMBOL *symbols;	/* ptr to array of symbol entries */
    } ERRSYMTAB;
typedef ERRSYMTAB *ERRSYMTAB_ID;


#endif /* INCerrSymTblh */
