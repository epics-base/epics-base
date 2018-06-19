/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */ 

#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef _POSIX_MAPPED_FILES
#include <sys/mman.h>
#endif

#define epicsExportSharedSymbols
#include "epicsMutex.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "errlog.h"
#include "epicsStackTrace.h"
#include "epicsStackTracePvt.h"

/* This routine is provided by osiClockTime.c */
epicsShareExtern void ClockTime_GetProgramStart(epicsTimeStamp *pDest);

#define FIND_ADDR_DEBUG 0

/* 
 * On some systems (linux, solaris) dladdr doesn't find local symbols
 * or symbols in the main executable.
 * Hence, we want to use dladdr() to find the file name
 * where a symbol is defined and if not more information is available
 * then proceed to lookup symbols in the ELF symbol tables.
 */

/* Macros to handle elf32 vs. elf64 access to unions etc. */

#define FLD(c,s,f)   (ELFCLASS32==c ? s.e32.f    : s.e64.f   )
#define ARR(c,s,i,f) (ELFCLASS32==c ? s.e32[i].f : s.e64[i].f)

/* Elf header      */
typedef union Ehdr_ {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
} Ehdr;

/* Section header  */
typedef union Shdr_ {
    Elf32_Shdr e32;
    Elf64_Shdr e64;
} Shdr;

/* Elf symbol     */
typedef union Sym_ {
    void       *raw;
    Elf32_Sym  *e32;
    Elf64_Sym  *e64;
} Sym;

/* Memory mapped portion of a file; we must
 * keep additional information because the
 * map's starting address + length must be
 * page-aligned (man mmap).
 */
typedef struct MMap_ {
    void    *addr;
    off_t    off;                        /* offset into the map where 'real' data start            */
    size_t   len;
    size_t   max;                        /* max offset: legal data from addr+off .. addr+off+max-1 */
    void     (*freeMap)(struct MMap_*);  /* 'method' to destroy the mapping                        */
} *MMap;

/* Structure describing symbol information
 * contained in a file.
 * We keep these around (so that the file 
 * doesn't have to be opened + parsed every
 * time we do a lookup).
 */
typedef struct ESyms_ {
    struct ESyms_ *next;  /* linked list; one struct per executable */
    const char    *fname; /* file name                              */
    int            fd;    /* file descriptor                        */
    uintptr_t      addr;  /* address where file is loaded           */
    MMap           symMap;
    MMap           strMap;
    size_t         nsyms;
    uint8_t        eclss;
} *ESyms;

/* LOCKING NOTE: if the ELF symbol facility is ever expanded to be truly used
 * in a multithreaded way then proper multiple-readers, single-writer locking
 * should be implemented.
 */

/* Linked list where we keep all our ESyms */
static ESyms             elfs       = 0;

static epicsMutexId      listMtx;
static epicsThreadOnceId listMtxInitId = EPICS_THREAD_ONCE_INIT;

static void listMtxInit(void *unused)
{
    listMtx = epicsMutexMustCreate();
}

static void
elfsLockWrite()
{
    epicsThreadOnce(&listMtxInitId, listMtxInit, 0);
    epicsMutexMustLock(listMtx);
}

static void
elfsUnlockWrite()
{
    epicsMutexUnlock(listMtx);
}

static void
freeMap(MMap m)
{
    if ( m ) {
        m->freeMap(m);
        free(m);
    }
}

/* Helper to read exactly 'sz' bytes into 'buf'
 * RETURNS: # chars read or negative value on error.
 */
static ssize_t
do_read(int fd, void *buf, ssize_t sz)
{
    ssize_t got;
    char   *ptr=(char*)buf;

    while ( sz > 0 ) {
        if ( (got=read(fd,ptr,sz)) <= 0 ) {
            return got;
        }
        ptr+=got;
        sz -=got;
    }
    return ptr-(char*)buf;
}

/* Elf file access -- can either be with mmap or by sequential read */

#ifdef _POSIX_MAPPED_FILES
/* Destructor for data that is mmap()ed */
static void
freeMapMmap(MMap m)
{
    if ( MAP_FAILED != m->addr )
        munmap( m->addr, m->len );
}

