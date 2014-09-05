/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */ 

/* Make sure dladdr() is visible on linux/freebsd/darwin */
#define _GNU_SOURCE
/* Some freebsd versions seem to export dladdr() only if __BSD_VISIBLE */
#define _BSD_VISIBLE
#define _DARWIN_C_SOURCE

#include "epicsStackTrace.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include <execinfo.h>
#include <errlog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* How many stack frames to capture               */
#define MAXDEPTH 100
/* How many chars to reserve for a line of output */
#define MAXSYMLEN 500


#define STACKTRACE_DEBUG 0

/* Darwin and GNU have dladdr() and Darwin's already finds local
 * symbols, too, whereas linux' does not.
 * Hence, on linux we want to use dladdr() and lookup static
 * symbols in the ELF symbol table.
 */

#include <dlfcn.h>

#if defined(__linux__) || defined(linux)
#define USE_ELF
#define USE_MMAP
#elif defined(freebsd)
#define USE_ELF
#define USE_MMAP
#endif

#ifdef  USE_ELF
#include <elf.h>
#include <errno.h>
#include <inttypes.h>

#ifdef  USE_MMAP
#include <sys/mman.h>
#endif /* USE_MMAP     */

#endif /* USE_ELF      */

/* Forward Declaration */
#define NO_OFF ((unsigned long)-1L)

static ssize_t
symDump(char *buf, size_t buf_sz, void *addr, const char *fnam, const char *snam, unsigned long off);

#ifdef USE_ELF

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
    uint8_t        class;
} *ESyms;

/* LOCKING NOTE: if the ELF symbol facility is ever expanded to be truly used
 * in a multithreaded way then proper multiple-readers, single-writer locking
 * should be implemented:
 *    - elfsLockWrite() must block until all readers have left
 *    - elfsLockRead()  must block until writer has left.
 *    - elfsLockConvertWriteRead() atomically converts writer holding the
 *      writer's lock into a reader.
 * Right now we just use a single, global lock (for the stack trace) since we
 * only need to guard against multiple threads dumping stacks simultaneously and
 * we do not lock the symbol table(s) at all.
 */

/* Linked list where we keep all our ESyms */
static ESyms             elfs       = 0;

static void
elfsLockWrite()
{
    /* Only a single writer can hold this while no readers are active */
}

static void
elfsUnlockWrite()
{
    /* Must wake up readers blocking in elfsLockRead()                */
}

static void
elfsLockConvertWriteRead()
{
    /* Must atomically convert a writer into a reader, i.e., unlock
     * the writer's lock and atomically acquire the reader's lock
     */
}

static void
elfsLockRead()
{
    /* Multiple readers can hold this while the writer is not active  */
}

