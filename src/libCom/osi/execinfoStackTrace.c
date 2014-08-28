/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */ 

#include "epicsStackTrace.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include <execinfo.h>
#include <errlog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#define MAXDEPTH 100

#define STACKTRACE_DEBUG 0

/* Darwin and GNU have dladdr() but Darwin's backtrace_symbols()
 * already prints local symbols, too, whereas linux' does not.
 * Hence, on linux we want to use dladdr() and lookup static
 * symbols in the ELF symbol table.
 */
#define  __USE_GNU

#ifdef   freebsd
/* Some freebsd versions seem to export dladdr() only if __BSD_VISIBLE */
#define  __BSD_VISIBLE  1
#endif

#include <dlfcn.h>

/* Check if we actually have the gnu/darwin extensions           */
#ifdef RTLD_DEFAULT

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

#else  /* RTLD_DEFAULT */
#undef  USE_ELF
#endif /* RTLD_DEFAULT */

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

/* Linked list where we keep all our ESyms */
static ESyms             elfs       = 0;
static epicsThreadOnceId elfsInitId = EPICS_THREAD_ONCE_INIT;
static epicsMutexId      elfsMtx;

static void elfsInit(void *unused)
{
	elfsMtx = epicsMutexMustCreate();
}

static void elfsLock(void)
{
	epicsThreadOnce( &elfsInitId, elfsInit, 0 );
	epicsMutexLock( elfsMtx );
}

static void elfsUnlock(void)
{
	epicsMutexUnlock( elfsMtx );
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

#if 0
/* IMPLEMENTATION IS INCORRECT: 
 * with only a single lock 'elfLookupAddr()' would
 * have to hold that the entire time while accessing
 * a symbol table. This is inefficient if there are
 * multiple readers. A better implementation would 
 * use a 'multiple-readers/single-writer' type of lock
 * to protect the symbol tables.
 * 
 * ATM we simply don't support destroying the
 * tables (after dumping a stack trace the process is
 * likely to be terminated anyways).
 */

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

	elfsLock();
	while ( (es = elfs) ) {
		elfs = es->next; 
		es->next = 0; /* paranoia */
		elfSymsDestroy(es);
	}
	elfsUnlock();

}
#endif

static void
elfLookupAddr(void *addr)
{
Dl_info    inf;
ESyms      es;
uintptr_t  minoff,off;
int        i;
Sym        sym;
Sym        nearest;
const char *strtab;
uint8_t    c;
size_t     idx;

	if ( ! dladdr(addr, &inf) || (!inf.dli_fname && !inf.dli_sname) ) {
		/* unable to lookup   */
		errlogPrintf("[%p]\n", addr);
		return;
	}

	if ( inf.dli_sname ) {
		/* Have a symbol name - just use it and be done */
		errlogPrintf("%s(%s+0x%lu): [%p]\n", inf.dli_fname ? inf.dli_fname : "", inf.dli_sname, addr - inf.dli_saddr, addr);
		return;
	}

	/* No symbol info; try to access ELF file and ready symbol table from there */

	elfsLock();

	/* See if we have loaded this file already */
	for ( es=elfs; es && strcmp(inf.dli_fname, es->fname); es = es->next )
		/* nothing else to do */; 

	if ( !es ) {
		if ( ! (es = elfRead(inf.dli_fname, (uintptr_t)inf.dli_fbase)) )  {
			elfsUnlock();
			/* this path can only be taken if there is no memory for '*es' */
			return;
		}
		es->next = elfs;
		elfs = es;
	}

	elfsUnlock();

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
		errlogPrintf("%s(%s+0x%"PRIxPTR"): [%p]\n", es->fname, strtab + idx, minoff, addr);
	} else {
		errlogPrintf("%s[%p]\n", es->fname, addr);
	}
}
#endif

epicsShareFunc void epicsStackTrace(void)
{
void **buf;
#ifndef USE_ELF
char **bts;
#endif
int    i,n;

	errlogFlush();

	if ( ! (buf = malloc(sizeof(*buf) * MAXDEPTH)) ) {
		errlogPrintf("epicsStackTrace(): not enough memory for backtrace\n");
		return;
	}

	n = backtrace(buf, MAXDEPTH);

	/* backtrace_symbols() only works for global symbols on linux.
	 * If we have dladdr() and then we can actually lookup local
	 * symbols, too.
	 */
#ifdef USE_ELF
	for ( i=0; i<n; i++ ) {
		elfLookupAddr(buf[i]);
	}
#if 0
	elfSymsFlush();
#endif
#else
	if ( (bts = backtrace_symbols(buf, n)) ) {
		for ( i=0; i<n; i++ ) {
			errlogPrintf("%s\n", bts[i]);
		}
		free(bts);
	} else {
		/* failed to create symbolic information; just print addresses */
		for ( i=0; i<n; i++ ) {
			errlogPrintf("[%p]\n", buf[i]);
		}
	}
#endif

	errlogFlush();

	free(buf);
}


epicsShareFunc int epicsStackTraceGetFeatures(void)
{
#ifdef USE_ELF
	return  EPICS_STACKTRACE_LCL_SYMBOLS
	      | EPICS_STACKTRACE_GBL_SYMBOLS
	      | EPICS_STACKTRACE_ADDRESSES;
#elif defined(__linux__) || defined(linux)
	return  EPICS_STACKTRACE_GBL_SYMBOLS
	        EPICS_STACKTRACE_ADDRESSES;
#else
	return  EPICS_STACKTRACE_LCL_SYMBOLS
	      | EPICS_STACKTRACE_GBL_SYMBOLS
	      | EPICS_STACKTRACE_ADDRESSES;
#endif
}