/* Obtain section data with mmap()      */
static MMap
getscn_mmap(int fd, uint8_t c, Shdr *shdr_p)
{
    off_t    n;
    MMap     rval = 0;
    size_t   pgsz  = sysconf(_SC_PAGESIZE);

    if ( 0 == (n = (off_t)FLD(c,(*shdr_p),sh_size)) ) {
        errlogPrintf("elfRead - getscn() -- no section data\n");
        goto bail;
    }

    if ( ! (rval = (MMap) malloc(sizeof(*rval))) ) {
        errlogPrintf("elfRead - getscn() -- no memory for section map\n");
        goto bail;
    }

    rval->freeMap = freeMapMmap;

    rval->off = (off_t) (FLD(c,(*shdr_p),sh_offset)   & (pgsz-1));
    rval->len = (n + rval->off + (pgsz - 1)) & ~(pgsz - 1);
    rval->max = rval->len - rval->off;

    if ( MAP_FAILED == (rval->addr = mmap(0, rval->len, PROT_READ, MAP_SHARED, fd, (off_t) (FLD(c,(*shdr_p),sh_offset) & ~(pgsz-1)))) ) {
        errlogPrintf("elfRead - getscn() -- mapping section contents: %s\n", strerror(errno));
        goto bail;
    }

    return rval;

bail:
    freeMap(rval);
    return 0;
}
#else
static MMap getscn_mmap(int fd, uint8_t c, Shrd *shdr_p)
{
	return 0;
}
#endif

/* Destructor for data that is read into a malloc()ed buffer */
static void
freeMapMalloc(MMap m)
{
    free(m->addr);
}

/* Read section data into a malloc()ed buffer                */
static MMap
getscn_read(int fd, uint8_t c, Shdr *shdr_p)
{
    ssize_t  n;
    MMap     rval = 0;

    if ( 0 == (n = (ssize_t) FLD(c,(*shdr_p),sh_size)) ) {
        errlogPrintf("elfRead - getscn() -- no section data\n");
        goto bail;
    }

    if ( ! (rval = (MMap) malloc(sizeof(*rval))) ) {
        errlogPrintf("elfRead - getscn() -- no memory for section map\n");
        goto bail;
    }

    rval->freeMap = freeMapMalloc;

    if ( ! (rval->addr = malloc(n)) ) {
        errlogPrintf("elfRead - getscn() -- no memory for section data\n");
        goto bail;
    }

    rval->off = 0;
    rval->len = n;
    rval->max = rval->len - rval->off;

    /* seek to symbol table contents */
    if ( (off_t)-1 == lseek(fd, (off_t) FLD(c,(*shdr_p),sh_offset), SEEK_SET) ) {
        errlogPrintf("elfRead - getscn() -- seeking to sh_offset: %s\n", strerror(errno));
        goto bail;
    }

    if ( n != do_read(fd, rval->addr, n) ) {
        errlogPrintf("elfRead - getscn() -- reading section contents: %s\n", strerror(errno));
        goto bail;
    }

    return rval;

bail:
    freeMap(rval);
    return 0;
}

static MMap
getscn(int fd, uint8_t c, Shdr *shdr_p)
{
    MMap rval = getscn_mmap(fd, c, shdr_p);

    if ( ! rval )
        rval = getscn_read(fd, c, shdr_p);

    return rval;
}

/* Release resources but keep filename so that
 * a file w/o symbol table is not read over and over again.
 */
static void
elfSymsRelease(ESyms es)
{
    if ( es ) {
        freeMap(es->symMap);
        es->symMap = 0;
        freeMap(es->strMap);
        es->strMap = 0;
        if ( es->fd >= 0 )
            close(es->fd);
        es->fd     = -1;
        es->nsyms  = 0;
    }
}