static void
elfsUnlockRead()
{
    /* Must wake up a (single) writer blocking in elfsLockWrite       */
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
static size_t
do_read(int fd, void *buf, size_t sz)
{
size_t got;
void   *ptr=buf;
    while ( sz > 0 ) {
        if ( (got=read(fd,ptr,sz)) < 0 ) {
            return got;
        }
        ptr+=got;
        sz -=got;
    }
    return ptr-buf;
}

#ifdef USE_MMAP

/* Destructor for data that is mmap()ed */
static void
freeMapMmap(MMap m)
{
    if ( MAP_FAILED != m->addr )
        munmap( m->addr, m->len );
}

/* Obtain section data with mmap()      */
static MMap
getscn(int fd, uint8_t c, Shdr *shdr_p)
{
size_t   n;
MMap     rval = 0;
size_t   pgsz  = sysconf(_SC_PAGESIZE);

    if ( 0 == (n=FLD(c,(*shdr_p),sh_size)) ) {
        errlogPrintf("elfRead - getscn() -- no section data\n");
        goto bail;
    }

    if ( ! (rval = malloc(sizeof(*rval))) ) {
        errlogPrintf("elfRead - getscn() -- no memory for section map\n");
        goto bail;
    }

    rval->freeMap = freeMapMmap;

    rval->off = FLD(c,(*shdr_p),sh_offset)   & (pgsz-1);
    rval->len = (n + rval->off + (pgsz - 1)) & ~(pgsz - 1);
    rval->max = rval->len - rval->off;

    if ( MAP_FAILED == (rval->addr = mmap(0, rval->len, PROT_READ, MAP_SHARED, fd, FLD(c,(*shdr_p),sh_offset) & ~(pgsz-1))) ) {
        errlogPrintf("elfRead - getscn() -- mapping section contents: %s\n", strerror(errno));
        goto bail;
    }

    return rval;

bail:
    freeMap(rval);
    return 0;
}

#else /* USE_MMAP */

/* Destructor for data that is read into a malloc()ed buffer */
static void
freeMapMalloc(MMap m)
{
    free(m->addr);
}

/* Read section data into a malloc()ed buffer                */
static MMap
getscn(int fd, uint8_t c, Shdr *shdr_p)
{
size_t   n;
MMap     rval = 0;

    if ( 0 == (n=FLD(c,(*shdr_p),sh_size)) ) {
        errlogPrintf("elfRead - getscn() -- no section data\n");
        goto bail;
    }

    if ( ! (rval = malloc(sizeof(*rval))) ) {
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
    if ( (off_t)-1 == lseek(fd, FLD(c,(*shdr_p),sh_offset), SEEK_SET) ) {
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
#endif /* USE_MMAP */

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
int         i,n;
Ehdr        ehdr;
Shdr        shdr;
uint8_t     c;
ESyms       es;
ssize_t     idx;
const char *cp;

    if ( !(es = malloc(sizeof(*es))) ) {
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

    switch ( (es->class = c = ehdr.e32.e_ident[EI_CLASS]) ) {
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

    /* read rest */
    if ( n != do_read(es->fd, ehdr.e32.e_ident + EI_NIDENT, n) ) {
        errlogPrintf("elfRead() -- unable to read ELF ehdr: %s\n", strerror(errno));
        goto bail;
    }

    /* seek to section header table */
    if ( (off_t)-1 == lseek(es->fd, FLD(c,ehdr,e_shoff), SEEK_SET) ) {
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
        errlogPrintf("elfRead() -- no symbol table found\n");
        goto bail;
    }

    if ( 0 == (n=FLD(c,shdr,sh_size)) ) {
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
    if ( (off_t)-1 == lseek(es->fd, FLD(c,ehdr,e_shoff) + n * FLD(c,shdr,sh_link), SEEK_SET) ) {
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
    cp = es->strMap->addr + es->strMap->off;
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
            errlogPrintf("elfLookupAddr(): Unexpected ELF object file type %u\n", FLD(c,ehdr,e_type));
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

/* Destroy all cached ELF symbol tables */
static void
elfSymsFlush()
{
ESyms es;

    elfsLockWrite();
    while ( (es = elfs) ) {
        elfs = es->next; 
        es->next = 0; /* paranoia */
        elfSymsDestroy(es);
    }
    elfsUnlockWrite();

}

static ESyms
elfSymsFind(const char *fname)
{
ESyms es;
    for ( es=elfs; es && strcmp(fname, es->fname); es = es->next )
        /* nothing else to do */; 
    return es;
}
#endif /* USE_ELF */

static ssize_t
elfLookupAddr(void *addr, char *buf, size_t buf_sz)
{
Dl_info    inf;
ssize_t    rval;

#ifdef USE_ELF
ESyms      es,nes;
uintptr_t  minoff,off;
int        i;
Sym        sym;
Sym        nearest;
const char *strtab;
uint8_t    c;
size_t     idx;
#endif

    if ( ! dladdr(addr, &inf) || (!inf.dli_fname && !inf.dli_sname) ) {
        /* unable to lookup   */
        return symDump(buf, buf_sz, addr, 0, 0, NO_OFF);
    }

    if ( inf.dli_sname ) {
        /* Have a symbol name - just use it and be done */
        return symDump(buf, buf_sz, addr, inf.dli_fname, inf.dli_sname, (unsigned long)(addr - inf.dli_saddr));
    }

#ifndef USE_ELF

    rval = symDump(buf, buf_sz, addr, inf.dli_fname, 0, NO_OFF);

#else

    /* No symbol info; try to access ELF file and ready symbol table from there */

    elfsLockRead();

    /* See if we have loaded this file already */
    es = elfSymsFind(inf.dli_fname);

    if ( !es ) {
        elfsUnlockRead();

        if ( ! (nes = elfRead(inf.dli_fname, (uintptr_t)inf.dli_fbase)) )  {
            /* this path can only be taken if there is no memory for '*nes' */
            if ( buf && buf_sz > 0 )
                *buf = 0;
            return 0;
        }

        elfsLockWrite();

        /* Has someone else intervened and already added this file while we were reading ? */
        es = elfSymsFind(inf.dli_fname);

        if ( es ) {
            /* undo our work in the unlikely event... */
            elfSymsDestroy( nes );
        } else {
            nes->next = elfs;
            es = elfs = nes;
        }
        elfsLockConvertWriteRead();
    }

    nearest.raw = 0;
    minoff      = (uintptr_t)-1LL;

    if ( es->nsyms ) {
        c       = es->class;
        sym.raw = es->symMap->addr + es->symMap->off;
        strtab  = es->strMap->addr + es->strMap->off;

        /* Do a brute-force search through the symbol table; if this is executed 
         * very often then it would be worthwhile constructing a sorted list of
         * symbol addresses but for the stack trace we don't care...
         */
#if (STACKTRACE_DEBUG & 1)
        printf("Looking for %p\n", addr);
#endif

        if ( ELFCLASS32 == c ) {
            for ( i=0; i<es->nsyms; i++ ) {
                if ( STT_FUNC != ELF32_ST_TYPE(sym.e32[i].st_info) )
                    continue;
                /* don't bother about undefined symbols */
                if ( 0 == sym.e32[i].st_shndx )
                    continue;
#if (STACKTRACE_DEBUG & 1)
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
#if (STACKTRACE_DEBUG & 1)
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
        rval = symDump(buf, buf_sz, addr, es->fname, strtab + idx, (unsigned long)minoff);
    } else {
        rval = symDump(buf, buf_sz, addr, es->fname, 0, NO_OFF);
    }

    elfsUnlockRead();

#endif /* USE_ELF */

    return rval;
}

static epicsThreadOnceId stackTraceInitId = EPICS_THREAD_ONCE_INIT;
static epicsMutexId      stackTraceMtx;

static void stackTraceInit(void *unused)
{
    stackTraceMtx = epicsMutexMustCreate();
}

static void stackTraceLock(void)
{
    epicsThreadOnce( &stackTraceInitId, stackTraceInit, 0 );
    epicsMutexLock( stackTraceMtx );
}

static void stackTraceUnlock(void)
{
    epicsMutexUnlock( stackTraceMtx );
}

static ssize_t
dump(char **buf, size_t *buf_sz, size_t *good, const char *fmt, ...)
{
va_list ap;
ssize_t rval, put;
    va_start(ap, fmt);
        if ( *buf ) {
            put = rval = vsnprintf(*buf, *buf_sz, fmt, ap);
            if ( put > *buf_sz )
                put = *buf_sz;
            *buf    += put;
            *buf_sz -= put;
        } else {
            rval = errlogVprintf(fmt, ap);
        }
    va_end(ap);
    if ( rval > 0 )
        *good += rval;
    return rval;
}


static ssize_t
symDump(char *buf, size_t buf_sz, void *addr, const char *fnam, const char *snam, unsigned long off)
{
size_t rval = 0;

    dump( &buf, &buf_sz, &rval, "[%*p]", sizeof(addr)*2 + 2, addr);
    if ( fnam ) {
        dump( &buf, &buf_sz, &rval, ": %s", fnam );
    }
    if ( snam ) {
        dump( &buf, &buf_sz, &rval, "(%s", snam );
        if ( NO_OFF != off ) {
            dump( &buf, &buf_sz, &rval, "+0x%lx", off);
        }
        dump( &buf, &buf_sz, &rval, ")" );
    }
    dump( &buf, &buf_sz, &rval, "\n");

    return rval;
}

epicsShareFunc void epicsStackTrace(void)
{
void   **buf;
char   *btsl  = 0;
size_t btsl_sz = sizeof(*btsl)*MAXSYMLEN;
int    i,n;

    if ( ! (buf = malloc(sizeof(*buf) * MAXDEPTH))
         || ! (btsl = malloc(btsl_sz))
       ) {
        free(buf);
        errlogPrintf("epicsStackTrace(): not enough memory for backtrace\n");
        return;
    }

    n = backtrace(buf, MAXDEPTH);

    stackTraceLock();

    errlogPrintf("Dumping a stack trace of thread '%s':\n", epicsThreadGetNameSelf());

    errlogFlush();

    for ( i=0; i<n; i++ ) {
        /* Somehow errlog doesn't like small, broken-up pieces of lines which is
         * why we assemble into the 'btsl' buffer and use a single errlogPrintf...
         */
        elfLookupAddr(buf[i], btsl, btsl_sz);
        errlogPrintf("%s", btsl);
    }
#ifdef USE_ELF
    elfSymsFlush();
#endif

    free(btsl);
    btsl = 0;

    errlogPrintf("\n");

    errlogFlush();

    stackTraceUnlock();

    free(buf);
}


epicsShareFunc int epicsStackTraceGetFeatures(void)
{
#if (STACKTRACE_DEBUG & 2)
    errlogPrintf("Configuration -- ELF: ");
#ifdef USE_ELF
    errlogPrintf("yes");
#else
    errlogPrintf("no");
#endif
    errlogPrintf(", MMAP: ");
#ifdef USE_MMAP
    errlogPrintf("yes");
#else
    errlogPrintf("no");
#endif
    errlogPrintf("\n");
#endif

    /* We are a bit conservative here. The actual
     * situation depends on how we are linked (something
     * we don't have under control at compilation time)
     * Linux' dladdr finds global symbols 
     * (not from dynamic libraries) when statically linked but
     * not when dynamically linked.
     * OTOH: for a stripped executable it is unlikely that
     * even the ELF reader is able to help much...
     */

#ifdef USE_ELF
    return  EPICS_STACKTRACE_LCL_SYMBOLS
          | EPICS_STACKTRACE_GBL_SYMBOLS
          | EPICS_STACKTRACE_DYN_SYMBOLS
          | EPICS_STACKTRACE_ADDRESSES;
#elif defined(__linux__) || defined(linux)
    return  EPICS_STACKTRACE_DYN_SYMBOLS
          | EPICS_STACKTRACE_ADDRESSES;
#else
    return  EPICS_STACKTRACE_LCL_SYMBOLS
          | EPICS_STACKTRACE_GBL_SYMBOLS
          | EPICS_STACKTRACE_DYN_SYMBOLS
          | EPICS_STACKTRACE_ADDRESSES;
#endif
}