static ESyms
elfRead(const char *fname, uintptr_t fbase)
{
    int         i;
    Ehdr        ehdr;
    Shdr        shdr;
    uint8_t     c;
    ESyms       es;
    ssize_t     idx,n;
    const char *cp;
    struct stat stat_b;

    if ( !(es = (ESyms) malloc(sizeof(*es))) ) {
        /* no memory -- give up */
        return 0;
    }

    memset(es, 0, sizeof(*es));
    es->fd    = -1;
    es->fname = fname;

    if ( (es->fd = open(fname, O_RDONLY)) < 0 ) {
        errlogPrintf("elfRead() -- unable to open file: %s\n", strerror(errno));
        goto bail;
    }

    if ( EI_NIDENT != do_read(es->fd, &ehdr, EI_NIDENT) ) {
        errlogPrintf("elfRead() -- unable to read ELF e_ident: %s\n", strerror(errno));
        goto bail;
    }

    if (    ELFMAG0 != ehdr.e32.e_ident[EI_MAG0]
         || ELFMAG1 != ehdr.e32.e_ident[EI_MAG1]
         || ELFMAG2 != ehdr.e32.e_ident[EI_MAG2]
         || ELFMAG3 != ehdr.e32.e_ident[EI_MAG3] ) {
        errlogPrintf("bad ELF magic number\n");
        goto bail;
    }

    if ( EV_CURRENT != ehdr.e32.e_ident[EI_VERSION] ) {
        errlogPrintf("bad ELF version\n");
        goto bail;
    }

    switch ( (es->eclss = c = ehdr.e32.e_ident[EI_CLASS]) ) {
        default:
            errlogPrintf("bad ELF class\n");
        goto bail;

        case ELFCLASS32:
            n = sizeof(Elf32_Ehdr);
        break;
        case ELFCLASS64:
            n = sizeof(Elf64_Ehdr);
        break;
    }
    n -= EI_NIDENT;

    if ( 0 == fstat(es->fd, &stat_b) ) {
        epicsTimeStamp progStartStamp;
        time_t progStartTime;

        ClockTime_GetProgramStart(&progStartStamp);
        epicsTimeToTime_t(&progStartTime, &progStartStamp);
        if ( stat_b.st_mtime >= progStartTime ) {
            errlogPrintf("elfRead() -- WARNING: '%s' was modified after program start -- symbol information may be inaccurate or invalid\n", fname);
        }
    }

    /* read rest */
    if ( n != do_read(es->fd, ehdr.e32.e_ident + EI_NIDENT, n) ) {
        errlogPrintf("elfRead() -- unable to read ELF ehdr: %s\n", strerror(errno));
        goto bail;
    }

    /* seek to section header table */
    if ( (off_t)-1 == lseek(es->fd, (off_t) FLD(c,ehdr,e_shoff), SEEK_SET) ) {
        errlogPrintf("elfRead() -- unable to seek to shoff: %s\n", strerror(errno));
        goto bail;
    }

    n = ELFCLASS32 == c ? sizeof(shdr.e32) : sizeof(shdr.e64);

    for ( i = 0; i<FLD(c,ehdr,e_shnum); i++ ) {
        if ( n != do_read(es->fd, &shdr, n) ) {
            errlogPrintf("elfRead() -- unable to read section header: %s\n", strerror(errno));
            goto bail;
        }
        if ( SHT_SYMTAB == FLD(c,shdr,sh_type) )
            break;
    }

    if ( i>=FLD(c,ehdr,e_shnum) ) {
        /* no SYMTAB -- try dynamic symbols */

        if ( (off_t)-1 == lseek(es->fd, (off_t) FLD(c,ehdr,e_shoff), SEEK_SET) ) {
            errlogPrintf("elfRead() -- unable to seek to shoff: %s\n", strerror(errno));
            goto bail;
        }

        for ( i = 0; i<FLD(c,ehdr,e_shnum); i++ ) {
            if ( n != do_read(es->fd, &shdr, n) ) {
                errlogPrintf("elfRead() -- unable to read section header: %s\n", strerror(errno));
                goto bail;
            }
            if ( SHT_DYNSYM == FLD(c,shdr,sh_type) )
                break;
        }
    }

    if ( i>=FLD(c,ehdr,e_shnum) ) {
        errlogPrintf("elfRead() -- no symbol table found\n");
        goto bail;
    }

    if ( 0 == (n = (ssize_t) FLD(c,shdr,sh_size)) ) {
        errlogPrintf("elfRead() -- no symbol table data\n");
        goto bail;
    }

    if ( !(es->symMap = getscn(es->fd, c, &shdr)) ) {
        errlogPrintf("elfRead() -- unable to read ELF symtab\n");
        goto bail;
    }

    es->nsyms = n / (ELFCLASS32==c ? sizeof(Elf32_Sym) : sizeof(Elf64_Sym));

    /* find and read string table */
    
    n = ELFCLASS32 == c ? sizeof(shdr.e32) : sizeof(shdr.e64);

    /* seek to section header table */
    if ( (off_t)-1 == lseek(es->fd, (off_t) (FLD(c,ehdr,e_shoff) + n * FLD(c,shdr,sh_link)), SEEK_SET) ) {
        errlogPrintf("elfRead() -- unable to lseek to ELF e_shoff: %s\n", strerror(errno));
        goto bail;
    }

    if ( n != do_read(es->fd, &shdr, n) ) {
        errlogPrintf("elfRead() -- unable to read ELF strtab section header: %s\n", strerror(errno));
        goto bail;
    }

    if ( !(es->strMap = getscn(es->fd,c,&shdr)) ) {
        errlogPrintf("elfRead() -- unable to read ELF strtab\n");
        goto bail;
    }

    /* Make sure there is a terminating NUL - unfortunately, memrchr is not portable */
    cp = (char*)es->strMap->addr + es->strMap->off;
    for ( idx = es->strMap->max - 1; i >= 0; i-- ) {
        if ( !cp[i] )
            break;
    }
    es->strMap->max = idx + 1;

    switch ( FLD(c,ehdr,e_type) ) {
        case ET_EXEC:
            /* Symbols in an executable already has absolute addresses   */
            es->addr = 0;
            break;
        case ET_DYN:
            /* Symbols in an shared library are relative to base address */
            es->addr  = fbase;
            break;
        default:
            errlogPrintf("dlLookupAddr(): Unexpected ELF object file type %u\n", FLD(c,ehdr,e_type));
            goto bail;
    }
        
    return es;

bail:
    elfSymsRelease(es);
    return es;
}

/* Destroy a cached ELF symbol table */
static void
elfSymsDestroy(ESyms es)
{
    if ( es ) {
        elfSymsRelease(es);
        free(es);
    }
}

/* Destroy all cached ELF symbol tables
 *
 * However - w/o proper locking for read access
 * this must not be used. Otherwise, readers
 * will hold stale pointers...
 *
 * We leave the commented code here to show
 * how the tables can be torn down.

void
elfSymTblFlush()
{
ESyms es;

    elfsLockWrite();
    while ( (es = elfs) ) {
        elfs = es->next; 
        es->next = 0;
    	elfsUnlockWrite();
        elfSymsDestroy(es);
    	elfsLockWrite();
    }
   	elfsUnlockWrite();
}

*/


/* This routine must be called with the write-lock held */
static ESyms
elfSymsFind(const char *fname)
{
    ESyms es;

    for ( es=elfs; es && strcmp(fname, es->fname); es = es->next )
        /* nothing else to do */; 
    return es;
}

int
epicsFindAddr(void *addr, epicsSymbol *sym_p)
{
    Dl_info    inf;
    ESyms      es,nes = 0;
    uintptr_t  minoff,off;
    size_t     i;
    Sym        sym;
    Sym        nearest;
    const char *strtab;
    uint8_t    c;
    size_t     idx;

    if ( ! dladdr(addr, &inf) || (!inf.dli_fname && !inf.dli_sname) ) {
        sym_p->f_nam = 0;
        sym_p->s_nam = 0;
        /* unable to lookup   */
        return 0;
    }

    sym_p->f_nam = inf.dli_fname;

    /* If the symbol is in the main executable then solaris' dladdr returns bogus info */
#ifndef __sun
    if ( (sym_p->s_nam = inf.dli_sname) ) {
        sym_p->s_val = inf.dli_saddr;
        /* Have a symbol name - just use it and be done */
        return 0;
    }
#endif

    /* No symbol info; try to access ELF file and ready symbol table from there */

    elfsLockWrite();

    /* See if we have loaded this file already */
    es = elfSymsFind(inf.dli_fname);

    if ( !es ) {

        elfsUnlockWrite();

        if ( ! (nes = elfRead(inf.dli_fname, (uintptr_t)inf.dli_fbase)) )  {
            /* this path can only be taken if there is no memory for '*nes' */
            return 0;
        }

        elfsLockWrite();

        /* Has someone else intervened and already added this file while we were reading ? */
        es = elfSymsFind(inf.dli_fname);

        if ( es ) {
            /* will undo our work in the unlikely event... */
        } else {
            nes->next = elfs;
            es  = elfs = nes;
            nes = 0;
        }
    }

    elfsUnlockWrite();

    /* Undo our work in the unlikely event that it was redundant */
    if ( nes )
        elfSymsDestroy( nes );

    nearest.raw = 0;
    minoff      = (uintptr_t)-1LL;

    if ( es->nsyms ) {
        c       = es->eclss;
        sym.raw = (char*)es->symMap->addr + es->symMap->off;
        strtab  = (char*)es->strMap->addr + es->strMap->off;

        /* Do a brute-force search through the symbol table; if this is executed 
         * very often then it would be worthwhile constructing a sorted list of
         * symbol addresses but for the stack trace we don't care...
         */
#if (FIND_ADDR_DEBUG & 1)
        printf("Looking for %p\n", addr);
#endif

        if ( ELFCLASS32 == c ) {
            for ( i=0; i<es->nsyms; i++ ) {
                if ( STT_FUNC != ELF32_ST_TYPE(sym.e32[i].st_info) )
                    continue;
                /* don't bother about undefined symbols */
                if ( 0 == sym.e32[i].st_shndx )
                    continue;
#if (FIND_ADDR_DEBUG & 1)
                printf("Trying: %s (0x%lx)\n", strtab + sym.e32[i].st_name, (unsigned long)(sym.e32[i].st_value + es->addr));
#endif
                if ( (uintptr_t)addr >= (uintptr_t)sym.e32[i].st_value + es->addr ) {
                    off = (uintptr_t)addr - ((uintptr_t)sym.e32[i].st_value + es->addr);
                    if ( off < minoff ) {
                        minoff      = off;
                        nearest.e32 = &sym.e32[i];
                    }
                }
            }
        } else {
            for ( i=0; i<es->nsyms; i++ ) {
                if ( STT_FUNC != ELF64_ST_TYPE(sym.e64[i].st_info) )
                    continue;
                /* don't bother about undefined symbols */
                if ( 0 == sym.e64[i].st_shndx )
                    continue;
#if (FIND_ADDR_DEBUG & 1)
                printf("Trying: %s (0x%llx)\n", strtab + sym.e64[i].st_name, (unsigned long long)(sym.e64[i].st_value + es->addr));
#endif
                if ( (uintptr_t)addr >= (uintptr_t)sym.e64[i].st_value + es->addr ) {
                    off = (uintptr_t)addr - ((uintptr_t)sym.e64[i].st_value + es->addr);
                    if ( off < minoff ) {
                        minoff      = off;
                        nearest.e64 = &sym.e64[i];
                    }
                }
            }
        }
    }

    if ( nearest.raw && ( (idx = ARR(c,nearest,0,st_name)) < es->strMap->max ) ) {
        sym_p->s_nam = strtab + idx;
        sym_p->s_val = (char*) ARR(c, nearest, 0, st_value) + es->addr;
    }

    return 0;
}

int epicsFindAddrGetFeatures(void)
{
    /* The static information given here may not be correct;
     * it also depends on
     *  - compilation (frame pointer optimization)
     *  - linkage (static vs. dynamic)
     *  - stripping
     */
    return  EPICS_STACKTRACE_LCL_SYMBOLS
          | EPICS_STACKTRACE_GBL_SYMBOLS
          | EPICS_STACKTRACE_DYN_SYMBOLS;
}
